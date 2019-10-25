// Copyright 2009-2018 NTESS. Under the terms
// of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.
//
// Copyright (c) 2009-2018, NTESS
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
#include "rdmaNic.h"
#include "rdmaNicCmds.h"
#include "rdmaNicResp.h"
#include "../nic/nic.h"

#define CALL_INFO_LAMBDA     __LINE__, __FILE__

using namespace SST::Aurora::RDMA;
using namespace SST::Interfaces;

const char* RdmaNicSubComponent::m_cmdName[] = {
    FOREACH_CMD(GENERATE_CMD_STRING)
};
const char* RdmaNicSubComponent::protoNames[] = {"Message","RdmaRead","RdmaWrite"};

RdmaNicSubComponent::RdmaNicSubComponent( ComponentId_t id, Params& params ) : NicSubComponent(id, params),
	m_streamIdCnt(0),
	m_recvPktsPending(0), m_recvDmaPending(false), m_recvDmaBlockedEvent(NULL),
    m_sendPktsPending(0), m_sendDmaPending(false), m_sendDmaBlockedEvent(NULL),
    m_pendingNetReq(NULL)
{

   if ( params.find<bool>("print_all_params",false) ) {
        printf("Aurora::RDMA::RdmaNicSubComponent()\n");
        params.print_all_params(std::cout);
    }

    m_dbg.init("@t:Aurora::RDMA::Nic::@p():@l ", params.find<uint32_t>("verboseLevel",0),
            params.find<uint32_t>("verboseMask",0), Output::STDOUT );

    m_dbg.debug(CALL_INFO,1,2,"\n");

	m_cmdFuncTbl.resize( NicCmd::NumCmds);
	m_cmdFuncTbl[NicCmd::CreateRQ] = &RdmaNicSubComponent::createRQ;
	m_cmdFuncTbl[NicCmd::PostRecv] = &RdmaNicSubComponent::postRecv;
	m_cmdFuncTbl[NicCmd::Send] = &RdmaNicSubComponent::send;
	m_cmdFuncTbl[NicCmd::CheckRQ] = &RdmaNicSubComponent::checkRQ;
	m_cmdFuncTbl[NicCmd::RegisterMem] = &RdmaNicSubComponent::registerMem;
	m_cmdFuncTbl[NicCmd::Read] = &RdmaNicSubComponent::read;
	m_cmdFuncTbl[NicCmd::Write] = &RdmaNicSubComponent::write;
}

void RdmaNicSubComponent::setup()
{
    char buffer[100];
    snprintf(buffer,100,"@t:%d:Aurora::RDMA::Nic::@p():@l ", getNodeNum());
    m_dbg.setPrefix(buffer);
}

void RdmaNicSubComponent::setNumCores( int num ) {
	NicSubComponent::setNumCores( num );
	m_coreTbl.resize( getNumCores() );
}

void RdmaNicSubComponent::handleEvent( int core, Event* event ) {

  	m_dbg.debug(CALL_INFO,1,2,"\n");

	if ( ! m_clocking ) {
		startClocking();
	} 

	NicCmd* cmd = static_cast<NicCmd*>(event); 
    m_dbg.debug(CALL_INFO,2,EVENT_DEBUG_MASK,"%s\n",m_cmdName[cmd->type]);
	assert(cmd->type < NicCmd::NumCmds);
	(this->*m_cmdFuncTbl[cmd->type])( core, event );
}

bool RdmaNicSubComponent::clockHandler( Cycle_t cycle ) {

	m_dbg.debug(CALL_INFO,3,1,"\n");

    if ( m_pendingNetReq && sendNetReq( m_pendingNetReq ) ) {
        m_dbg.debug(CALL_INFO,2,1,"sent pendingNetReq\n");
        m_pendingNetReq = NULL;
        netReqSent();
    }

    while ( ! m_selfEventQ.empty() ) {
        handleSelfEvent( m_selfEventQ.front() );
        m_selfEventQ.pop();
    }

    bool stop = processRecv() && processSend( cycle );

    if ( stop ) {

        m_dbg.debug(CALL_INFO,2,1,"sendQ %zu\n",m_sendQ.size());
        stopClocking(cycle);
    }

    return stop;
}

