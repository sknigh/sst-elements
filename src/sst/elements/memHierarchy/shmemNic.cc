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

#include <sst_config.h>
#include <sst/core/params.h>
#include <sst/core/simulation.h>

#include "shmemNic.h"

using namespace SST::MemHierarchy;

int ShmemNicNetworkEvent::maxSize = 64;

ShmemNic::ShmemNic(ComponentId_t id, Params &params) : Component(id), m_maxLocalCmdQSize(64), m_clockWork(0), m_maxRemoteCmdQSize(64),
        m_localCmdQ(2), m_remoteCmdQ(2), m_nextInVc(0), m_nextOutVc(0), m_cmdCnt(0),m_finishLocalCmdCnt(0),m_finishRemoteCmdCnt(0), m_pendingCmdCnt(2,0),
        m_cacheLineSize(0)
{
    m_nicId = params.find<int>("nicId", -1);
    m_pesPerNode = params.find<int>("pesPerNode", 0);

    m_ioBaseAddr = params.find<uint64_t>( "baseAddr", 0x100000000 );

    m_cmdQSize = params.find<int>("cmdQSize", 64);

    size_t tmp = m_cmdQSize * sizeof(NicCmd) + 64;
    tmp =  ((tmp-1u) & ~(4096-1u)) + 4096;
    m_perPeMemSize = params.find<size_t>("perPeMemSize", tmp );

    m_cacheLineSize = params.find<uint64_t>("cacheLineSize",0);
    uint64_t hostQueueInfoBaseAddr  = params.find<uint64_t>("hostQueueInfoBaseAddr", 0 );
    size_t   hostQueueInfoSizePerPe = params.find<size_t>("hostQueueInfoSizePerPe", 64 );

    m_hostQueueBaseAddr   = params.find<uint64_t>("hostQueueBaseAddr", 0x10000 );
    size_t   hostQueueSizePerPe  = params.find<size_t>("hostQueueSizePerPe", 4096 );

    uint64_t shmemBaseAddr = params.find<uint64_t>("shmemBaseAddr", 1048576 * 2 );
    size_t   shmemMemPerPe = params.find<size_t>("shmemMemPerPe", 0x100000 );

    m_maxHostCmdQSize = params.find<int>("maxCmdQSize",256);
    m_respQSize = params.find<int>("respQSize", m_cmdQSize);
    m_maxPendingCmds = params.find<size_t>("maxPendingCmds", m_pesPerNode ); 

    assert( m_hostQueueBaseAddr >= hostQueueInfoBaseAddr + hostQueueInfoSizePerPe * m_pesPerNode );
    assert( shmemBaseAddr >= m_hostQueueBaseAddr + hostQueueSizePerPe * m_pesPerNode );
    assert( sizeof(NicResp) * m_respQSize <= hostQueueSizePerPe );
    assert( sizeof(NicCmd) * m_cmdQSize <= m_perPeMemSize - 64 );// last 64 bytes of this block of memory is for queue info

    printf("sizeof(NicCmd)=%zu sizeof(NicResp)=%zu\n",sizeof(NicCmd),sizeof(NicResp));

    // Output for debug
    char buffer[100];
    snprintf(buffer,100,"@t:%d:ShmemNic::@p():@l ",m_nicId);
    dbg.init( buffer, 
        params.find<int>("debug_level", 0),
        params.find<int>("debug_mask",0),
        (Output::output_location_t)params.find<int>("debug", 0));

    assert( m_nicId != -1 );
    assert( m_cmdQSize );
    assert( m_pesPerNode );
    assert( m_ioBaseAddr != -1 );
    assert( m_perPeMemSize );

    if ( m_nicId == 0 ) {
        dbg.debug(CALL_INFO,1,DBG_X_FLAG,"nicId=%d cmdQSize=%d respQSize=%d pesPerNode=%d\n", m_nicId, m_cmdQSize, m_respQSize, m_pesPerNode );
        dbg.debug(CALL_INFO,1,DBG_X_FLAG,"nicBaseAddr=%#" PRIx64 " sizerPerThread=%zu\n",  m_ioBaseAddr, m_perPeMemSize ); 
        dbg.debug(CALL_INFO,1,DBG_X_FLAG,"hostQueueInfoBaseAddr=%#" PRIx64 " hostQueueInfoSizePerPe=%zu\n", hostQueueInfoBaseAddr, hostQueueInfoSizePerPe ); 
        dbg.debug(CALL_INFO,1,DBG_X_FLAG,"hostQueueBaseAddr=%#" PRIx64 " hostQueueSizePerPe=%zu\n",  m_hostQueueBaseAddr, hostQueueSizePerPe);
        dbg.debug(CALL_INFO,1,DBG_X_FLAG,"shmemBaseAddr=%#" PRIx64 " shmemMemPerPe=%zu\n", shmemBaseAddr, shmemMemPerPe );
    }

    m_backing.resize( m_perPeMemSize * m_pesPerNode );

    m_threadInfo.resize( m_pesPerNode );

    for ( int i = 0; i < m_pesPerNode; i++ ) {

        auto& info = m_threadInfo[i];
        info.pendingCnt = 0;

        info.shmemRegion.baseAddr = shmemBaseAddr + shmemMemPerPe * i; 
        info.shmemRegion.length = shmemMemPerPe; 

        uint64_t tailAddr = hostQueueInfoBaseAddr + i * hostQueueInfoSizePerPe;
        m_hostCmdQueueV.push_back( HostCmdQueueInfo( tailAddr ) );

        // put it at the end of the thread nic memory space 
        uint32_t* tailPtr = (uint32_t*) (m_backing.data() + m_perPeMemSize * i + m_perPeMemSize  - 4); 
        uint64_t queueAddr = m_hostQueueBaseAddr + i * hostQueueSizePerPe;
        uint64_t headAddr =  hostQueueInfoBaseAddr + i * hostQueueInfoSizePerPe + 4;
        m_hostRespQueueV.push_back( HostRespQueueInfo( tailPtr, queueAddr, headAddr ) );
    }

    m_link = loadUserSubComponent<MemLinkBase>("cpulink");

    string link_lat         = params.find<std::string>("direct_link_latency", "10 ns");

    if (!m_link && isPortConnected("direct_link")) {
        Params linkParams = params.find_prefix_params("cpulink.");
        linkParams.insert("port", "direct_link");
        linkParams.insert("latency", link_lat, false);
        linkParams.insert("accept_region", "1", false);
        m_link = loadAnonymousSubComponent<MemLinkBase>("memHierarchy.MemLink", "cpulink", 0, ComponentInfo::SHARE_PORTS | ComponentInfo::INSERT_STATS, linkParams);
    } else if (!m_link) {

        if (!isPortConnected("network")) {
            out.fatal(CALL_INFO,-1,"%s, Error: No connected port detected. Connect 'direct_link' or 'network' port.\n", getName().c_str());
        }

        Params nicParams = params.find_prefix_params("memNIC.");
        nicParams.insert("group", "4", false);
        nicParams.insert("accept_region", "1", false);

        if (isPortConnected("network_ack") && isPortConnected("network_fwd") && isPortConnected("network_data")) {
            nicParams.insert("req.port", "network");
            nicParams.insert("ack.port", "network_ack");
            nicParams.insert("fwd.port", "network_fwd");
            nicParams.insert("data.port", "network_data");
            m_link = loadAnonymousSubComponent<MemLinkBase>("memHierarchy.MemNICFour", "cpulink", 0, ComponentInfo::SHARE_PORTS | ComponentInfo::INSERT_STATS, nicParams);
        } else {
            nicParams.insert("port", "network");
            m_link = loadAnonymousSubComponent<MemLinkBase>("memHierarchy.MemNIC", "cpulink", 0, ComponentInfo::SHARE_PORTS | ComponentInfo::INSERT_STATS, nicParams);
        }
    }

    m_region.start = m_ioBaseAddr; 
    m_region.end = m_ioBaseAddr + m_perPeMemSize * m_pesPerNode;
    m_region.interleaveSize = 0;
    m_region.interleaveStep = 0;

    m_link->setRegion(m_region);

    m_clockLink = m_link->isClocked();
    m_link->setRecvHandler( new Event::Handler<ShmemNic>(this, &ShmemNic::handleTargetEvent));
    m_link->setName(getName());

    // Clock handler
    std::string clockFreq = params.find<std::string>("clock", "1GHz");
    m_clockHandler = new Clock::Handler<ShmemNic>(this, &ShmemNic::clock);
    m_clockTC = registerClock( clockFreq, m_clockHandler );

    // Set up the linkcontrol
    m_linkControl = loadUserSubComponent<Interfaces::SimpleNetwork>( "rtrLink", ComponentInfo::SHARE_NONE, 2 );
    assert( m_linkControl );

    m_tailWriteQnum = 0;
    m_shmemOpQnum = 1;
    m_respQueueMemChannel = 2;
    int numSrc = 3;

    int maxPending = params.find<int>("maxMemReqs",128);
    m_memReqQ = new MemRequestQ( this, maxPending, maxPending/numSrc, numSrc );

    m_hostToNicLatency = registerStatistic<uint64_t>("hostToNicLatency");
    m_famGetLatency = registerStatistic<uint64_t>("FamGetLatency");
    m_statCyclesPerIncOp = registerStatistic<uint64_t>("cyclePerIncOp");
    m_statCyclesPerIncOpRead = registerStatistic<uint64_t>("cyclePerIncOpRead");
    m_statAddrIncOp = registerStatistic<uint64_t>("addrIncOp");
    m_statPendingMemResp = registerStatistic<uint64_t>("pendingMemResp");
    m_statPendingCmds = registerStatistic<uint64_t>("pendingCmds");
    m_statHostCmdQ = registerStatistic<uint64_t>("hostCmdQ");
    m_statHeadUpdateQ = registerStatistic<uint64_t>("headUpdateQ");
    m_statLocalQ.push_back( registerStatistic<uint64_t>("localQ_0") );
    m_statLocalQ.push_back( registerStatistic<uint64_t>("localQ_1") );
    m_statRemoteQ.push_back( registerStatistic<uint64_t>("remoteQ_0") );
    m_statRemoteQ.push_back( registerStatistic<uint64_t>("remoteQ_1") );
}

