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


#ifndef COMPONENTS_AURORA_RDMA_NIC_H
#define COMPONENTS_AURORA_RDMA_NIC_H

#include "nic/nic.h"
#include "nic/nicSubComponent.h"
#include "sst/elements/hermes/rdma.h"

#include <queue>

#include "rdmaNicCmds.h"
#include "include/networkPkt.h"

#define SEND_DEBUG_MASK (1<<0)
#define RECV_DEBUG_MASK (1<<1)
#define EVENT_DEBUG_MASK (1<<2)

namespace SST {
namespace Aurora {
namespace RDMA {

class RdmaNicSubComponent : public Aurora::NicSubComponent {

	static const char* protoNames[];

  public:
    SST_ELI_REGISTER_SUBCOMPONENT_DERIVED(
        RdmaNicSubComponent,
        "aurora",
        "rdmaNic",
        SST_ELI_ELEMENT_VERSION(1,0,0),
        "",
        SST::Aurora::RDMA::RdmaNicSubComponent    
    )

    SST_ELI_DOCUMENT_PARAMS(
        {"verboseLevel","Sets the level of debug verbosity",""},
        {"verboseMask","Sets the debug mask",""},
    )

    RdmaNicSubComponent( Component* owner, Params& params ) : NicSubComponent(owner) {} 
    RdmaNicSubComponent( ComponentId_t id, Params& params );
	void setup();

	void handleEvent( int core, Event* event );
    bool clockHandler( Cycle_t );
	void setNumCores(int);

  private:

	enum PktProtocol { Message, RdmaRead, RdmaWrite };
	typedef std::function<void( )> Callback;

	void createRQ( int coreNum, Event* event );
	void postRecv( int coreNum, Event* event );
	void send( int coreNum, Event* event );
	void checkRQ( int coreNum, Event* event );
	void registerMem( int coreNum, Event* event );
	void read( int coreNum, Event* event );
	void write( int coreNum, Event* event );


	class SendEntry {
	  public:

	  	SendEntry( int id, int srcCore, size_t length ) : m_streamId(id), m_srcCore(srcCore),
			m_length(length), m_currentOffset(0), m_completedDMAs(0), m_finiCallback(NULL)  
		{
			m_totalDMAs = length / RdmaNicSubComponent::getPktSize();
			if ( length % RdmaNicSubComponent::getPktSize() ) { 
				++m_totalDMAs;
			}
		}
		virtual ~SendEntry() {
			if ( m_finiCallback ) {
				(*m_finiCallback)();
				delete m_finiCallback;
			}	
		} 

		Callback* m_finiCallback;
		void setCallback( Callback* callback  ) { m_finiCallback = callback; }

		void incCompletedDMAs() { ++m_completedDMAs; }
		void decRemainingBytes( int num ) { m_currentOffset += num; }
		bool done() { return m_totalDMAs == m_completedDMAs; }
		int getSrcCore() { return m_srcCore; };
		size_t length() { return m_length; }
		int remainingBytes() { return m_length - m_currentOffset; }
		int streamId() { return m_streamId; }
		uint32_t streamOffset() { return m_currentOffset; } 

		unsigned char isHead() {
			return  ( 0 == m_currentOffset );
		}

		virtual int getDestNode() = 0;
		virtual int destPid() = 0;
		virtual unsigned char pktProtocol() = 0;
		virtual void* getBacking() = 0; 

	  protected:
		size_t m_currentOffset;

	  private:
		int m_totalDMAs;
		int m_completedDMAs;
		int	m_srcCore;
		int m_length;
		int m_streamId;
	};

	class MsgSendEntry : public SendEntry {
	  public:
		MsgSendEntry( int id, int srcCore, SendCmd* cmd ) : SendEntry( id, srcCore, cmd->length ), cmd( cmd ) { }	
		~MsgSendEntry() { delete cmd; }

		int getDestNode() { return cmd->proc.node; }
		int destPid() { return cmd->proc.pid; }
		unsigned char pktProtocol() { return Message; }
		Hermes::RDMA::RqId rqId() { return cmd->rqId; }

