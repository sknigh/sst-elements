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


#ifndef COMPONENTS_AURORA_RVMA_NIC_H
#define COMPONENTS_AURORA_RVMA_NIC_H

#include <queue>
#include "nic/nic.h"
#include "nic/nicSubComponent.h"
#include "rvmaNicCmds.h"
#include "include/networkPkt.h"
#include "sst/elements/hermes/rvma.h"


using namespace SST::Interfaces;
namespace SST {
namespace Aurora {
namespace RVMA {

class RvmaNicSubComponent : public Aurora::NicSubComponent {

  public:
    SST_ELI_REGISTER_SUBCOMPONENT_DERIVED(
        RvmaNicSubComponent,
        "aurora",
        "rvmaNic",
        SST_ELI_ELEMENT_VERSION(1,0,0),
        "",
        SST::Aurora::RVMA::RvmaNicSubComponent 
    )

    SST_ELI_DOCUMENT_PARAMS(
        {"verboseLevel","Sets the level of debug verbosity",""},
        {"verboseMask","Sets the debug mask",""},
    )

    RvmaNicSubComponent( Component* owner, Params& params ) : NicSubComponent(owner) {} 
    RvmaNicSubComponent( ComponentId_t id, Params& params );
	void setup();

	void handleEvent( int core, Event* event );
    bool clockHandler( Cycle_t );

	class Buffer {
	  public:
		Buffer ( Hermes::RVMA::EpochType type, size_t threshold, Hermes::MemAddr addr, size_t size, Hermes::RVMA::Completion* completion ) : 
			m_epochType(type), epochThreshold(threshold), addr(addr), maxSize(size), completion(completion), bytesRcvd(0), epochCount(0)
		{
			printf("%p\n",this);
			//printf("%s() this=%p maxSize=%zu completion=%p\n",__func__,this,size,completion);
		}

		void copy( void* ptr, size_t offset, size_t length ) {
			//printf("%s() this=%p offset=%zu length=%zu maxSize=%zu\n",__func__,this,offset,length,maxSize);
			if( offset + length > maxSize ) {
				printf("offset-%zu length=%zu maxSize=%zu\n",offset,length,maxSize);
				assert(0);
			}
			if ( addr.getBacking() ) {
				memcpy( addr.getBacking(offset), ptr, length);
			}
			bytesRcvd += length;

			if ( isEndOfEpoch() ) {
				endEpoch();
			}
		}

		void incEpochCount( size_t length ) {
			if ( m_epochType == Hermes::RVMA::Op ) {
				++epochCount;
			} else {
				epochCount += length;
			}
			//printf("%s() epochCount=%zu\n",__func__,epochCount);
		}

		bool isEndOfEpoch() {
			return epochThreshold == epochCount;
		}

		void endEpoch() {
			getCompletion()->count = epochCount;
			getCompletion()->addr = getAddr();
		}

		size_t getEpochCount() { return epochCount; }
		Hermes::RVMA::Completion* getCompletion() { return completion; }
		size_t getBytesRcvd() { return bytesRcvd; }
		Hermes::MemAddr& getAddr() { return addr; }

	  private:
		Hermes::RVMA::EpochType m_epochType;
		size_t epochThreshold;
		size_t epochCount;

		Hermes::MemAddr addr;
		Hermes::RVMA::Completion* completion;
		size_t maxSize;
		size_t bytesRcvd;
	};

	class RecvStream {
	  public:
		RecvStream( int window, Buffer* buffer, size_t length ) : m_window(window),
				m_buffer(buffer), m_length(length), m_numRcvdPkts(0), m_numTotalPkts(0) {}
		void setNumPkts( int num ) { m_numTotalPkts = num; }
		void recv( void* ptr, size_t offset, size_t length ) {
			m_buffer->copy( ptr, offset, length ); 
		}
		Buffer& buffer() { return *m_buffer; }
		int getWindowNum() { return m_window; }
		int incRcvdPkts() { ++ m_numRcvdPkts; }
		bool rcvdAllPkts() { return m_numTotalPkts == m_numRcvdPkts; }
	  private:
		int m_window;
		size_t m_length;
		int m_numRcvdPkts;
		int m_numTotalPkts;
		Buffer* m_buffer;
	};