void ShmemNic::handleTargetEvent(SST::Event* event) {

    MemEventBase *meb = static_cast<MemEventBase*>(event);
    Command cmd = meb->getCmd();
    MemEvent * ev = static_cast<MemEvent*>(meb);
#if 0
    dbg.debug( CALL_INFO,1,DBG_X_FLAG,"(%s) Received: '%s'\n", getName().c_str(), meb->getBriefString().c_str());
#endif

    //printf("baseAddr=%" PRIx64 "\n",ev->getBaseAddr());
    //printf("addr=%" PRIx64 "\n",ev->getAddr());
    //printf("%d\n",ev->queryFlag(MemEvent::F_NONCACHEABLE));

    switch (cmd) {
        // write non-cacheable
        case Command::GetX: {
            int thread = ( ev->getAddr() - m_ioBaseAddr ) / m_perPeMemSize;

            NicCmd* cmd = reinterpret_cast<NicCmd*>(ev->getPayload().data());
#if 1 
            std::ostringstream tmp(std::ostringstream::ate);
            tmp << std::hex;
            for ( int i = 0; i < ev->getSize(); i++ ) {
                tmp << "0x" << (int) ev->getPayload().at(i)  << ",";
            }
#endif
            dbg.debug( CALL_INFO,1,DBG_X_FLAG,"Write size=%zu addr=%" PRIx64 " offset=%llu thread=%d handle=%d, %s\n",
                    ev->getPayload().size(), ev->getAddr(), ev->getAddr() - m_ioBaseAddr, thread, cmd->handle, tmp.str().c_str() );

            if ( 0 == ( ev->getAddr() - m_ioBaseAddr ) % sizeof(NicCmd) ) {
                m_hostToNicLatency->addData( getCurrentSimTimeNano() - cmd->timeStamp);

                dbg.debug( CALL_INFO,1,DBG_X_FLAG,"new command from thread %d\n",thread);
                ++m_threadInfo[thread].pendingCnt;
                m_headUpdateQ.push( thread );
                m_statHeadUpdateQ->addData( m_headUpdateQ.size() );
                ++m_cmdCnt;
            }

            memcpy( m_backing.data() + ev->getAddr() - m_ioBaseAddr, ev->getPayload().data(), ev->getSize() ); 
			{
				MemEventBase* resp = ev->makeResponse();
				if ( m_link->spaceToSend( resp ) ) {
					m_link->send( resp );
				} else {
					assert(0);
				}
			}
            delete ev;

        } break;

        // read non-cacheable
        case Command::GetS: {
            int thread = ( ev->getAddr() - m_ioBaseAddr ) / m_perPeMemSize;

            MemEvent * resp = ev->makeResponse();
            resp->setPayload(resp->getSize(), m_backing.data() + ev->getAddr() - m_ioBaseAddr);
        
            assert(0);

            m_link->send( resp );
            delete ev;
        } break;

        case Command::GetSResp: // PrRead or GetS  Resp
        case Command::GetXResp: // PrWrite or GetX Resp
        case Command::NACK:
            m_memReqQ->handleResponse( ev );
            break;

        default:
            out.fatal(CALL_INFO,-1,"ShmemNic received unrecognized command: %s", CommandString[(int)cmd]);
    }

}