		void* getBacking() {
			unsigned char * ptr = (unsigned char*) cmd->src.getBacking();
			if ( ptr ) {
				ptr += m_currentOffset;
			}	
			return ptr;
		}

	  private:
		SendCmd* cmd;
	};

	class RdmaReadEntry : public SendEntry {
      public:
		RdmaReadEntry( int srcCore, ReadCmd* cmd ) : SendEntry( -1, srcCore, cmd->length ), cmd( cmd ) {}	
		~RdmaReadEntry() { delete cmd; }
		int getDestNode() { return cmd->proc.node; }
		int destPid() { return cmd->proc.pid; }
		unsigned char pktProtocol() { return RdmaRead; }
		Hermes::RDMA::RqId rqId() { return -1; }
		void* getBacking() { return NULL; }
		Hermes::RDMA::Addr getDestAddr() { return cmd->destAddr.getSimVAddr(); }
		Hermes::RDMA::Addr getSrcAddr() { return cmd->srcAddr; }
	  private:
		ReadCmd* cmd;
	};

	class RdmaWriteEntry : public SendEntry {
	  public:
		RdmaWriteEntry( int id, int core, int destNid, int destPid,
				Hermes::MemAddr& srcAddr, Hermes::RDMA::Addr destAddr, size_t length, bool isReadResp = false ) :
			SendEntry( id, core, length ), m_destNid(destNid), m_destPid(destPid),
		   	m_srcAddr(srcAddr), m_destAddr(destAddr), m_length(length), m_isReadResp( isReadResp ) {} 	

		~RdmaWriteEntry() { }

		unsigned char pktProtocol() {
			return RdmaWrite;
		}
		bool isReadResp() { return m_isReadResp; }

		int getDestNode() { return m_destNid; }
		int destPid() { return m_destPid; }

		Hermes::RDMA::Addr destAddr() { return m_destAddr; }

		//
		// FIX ME 
		//
		// backing must be absolute not relative for out of order pkts
		// 
		void* getBacking() {
			unsigned char * ptr = (unsigned char*) m_srcAddr.getBacking();
			if ( ptr ) {
				ptr += m_currentOffset;
			}	
			return ptr;
		}

	  private:
		int m_destNid;
		int m_destPid;
		Hermes::MemAddr m_srcAddr;
		Hermes::RDMA::Addr m_destAddr;
		size_t m_length;
		bool m_isReadResp;
	};

	class RdmaRecvEntry { 
	  public:
		RdmaRecvEntry( Hermes::MemAddr& addr, size_t length, bool isReadResp = false ) :
			m_addr(addr), m_length(length), m_bytesRecvd(0), m_isReadResp(isReadResp) {} 


		bool recv( void* ptr, uint32_t offset, size_t length ) {
			uint8_t* backing = (uint8_t*) m_addr.getBacking();
			if ( backing ) { 
				memcpy( backing + offset, ptr, length );
			}
			//printf("RdmaRecvEntry::%s() offset=%d length=%zu backing=%p\n",__func__, offset, length, backing);
			m_bytesRecvd += length;
			return ( m_bytesRecvd == m_length );
		}
		bool isReadResp() { return m_isReadResp; }

	  private:
        Hermes::MemAddr m_addr;
        size_t m_length;
		size_t m_bytesRecvd;
		bool m_isReadResp;
	};
	
	class RecvBuf;

    class SelfEvent : public Event {
      public:
        enum Type { RxLatency, RxDmaDone, TxLatency, TxDmaDone } type;

        SelfEvent( NetworkPkt* pkt, SendEntry* entry, size_t length, bool lastPkt ) :
			Event(), type(TxLatency), pkt(pkt), sendEntry(entry), length(length ), lastPkt(lastPkt) {}

        SelfEvent( NetworkPkt* pkt, SendEntry* entry, bool lastPkt ) :
			Event(), type(TxDmaDone), pkt(pkt), sendEntry(entry), lastPkt(lastPkt) {}

