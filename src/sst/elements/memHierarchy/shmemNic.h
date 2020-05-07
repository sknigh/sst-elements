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

#include <sst/core/interfaces/simpleMem.h>
#include <sst/core/interfaces/simpleNetwork.h>

#include <shmemNicCmds.h>

#define DBG_X_FLAG (1<<0)
#define DBG_MEMEVENT_FLAG (1<<1)
#define DBG_FAM_FLAG (1<<2)

namespace SST {
namespace MemHierarchy {

class ShmemNicNetworkEvent : public Event {

    static int maxSize;

  public:
    ShmemNicNetworkEvent() : Event(), pktOverhead(0) { buf.reserve(maxSize); }
    ShmemNicNetworkEvent(const ShmemNicNetworkEvent* me) : Event() { copy( *this, *me ); }
    ShmemNicNetworkEvent(const ShmemNicNetworkEvent& me) : Event() { copy( *this, me ); }

    static int getMaxSizeBytes() { return maxSize; }
    int getPayloadSizeBytes() { return pktOverhead + bufSize(); }

    virtual Event* clone(void) override {
        return new ShmemNicNetworkEvent(*this);
    }

    void setPktOverhead( int val ) { pktOverhead = val; }
    void setType( int val ) { type = val; } 
    void setOp( int val ) { op = val; } 
    void setDataType( int val ) { dataType = val; }
    void setDestPid( int val ) { destPid = val; }
    void setSrcPid( int val ) { srcPid = val; }
    void setSrcNode( int val ) { srcNode = val; }
    void setAddress( int val ) { address = val; } 
    void setHandle( uint32_t val ) { handle = val; }

    int getSrcNode( ) { return srcNode; }
    int getType() { return type; }
    int getOp() { return op; }
    int getDataType() { return dataType; }
    int getDestPid() { return destPid; }
    int getSrcPid() { return srcPid; }
    std::vector<uint8_t>& getData() { return buf; }
    uint64_t getAddress() { return address; } 
    uint32_t getHandle() { return handle; }

  private:

    void copy( ShmemNicNetworkEvent& to, const ShmemNicNetworkEvent& from ) {
        to.pktOverhead = from.pktOverhead;
        to.type = from.type;
        to.op = from.op;
        to.dataType = from.dataType;
        to.address = from.address;
        to.destPid = from.destPid;
        to.srcPid = from.srcPid;
        to.srcNode = from.srcNode;
        to.buf = from.buf;
        to.handle = from.handle;
    }

    int bufSize() { return buf.size(); }

    int pktOverhead;

    int type;
    int op;
    int dataType;
    uint64_t address;

    int destPid;
    int srcPid;
    int srcNode;
    uint32_t handle;

    std::vector<uint8_t> buf;

  public:
    void serialize_order(SST::Core::Serialization::serializer &ser)  override {
        Event::serialize_order(ser);
        ser & pktOverhead;
        ser & op;
        ser & dataType;
        ser & address;
        ser & destPid;
        ser & srcPid;
        ser & srcNode;
        ser & buf;
        ser & type;
        ser & handle;
    }

    ImplementSerializable(SST::MemHierarchy::ShmemNicNetworkEvent);
};

class ShmemNic : public SST::Component {
  public:

    SST_ELI_REGISTER_COMPONENT(ShmemNic, "memHierarchy", "ShmemNic", SST_ELI_ELEMENT_VERSION(1,0,0),
        "Shmem NIC, interfaces to a main memory model as memory and CPU", COMPONENT_CATEGORY_MEMORY)

#define MEMCONTROLLER_ELI_PORTS \
            {"direct_link", "Direct connection to a cache/directory controller", {"memHierarchy.MemEventBase"} },\
            {"network",     "Network connection to a cache/directory controller; also request network for split networks", {"memHierarchy.MemRtrEvent"} },\
            {"network_ack", "For split networks, ack/response network connection to a cache/directory controller", {"memHierarchy.MemRtrEvent"} },\
            {"network_fwd", "For split networks, forward request network connection to a cache/directory controller", {"memHierarchy.MemRtrEvent"} },\
            {"network_data","For split networks, data network connection to a cache/directory controller", {"memHierarchy.MemRtrEvent"} },\
            {"cache_link",  "Link to Memory Controller", { "memHierarchy.memEvent" , "" } }, \