bool ShmemNic::FamGetRespCmd::process( Cycle_t cycle ) {
    bool retval = false;
    switch ( m_state ) {
      case Write:

        if ( getUnit() ) {
            Nic().dbg.debug( CALL_INFO,1,DBG_X_FLAG,"write %#" PRIx64 " dataType=%d\n", m_valueAddr, getDataType()  );

			m_data = 0;
            Nic().m_memReqQ->write( Nic().m_shmemOpQnum, m_valueAddr, Nic().dataTypeSize(getDataType()), m_data, m_callback );
            m_state = WriteWait; 
        }
        break;

      case WriteWait:
        if ( ! getMemEvent() ) { 
            break;
        }

        Nic().dbg.debug( CALL_INFO,1,DBG_X_FLAG,"back from write value %" PRIx64 "\n", m_valueAddr );

        delete getMemEvent();
#if 0
        clearMemEvent();
        m_state = WriteFlag;

      case WriteFlag:

        if ( getUnit() ) {
            Nic().dbg.debug( CALL_INFO,1,DBG_X_FLAG," write %#" PRIx64 " dataType=%d\n", m_flagAddr, getDataType()  );

            m_data = 1;
            Nic().m_memReqQ->write( Nic().m_shmemOpQnum, m_flagAddr, Nic().dataTypeSize(getDataType()), m_data, m_callback );
            m_state = WriteFlagWait; 
        }
        break;

      case WriteFlagWait:
        if ( ! getMemEvent() ) { 
            break;
        }

        Nic().dbg.debug( CALL_INFO,1,DBG_X_FLAG,"back from write flag %" PRIx64 "\n", m_flagAddr );

        delete getMemEvent();
#endif
        retval = true;
        break;
    }
    return retval;
}