bool RdmaNicSubComponent::processSend( Cycle_t cycle ) {
    return processSendQ(cycle);
}

void RdmaNicSubComponent::handleSelfEvent( Event* e ) {
    SelfEvent* event = static_cast<SelfEvent*>(e);

    if ( ! m_clocking ) { startClocking(); }

    m_selfEventQ.push( event );
}

void RdmaNicSubComponent::handleSelfEvent( SelfEvent* event ) {

    m_dbg.debug(CALL_INFO,2,1,"type %d\n",event->type);
    switch ( event->type ) {
      case SelfEvent::TxLatency:
        processSendPktStart( event->pkt, event->sendEntry, event->length, event->lastPkt  );
        break;

      case SelfEvent::TxDmaDone:
        processSendPktFini( event->pkt, event->sendEntry, event->lastPkt  );
        break;

      case SelfEvent::RxLatency:
        processRecvPktStart( event );
        break;

      case SelfEvent::RxDmaDone:
		processRecvPktFini( event );
        break;

      default:
        assert(0);
    }
    delete event;
}

void RdmaNicSubComponent::createRQ( int coreNum, Event* event ) {
	Core& core = m_coreTbl[coreNum];
    int retval = -1;
	CreateRqCmd* cmd = static_cast<CreateRqCmd*>(event);
    m_dbg.debug(CALL_INFO,1,2,"rqId=%d\n",(int)cmd->rqId);

	if ( core.m_rqs.find( cmd->rqId) == core.m_rqs.end() ) {
		core.m_rqs[ cmd->rqId ];
		retval = 0;
	}

    sendResp( coreNum, new RetvalResp(retval) );

    delete event;
}	

void RdmaNicSubComponent::postRecv( int coreNum, Event* event ) {
	Core& core = m_coreTbl[coreNum];
    int retval = -1;
	PostRecvCmd* cmd = static_cast<PostRecvCmd*>(event);
    m_dbg.debug(CALL_INFO,1,2,"rqId=%d addr=0x%" PRIx64 " backing=%p length=%zu\n",
			(int) cmd->rqId, cmd->addr.getSimVAddr(), cmd->addr.getBacking(), cmd->length );

	if ( core.m_rqs.find(cmd->rqId) != core.m_rqs.end() ) {
		core.m_rqs[ cmd->rqId ].push_back( new RecvBuf( cmd ) );
		retval = 0;
	}
	m_dbg.debug(CALL_INFO,1,2,"rqId=%d m_rqs.size=%zu\n", cmd->rqId, core.m_rqs[cmd->rqId].size() );

	sendCredit( coreNum );
}

void RdmaNicSubComponent::send( int coreNum, Event* event ) {
    SendCmd* cmd = static_cast<SendCmd*>(event);

    m_dbg.debug(CALL_INFO,1,SEND_DEBUG_MASK,"nid=%d pid=%d size=%zu rqId=%" PRIu16 " srcAddr=0x%" PRIx64 " backing=%p\n",
            cmd->proc.node, cmd->proc.pid, cmd->length,cmd->rqId, cmd->src.getSimVAddr(), cmd->src.getBacking());

    m_sendQ.push_back( new MsgSendEntry( m_streamIdCnt++, coreNum, cmd ) );

    sendResp( coreNum, new RetvalResp(0) );
}

void RdmaNicSubComponent::checkRQ( int coreNum, Event* event ) {

	Core& core = m_coreTbl[coreNum];
    CheckRqCmd* cmd = static_cast<CheckRqCmd*>(event);

    m_dbg.debug(CALL_INFO,1,2,"rqId=%d blocking=%d\n",(int)cmd->rqId,cmd->blocking);

	if ( core.validRqId( cmd->rqId ) ) {
		
		assert( core.m_checkRqCmd == NULL );

		bool retval = cmd->status->length != -1;
		if ( ! cmd->blocking || retval ) {
    		m_dbg.debug(CALL_INFO,1,2,"checkRqStatus %s\n",retval? "have message" : "no message");
			if ( retval ) {
				sendResp( coreNum, new CheckRqResp( 1 ) );
			} else {
				sendResp( coreNum, new CheckRqResp( 0 ) );
			}
			delete cmd;
		} else {
    		m_dbg.debug(CALL_INFO,1,2,"set checkRqCmd\n");
			core.m_checkRqCmd = cmd;
		}

	} else {
		sendResp( coreNum, new CheckRqResp( -1 ) );
		delete cmd;
	}
}