    SST_ELI_DOCUMENT_STATISTICS(
        { "cyclePerIncOpRead",    "latency of inc op",     "ns", 1 },
        { "cyclePerIncOp",    "latency of inc op",     "ns", 1 },
        { "addrIncOp",    "addr of memory access",     "addr", 1 },
        { "pendingMemResp",  "number of mem request waiting for response",   "request", 1 },
        { "pendingCmds",  "number of SHMEM commands current in process",   "request", 1 },
        { "localQ_0",  "",   "request", 1 },
        { "localQ_1",  "",   "request", 1 },
        { "remoteQ_0",  "",   "request", 1 },
        { "remoteQ_1",  "",   "request", 1 },
        { "hostCmdQ",  "",   "request", 1 },
        { "headUpdateQ",  "",   "request", 1 },
        { "FamGetLatency",  "",   "request", 1 },
        { "hostToNicLatency",  "",   "request", 1 }
    )

    SST_ELI_DOCUMENT_PORTS( MEMCONTROLLER_ELI_PORTS )

    ShmemNic(ComponentId_t id, Params &params);

    virtual void init(unsigned int);
    virtual void setup();
    void finish();

  protected:
    ShmemNic();  // for serialization only
    ~ShmemNic() {}

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

    int calcNic( int pe ) {
        return pe / m_pesPerNode;
    }

    class MemRequestQ {

        struct SrcChannel {
            SrcChannel( int maxSrcQsize ) : maxSrcQsize(maxSrcQsize), pendingCnts(0) {}
            bool full() {
                return  ! ( queue.size() + waiting.size() + ready.size() < maxSrcQsize); 
            }
            std::queue<void*>       waiting;
            std::set<void*>         ready;
            std::queue<MemRequest*> queue; 
            int maxSrcQsize;
            int pendingCnts;
        };

      public:
        MemRequestQ( ShmemNic* nic, int maxPending, int maxSrcQsize, int numSrcs ) : 
            m_nic(nic), m_maxPending(maxPending), m_curSrc(0), m_reqSrcQs(numSrcs,maxSrcQsize), m_pendingPair(NULL,NULL)
        {}

        virtual ~MemRequestQ() { }
        void print( Cycle_t cycle ) {
            printf("%" PRIu64 " %d:  pendingReq=%zu :",cycle, Nic().m_nicId, m_pendingReq.size() );
            for ( int i = 0; i < m_reqSrcQs.size(); i++) {
                printf("src=%d queue=%zu waiting=%zu ready=%zu, ",i,m_reqSrcQs[i].queue.size(), m_reqSrcQs[i].waiting.size(), m_reqSrcQs[i].ready.size() );
            }
            printf("\n");
        }

        bool full( int srcNum ) { 
            return  m_reqSrcQs[srcNum].full();
        }
        void queueReq( int srcNum, MemRequest* req ) { m_reqSrcQs[srcNum].queue.push(req); }
        ShmemNic& Nic() { return *m_nic; }

        void reserve( int srcNum, void* key ) {
            return m_reqSrcQs[srcNum].waiting.push(key);
        }

        bool reservationReady( int srcNum, void* key ) {
            if ( m_reqSrcQs[srcNum].ready.find( key ) != m_reqSrcQs[srcNum].ready.end() ) {
                m_reqSrcQs[srcNum].ready.erase(key);
                return true;
            } 
            return false;
        }