bool ShmemNic::FamGetCmd::process( Cycle_t cycle ) {
    bool retval = false;

    switch ( m_state ) {

    case Read:
        if ( getUnit() ) {
            int thread = 0;
            ShmemMemRegion& region = Nic().m_threadInfo[thread].shmemRegion;
            Nic().dbg.debug( CALL_INFO,1,DBG_X_FLAG,"srcNode=%d srcPid=%d, read %#" PRIx64 " + %#" PRIx64"  dataType=%d\n",
                        getSrcNode(), getSrcPid(), region.baseAddr, getAddr(), getDataType()  );

            Nic().m_memReqQ->read( Nic().m_shmemOpQnum,  region.baseAddr + getAddr(), Nic().dataTypeSize(getDataType()), m_callback );
            setIssueTime( Nic().getCurrentSimTimeNano() );
            m_state = ReadWait; 
        }
        break;

    case ReadWait:
        if ( ! getMemEvent() ) { 
            break; 
        }

        Nic().dbg.debug( CALL_INFO,1,DBG_X_FLAG,"back from read %" PRIx64 " numBytes=%d\n", getAddr(), getMemEvent()->getSize() );
        Nic().m_famGetLatency->addData( Nic().getCurrentSimTimeNano() - getIssueTime() );

        memcpy( &m_data, getMemEvent()->getPayload().data(), getMemEvent()->getSize() ); 

        delete getMemEvent();
        clearMemEvent();
        retval = true;
        break;

    }
    return retval;
}
bool ShmemNic::FamPutCmd::process( Cycle_t cycle ) {

    int retval = false;

    switch ( m_state ) {
    case Write:

        if ( getUnit() ) {
            int thread = 0;
            ShmemMemRegion& region = Nic().m_threadInfo[thread].shmemRegion;
            Nic().dbg.debug( CALL_INFO,1,DBG_X_FLAG,"srcNode=%d srcPid=%d, write %#" PRIx64 " + %#" PRIx64"  dataType=%d\n",
                        getSrcNode(), getSrcPid(), region.baseAddr, getAddr(), getDataType()  );

			m_data = 0;
            Nic().m_memReqQ->write( Nic().m_shmemOpQnum, region.baseAddr + getAddr(), Nic().dataTypeSize(getDataType()), m_data, m_callback );
            m_state = WriteWait; 
        }
        break;

    case WriteWait:
        if ( ! getMemEvent() ) { 
            break;
        }

        Nic().dbg.debug( CALL_INFO,1,DBG_X_FLAG,"back from write %" PRIx64 "\n", getAddr() );

        delete getMemEvent();
        retval = true;
        break;
    }
    return retval;
}

bool ShmemNic::IncCmd::process( Cycle_t cycle ) {

    bool retval = false;

    switch ( m_state ) {

    case Read:
        if ( getUnit() ) {
            int thread = getDestPid();
            ShmemMemRegion& region = Nic().m_threadInfo[thread].shmemRegion;
            setIssueTime( Nic().getCurrentSimTimeNano() );

            Nic().dbg.debug( CALL_INFO,1,DBG_X_FLAG,"srcNode=%d destPid=%d destThread=%d, read %#" PRIx64 " + %#" PRIx64"  dataType=%d\n",
                        getSrcNode(), getDestPid(), thread, region.baseAddr, getAddr(), getDataType()  );

            Nic().m_memReqQ->read( Nic().m_shmemOpQnum,  region.baseAddr + getAddr(), Nic().dataTypeSize(getDataType()), m_callback );
            m_state = ReadWait; 
        }
        break;

    case ReadWait:
        if ( ! getMemEvent() ) { 
            break; 
        }
        Nic().m_statCyclesPerIncOpRead->addData( Nic().getCurrentSimTimeNano() - getIssueTime() );

        Nic().dbg.debug( CALL_INFO,1,DBG_X_FLAG,"back from read %" PRIx64 " numBytes=%d\n", getAddr(), getMemEvent()->getSize() );
        m_endCycle = cycle + 2;

        memcpy( &m_data, getMemEvent()->getPayload().data(), getMemEvent()->getSize() ); 
        m_data += 1;

        delete getMemEvent();
        clearMemEvent();

        m_state = Increment;
        break;

    case Increment:
        if ( cycle != m_endCycle ) { 
            break; 
        }
        Nic().dbg.debug( CALL_INFO,1,DBG_X_FLAG,"back from inc wait %" PRIx64 "\n", getAddr() );
        m_state = Write;

    case Write:

        if ( getUnit() ) {
            int thread = getDestPid();
            ShmemMemRegion& region = Nic().m_threadInfo[thread].shmemRegion;
            Nic().dbg.debug( CALL_INFO,1,DBG_X_FLAG,"write %" PRIx64 "\n", getAddr() );
            Nic().m_memReqQ->write( Nic().m_shmemOpQnum, region.baseAddr + getAddr(), Nic().dataTypeSize(getDataType()), m_data, m_callback );
            m_state = WriteWait; 
        }
        break;

    case WriteWait:
        if ( ! getMemEvent() ) { 
            break;
        }

        Nic().dbg.debug( CALL_INFO,1,DBG_X_FLAG,"back from write %" PRIx64 "\n", getAddr() );
        Nic().m_statCyclesPerIncOp->addData( Nic().getCurrentSimTimeNano() - getIssueTime() );

        delete getMemEvent();
        retval = true;
        break;
    }
    return retval;
}