	class SendEntry {
	  public:
		SendEntry(int id, int srcCore, PutCmd* cmd ) : srcCore(srcCore), cmd(cmd), currentOffset(0), completedDMAs(0), streamId(id) { } 
		~SendEntry() {
			if ( cmd->handle ) {
				*cmd->handle = 1;
			}
			delete cmd;
		}

		void decRemainingBytes( int num ) { currentOffset += num; }
		int remainingBytes() { return cmd->size - currentOffset; }
		size_t offset() { return currentOffset + cmd->offset; }
		int getDestNode() { return cmd->proc.node; }
		uint64_t virtAddr() { return cmd->virtAddr; }
		size_t length() { return cmd->size; }
		int destPid() { return cmd->proc.pid; }
		void* dataAddr() { return cmd->srcAddr.getBacking(); }
		int getSrcCore() { return srcCore; }
	  	PutCmd& getCmd() { return *cmd; }
		bool isDOne() { return lastPkt; }
		int getStreamId() { return streamId; }

	  private:
		int completedDMAs;
		int     srcCore;
	  	PutCmd* cmd;
		size_t currentOffset;
		bool lastPkt;
		int streamId;
	};

	class SelfEvent : public Event {
	  public:
		enum Type { RxLatency, RxDmaDone, TxLatency, TxDmaDone } type;
		SelfEvent( NetworkPkt* pkt, SendEntry* entry, size_t length, bool lastPkt ) : Event(), type(TxLatency), pkt(pkt),
						sendEntry(entry), length(length ), lastPkt(lastPkt) {}
		SelfEvent( NetworkPkt* pkt, SendEntry* entry, bool lastPkt ) : Event(), type(TxDmaDone), pkt(pkt), sendEntry(entry), lastPkt(lastPkt) {}


		SelfEvent( NetworkPkt* pkt ) : Event(), type(RxLatency), pkt(pkt) {}
		SelfEvent( int pid, RecvStream* stream, NetworkPkt* pkt, size_t length, uint64_t rvmaOffset  ) :
			Event(), type(RxDmaDone), pid(pid), stream(stream), pkt(pkt), length(length), rvmaOffset(rvmaOffset) {}
		NetworkPkt* pkt;
		int pid;
		RecvStream* stream;
		size_t length;
		uint64_t rvmaOffset;
		SendEntry* sendEntry;
		bool lastPkt;

		NotSerializable(SelfEvent)
	};

	class Window {
	  public:
		Window( ) : m_active(false) {}
		void setActive( Hermes::RVMA::VirtAddr addr, size_t threshold, Hermes::RVMA::EpochType type, bool oneTime = false ) {
			m_active = true; 
			m_addr = addr;
			m_threshold = threshold;
			m_epochType = type;
			m_oneTime = oneTime; 
		}

		void setMaxes( int maxAvailBuffers, int maxCompBuffers ) { 
			m_maxAvailBuffers = maxAvailBuffers;
			m_maxCompBuffers =  maxCompBuffers;
		}

		int m_maxAvailBuffers;
		int m_maxCompBuffers;
		bool isActive() { return m_active; }
		bool isOneTime() { return m_oneTime; }

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

		Buffer* getActiveBuffer( size_t length ) {
			if ( m_availBuffers.empty() ) {
				return NULL;
			}

			Buffer* buffer = m_availBuffers.front();

			// note we inc the epoch count here so any new active buffer requests go to the next buffer
			buffer->incEpochCount(length);

			if ( buffer->isEndOfEpoch() ) {
				switchActiveBuffer();
			}

			return buffer;
		}