void RdmaNicSubComponent::registerMem( int coreNum, Event* event ) {

	Core& core = m_coreTbl[coreNum];
    RegisterMemCmd* cmd = static_cast<RegisterMemCmd*>(event);
    m_dbg.debug(CALL_INFO,1,2,"addr=0x%" PRIx64 " length=%zu\n",cmd->addr.getSimVAddr(), cmd->length );

	Hermes::RDMA::MemRegionId id; 
	int retval = core.registerMem( cmd->addr, cmd->length, id ); 
    sendResp( coreNum, new RegisterMemResp( id, retval ) );

	delete cmd;
}

void RdmaNicSubComponent::read( int coreNum, Event* event ) {
    ReadCmd* cmd = static_cast<ReadCmd*>(event);
    m_dbg.debug(CALL_INFO,1,2,"node=%d pid=%d destAddr=0x%" PRIx64 " srcAddr=0x%" PRIx64 " length=%zu\n", 
			cmd->proc.node, cmd->proc.pid, cmd->destAddr.getSimVAddr(), cmd->srcAddr, cmd->length );
	
    m_sendQ.push_back( new RdmaReadEntry( coreNum, cmd ) );
}

void RdmaNicSubComponent::write( int coreNum, Event* event ) {
    WriteCmd* cmd = static_cast<WriteCmd*>(event);
    m_dbg.debug(CALL_INFO,1,2,"node=%d pid=%d destAddr=0x%" PRIx64 " srcAddr=0x%" PRIx64 " length=%zu\n", 
			cmd->proc.node, cmd->proc.pid, cmd->destAddr, cmd->srcAddr.getSimVAddr(), cmd->length );

	RdmaWriteEntry* entry= new RdmaWriteEntry( m_streamIdCnt++, coreNum, cmd->proc.node, cmd->proc.pid,
								cmd->srcAddr, cmd->destAddr, cmd->length );

	Callback* callback = new Callback;
	*callback = [=]() {
			m_dbg.debug(CALL_INFO,1,2,"write is done\n");
   			sendResp( coreNum, new RetvalResp(0) );
		}; 
	entry->setCallback( callback );

	m_sendQ.push_back( entry );

	delete cmd;
}