ShmemNic::ShmemCmd* ShmemNic::createCmd( NicCmd& cmd, int thread ) {

    dbg.debug( CALL_INFO,1,DBG_X_FLAG,"thread=%d type=%d handle=%d\n", thread, cmd.type, cmd.handle);

    if ( cmd.type == NicCmd::Type::Shmem ) {
        switch ( cmd.data.shmem.op ) {
            case NicCmd::Data::Shmem::Inc: {
                int destNode = cmd.data.shmem.pe / m_pesPerNode;
                int destPid = cmd.data.shmem.pe % m_pesPerNode;
                return new IncCmd( this, ShmemInc, Int64, m_nicId, thread, destNode, destPid, cmd.data.shmem.destAddr );
            } 
            case NicCmd::Data::Shmem::Quiet: {
                assert( m_waitQuiet.find(thread) == m_waitQuiet.end() );
                --m_threadInfo[thread].pendingCnt;
                m_waitQuiet[thread]=cmd.handle;
                return NULL;
            } 
            default:
            assert(0);
        }
    } else {
        dbg.debug( CALL_INFO,1,DBG_FAM_FLAG,"op=%d, destNode=%d, addr=%#" PRIx64 "\n",cmd.data.fam.op, cmd.data.fam.destNode, cmd.data.fam.destAddr);
        switch ( cmd.data.fam.op ) {
            case NicCmd::Data::Fam::Get:
                saveRespInfo( thread, cmd.handle, new RespInfo( Int64, cmd.respAddr, cmd.numData ) );
            
                return new FamGetCmd( this, (ShmemOp) FamGet, Int64, m_nicId, thread, cmd.data.fam.destNode, cmd.data.fam.destAddr, 
                     cmd.handle );

            case NicCmd::Data::Fam::Put:
                return new FamPutCmd( this, (ShmemOp) FamPut, Int64, m_nicId, thread, cmd.data.fam.destNode, cmd.data.fam.destAddr, cmd.value );

            default:
                assert(0);
        }
    }
    return NULL;
}

ShmemNic::ShmemCmd* ShmemNic::createCmd( ShmemNicNetworkEvent* event ) {

    dbg.debug( CALL_INFO,1,DBG_X_FLAG,"%d\n",event->getOp());

    if ( event->getType() == NicCmd::Type::Shmem ) {

        switch ( event->getOp() ) {
          case ShmemInc:
            return new IncCmd( this, (ShmemOp) event->getOp(), (DataType) event->getDataType(),
                                event->getSrcNode(), event->getSrcPid(), m_nicId, event->getDestPid(), event->getAddress() ); 
          default:
            assert(0);
        }

    } else {
        dbg.debug( CALL_INFO,1,DBG_X_FLAG,"op=%d, srcNode=%d, srcPid=%d addr=%#" PRIx64 "\n",event->getOp(), event->getSrcNode(), event->getSrcPid(), event->getAddress());
        switch ( event->getOp() ) {
           case FamGet:
            return new FamGetCmd( this, (ShmemOp) event->getOp(), (DataType) event->getDataType(), event->getSrcNode(), event->getSrcPid(), 
                    m_nicId, event->getAddress(), event->getHandle(), event );
          case FamPut:
            return new FamPutCmd( this, (ShmemOp) event->getOp(), (DataType) event->getDataType(), event->getSrcNode(), event->getSrcPid(), 
                    m_nicId, event->getAddress(), 0xdeadbeef, event );
          default:
            assert(0);
        }
    }
}