        void sendReq( MemRequest* req ) {

            MemEvent* ev  = NULL;
            std::vector<uint8_t> payload;
            req->reqTime=Simulation::getSimulation()->getCurrentSimCycle();

            uint64_t baseAddr = req->addr & ~(Nic().m_cacheLineSize - 1);

            switch ( req->m_op ) {
              case MemRequest::Read:
                Nic().dbg.debug(CALL_INFO,1,DBG_MEMEVENT_FLAG,"read addr=%#" PRIx64 " baseAddr=%#" PRIx64 " dataSize=%d\n",req->addr,baseAddr, req->dataSize);
                ev = new MemEvent(Nic().getName(), req->addr, baseAddr, Command::PrRead, req->dataSize);
                break;

              case MemRequest::Write:

                if ( req->buf.empty() ) {
                    Nic().dbg.debug(CALL_INFO,1,DBG_MEMEVENT_FLAG,"write addr=%#" PRIx64 " baseAddr=%#" PRIx64 " data=%llu dataSize=%d\n",
                                    req->addr,baseAddr,req->data,req->dataSize);
                    for ( int i = 0; i < req->dataSize; i++ ) {
                        payload.push_back( (req->data >> i*8) & 0xff );
                    }
                    ev = new MemEvent(Nic().getName(), req->addr, baseAddr, Command::PrWrite, payload);
                } else {
                    Nic().dbg.debug(CALL_INFO,1,DBG_MEMEVENT_FLAG,"write addr=%#" PRIx64 " baseAddr=%#" PRIx64 " dataSize=%d\n",req->addr,baseAddr,req->dataSize);
                    ev = new MemEvent(Nic().getName(), req->addr, baseAddr, Command::PrWrite, req->buf);
                }
                break;

              case MemRequest::Fence:
                assert(0);
                break;
            }

            if ( ev ) {
                ev->setDst(Nic().m_link->findTargetDestination(req->addr));
                if ( Nic().m_link->spaceToSend( ev ) ) {
                    m_pendingMap[ req->addr ].push_back( req );
                    m_pendingReq[ ev->getID() ] = req;
                    Nic().m_link->send(ev);
                } else {
                    Nic().dbg.debug(CALL_INFO,1,DBG_MEMEVENT_FLAG,"link blocked\n");
                    m_pendingPair = std::pair<MemRequest*,MemEventBase*>(req,ev);
                }
            }
        }

        void fence( int srcNum ) {
            m_reqSrcQs[srcNum].queue.push( new MemRequest(srcNum) );
        } 

        void write( int srcNum, uint64_t addr, int dataSize, uint8_t* data, MemRequest::Callback* callback = NULL ) {
            Nic().dbg.debug(CALL_INFO,1,DBG_X_FLAG,"srcNum=%d addr=%#" PRIx64 " dataSize=%d\n",srcNum,addr,dataSize);
            m_reqSrcQs[srcNum].queue.push( new MemRequest( srcNum, addr, dataSize, data, callback ) );
        }

        void write( int srcNum, uint64_t addr, int dataSize, uint64_t data, MemRequest::Callback* callback = NULL ) {
            Nic().dbg.debug(CALL_INFO,1,DBG_X_FLAG,"srcNum=%d addr=%#" PRIx64 " data=%llu dataSize=%d\n",srcNum,addr,data,dataSize);
            m_reqSrcQs[srcNum].queue.push( new MemRequest( srcNum, addr, dataSize, data, callback ) );
        }

        void read( int srcNum, uint64_t addr, int dataSize, MemRequest::Callback* callback = NULL  ) {
            Nic().dbg.debug(CALL_INFO,1,DBG_X_FLAG,"srcNum=%d addr=%#" PRIx64 " dataSize=%d\n",srcNum,addr,dataSize);
            m_reqSrcQs[srcNum].queue.push( new MemRequest( srcNum, addr, dataSize, callback ) );
        }

        std::map< uint64_t, std::list<MemRequest* > > m_pendingMap;

        std::queue< std::pair< MemEvent*, MemRequest*> > m_retryQ;