bool RdmaNicSubComponent::processSendQ( Cycle_t cycle ) {

	m_dbg.debug(CALL_INFO,3,SEND_DEBUG_MASK,"%d %d %d\n",! m_sendQ.empty(), m_sendPktsPending, m_pendingNetReq != NULL);

	if ( m_sendQ.empty() || m_sendPktsPending  > 1 ) {
        return true;
    }
	SendEntry& entry = *m_sendQ.front();

	NetworkPkt* pkt = new NetworkPkt( getPktSize() );

	m_dbg.debug(CALL_INFO,2,SEND_DEBUG_MASK,"protocol %s\n",protoNames[entry.pktProtocol()]);
	pkt->setSrcNid( getNodeNum() );
	pkt->setSrcPid( entry.getSrcCore() );
	pkt->setDestPid( entry.destPid() );
	pkt->setProto( entry.pktProtocol() );
	pkt->setStreamId( entry.streamId() );
	pkt->setStreamOffset( entry.streamOffset() );

	if ( entry.isHead() ) {

		pkt->setHead();

		if ( entry.pktProtocol() == Message ) {
			MsgSendEntry* msgEntry = static_cast<MsgSendEntry*>(&entry);

			Hermes::RDMA::RqId rqId = msgEntry->rqId();  
			pkt->payloadPush( &rqId, sizeof( rqId ) );
			m_dbg.debug(CALL_INFO,1,SEND_DEBUG_MASK,"message stream %d head pkt rqId=%d\n", entry.streamId(), (int)rqId);

		} else if ( entry.pktProtocol() == RdmaRead ) {

			RdmaReadEntry* readEntry = static_cast<RdmaReadEntry*>(&entry);

			Hermes::RDMA::Addr destAddr = readEntry->getDestAddr();
			Hermes::RDMA::Addr srcAddr = readEntry->getSrcAddr();
			pkt->payloadPush( &destAddr, sizeof( destAddr ) );
			pkt->payloadPush( &srcAddr, sizeof( srcAddr) );
			readEntry->decRemainingBytes( readEntry->length() );

		}  else {
			RdmaWriteEntry* writeEntry = static_cast<RdmaWriteEntry*>(&entry);

			int readResp = 0;
			if ( writeEntry->isReadResp() ) {
				readResp = 1;
			}
			pkt->payloadPush( &readResp, sizeof(readResp) );

			Hermes::RDMA::Addr destAddr = writeEntry->destAddr();
			pkt->payloadPush( &destAddr, sizeof(destAddr) );
			m_dbg.debug(CALL_INFO,1,SEND_DEBUG_MASK,"rdma write, stream %d, head pkt, target addr=0x%" PRIx64 "\n",entry.streamId(), destAddr);
		}

		size_t length = entry.length();	
		pkt->payloadPush( &length, sizeof( length ) );
		m_dbg.debug(CALL_INFO,2,SEND_DEBUG_MASK,"head packet, stream length=%zu\n", length );
	}

	int bytesLeft = pkt->pushBytesLeft();
	m_dbg.debug(CALL_INFO,2,SEND_DEBUG_MASK,"pkt space avail %d\n",bytesLeft);
	int payloadSize = entry.remainingBytes() <  bytesLeft ? entry.remainingBytes() : bytesLeft;  

	pkt->payloadPush( entry.getBacking(), payloadSize );

	m_dbg.debug(CALL_INFO,2,SEND_DEBUG_MASK,"payloadSize %d\n",payloadSize);
	entry.decRemainingBytes( payloadSize );

	++m_sendPktsPending;

	m_selfLink->send( m_txLatency, new SelfEvent( pkt, &entry, payloadSize, 0 == entry.remainingBytes() ) );

	if ( 0 == entry.remainingBytes() ) {
		m_dbg.debug(CALL_INFO,1,SEND_DEBUG_MASK,"all pkts generated, pop sendQ\n");
		m_sendQ.pop_front();
	}
	return true;
}

void RdmaNicSubComponent::processSendPktStart( NetworkPkt* pkt, SendEntry* entry, size_t length, bool lastPkt ) {

    m_dbg.debug(CALL_INFO,2,1,"pkt send start latency complete\n");

    SelfEvent* event = new SelfEvent( pkt, entry, lastPkt );

    if ( m_sendDmaPending ) {
        m_dbg.debug(CALL_INFO,2,1,"pkt DMA blocked\n");
        assert( NULL == m_sendDmaBlockedEvent );
        event->length = length;
        m_sendDmaBlockedEvent = event;
        m_dbg.debug(CALL_INFO,2,1,"have blocked dma request %d %zu\n",m_sendDmaBlockedEvent->type,m_sendDmaBlockedEvent->length);
    } else {
        m_dbg.debug(CALL_INFO,2,1,"start DMA xfer delay\n");
        m_sendDmaPending = true;
        m_selfLink->send( calcFromHostBW_Latency( length ), event );
    }
}

void RdmaNicSubComponent::processSendPktFini( NetworkPkt* pkt, SendEntry* entry, bool lastPkt ) {

    m_dbg.debug(CALL_INFO,2,1,"pkt send DMA latency complete\n");

    --m_sendPktsPending;

    SimpleNetwork::Request* req = makeNetReq( pkt, entry->getDestNode() );

    if ( lastPkt ) {
        m_dbg.debug(CALL_INFO,2,1,"send entry DMA's done\n");
        int srcCore = entry->getSrcCore();
        delete entry;
    }

    if ( sendNetReq( req ) ) {
        m_dbg.debug(CALL_INFO,2,1,"send packet\n");
        netReqSent();
    } else {
        m_dbg.debug(CALL_INFO,2,1,"blocking on network TX pendingNetReq\n");
        m_pendingNetReq = req;
    }
}