bool ShmemNic::sendRemoteCmd( int vc, ShmemCmd* cmd ) 
{
    dbg.debug( CALL_INFO,1,DBG_X_FLAG,"op=%d node=%d addr=%" PRIu64 " \n",
                        cmd->getOp(),cmd->getDestNode(),cmd->getAddr());

    if ( ! m_linkControl->spaceToSend( vc, ShmemNicNetworkEvent::getMaxSizeBytes() * 8 ) ) {
        return false;
    }

    ShmemNicNetworkEvent* ev = new ShmemNicNetworkEvent;
    ev->setType( cmd->getType() );
    ev->setOp( cmd->getOp() );
    ev->setDataType( cmd->getDataType() );
    ev->setSrcNode( cmd->getSrcNode() );
    ev->setPktOverhead( 16 );
    ev->setHandle( cmd->getHandle() );

    ev->setAddress( cmd->getAddr() );
    ev->setSrcPid( cmd->getSrcPid() );
    ev->setDestPid( cmd->getDestPid() );
    ev->setSrcNode( m_nicId );

	Interfaces::SimpleNetwork::Request* req = new Interfaces::SimpleNetwork::Request();
    req->dest = cmd->getDestNode();
    req->src = m_nicId;
    req->size_in_bits = ev->getPayloadSizeBytes() * 8;
    req->vn = 0;
    req->givePayload( ev );
	m_linkControl->send( req, vc );	
    return true;
}

bool ShmemNic::clock(Cycle_t cycle) {

    bool unclock = m_link->clock();

#if 0 
    //if ( m_nicId == 0 && ( cycle % 10000 ) == 0 ) { 
    //if ( m_nicId == 0 ) {
    if ( ( cycle % 10000 ) == 0 ) { 
        printStuff(cycle);
    }
#endif

    processPendingQ(cycle);
    
    processHostRespQ();
    processAckVC();
    processReqVC();

    processThreadCmdQs();
    processHostCmdQ();

    feedTheNetwork();

    feedThePendingQ();

    processQuiet();

    m_memReqQ->process();

    return false;
}

void  ShmemNic::processQuiet() {
    auto iter = m_waitQuiet.begin();
    for ( ; iter != m_waitQuiet.end(); ++iter ) {
        if ( 0 == m_threadInfo[iter->first].pendingCnt ) {
            dbg.debug( CALL_INFO,1,DBG_X_FLAG,"thread %d is quiet, handle %d\n",iter->first,iter->second);
            NicResp resp;
            resp.handle = iter->second;
            sendRespToHost( resp, iter->first );

            m_waitQuiet.erase( iter );
            // only do one per clock
            break;
        } 
    } 
}

void ShmemNic::completeCmd( ShmemCmd* cmd ) 
{
    if ( cmd->isResp()  ) {
        dbg.debug( CALL_INFO,1,DBG_X_FLAG,"response\n" );
        m_hostRespQ.push( cmd );
    } else if ( cmd->isLocal()  ) {
        int thread = cmd->getSrcPid();
        dbg.debug( CALL_INFO,1,DBG_X_FLAG,"thread=%d command is complete\n",thread );

        --m_threadInfo[thread].pendingCnt;
        ++m_finishLocalCmdCnt;
        --m_pendingCmdCnt[0];
        delete cmd;
    } else {
        --m_pendingCmdCnt[1];
        assert( m_remoteCmdQ[1].size() < m_maxRemoteCmdQSize );

        dbg.debug( CALL_INFO,1,DBG_X_FLAG,"remote command is complete\n"); 

        cmd->setDestPid( cmd->getSrcPid() );
        cmd->setDestNode( cmd->getSrcNode() );
        cmd->setSrcNode( m_nicId );
        m_remoteCmdQ[1].push( cmd );
        m_statRemoteQ[1]->addData( m_remoteCmdQ[1].size() );
    }
}

void ShmemNic::processThreadCmdQs( ) {
    if ( ! m_memReqQ->full( m_tailWriteQnum ) && m_hostCmdQ.size() < m_maxHostCmdQSize ) {
        if ( ! m_headUpdateQ.empty() ) {

            int thread = m_headUpdateQ.front();
            m_headUpdateQ.pop();

            HostCmdQueueInfo& info = m_hostCmdQueueV[thread]; 

            dbg.debug( CALL_INFO,1,DBG_X_FLAG,"thread %d cmd available tail=%d\n",thread,info.localTailIndex);

            ShmemCmd* cmd = createCmd( thread, info.localTailIndex );
            if ( cmd ) {
                m_hostCmdQ.push( cmd ); 
                m_statHostCmdQ->addData( m_hostCmdQ.size() );
            }
            ++info.localTailIndex;
            info.localTailIndex %= m_cmdQSize;		
            dbg.debug( CALL_INFO,1,DBG_X_FLAG,"write tail=%d at %#" PRIx64 "\n",info.localTailIndex, info.tailAddr );
            m_memReqQ->write( m_tailWriteQnum, info.tailAddr, 4, info.localTailIndex );
        }
    }
}

void ShmemNic::processHostRespQ( ) {
    if ( ! m_memReqQ->full( m_respQueueMemChannel ) ) {
        if ( ! m_hostRespQ.empty() ) {    
            assert(0);
#if  0
            int thread = cmd->getDestPid();
#endif
        }
    }
}