        void handleResponse( MemEvent* event ) {
            try {
                MemRequest* req = m_pendingReq.at( event->getID() );
                Nic().dbg.debug(CALL_INFO,1,DBG_X_FLAG,"cycles=%" PRIu64 "\n", Simulation::getSimulation()->getCurrentSimCycle()-req->reqTime );

                bool drop = false;
                try {
                    auto& q = m_pendingMap.at(req->addr);
                    auto iter = q.begin(); 

                    int pos = 0;
                    for ( ; iter != q.end(); ++iter ) {
                        if ( (*iter) == req ) {
                            q.erase(iter);
                            break;
                        }
                        ++pos;
                    }

                    if ( event->getCmd() == Command::NACK ) {
                        if ( req->m_op == MemRequest::Read || q.size() == pos ) {
#if 0
                            printf("retry request for 0x%" PRIx64 "\n", req->addr );
#endif
                            m_retryQ.push( std::make_pair(event, req) );
                        } else {
                            drop = true;
#if 0
                            printf("drop request for 0x%" PRIx64 " %d %zu\n", req->addr, pos, q.size() );
#endif
                        }
                    } 

                    if ( q.empty() ) {
                        m_pendingMap.erase(req->addr);
                    }

                } catch (const std::out_of_range& oor) {
                    Nic().out.fatal(CALL_INFO,-1,"Can't find request\n");
                }

                if ( event->getCmd() != Command::NACK || drop ) {
                    m_pendingReq.erase( event->getID() );
                    req->handleResponse( event );
                    --m_reqSrcQs[req->src].pendingCnts;
                    delete req;
                }

            } catch (const std::out_of_range& oor) {
                Nic().out.fatal(CALL_INFO,-1,"Can't find request\n");
            } 
        }

        bool process( int num = 1) {
            bool worked = false;
            Nic().m_statPendingMemResp->addData( m_pendingReq.size() );
            if ( m_pendingPair.first ) {
                if ( Nic().m_link->spaceToSend( m_pendingPair.second ) ) {
                    m_pendingMap[ m_pendingPair.first->addr ].push_back( m_pendingPair.first );
                    m_pendingReq[ m_pendingPair.second->getID() ] = m_pendingPair.first;
                    Nic().m_link->send( m_pendingPair.second );
                    m_pendingPair.first = NULL;
                } else {
                    Nic().dbg.debug(CALL_INFO,1,DBG_MEMEVENT_FLAG,"link blocked\n");
                }
                return true;
            }
            if ( ! m_retryQ.empty() ) {
                auto entry = m_retryQ.front(); 

#if 0
                printf("%s() handle retry %" PRIx64 " \n",__func__,entry.second->addr);
#endif

               if ( Nic().m_link->spaceToSend( entry.first->getNACKedEvent() ) ) {
                	m_pendingReq[ entry.first->getID() ] = entry.second;
                	m_pendingMap[entry.second->addr].push_back( entry.second );
                    Nic().m_link->send( entry.first->getNACKedEvent() );
                	delete entry.first;
                	m_retryQ.pop();
                } else {
                    Nic().dbg.debug(CALL_INFO,1,DBG_MEMEVENT_FLAG,"link blocked\n");
                }

                return true;
            }
            if ( m_pendingReq.size() < m_maxPending ) {
                for ( int i = 0; i < m_reqSrcQs.size(); i++ ) {
                    int pos = (i + m_curSrc) % m_reqSrcQs.size();
                     
                    std::queue<MemRequest*>& q = m_reqSrcQs[pos].queue;

                    if ( ! q.empty() ) {
                        worked = true;
                        if ( q.front()->isFence() ) {
                            if ( m_reqSrcQs[pos].pendingCnts ) {
                                continue;
                            } else {
								delete q.front();
                                q.pop();
                            }
                        }

                        ++m_reqSrcQs[pos].pendingCnts;

                        sendReq( q.front());
#if 0
                        MemRequest* req = q.front();
                        printf("%d:%s() addr 0x%" PRIx64 " %s\n",Nic().m_nicId,__func__,req->addr,req->m_op == MemRequest::Read ? "Read": "Write" );
#endif
                        q.pop();
                        auto& w = m_reqSrcQs[pos].waiting;
                        if ( ! w.empty() ) {
                            m_reqSrcQs[pos].ready.insert( w.front() ); 
                            w.pop();
                        }
                        break;
                    }
                }
                m_curSrc = ( m_curSrc + 1 ) % m_reqSrcQs.size();
            }
            return worked;
        }