        SelfEvent( NetworkPkt* pkt ) : Event(), type(RxLatency), pkt(pkt) {}
		SelfEvent( NetworkPkt* pkt, size_t length ) : Event(), type(RxDmaDone), pkt(pkt), length(length) {} 
		SelfEvent( NetworkPkt* pkt, RecvBuf* buffer, size_t length ) : Event(), type(RxDmaDone), pkt(pkt), buffer(buffer), length(length) {} 
			
        NetworkPkt*	pkt;
		RecvBuf*	buffer;
        SendEntry*	sendEntry;
        size_t		length;
        bool		lastPkt;

        NotSerializable(SelfEvent)
    };

	class RecvBuf {
	  public:
		RecvBuf( PostRecvCmd* cmd ) : cmd(cmd), m_offset(0) { 
			//printf("RecvBuf::%s() this=%p\n",__func__,this);
		}
		~RecvBuf() { delete cmd; }

		bool isLastPkt( size_t length ) { return m_offset + length == m_recvLength; }  

		bool recv( void* ptr, size_t length ) {
			unsigned char* buf = (unsigned char* )cmd->addr.getBacking();
#if 0
			printf("RecvBuf::%s() this=%p Addr=0x%" PRIx64 " backing=%p m_offset=%zu length=%zu\n",
					__func__, this, cmd->addr.getSimVAddr(), buf, m_offset, length);
#endif
			if ( buf ) {
				memcpy( buf + m_offset, ptr, length );
			}
			m_offset += length;

			if ( isComplete() ) {
				cmd->status->procAddr = m_proc;
				cmd->status->length = m_recvLength;
				cmd->status->addr = cmd->addr;
				return true;
			} else {
				return false;
			}
		}

		bool isComplete() { return ( m_offset == m_recvLength );  }
		void setRqId( Hermes::RDMA::RqId rqId ) { m_rqId = rqId; }
		void setRecvLength( size_t length ) { m_recvLength = length; }
		void setProc( int nid, int pid ) { m_proc.node = nid; m_proc.pid = pid;}
		size_t length() { return cmd->length; }
		Hermes::MemAddr addr() { return cmd->addr; }
		Hermes::ProcAddr& proc() { return m_proc; }
		Hermes::RDMA::RqId rqId() { return m_rqId; }
		Hermes::RDMA::Status* status() { return cmd->status; }

	  private:
		PostRecvCmd* cmd; 
		size_t m_offset;
		size_t m_recvLength;
		Hermes::RDMA::RqId m_rqId;
		Hermes::ProcAddr m_proc;
	};

	struct Core {

		Core() : m_checkRqCmd(NULL) {}

		RecvBuf* findRecvBuf( Hermes::RDMA::RqId rqId, size_t length ) {
			//printf("Core::%s() rdId=%d, m_rqs.size()=%zu\n",__func__,(int)rqId, m_rqs[rqId].size());
			RecvBuf* buf = NULL;
			recvBufMap_t::iterator iter = m_rqs.find(rqId);
			if ( iter != m_rqs.end() && ! iter->second.empty() ) {
				if ( length <= iter->second.front()->length()  ) {
					buf = iter->second.front();
					iter->second.pop_front();
				} else {
					buf = NULL;
				}
			}
			return buf;
		}

		int registerMem( Hermes::MemAddr& addr, size_t length, Hermes::RDMA::MemRegionId& id) {
			id = 10;
			m_memRegions[id].addr = addr; 
			m_memRegions[id].length = length; 
			return 0;
		}		

		bool validRqId( Hermes::RDMA::RqId rqId ) {
			return m_rqs.find(rqId) != m_rqs.end();
		}	

		bool activeStream( int srcNid, int srcPid ) {
			uint64_t key = genKey( srcNid, srcPid );
			return m_activeStreams.find(key) != m_activeStreams.end();
		}

		void setActiveBuf( int srcNid, int srcPid, RecvBuf* buf ) {
			assert( ! activeStream( srcNid, srcPid ) );
			uint64_t key = genKey( srcNid, srcPid );
			m_activeStreams[key] = buf;
		}	

		void clearActiveStream( int srcNid, int srcPid ) {
			assert( activeStream( srcNid, srcPid ) );
			uint64_t key = genKey( srcNid, srcPid );
			m_activeStreams.erase(key);
		}	

