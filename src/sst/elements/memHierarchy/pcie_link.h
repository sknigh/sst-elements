// Copyright 2013-2019 NTESS. Under the terms
// of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.
//
// Copyright (c) 2013-2019, NTESS
// All rights reserved.
//
// Portions are copyright of other developers:
// See the file CONTRIBUTORS.TXT in the top level directory
// the distribution for more information.
//
// This file is part of the SST software package. For license
// information, see the LICENSE file in the top level directory of the
// distribution.

#ifndef _MEMHIERARCHY_PCIE_LINK_SUBCOMPONENT_H_
#define _MEMHIERARCHY_PCIE_LINK_SUBCOMPONENT_H_

#include <string>
#include <map>
#include <queue>

#include <sst/core/event.h>
#include <sst/core/output.h>
#include <sst/core/subcomponent.h>

#include "sst/elements/memHierarchy/memEventBase.h"
#include "sst/elements/memHierarchy/memEvent.h"
#include "sst/elements/memHierarchy/util.h"
#include "sst/elements/memHierarchy/memTypes.h"
#include "sst/elements/memHierarchy/memLinkBase.h"

namespace SST {
namespace MemHierarchy {

class PCIE_Link : public MemLinkBase {

	class SelfEvent : public Event {
	  public:
		enum Type { OutQ, InQ } type;
		SelfEvent( Type type ) : Event(), type( type ) {}	
		NotSerializable( SST::MemHierarchy::PCIE_Link::SelfEvent);
	};

	class LinkEvent : public Event {

	  public:

		enum Type { TLP, DLLP } type;

		LinkEvent() : Event() {}
		LinkEvent(MemEventBase* ev, size_t pktLen) : Event(), memEventBase(ev), pktLen(pktLen), type( TLP ) {}
		LinkEvent( Type type, size_t pktLen ) : Event(), type(type), pktLen(pktLen) {}
#if 0
		LinkEvent(const LinkEvent* me) : Event() { copy( *this, *me ); }
    	LinkEvent(const LinkEvent& me) : Event() { copy( *this, me ); }
#endif

		MemEventBase* memEventBase;
		size_t pktLen;

      private:

#if 0
    	void copy( LinkEvent& to, const LinkEvent& from ) {
			to.memEventBase = from.memEventBase;
			to.type = from.type;
		}
#endif

	  public:
#if 0
   		void serialize_order(SST::Core::Serialization::serializer &ser)  override {
        	Event::serialize_order(ser);
			ser & memEventBase;
			ser & type;
    	}
    	ImplementSerializable(SST::MemHierarchy::PCIE_Link::LinkEvent);
#endif

		NotSerializable( SST::MemHierarchy::PCIE_Link::LinkEvent);
	};

public:
    
    SST_ELI_REGISTER_SUBCOMPONENT_DERIVED(PCIE_Link, "memHierarchy", "PCIE_Link", SST_ELI_ELEMENT_VERSION(1,0,0),
            "", SST::MemHierarchy::MemLinkBase)

   	SST_ELI_DOCUMENT_PARAMS( )

    SST_ELI_DOCUMENT_STATISTICS()

    SST_ELI_DOCUMENT_SUBCOMPONENT_SLOTS(
#if 0
            {"ack", "Link control subcomponent to acknowledgement network", "SST::Interfaces::SimpleNetwork"},
#endif
	)

/* Begin class definition */    

    PCIE_Link(ComponentId_t id, Params &params);
public:
    /* Destructor */
    ~PCIE_Link() { }

    /* Functions called by parent for handling events */
    bool clock();
    virtual void send(MemEventBase * ev); 

    std::set<EndpointInfo>* getSources() { return NULL; }
    std::set<EndpointInfo>* getDests() { return NULL; }
    bool isDest(std::string UNUSED(str)) { return false; }
    bool isSource(std::string UNUSED(str)) { return false; }
    std::string findTargetDestination(Addr addr) { return ""; }

    /* Send and receive functions for MemLink */
    void sendInitData(MemEventInit * ev) {};
    MemEventInit* recvInitData( ) { return NULL; }

    MemEventBase * recv() { return NULL;}

    /* Initialization and finish */
    void init(unsigned int phase);
    void setup();

private:
	void handleEvent(SST::Event *ev);
	void selfLinkHandler(Event* ev);
	void recvPkt( LinkEvent* le );
	void pushLinkEvent( LinkEvent* le );

	
	size_t calcPktLength( MemEventBase* ev) {
		auto* me = static_cast<MemEvent*>(ev);
		switch ( ev->getCmd() ) {
			case Command::GetX: // write non-cacheable
			case Command::PrWrite:
				return  m_dllpHdrLength + m_tlpSeqLength + m_tlpHdrLength + me->getPayload().size() + m_lcrcLength;

			case Command::GetS: // read non-cacheable
			case Command::PrRead:
				return  m_dllpHdrLength + m_tlpSeqLength + m_tlpHdrLength + m_lcrcLength;

			case Command::GetXResp: // PrWrite or GetX  Resp
				return  m_dllpHdrLength + m_tlpSeqLength + m_tlpHdrLength + m_lcrcLength;

			case Command::GetSResp: // PrRead or GetS  Resp
				return  m_dllpHdrLength + m_tlpSeqLength + m_tlpHdrLength + me->getPayload().size() + m_lcrcLength;

			default:
				assert(0);
		}
		return 16;
	}

	SimTime_t calcNumClocks(size_t length ) {
		SimTime_t clocks = ( length / m_linkWidthBytes ) + ( length % m_linkWidthBytes ? 1 : 0 ); 
		m_dbg.debug(CALL_INFO,1,0,"length=%zu bytesPerClock=%d clocks=%" PRIu64 " lat=%" PRIu64" ps.\n",
				length, m_linkWidthBytes, clocks, clocks * m_latPerByte_ps);
		return clocks; 
	}

	uint32_t m_linkWidthBytes;
	SimTime_t m_latPerByte_ps;

	uint32_t m_tlpHdrLength;
	uint32_t m_tlpSeqLength;
	uint32_t m_lcrcLength;
	uint32_t m_dllpHdrLength;
	
	uint32_t m_fc_numPH;
	uint32_t m_fc_numPD;
	uint32_t m_fc_numNPH;
	uint32_t m_fc_numNPD;
	uint32_t m_fc_numCPLH;
	uint32_t m_fc_numCPLD;
	Output m_dbg;
	Link* m_link;
	Link* m_selfLink;
	std::map<Event::id_type, MemEventBase*> m_req;
	std::queue<LinkEvent*> m_outQ;
	std::queue<LinkEvent*> m_inQ;
};

} //namespace memHierarchy
} //namespace SST

#endif