      protected:
        std::map< SST::Event::id_type, MemRequest* > m_pendingReq;
        std::vector< SrcChannel > m_reqSrcQs;
      private:
        ShmemNic* m_nic;
        int m_curSrc;
        int m_maxPending;
        std::pair<MemRequest*,MemEventBase*> m_pendingPair;

    };

    MemRequestQ* m_memReqQ; 
    int m_tailWriteQnum;
    int m_respQueueMemChannel;
    int m_shmemOpQnum;

    struct HostCmdQueueInfo { 
        HostCmdQueueInfo( uint64_t  tailAddr ) :
            tailAddr(tailAddr), localTailIndex(0) {}

        uint64_t  tailAddr;
        uint32_t  localTailIndex;
    };

    struct HostRespQueueInfo { 
        HostRespQueueInfo( uint32_t* tailPtr, uint64_t  queueAddr,  uint64_t  headAddr ) :
            tailPtr(tailPtr), queueAddr(queueAddr), headAddr(headAddr), localHeadIndex(0) {}

        uint64_t cmdAddr( int index ) {
            return queueAddr + index * sizeof(NicResp);
        }
        uint32_t* tailPtr;
        uint64_t  queueAddr;
        uint64_t  headAddr;
        uint32_t  localHeadIndex;
    };

    std::vector<HostRespQueueInfo> m_hostRespQueueV;
    std::vector<HostCmdQueueInfo> m_hostCmdQueueV;

    Output out;
    Output dbg;

    MemLinkBase* m_link;         // Link to the rest of memHierarchy
    bool m_clockLink;            // Flag - should we call clock() on this link or not

    uint64_t m_ioBaseAddr;
    std::vector<uint8_t> m_backing;
    int m_pesPerNode;
    size_t m_perPeMemSize;

    size_t m_cacheLineSize;
    uint64_t m_hostQueueBaseAddr;

    enum DataType { Int64 }; 

    struct ShmemMemRegion {
        uint64_t baseAddr;
        size_t   length;
    };

    struct RespInfo {
        RespInfo( DataType dataType, uint64_t dataAddr, size_t numData ) : dataType(dataType), dataAddr(dataAddr), numData(numData) {}
        DataType dataType;
        uint64_t dataAddr;
        size_t   numData;
    };

    struct ThreadInfo {
        ShmemMemRegion shmemRegion;
        std::map<uint32_t,RespInfo*> respMap;
        int pendingCnt;
    };

    std::vector<ThreadInfo> m_threadInfo;

    enum ShmemOp { ShmemInc, ShmemPut, FamGet, FamPut };

    int dataTypeSize( DataType type ) {
        switch ( type ) {
            case Int64:
                return 8;
            default:
                assert(0);
        }
    }

    class ShmemCmd {
        enum { GetUnit, WaitUnit } m_state;

      public:
        ShmemCmd( ShmemNic* nic, DataType dataType, uint32_t handle = 0 ) :
			m_nic(nic), m_dataType(dataType), m_srcNode(-1), m_memEvent(NULL), m_state(GetUnit), m_netEvent(NULL), m_handle(handle), m_callback(NULL)
        {
            m_callback = new MemRequest::Callback;
            *m_callback = std::bind( &ShmemNic::ShmemCmd::setMemEvent, this, std::placeholders::_1 );
        }