		void switchActiveBuffer( ) {

			// if one time buffer this window will go away so we don't need to switch to next active buffer 
			if ( isOneTime() ) {
				return;
			}

			if ( numCompBuffers() == m_maxCompBuffers ) {
				m_usedBuffers.pop_front();
			}

			m_usedBuffers.push_back( m_availBuffers.front() );
			m_availBuffers.pop_front();
		}

		int getEpoch( int* epoch ) { 
			if ( m_usedBuffers.empty()  ) {
				return -1;
			}
			*epoch = m_usedBuffers.size();
			return 0; 
		}

		int incEpoch() { 
			if ( m_availBuffers.empty() ) {
                return -1;
            }
			m_availBuffers.front()->endEpoch();
			switchActiveBuffer();
			return 0; 
		}

		void getBufPtrs( std::vector<Hermes::RVMA::Completion>& vec, int max ) {
			std::deque<Buffer*>::iterator iter = m_usedBuffers.begin();
			for ( int i = 0; iter != m_usedBuffers.end() &&  i < max; i++, ++iter ) {
				vec.push_back( *(*iter)->getCompletion() );
			}	
		}

		int numAvailBuffers() { return m_availBuffers.size(); }
		int numCompBuffers() { return m_usedBuffers.size(); }

		int postBuffer( Hermes::MemAddr addr, size_t size, Hermes::RVMA::Completion* completion ) {

			if ( numAvailBuffers() == m_maxAvailBuffers ) {
				return -1;
			}

			std::deque<Buffer*>::iterator iter;
			for ( iter = m_usedBuffers.begin(); iter != m_usedBuffers.end(); ++iter ) {
				if ( (*iter)->getAddr().getSimVAddr() == addr.getSimVAddr() ) {
					m_usedBuffers.erase(iter);
					m_availBuffers.push_back( new Buffer( m_epochType, m_threshold, addr, size, completion) );
					return 0;
				}
			}
			for ( iter = m_availBuffers.begin(); iter != m_availBuffers.end(); ++iter ) {
				if ( (*iter)->getAddr().getSimVAddr() == addr.getSimVAddr() ) { 
					return -1;
				}
			}
			m_availBuffers.push_back( new Buffer( m_epochType, m_threshold, addr, size, completion) );
			return 0;
		}
		Hermes::RVMA::VirtAddr getWinAddr() { return m_addr; }

		bool checkWinAddr( Hermes::RVMA::VirtAddr addr ) {
			return ( addr == m_addr );
		}

	  private:
		bool m_active;
		Hermes::RVMA::VirtAddr m_addr;
	   	size_t m_threshold;
		bool m_oneTime;
		Hermes::RVMA::EpochType m_epochType;
		std::deque<Buffer*> m_availBuffers;
		std::deque<Buffer*> m_usedBuffers;
	};

  private:

	bool processSend( Cycle_t );
	bool processRecv();
	void processSendPktStart( NetworkPkt*, SendEntry*, size_t length, bool lastPkt );
	void processSendPktFini( NetworkPkt*, SendEntry*, bool lastPkt );
	void processRecvPktStart( NetworkPkt* );
	void processRecvPktFini( int pid, RecvStream*, NetworkPkt*, uint64_t rvmaOffset, size_t length );
	void setNumCores( int num );
	void initWindow( int coreNum, Event* event );
	void closeWindow( int coreNum, Event* event );

	SimpleNetwork::Request* makeNetReq( NetworkPkt* pkt, int destNode );
	bool sendNetReq( SimpleNetwork::Request* );
	void netReqSent( );

	void incEpoch( int coreNum, Event* event );
	void getEpoch( int coreNum, Event* event );
	void getBufPtrs( int coreNum, Event* event );
	void postBuffer( int coreNum, Event* event );
	void postOneTimeBuffer( int coreNum, Event* event );
	void put( int coreNum, Event* event );
	void mwait( int coreNum, Event* event );

