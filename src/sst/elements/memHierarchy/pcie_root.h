// Copyright 2009-2019 NTESS. Under the terms
// of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.
//
// Copyright (c) 2009-2019, NTESS
// All rights reserved.
//
// Portions are copyright of other developers:
// See the file CONTRIBUTORS.TXT in the top level directory
// the distribution for more information.
//
// This file is part of the SST software package. For license
// information, see the LICENSE file in the top level directory of the
// distribution.

#ifndef MEMHIERARCHY_SHMEM_NIC_H
#define MEMHIERARCHY_SHMEM_NIC_H

#include <sst/core/sst_types.h>

#include <sst/core/component.h>
#include <sst/core/event.h>

#include "sst/elements/memHierarchy/memEvent.h"
#include "sst/elements/memHierarchy/memLinkBase.h"

namespace SST {
namespace MemHierarchy {

class PCIE_Root : public SST::Component {
  public:

    SST_ELI_REGISTER_COMPONENT(PCIE_Root, "memHierarchy", "PCIE_Root", SST_ELI_ELEMENT_VERSION(1,0,0),
        "PCIE Root, interfaces to a main memory model", COMPONENT_CATEGORY_MEMORY)

#define MEMCONTROLLER_ELI_PORTS \
            {"direct_link", "Direct connection to a cache/directory controller", {"memHierarchy.MemEventBase"} },\
            {"network",     "Network connection to a cache/directory controller; also request network for split networks", {"memHierarchy.MemRtrEvent"} },\
            {"network_ack", "For split networks, ack/response network connection to a cache/directory controller", {"memHierarchy.MemRtrEvent"} },\
            {"network_fwd", "For split networks, forward request network connection to a cache/directory controller", {"memHierarchy.MemRtrEvent"} },\
            {"network_data","For split networks, data network connection to a cache/directory controller", {"memHierarchy.MemRtrEvent"} },\
            {"cache_link",  "Link to Memory Controller", { "memHierarchy.memEvent" , "" } }, \

    SST_ELI_DOCUMENT_STATISTICS(
        { "bytesRead",    "bytes read",  "count", 1 },
        { "bytesWritten",   "bytes written", "count", 1 },
        { "opRate",   "latency between sends to pci_link", "latency", 1 },
        { "opLatencyRead",   "latency of read pci_link", "latency", 1 },
        { "opLatencyWrite",   "latency of write pci_link", "latency", 1 },
    )

    SST_ELI_DOCUMENT_PORTS( MEMCONTROLLER_ELI_PORTS )

    PCIE_Root(ComponentId_t id, Params &params);

    virtual void init(unsigned int);
    void processInitEvent( MemEventInit* me );
    virtual void setup();
    void finish();

	Addr translateToLocal(Addr addr);
	Addr translateToGlobal(Addr addr);

  protected:
    PCIE_Root();  // for serialization only
    ~PCIE_Root() {}

  private:

    struct MemRequest {

        typedef std::function<void(MemEvent*)> Callback;
        
        enum Op { Write, Read, Fence } m_op;
        MemRequest( int src, uint64_t addr, int dataSize, uint8_t* data, Callback* callback = NULL  ) : 
            callback(callback), src(src), m_op(Write), addr(addr), dataSize(dataSize) { 
            buf.resize( dataSize );
            memcpy( buf.data(), data, dataSize );
        }
        MemRequest( int src, uint64_t addr, int dataSize, uint64_t data, Callback* callback = NULL  ) : 
            callback(callback), src(src), m_op(Write), addr(addr), dataSize(dataSize), data(data) { }
        MemRequest( int src, uint64_t addr, int dataSize, Callback* callback = NULL ) :
            callback(callback), src(src), m_op(Read), addr(addr), dataSize(dataSize) { }
        MemRequest( int src, Callback* callback = NULL ) : callback(callback), src(src), m_op(Fence), dataSize(0) {} 
        ~MemRequest() { }
        bool isFence() { return m_op == Fence; }
        uint64_t reqTime;

        virtual void handleResponse( MemEvent* event ) { 
            if ( callback ) {
                (*callback)( event );
            } else {
                delete event; 
            }
        }

        Callback* callback;
        int      src;
        uint64_t addr;
        int      dataSize;
        uint64_t data;
        std::vector<uint8_t> buf;
    };

    void updateStatistics( bool isRead, SimTime_t lat ) {
        if ( isRead ) {
            m_opLatencyRead->addData( lat );
        } else {
            m_opLatencyWrite->addData( lat );
        }
    }

	bool isRead( MemEvent* me ) {
		dbg.debug( CALL_INFO,1,0,"%s %s %s\n",getName().c_str(),
			me->getBriefString().c_str(), me->queryFlag(MemEvent::F_NONCACHEABLE)? "NONCACHEABLE":"CACHEABLE");
		switch ( me->getCmd() )  {
			case Command::PutM:
			case Command::PrWrite:
				return false;

			case Command::GetX:
				if ( me->queryFlag(MemEvent::F_NONCACHEABLE) ) {
					return false;
				} else {
					return true;
				}

			case Command::GetS:
			case Command::PrRead:
				return true;

			default:
				printf("%s %s() %s\n",getName().c_str(),__func__,me->getBriefString().c_str());
				assert(0);
				return false;
		}
	}

    void handleTargetEvent( SST::Event* );
    void handlePCI_linkEvent( SST::Event* );
    virtual bool clock( SST::Cycle_t );

	bool handleResponseFromHost( MemEvent* );
	void handleNackFromHost( MemEvent*, bool );

	MemLinkBase* m_pciLink;      // PCI Link 
	MemLinkBase* m_memLink;      // Link to the rest of memHierarchy
	bool m_clockLink;            // Flag - should we call clock() on this link or not

	TimeConverter *m_clockTC;
	Clock::HandlerBase *m_clockHandler;

	MemRegion m_region; // Which address region we are, for translating to local addresses

	Output out;
	Output dbg;

	std::queue<MemEventBase*> m_toHostEventQ;
	std::queue<MemEventBase*> m_toDevEventQ;

	std::map< Addr, std::list<Event::id_type> > m_devOriginPendingMap;

    struct Entry {
        Entry( MemEventBase* event, SimTime_t time, bool isRead ) : event(event), time(time), isRead(isRead) {}
		MemEventBase* event;
        SimTime_t time;
		bool isRead;
    };

	std::map< SST::Event::id_type, Entry > m_hostOriginPendingMap;

	Addr m_privateMemOffset;

	Statistic<uint64_t>* m_opRate;
	SimTime_t 			 m_lastSend;

    Statistic<uint64_t>* m_bytesRead;
    Statistic<uint64_t>* m_bytesWritten;

    Statistic<uint64_t>* m_opLatencyRead;
    Statistic<uint64_t>* m_opLatencyWrite;
};

}
}

#endif