		RecvBuf* findActiveBuf( int srcNid, int srcPid ) {
			uint64_t key = genKey( srcNid, srcPid );
			
			streamMap_t::iterator iter = m_activeStreams.find(key);

			RecvBuf* buf = NULL;
			if ( iter != m_activeStreams.end() ) {
				buf = iter->second;
			}

			return buf;
		}

		uint64_t genKey( int srcNid, int srcPid ) {
			return (uint64_t) srcNid << 32 | srcPid;
		}

		bool findMemAddr( Hermes::RDMA::Addr addr, size_t length, Hermes::MemAddr& memAddr ) {
			//printf("want addr=0x%" PRIx64 " length=%zu\n",addr,length);
			memRegionMap_t::iterator iter = m_memRegions.begin();
			for ( ; iter != m_memRegions.end(); ++iter ) {
				MemRegion& region = iter->second;
				Hermes::RDMA::Addr regionStart = region.addr.getSimVAddr();
				Hermes::RDMA::Addr regionEnd   = regionStart + region.length;
				//printf("current regionStartr=0x%" PRIx64 " length=%zu\n",regionStart,region.length);

				if ( addr >= regionStart && addr + length <= regionEnd ) {
					memAddr = region.addr.offset( addr - regionStart );
					return true;
				}
			}
			return false;
		}

		typedef std::map<Hermes::RDMA::RqId, std::deque<RecvBuf*> > recvBufMap_t;
		typedef std::map<uint64_t,RecvBuf*>  streamMap_t;
		typedef std::map<int,RdmaRecvEntry*>  rdmaRecvMap_t;

		struct MemRegion {
			Hermes::MemAddr addr;
			size_t length;
		};

		typedef std::map<Hermes::RDMA::MemRegionId, MemRegion >  memRegionMap_t;


		memRegionMap_t m_memRegions;

		streamMap_t m_activeStreams;
		recvBufMap_t		m_rqs;

		CheckRqCmd* m_checkRqCmd;
		rdmaRecvMap_t m_rdmaRecvMap;
	};

	void handleSelfEvent( Event* );
	void handleSelfEvent( SelfEvent* );
    void processSendPktStart( NetworkPkt*, SendEntry*, size_t length, bool lastPkt );
    void processSendPktFini( NetworkPkt*, SendEntry*, bool lastPkt );

	bool processRecv();
	int processRdmaSendQ( Cycle_t cycle );
	bool processSend( Cycle_t cycle );
	bool processSendQ( Cycle_t cycle );
	void processPkt( NetworkPkt* );

	void processRecvPktStart( SelfEvent* );
	void processRecvPktFini( SelfEvent* );

	void processRdmaReadPkt( NetworkPkt* );

	SelfEvent* processRdmaWritePktStart( NetworkPkt* );
	void processRdmaWritePktFini( NetworkPkt* );

	SelfEvent* processMsgPktStart( NetworkPkt* );
	void processMsgPktFini( RecvBuf* buffer, NetworkPkt* pkt, size_t length );

    Interfaces::SimpleNetwork::Request* makeNetReq( NetworkPkt* pkt, int destNode );
    bool sendNetReq( Interfaces::SimpleNetwork::Request* );
    void netReqSent( );

	std::vector<Core>   m_coreTbl;

	typedef void (RdmaNicSubComponent::*MemFuncPtr)( int, Event* );

	std::vector<MemFuncPtr> m_cmdFuncTbl;

	static const char *m_cmdName[];

	int m_vc;
	std::deque< SendEntry* > m_sendQ;

	int m_streamIdCnt;
	size_t m_clockCnt;

    bool m_sendStartBusy;
    bool m_recvStartBusy;
	bool m_recvDmaPending;
    bool m_sendDmaPending;
    SelfEvent* m_sendDmaBlockedEvent;
	SelfEvent* m_recvDmaBlockedEvent;

	std::queue<SelfEvent*> m_selfEventQ;
	Interfaces::SimpleNetwork::Request* m_pendingNetReq;
};

}
}
}
#endif