	void handleSelfEvent( Event* );
	void handleSelfEvent( SelfEvent* );
	bool processSendQ( Cycle_t cycle );

	class Core {

	  public:
		Core() : m_mwaitCmd(NULL) {}

		RecvStream* createRecvStream( Hermes::RVMA::VirtAddr winAddr, size_t length ) {

			for ( int i = 0; i < m_windowTbl.size(); i++ ) {
				if ( m_windowTbl[i].isActive() ) {
					if ( m_windowTbl[i].checkWinAddr( winAddr ) ) {
						Buffer* buffer = m_windowTbl[i].getActiveBuffer( length );
						assert( buffer );
						return new RecvStream( i, buffer, length );
					}
				}	
			}
			return NULL;
		}

		uint64_t genKey( int srcNid, uint16_t srcPid, StreamId streamId ) {
			return (uint64_t) srcNid << 32 | srcPid << 16 | streamId ;
		}

		bool activeRecvStream( int srcNid, int srcPid, StreamId streamId ) {
			uint64_t key = genKey( srcNid, srcPid, streamId );
			return m_activeStreams.find(key) != m_activeStreams.end();
		}

		void setActiveRecvStream( int srcNid, int srcPid, StreamId streamId, RecvStream* buf ) {
			assert( ! activeRecvStream( srcNid, srcPid, streamId ) );
			uint64_t key = genKey( srcNid, srcPid, streamId );
			m_activeStreams[key] = buf;
		}

		void clearActiveRecvStream( int srcNid, int srcPid, StreamId streamId ) {
			assert( activeRecvStream( srcNid, srcPid, streamId ) );
			uint64_t key = genKey( srcNid, srcPid, streamId );
			m_activeStreams.erase(key);
		}

		RecvStream* findActiveRecvStream( int srcNid, int srcPid, StreamId streamId ) {
			uint64_t key = genKey( srcNid, srcPid, streamId );

			streamMap_t::iterator iter = m_activeStreams.find(key);

			RecvStream* buf = NULL;
			if ( iter != m_activeStreams.end() ) {
				buf = iter->second;
			}

			return buf;
		}

		size_t getWindowTblSize() { return m_windowTbl.size(); }
		void windowTblResize(int num) { return m_windowTbl.resize(num); }
		void setWaitCmd(MwaitCmd* cmd) { m_mwaitCmd = cmd; }
		void clearWaitCmd() { delete m_mwaitCmd; m_mwaitCmd = NULL; } 
		Window& getWindow(int num) { return m_windowTbl[num]; }
		MwaitCmd* getWaitCmd() { return m_mwaitCmd; }

	  private:
		std::vector<Window> m_windowTbl;
		MwaitCmd*   		m_mwaitCmd;
		std::deque< Buffer* > m_completed;
		typedef std::map<uint64_t,RecvStream*>  streamMap_t;
		streamMap_t m_activeStreams;
	};

	std::vector<Core>   m_coreTbl;
	int m_maxAvailBuffers;
	int m_maxCompBuffers;
	int m_maxNumWindows;

	typedef void (RvmaNicSubComponent::*MemFuncPtr)( int, Event* );

	std::vector<MemFuncPtr> m_cmdFuncTbl;

	static const char *m_cmdName[];
	std::queue< SendEntry* > m_sendQ;

	int m_recvPktsPending;
	bool m_recvDmaPending;

	int m_sendPktsPending;
	bool m_sendDmaPending;

	SelfEvent* m_recvDmaBlockedEvent;
	SelfEvent* m_sendDmaBlockedEvent;

	std::queue<SelfEvent*> m_selfEventQ;
	SimpleNetwork::Request* m_pendingNetReq;
	StreamId m_streamIdCnt;
	std::vector<uint64_t> m_curSentPktNum;
	std::vector<uint64_t> m_curRecvPktNum;
};

}
}
}
#endif
