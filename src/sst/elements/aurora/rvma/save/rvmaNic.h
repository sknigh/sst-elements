// Copyright 2013-2018 NTESS. Under the terms
// of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.
//
// Copyright (c) 2013-2018, NTESS
// All rights reserved.
//
// Portions are copyright of other developers:
// See the file CONTRIBUTORS.TXT in the top level directory
// the distribution for more information.
//
// This file is part of the SST software package. For license
// information, see the LICENSE file in the top level directory of the
// distribution.


#ifndef COMPONENTS_RVMA_NIC_H
#define COMPONENTS_RVMA_NIC_H

#include <queue>
#include "include/foo.h"
#include "rvmaNicCmds.h"
#include "rvmaNetworkPkt.h"
#include "sst/elements/hermes/rvma.h"

namespace SST {
namespace Snotra {
namespace RVMA {

class RvmaNicSubComponent : public Snotra::NicSubComponent {

  public:
    SST_ELI_REGISTER_SUBCOMPONENT(
        RvmaNicSubComponent,
        "Snotra",
        "RVMAnic",
        SST_ELI_ELEMENT_VERSION(1,0,0),
        "",
        ""
    )

    SST_ELI_DOCUMENT_PARAMS(
        {"verboseLevel","Sets the level of debug verbosity",""},
        {"verboseMask","Sets the debug mask",""},
    )

    RvmaNicSubComponent( Component* owner, Params& params );
	void setup();

	void handleEvent( int core, Event* event );
    bool clockHandler( Cycle_t );

	struct Buffer {
		Buffer ( Hermes::MemAddr addr, size_t size, Hermes::RVMA::Completion* completion) : addr(addr), size(size), completion(completion) {}
		Hermes::MemAddr addr;
		size_t size;
		size_t count;
		Hermes::RVMA::Completion* completion;
	};

	Link* m_selfLink;

	enum Protocol { RVMA, RDMA, MSG };  

	class SendEntry {
	  public:
	  	SendEntry( int srcCore, PutCmd* cmd ) : srcCore(srcCore), cmd(cmd), currentOffset(0), completedDMAs(0), state(Head)  {
			totalDMAs = cmd->size / RvmaNicSubComponent::getPktSize();
			if ( cmd->size % RvmaNicSubComponent::getPktSize() ) { 
				++totalDMAs;
			}
		} 

		void incCompletedDMAs() { ++completedDMAs; }
		void decRemainingBytes( int num ) { currentOffset += num; }
		int remainingBytes() { return cmd->size - currentOffset; }
		size_t offset() { return currentOffset + cmd->offset; }
		int getDestNode() { return cmd->proc.node; }
		bool done() { return totalDMAs == completedDMAs; }
		~SendEntry() { delete cmd; }
		bool isHead() { return state == Head; }
		uint64_t virtAddr() { return cmd->virtAddr; }
		size_t length() { return cmd->size; }
		int destPid() { return cmd->proc.pid; }
		void clrHead() { state = Body; }

	  private:
		enum { Head, Body } state;
		int totalDMAs;
		int completedDMAs;
		int     srcCore;
	  	PutCmd* cmd;
		size_t currentOffset;
	};

	class SelfEvent : public Event {
	  public:
		SelfEvent( int slot, SendEntry* entry ) : slot(slot), entry(entry) {}
		int slot;
		SendEntry* entry;

		NotSerializable(SelfEvent)
	};


	class DMAslot {
		enum { Idle, Wait, Ready } state;
	  public:
		DMAslot() : state(Idle) {}	

		void init( NetworkPkt* pkt, int destNode ) {
			m_pkt = pkt;
			m_destNode = destNode;	
			state = Wait;
		}

		NetworkPkt* pkt() { return m_pkt; }

		bool ready() { return state == Ready; }
		int getDestNode() { return m_destNode; }
		void setIdle() { state = Idle; } 
		void setReady() { state = Ready; }

	  private:
		NetworkPkt* 	m_pkt;
		int 			m_destNode;
	};

	std::vector< DMAslot > m_dmaSlots;
	int m_firstActiveDMAslot;
	int m_firstAvailDMAslot;
	int m_activeDMAslots;

	class Window {
	  public:
		Window() : m_active(false) {}
		void setActive( Hermes::RVMA::VirtAddr addr, size_t threshold, Hermes::RVMA::EpochType type) {
			m_active = true; 
			m_addr = addr;
			m_threshold = threshold;
			m_epochType = type;
			m_count = 0;
		}
		bool isActive() { return m_active; }

		void close() {
			while ( ! m_usedBuffers.empty() ) {
				delete m_usedBuffers.front();
				m_usedBuffers.pop_front();
			}
			while ( ! m_availBuffers.empty() ) {
				delete m_availBuffers.front();
				m_availBuffers.pop_front();
			}
			m_active = false;
		}