        ShmemCmd( ShmemNic* nic, ShmemOp op, DataType dataType, int srcNode, int srcPid, int destNode, uint64_t addr ) :
            m_nic(nic), m_addr(addr), m_memEvent( NULL ), m_op(op), m_dataType(dataType), m_srcNode( srcNode ), m_srcPid(srcPid), 
            m_destNode(destNode), m_state( GetUnit), m_type( NicCmd::Type::Fam ), m_netEvent(NULL), m_handle(0), m_callback(NULL)
        {
			if ( nic->m_nicId == m_destNode ) {
            	m_callback = new MemRequest::Callback;
            	*m_callback = std::bind( &ShmemNic::ShmemCmd::setMemEvent, this, std::placeholders::_1 );
			}
        }

        ShmemCmd( ShmemNic* nic, ShmemOp op, DataType dataType, int srcNode, int srcPid, int destNode, int destPid, uint64_t addr ) :
            m_nic(nic), m_addr(addr), m_memEvent( NULL ), m_op(op), m_dataType(dataType), m_srcNode( srcNode ), m_srcPid(srcPid), 
            m_destNode(destNode), m_destPid(destPid), m_state( GetUnit), m_type( NicCmd::Type::Shmem ), m_netEvent(NULL), m_handle(0), m_callback(NULL)
        {
			if ( nic->m_nicId == m_destNode ) {
            	m_callback = new MemRequest::Callback;
            	*m_callback = std::bind( &ShmemNic::ShmemCmd::setMemEvent, this, std::placeholders::_1 );
			}
        }

        virtual ~ShmemCmd() { 
			if ( m_callback ) {
            	delete m_callback; 
			}
            if ( m_netEvent ) {
                delete m_netEvent;
            }
        }

        void setHandle( uint32_t  handle ) { m_handle = handle; }
        void setNetEvent( ShmemNicNetworkEvent* event ) { m_netEvent = event; }
        std::vector<uint8_t>& getData() { assert(m_netEvent); return m_netEvent->getData(); }

        MemRequest::Callback* m_callback;
        virtual bool process( Cycle_t ) = 0;
        void setMemEvent( MemEvent* ev ) { 
            Nic().dbg.debug( CALL_INFO,1,DBG_X_FLAG,"\n");
            assert( NULL == m_memEvent);  
            m_memEvent = ev; 
        } 
        MemEvent* getMemEvent() { return m_memEvent; }
        virtual void clearMemEvent() {  m_memEvent = NULL; } 

        ShmemNic& Nic() { return *m_nic; }
		virtual bool isResp() { return false; }
        int getType() { return m_type; }
        bool isLocal() { return m_srcNode == m_destNode; }
        ShmemOp getOp() { return m_op; }
        DataType getDataType() { return m_dataType; }

        int getSrcPid() { return m_srcPid; }
        int getDestPid() { return m_destPid; }
        void setDestPid( int val ) { m_destPid = val; }

        void setSrcNode( int val ) { m_srcNode = val; }
        int getSrcNode() { return m_srcNode; }
        void setDestNode( int val ) { m_destNode = val; }
        int getDestNode() { return m_destNode; }

        uint64_t getAddr() { return m_addr; }
        void setIssueTime( SimTime_t time ) { m_issueTime = time; }
        SimTime_t getIssueTime() { return m_issueTime; }
        uint32_t getHandle() { return m_handle; }

        bool getUnit() {
            bool retval = false;
            switch ( m_state ) {
              case GetUnit:
                if ( Nic().m_memReqQ->full( Nic().m_shmemOpQnum ) ) {
                    Nic().dbg.debug( CALL_INFO,1,DBG_X_FLAG,"reserve unit %" PRIx64 "\n", getAddr() );
                    Nic().m_memReqQ->reserve(  Nic().m_shmemOpQnum, this );
                    m_state = WaitUnit;
                } else {
                    retval = true;
                }
                break;

              case WaitUnit:

                if ( Nic().m_memReqQ->reservationReady(  Nic().m_shmemOpQnum, this ) ) {
                    Nic().dbg.debug( CALL_INFO,1,DBG_X_FLAG,"reservation ready %" PRIx64 "\n", getAddr() );
                    m_state = GetUnit;
                    retval = true;
                }
            }
            return retval;
        } 