void RdmaNicSubComponent::netReqSent(  )
{
    m_dbg.debug(CALL_INFO,2,1,"\n");
    if ( m_sendDmaBlockedEvent ) {
        m_dbg.debug(CALL_INFO,2,1,"have blocked dma request %d %zu\n",m_sendDmaBlockedEvent->type,m_sendDmaBlockedEvent->length);
        m_selfLink->send( calcFromHostBW_Latency(m_sendDmaBlockedEvent->length), m_sendDmaBlockedEvent );
        m_sendDmaBlockedEvent = NULL;
    } else {
        m_sendDmaPending = false;
    }
}

bool RdmaNicSubComponent::sendNetReq( Interfaces::SimpleNetwork::Request* req )
{
    if ( getNetworkLink().spaceToSend( m_vc, req->size_in_bits ) ) {
    	m_dbg.debug(CALL_INFO,2,1,"success\n");
        getNetworkLink().send( req, m_vc );
        return true;
    }
   	m_dbg.debug(CALL_INFO,2,1,"blocked on network\n");
    return false;
}

Interfaces::SimpleNetwork::Request* RdmaNicSubComponent::makeNetReq( NetworkPkt* pkt, int destNode )
{
    m_dbg.debug(CALL_INFO,3,1,"send packet\n");
    Interfaces::SimpleNetwork::Request* req = new SimpleNetwork::Request();
    req->dest = destNode;
    req->src = getNodeNum();
    req->size_in_bits = pkt->payloadSize()*8;
    req->vn = m_vc;
    req->givePayload( pkt );
    return req;
}

bool RdmaNicSubComponent::processRecv() {
    if ( m_recvPktsPending > 1 ) {
        return true;
    }

    Interfaces::SimpleNetwork::Request* req = getNetworkLink().recv( m_vc );
    if ( req ) {
        NetworkPkt* pkt = static_cast<NetworkPkt*>(req->takePayload());
        m_dbg.debug(CALL_INFO,3,1,"got network packet\n");
        m_selfLink->send( m_rxLatency, new SelfEvent( pkt ) );
        ++m_recvPktsPending;
        delete req;
    }
    return true;
}


void RdmaNicSubComponent::processRecvPktStart( SelfEvent* e )
{
	NetworkPkt* pkt = e->pkt;
	SelfEvent* event = NULL;
	m_dbg.debug(CALL_INFO,2,RECV_DEBUG_MASK,"pkt is protocol %s\n",protoNames[pkt->getProto()]);

	switch ( pkt->getProto() ) {
	  case Message:
		event = processMsgPktStart( pkt );
		break;
	  case RdmaRead:
		processRdmaReadPkt( pkt );
		break;
	  case RdmaWrite:
		event = processRdmaWritePktStart( pkt );
		break;
	}

	if ( event ) {
    	if ( m_recvDmaPending ) {
        	m_dbg.debug(CALL_INFO,2,1,"pkt DMA blocking\n");
        	assert( NULL == m_recvDmaBlockedEvent );
        	m_recvDmaBlockedEvent = event;
    	} else {
        	m_selfLink->send( calcToHostBW_Latency( event->length ), event );
        	m_recvDmaPending = true;
    	}
	}
}

void RdmaNicSubComponent::processRecvPktFini( SelfEvent* event )
{
	NetworkPkt* pkt = event->pkt;
	m_dbg.debug(CALL_INFO,2,RECV_DEBUG_MASK,"pkt is protocol %s\n",protoNames[pkt->getProto()]);

	--m_recvPktsPending;

	switch ( pkt->getProto() ) {
	  case Message:
        processMsgPktFini( event->buffer, event->pkt, event->length );
		break;
	  case RdmaWrite:
		processRdmaWritePktFini( event->pkt );
		break;
	  case RdmaRead:
		assert(0);
		break;
	}

    if ( m_recvDmaBlockedEvent ) {
        m_selfLink->send( calcToHostBW_Latency(m_recvDmaBlockedEvent->length), m_recvDmaBlockedEvent );
        m_recvDmaBlockedEvent = NULL;
    } else {
        m_recvDmaPending = false;
    }
}