void ShmemNic::processFamResp( ShmemNicNetworkEvent* event )
{
    dbg.debug(CALL_INFO,1,DBG_X_FLAG,"destPid=%d handle=%d\n", event->getDestPid(), event->getHandle());
    switch( event->getOp()  ) {
        case FamGet: {
            RespInfo* info = clearRespInfo( event->getDestPid(), event->getHandle() );
            dbg.debug(CALL_INFO,1,DBG_X_FLAG,"FamGet destPid=%d handle=%d\n",event->getDestPid(),event->getHandle());
            if ( info->numData == 1 ) {

                NicResp resp;
                resp.handle = event->getHandle();
                resp.value = 0xdeadbeef;
                sendRespToHost( resp, event->getDestPid() );
            } else {
                assert(0);
#if 0
                m_localCmdQ[0].push( new FamGetRespCmd( this, info->dataType, event->getHandle() ) );
#endif
            }
            delete info;
            break;
        }
        case FamPut: 
            dbg.debug(CALL_INFO,1,DBG_X_FLAG,"FamPut pid=%d handl=%d\n",event->getDestPid(), event->getHandle());
            break;
        default:
            assert(0);
    }
    delete event;
}

void ShmemNic::sendRespToHost( NicResp& resp, int thread )
{
    HostRespQueueInfo& info = m_hostRespQueueV[thread];
    dbg.debug( CALL_INFO,1,DBG_X_FLAG,"tailPointer=%d localHeadIndex=%d\n", *info.tailPtr, info.localHeadIndex  );
    if ( *info.tailPtr != ( info.localHeadIndex + 1 ) % m_respQSize ) {

         resp.timeStamp = getCurrentSimTimeNano(); 
         dbg.debug( CALL_INFO,1,DBG_X_FLAG,"write command to %#" PRIx64 "\n", info.cmdAddr(info.localHeadIndex) );
         m_memReqQ->write( m_respQueueMemChannel, info.cmdAddr(info.localHeadIndex), sizeof(resp), reinterpret_cast<uint8_t*>(&resp) );
         m_memReqQ->fence( m_respQueueMemChannel );
         ++info.localHeadIndex;
         info.localHeadIndex %= m_respQSize;		
         m_memReqQ->write( m_respQueueMemChannel, info.headAddr, 4, info.localHeadIndex );
     } else {
        assert(0);
    }
}

void ShmemNic::processPendingQ( Cycle_t cycle ) {
    std::deque< ShmemCmd* >::iterator iter = m_pendingCmd.begin();
    for ( ; iter != m_pendingCmd.end(); ) {
        if ( (*iter)->process( cycle ) ) {

            completeCmd( *iter );

            dbg.debug( CALL_INFO,1,DBG_X_FLAG,"move command to completing \n");
            iter = m_pendingCmd.erase(iter);
        } else {
            ++iter;
        }
    }
}

void ShmemNic::processHostCmdQ() {
    if ( ! m_hostCmdQ.empty() ) {
        if ( m_hostCmdQ.front()->isLocal() ) {
            if ( m_localCmdQ[0].size() < m_maxLocalCmdQSize ) {
                m_localCmdQ[0].push( m_hostCmdQ.front() );

                m_statLocalQ[0]->addData( m_localCmdQ[0].size() );

                m_hostCmdQ.pop();
            }
        } else {
            if ( m_remoteCmdQ[0].size() < m_maxRemoteCmdQSize ) {
                m_remoteCmdQ[0].push( m_hostCmdQ.front() );

                m_statRemoteQ[0]->addData( m_remoteCmdQ[0].size() );

                m_hostCmdQ.pop();
            }
        }
    }
}

void ShmemNic::feedTheNetwork() {
    for ( int i = 0; i < 2; i++ ) {
        int pos = m_nextOutVc % 2;

        ++m_nextOutVc;

        if ( ! m_remoteCmdQ[pos].empty() ) {
            if ( sendRemoteCmd( pos, static_cast<ShmemCmd*>( m_remoteCmdQ[pos].front() ) ) ) {
                delete m_remoteCmdQ[pos].front();
                m_remoteCmdQ[pos].pop();
                break;
            }
        }
    }
}

void ShmemNic::feedThePendingQ() {
    if ( m_pendingCmd.size() < m_maxPendingCmds ) {
        for ( int i = 0; i < 2; i++ ) {
            int pos = m_nextInVc % 2;
            ++m_nextInVc;

            if ( ! m_localCmdQ[pos].empty() && canSched(pos) ) {
                dbg.debug( CALL_INFO,1,DBG_X_FLAG,"issue shmem command\n");
                ++m_pendingCmdCnt[pos];
                m_pendingCmd.push_back( m_localCmdQ[pos].front() );
                m_statPendingCmds->addData( m_pendingCmd.size() );
                m_localCmdQ[pos].pop();
                break;
            }
        }
    }
}