		Buffer* recv( size_t offset, void* ptr, size_t nbytes ) {

			//printf("%s()  offset=%zu nbytes=%zu\n",__func__,offset,nbytes);
			if ( m_availBuffers.empty() ) {
				printf("dumping %zu bytes\n",nbytes);
				return NULL;
			}

			Buffer* buffer = m_availBuffers.front();

			// buffer->copy( ptr, nbytes );
		
			m_count += nbytes;

			//printf("m_count %zu\n",m_count);

			if ( m_count == m_threshold ) {
				endEpoch( buffer );
				return buffer;
			}
			return NULL;
		}

		void endEpoch( Buffer* buffer ) {
			//printf("%s() %d\n",__func__,__LINE__);
			buffer->completion->count = m_count;	
			buffer->completion->addr = buffer->addr;	
			m_usedBuffers.push_back( buffer );
			m_availBuffers.pop_front();
			m_count = 0;
		}

		int getEpoch(int* epoch ) { 
			if ( totalBuffers() == 0 ) {
				return -1;
			}
			*epoch = m_usedBuffers.size();
			return 0; 
		}

		int incEpoch() { 
			if ( m_availBuffers.empty() ) {
                return -1;
            }
			endEpoch( m_availBuffers.front() ); 
			return 0; 
		}

		void getBufPtrs( std::vector<Hermes::RVMA::Completion>& vec, int max ) {
			std::deque<Buffer*>::iterator iter = m_usedBuffers.begin();
			for ( int i = 0; iter != m_usedBuffers.end() &&  i < max; i++, ++iter ) {
				vec.push_back( *(*iter)->completion );
			}	
		}
		int totalBuffers() { return m_availBuffers.size() + m_usedBuffers.size(); }

		int postBuffer( Hermes::MemAddr addr, size_t size, Hermes::RVMA::Completion* completion ) {
			std::deque<Buffer*>::iterator iter;

			//printf("%s():%d\n",__func__,__LINE__);

			for ( iter = m_usedBuffers.begin(); iter != m_usedBuffers.end(); ++iter ) {
				if ( (*iter)->addr.getSimVAddr() == addr.getSimVAddr() ) {
					//printf("%s():%d reuse buffer\n",__func__,__LINE__);
					m_availBuffers.push_back( *iter );
					m_usedBuffers.erase(iter);
					return 0;
				}
			}
			for ( iter = m_availBuffers.begin(); iter != m_availBuffers.end(); ++iter ) {
				if ( (*iter)->addr.getSimVAddr() == addr.getSimVAddr() ) {
					return -1;
				}
			}
			//printf("%s():%d insert new buffer\n",__func__,__LINE__);
			m_availBuffers.push_back( new Buffer( addr, size, completion) );
			return 0;
		}
		Hermes::RVMA::VirtAddr getVirtAddr() { return m_addr; }

		bool checkVirtAddr( Hermes::RVMA::VirtAddr addr ) {
			return ( addr >= m_addr && addr < m_addr + m_threshold );
		}

	  private:
		bool m_active;
		Hermes::RVMA::VirtAddr m_addr;
	   	size_t m_threshold;
		size_t m_count;
		Hermes::RVMA::EpochType m_epochType;
		std::deque<Buffer*> m_availBuffers;
		std::deque<Buffer*> m_usedBuffers;
	};

  private:

	void processRVMA( NetworkPkt* );
	void initWindow( int core, Event* );
	void closeWindow( int core, Event* );
	void postBuffer( int core, Event* );
	void incEpoch( int core, Event* );
	void getEpoch( int core, Event* );
	void getBufPtrs( int core, Event* );
	void put( int core, Event* );
	void mwait( int core, Event* );
	void setNumCores( int num );

	void handleSelfEvent( Event* );
	void processSendQ( Cycle_t cycle );
	void processDMAslots();
	void processRecv();

	struct Core {
		Core() : m_mwaitCmd(NULL) {}
		std::vector<Window> m_windowTbl;
		MwaitCmd*   		m_mwaitCmd;

		int findWindow( Hermes::RVMA::VirtAddr virtAddr ) {
			for ( int i = 0; i < m_windowTbl.size(); i++ ) {
				if ( m_windowTbl[i].isActive() ) {
					//printf("0x%" PRIx64 "\n",m_windowTbl[i].getVirtAddr() );
					if ( m_windowTbl[i].checkVirtAddr( virtAddr ) ) {
						return i;
					}
				}	
			}
			return -1;
		}
	};

	std::vector<Core>   m_coreTbl;
	int m_maxBuffers;
	int m_maxNumWindows;

	typedef void (RvmaNicSubComponent::*MemFuncPtr)( int, Event* );

	std::vector<MemFuncPtr> m_cmdFuncTbl;

	static const char *m_cmdName[];
	Output m_dbg;
	std::queue< SendEntry* > m_sendQ;

	int m_vc;

	int m_activeRecvWindow;
	int m_activeRecvCore;
};

}
}
}
#endif