RdmaNicSubComponent::SelfEvent* RdmaNicSubComponent::processMsgPktStart( NetworkPkt* pkt )
{
	int destPid = pkt->getDestPid();
	int srcPid = pkt->getSrcPid();
	int srcNid = pkt->getSrcNid();
	size_t xferLen = pkt->popBytesLeft();

	assert( destPid < m_coreTbl.size() );
	Core& core = m_coreTbl[destPid];

	m_dbg.debug(CALL_INFO,1,RECV_DEBUG_MASK,"srcNid=%d srcPid %d destPid %d steramId=%d\n",
			pkt->getSrcNid(), pkt->getSrcPid(), pkt->getDestPid(), pkt->getStreamId() );

	RecvBuf* buffer = NULL;
	if ( pkt->isHead() ) {
		Hermes::RDMA::RqId rqId;
		size_t msgLength;
		pkt->payloadPop( &rqId, sizeof( rqId ) );
		pkt->payloadPop( &msgLength, sizeof( msgLength ) );
		m_dbg.debug(CALL_INFO,1,RECV_DEBUG_MASK,"head rdId=%d msgLength=%zu\n", (int) rqId, msgLength );

 		xferLen = pkt->popBytesLeft();
		assert ( ! core.activeStream( srcNid, srcPid ) ); 

		buffer = core.findRecvBuf( rqId, msgLength );
		m_dbg.debug(CALL_INFO,1,2,"rqId=%d m_rqs.size=%zu\n", rqId, core.m_rqs[rqId].size() );

		if ( ! buffer ) {
			m_dbg.output("RDMA NIC %d rdId %d no buffer\n", getNodeNum(), (int) rqId);
			assert(0);
		} else {
			buffer->setRecvLength( msgLength );
			buffer->setRqId( rqId );
			buffer->setProc( srcNid, srcPid );

			if ( xferLen != msgLength ) {
				core.setActiveBuf( srcNid, srcPid, buffer );
			}
		}
	} else {
		buffer = core.findActiveBuf( srcNid, srcPid  );
		if ( buffer->isLastPkt( xferLen ) ) {
			m_dbg.debug(CALL_INFO,1,RECV_DEBUG_MASK,"buffer is done clear active stream\n");
			core.clearActiveStream( srcNid, srcPid );
		} 
	}

    return new SelfEvent( pkt, buffer, xferLen );
}

void RdmaNicSubComponent::processMsgPktFini( RecvBuf* buffer, NetworkPkt* pkt, size_t length )
{
	int destPid = pkt->getDestPid();
	int srcPid = pkt->getSrcPid();
	int srcNid = pkt->getSrcNid();
	Core& core = m_coreTbl[destPid];
    m_dbg.debug(CALL_INFO,2,1,"DMA finished destPid=%d length=%zu\n",destPid, length );

	if ( buffer ) {

		if ( buffer->recv( pkt->payload(), pkt->popBytesLeft() ) ) {
			m_dbg.debug(CALL_INFO,1,RECV_DEBUG_MASK,"buffer is done clear active stream\n");
		}

		m_dbg.debug( CALL_INFO_LAMBDA, "processMsg", 2, RECV_DEBUG_MASK, "DMA done\n");

		if ( buffer->isComplete() ) {

			m_dbg.debug( CALL_INFO_LAMBDA, "processMsg", 1, RECV_DEBUG_MASK, "buffer is complete\n");
			if ( core.m_checkRqCmd && core.m_checkRqCmd->status == buffer->status() ) {

				m_dbg.debug( CALL_INFO_LAMBDA, "processMsg", 1, RECV_DEBUG_MASK, "wake up checkRQ\n");
				sendResp( destPid, new CheckRqResp( 1 ) );
				delete core.m_checkRqCmd;
				core.m_checkRqCmd = NULL;
				delete buffer;
			}
		}	
	} else {
		m_dbg.output("NIC %d dump msg pkt from nid=%d pid=%d\n", getNodeNum(), srcNid, srcPid );
	}

	delete pkt;
}