void ShmemNic::processReqVC()
{
    if ( m_localCmdQ[1].size() < m_maxLocalCmdQSize ) {
        SST::Interfaces::SimpleNetwork::Request* req = m_linkControl->recv(0);
        if ( req ) {
            Event* payload;
            if ( ( payload = req->takePayload() ) ) {

                dbg.debug(CALL_INFO,1,DBG_X_FLAG,"got packet\n");

                ShmemNicNetworkEvent* event = static_cast<ShmemNicNetworkEvent*>(payload);
            
                m_localCmdQ[1].push( createCmd( event ) );

                m_statLocalQ[1]->addData( m_localCmdQ[1].size() );
            }
            delete req;
        }
    }
}
void ShmemNic::processAckVC()
{
    SST::Interfaces::SimpleNetwork::Request* req = m_linkControl->recv(1);
    if ( req ) {
        Event* payload;
        if ( ( payload = req->takePayload() ) ) {


            ShmemNicNetworkEvent* event = static_cast<ShmemNicNetworkEvent*>(payload);
            
            int thread = event->getDestPid();
            processAck( event );
    
            assert(m_threadInfo[thread].pendingCnt);
            --m_threadInfo[thread].pendingCnt;
            dbg.debug(CALL_INFO,1,DBG_X_FLAG,"ACK for thread %d, pendingCnt %d\n",thread, m_threadInfo[thread].pendingCnt);
            
            ++m_finishRemoteCmdCnt;
        }
        delete req;
    }
}

void ShmemNic::processAck(ShmemNicNetworkEvent* event) {
    int thread = event->getDestPid();
    dbg.debug( CALL_INFO,1,DBG_X_FLAG,"thread=%d command got ACK for %s %d\n",thread, event->getType() == NicCmd::Type::Shmem ?"Shmem":"Fam",event->getOp() );

    if ( event->getType() == NicCmd::Type::Shmem ) {
        switch( event->getOp()) {
          case ShmemInc:
                dbg.debug(CALL_INFO,2,0,"ShmemInc\n");
                break;
          default:
            assert(0);
        }
    } else if ( event->getType() == NicCmd::Type::Fam ) {
        processFamResp( event );

    } else {
        assert(0);
    }
}


void ShmemNic::init(unsigned int phase) {
    m_link->init(phase);

    m_region = m_link->getRegion(); // This can change during init, but should stabilize before we start receiving init data

#if 1 
    /* Inherit region from our source(s) */
    if (!phase) {
        /* Announce our presence on link */
		int requestWidth = 8;
        m_link->sendInitData(new MemEventInitCoherence(getName(), Endpoint::Memory, true, false, requestWidth, false));
    }

    while (MemEventInit *ev = m_link->recvInitData()) {
        if (ev->getCmd() == Command::NULLCMD) {
            if (ev->getInitCmd() == MemEventInit::InitCommand::Coherence) {
                MemEventInitCoherence * mEv = static_cast<MemEventInitCoherence*>(ev);
                if ( m_cacheLineSize == 0 ) {
                    m_cacheLineSize = mEv->getLineSize();
                }
                assert( m_cacheLineSize == mEv->getLineSize() ); 
            }
            delete ev;
        } else {
            assert(0);
        }
    }
#endif

    m_linkControl->init(phase);
}

void ShmemNic::setup(void) {
    m_link->setup();
    assert( m_cacheLineSize % sizeof(NicCmd) == 0 );  
    assert( m_cacheLineSize % sizeof(NicResp) == 0 );  
    assert( m_hostQueueBaseAddr % m_cacheLineSize == 0 );
}


void ShmemNic::finish(void) {

#if 0
    printStuff(0);
    printf("clockWork=%" PRIu64 " total=%" PRIu64 " %f %% \n", m_clockWork, getNextClockCycle(m_clockTC)-1, (double) m_clockWork/ (double)(getNextClockCycle(m_clockTC)-1));
#endif
    delete m_memReqQ;
    m_link->finish();
}

void ShmemNic::printStuff( Cycle_t cycle ) {
        dbg.output("%s()\n",__func__);
        printf("%" PRIu64 " %d: host=%zu pending=%zu head=%zu\n", cycle, m_nicId, m_hostCmdQ.size(), m_pendingCmd.size(), m_headUpdateQ.size() );
        printf("%" PRIu64 " %d: vc=0 local=%zu remote=%zu pendingCmdCnt=%d\n", cycle, m_nicId, m_localCmdQ[0].size(), m_remoteCmdQ[0].size(), m_pendingCmdCnt[0] );
        printf("%" PRIu64 " %d: vc=1 local=%zu remote=%zu pendingCmdCnt=%d\n", cycle, m_nicId, m_localCmdQ[1].size(), m_remoteCmdQ[1].size(), m_pendingCmdCnt[1] );

        printf("%" PRIu64 " %d: cmdCnt=%" PRIu64 " localFini=%" PRIu64 " remoteFini=%" PRIu64 " :", cycle, m_nicId, m_cmdCnt, m_finishLocalCmdCnt, m_finishRemoteCmdCnt );
        for ( int i = 0; i < m_pesPerNode; i++ ) {
            printf("%d ",m_threadInfo[i].pendingCnt);
        }
        printf("\n");
        m_memReqQ->print( cycle );
}