      private:
        int m_type;
        ShmemOp m_op;
        DataType  m_dataType;
        int m_destNode;
        int m_srcNode;
        int m_srcPid;
        int m_destPid;
        MemEvent* m_memEvent;
        ShmemNic* m_nic;
        uint64_t m_addr;
        uint32_t m_handle;
        SimTime_t m_issueTime;
        ShmemNicNetworkEvent* m_netEvent;
    };

    class IncCmd : public ShmemCmd {
        enum { Read, ReadWait, Increment, Write, WriteWait } m_state;

      public:
        IncCmd( ShmemNic* nic, ShmemOp op, DataType dataType, int srcNode, int srcPid, int destNode, int destPid, uint64_t addr ) :
            ShmemCmd( nic, op, dataType, srcNode, srcPid, destNode, destPid, addr ), m_state( Read ) 
        {
            Nic().dbg.debug( CALL_INFO,1,DBG_X_FLAG,"datatype %d srcNode=%d srcPid=%d destNode=%d destPid=%d addr=%#" PRIx64 "\n",
                    getDataType(), srcNode, srcPid, destNode, destPid, addr );
        } 

        bool process( Cycle_t );

      private:
        uint64_t m_data;
        Cycle_t m_endCycle;
    };


    class FamGetRespCmd : public ShmemCmd {
        enum { Write, WriteWait  } m_state;
      public:
        FamGetRespCmd( ShmemNic* nic, DataType dataType, uint64_t valueAddr ) :
            ShmemCmd( nic, dataType ), m_state( Write ), m_valueAddr(valueAddr)
        { }
        bool process( Cycle_t );
		virtual bool isResp() { return true; }
      private:
        uint64_t m_data;
        uint64_t m_valueAddr;
    };

    class FamGetCmd : public ShmemCmd {
        enum { Read, ReadWait } m_state;

      public:
        FamGetCmd( ShmemNic* nic, ShmemOp op, DataType dataType, int srcNode, int srcPid, int destNode, uint64_t addr, uint32_t handle, ShmemNicNetworkEvent* event = NULL ) :
            ShmemCmd( nic, op, dataType, srcNode, srcPid, destNode, addr ), m_state( Read ) 
        {
            setNetEvent(event);
            setHandle( handle );
            Nic().dbg.debug( CALL_INFO,1,DBG_X_FLAG,"datatype %d srcNode=%d srcPid=%d destNode=%d addr=%#" PRIx64 "\n",
                       getDataType(), srcNode, srcPid, destNode, addr );
        } 

        bool process( Cycle_t );

      private:
        uint64_t m_data;
        Cycle_t m_endCycle;
    };

    class FamPutCmd : public ShmemCmd {
        enum { Write, WriteWait } m_state;

      public:
        FamPutCmd( ShmemNic* nic, ShmemOp op, DataType dataType, int srcNode, int srcPid, int destNode, uint64_t addr, uint64_t value, ShmemNicNetworkEvent* event = NULL ) :
            ShmemCmd( nic, op, dataType, srcNode, srcPid, destNode, addr ), m_state( Write ) 
        {
            setNetEvent(event);
            Nic().dbg.debug( CALL_INFO,1,DBG_X_FLAG,"datatype %d\n",getDataType() );
        } 

        bool process( Cycle_t );

      private:
        uint64_t m_data;
        Cycle_t m_endCycle;
    };

    std::queue<ShmemCmd*> m_hostRespQ;


    void pushResp( uint32_t handle );