RdmaNicSubComponent::SelfEvent* RdmaNicSubComponent::processRdmaWritePktStart( NetworkPkt* pkt )
{
	int destPid = pkt->getDestPid();
    Core& core = m_coreTbl[destPid];
	m_dbg.debug(CALL_INFO,2,RECV_DEBUG_MASK,"streamId=%d streamOffset=%d pktBytes=%zu\n",
			pkt->getStreamId(), pkt->getStreamOffset(), pkt->popBytesLeft());

	if ( pkt->isHead() ) {
		int isReadResp;
		Hermes::RDMA::Addr addr;
		size_t length;
		pkt->payloadPop( &isReadResp, sizeof( isReadResp ) );
		pkt->payloadPop( &addr, sizeof( addr) );
		pkt->payloadPop( &length, sizeof( length ) );
		m_dbg.debug(CALL_INFO,1,RECV_DEBUG_MASK,"stream %d head pkt readResp=%d addr=0x%" PRIx64 " length=%zu\n",
			   pkt->getStreamId(), isReadResp, addr, length );
		assert( core.m_rdmaRecvMap.find( pkt->getStreamId() ) == core.m_rdmaRecvMap.end() );

		Hermes::MemAddr destMemAddr;
		if ( core.findMemAddr( addr, length, destMemAddr ) ) {
			core.m_rdmaRecvMap[ pkt->getStreamId() ] = new RdmaRecvEntry( destMemAddr, length, (bool) isReadResp );
		} else {
			m_dbg.output("RDMA NIC %d dump pkt from nid=%d pid=%d\n", getNodeNum(), pkt->getSrcNid(), pkt->getSrcPid() );
			return NULL;
		}
		
	} else {
		m_dbg.debug(CALL_INFO,2,RECV_DEBUG_MASK,"body pkt\n" );
		assert( core.m_rdmaRecvMap.find( pkt->getStreamId() ) != core.m_rdmaRecvMap.end() );
	}

    return new SelfEvent( pkt, pkt->popBytesLeft() );
}

void RdmaNicSubComponent::processRdmaWritePktFini( NetworkPkt* pkt )
{
	int destPid = pkt->getDestPid();
   	Core& core = m_coreTbl[pkt->getDestPid()];
	if ( core.m_rdmaRecvMap[ pkt->getStreamId() ]->recv( pkt->payload(), pkt->getStreamOffset(), pkt->popBytesLeft() )  ) {
		m_dbg.debug(CALL_INFO_LAMBDA,"processRdmaWrite",1,RECV_DEBUG_MASK,"stream %d is done\n",pkt->getStreamId());
		if ( core.m_rdmaRecvMap[ pkt->getStreamId() ]->isReadResp() ) {
   			sendResp( pkt->getDestPid(), new RetvalResp( 0 ) );
   		}
		delete core.m_rdmaRecvMap[ pkt->getStreamId() ];
		core.m_rdmaRecvMap.erase( pkt->getStreamId() );
	}

	delete pkt;
}

void RdmaNicSubComponent::processRdmaReadPkt( NetworkPkt* pkt )
{
	int destPid = pkt->getDestPid();
    assert( destPid < m_coreTbl.size() );
    Core& core = m_coreTbl[destPid];

    --m_recvPktsPending;

	Hermes::RDMA::Addr srcAddr;
	Hermes::RDMA::Addr destAddr;
	size_t length;

	pkt->payloadPop( &destAddr, sizeof( destAddr) );
	pkt->payloadPop( &srcAddr, sizeof( srcAddr ) );
	pkt->payloadPop( &length, sizeof( length ) );

	m_dbg.debug(CALL_INFO,1,RECV_DEBUG_MASK,"srcNid=%d srcPid %d destPid %d Read srcAddr=0x%" PRIx64 " destAddr=0x%" PRIx64 " length=%zu \n",
			pkt->getSrcNid(), pkt->getSrcPid(), pkt->getDestPid(), srcAddr, destAddr, length);

	Hermes::MemAddr srcMemAddr;
	if ( core.findMemAddr( srcAddr, length, srcMemAddr ) ) {
		m_sendQ.push_back( new RdmaWriteEntry( m_streamIdCnt++, pkt->getDestPid(), pkt->getSrcNid(), pkt->getSrcPid(),
								srcMemAddr, destAddr, length, true ) );
	} else {
		m_dbg.output("NIC %d RdmaRead dump pkt from nid=%d pid=%d\n", getNodeNum(), pkt->getSrcNid(), pkt->getSrcPid() );
	}
	delete pkt;
}