    void saveRespInfo( int pid, uint32_t handle, RespInfo* info ) {
        auto& tmp = m_threadInfo[pid].respMap;

        auto ret = tmp.insert(std::pair<uint32_t,RespInfo*>(handle,info) );
        if ( ret.second == false ) {
            out.fatal(CALL_INFO,-1,"Can't insert RespInfo for pid %d handle=%d\n",pid,handle);
        }
    }
    RespInfo* clearRespInfo( int pid, uint32_t handle ) {
        auto& tmp = m_threadInfo[pid].respMap;

        try {
            RespInfo* info = tmp.at(handle);
            tmp.erase(handle);
            return info; 
        } catch (const std::out_of_range& oor) {
            out.fatal(CALL_INFO,-1,"Can't find RespInfo for pid %d handle=%d\n",pid,handle);
        }
        assert(0);
    }

    ShmemCmd* createCmd( NicCmd& cmd, int thread );
    ShmemCmd* createCmd( ShmemNicNetworkEvent* event );

    ShmemCmd* createCmd( int thread, int pos ) {
        NicCmd* cmd = (NicCmd*) (m_backing.data() + thread * m_perPeMemSize);
        dbg.debug( CALL_INFO,1,DBG_X_FLAG,"thread=%d pos=%d addr=%#lx\n", thread, pos, thread * m_perPeMemSize );
        return createCmd( cmd[pos], thread );
    }

    std::map< int, uint32_t > m_waitQuiet;
    std::vector< int > m_pendingCmdCnt;

    bool canSched( int vc ) {
        if ( vc == 1 ) {
            return  m_remoteCmdQ[vc].size() + m_pendingCmdCnt[vc] < m_maxRemoteCmdQSize;
        } else {
            return true;
        }
    }

    void processAckVC();
    void processReqVC();

    bool sendRemoteCmd( int vc, ShmemCmd* );
    void handleTargetEvent( SST::Event* );
    virtual bool clock( SST::Cycle_t );
    void printStuff(Cycle_t);

    void completeCmd(ShmemCmd*);
    void processHostCmdQ();
    void processThreadCmdQs();
    void processPendingQ(Cycle_t);
    void processHostRespQ();
    void feedTheNetwork();
    void feedThePendingQ();
    void processAck(ShmemNicNetworkEvent* event);
    void sendRespToHost( NicResp&,int thread );
    void processFamResp( ShmemNicNetworkEvent* ); 

    void processQuiet(); 

    MemRegion m_region; // Which address region we are, for translating to local addresses

    TimeConverter *m_clockTC;
    Clock::HandlerBase *m_clockHandler;

    Interfaces::SimpleNetwork*     m_linkControl;

    int m_cmdQSize;
    int m_respQSize;

    std::queue<ShmemCmd*> m_hostCmdQ;
    int m_maxHostCmdQSize;

    std::vector< std::queue<ShmemCmd*> > m_localCmdQ;
    int m_maxLocalCmdQSize;

    int m_nextInVc;
    int m_nextOutVc;

    std::vector< std::queue<ShmemCmd*> > m_remoteCmdQ;
    int m_maxRemoteCmdQSize;

    std::deque<ShmemCmd*> m_pendingCmd;
    int m_maxPendingCmds;

    std::queue<int> m_headUpdateQ;

    int m_nicId;
    uint64_t m_clockWork;

    uint64_t m_cmdCnt;
    uint64_t m_finishLocalCmdCnt;
    uint64_t m_finishRemoteCmdCnt;

    Statistic<uint64_t>* m_hostToNicLatency;
    Statistic<uint64_t>* m_famGetLatency;
    Statistic<uint64_t>* m_statCyclesPerIncOp;
    Statistic<uint64_t>* m_statCyclesPerIncOpRead;
    Statistic<uint64_t>* m_statAddrIncOp;
    Statistic<uint64_t>* m_statPendingMemResp;
    Statistic<uint64_t>* m_statPendingCmds;
    Statistic<uint64_t>* m_statHostCmdQ;
    Statistic<uint64_t>* m_statHeadUpdateQ;
    std::vector< Statistic<uint64_t> * > m_statLocalQ;
    std::vector< Statistic<uint64_t> * > m_statRemoteQ;
};

}
}

#endif
