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
#include "rvmaNic.h"
#include "rvmaNicResp.h"

using namespace SST::Aurora::RVMA;

const char* RvmaNicSubComponent::m_cmdName[] = {
    FOREACH_CMD(GENERATE_CMD_STRING)
};

RvmaNicSubComponent::RvmaNicSubComponent( ComponentId_t id, Params& params ) : NicSubComponent(id, params ),
	m_vc(0),
	m_recvStartBusy(false), m_recvDmaPending(false), m_recvDmaBlockedEvent(NULL),
	m_sendStartBusy(false), m_sendDmaPending(false), m_sendDmaBlockedEvent(NULL),
	m_pendingNetReq(NULL)
{
   if ( params.find<bool>("print_all_params",false) ) {
        printf("Aurora::RVMA::RvmaNicSubComponent()\n");
        params.print_all_params(std::cout);
    }

    m_dbg.init("@t:Aurora::RVMA::Nic::@p():@l ", params.find<uint32_t>("verboseLevel",0),
            params.find<uint32_t>("verboseMask",0), Output::STDOUT );

    m_dbg.debug(CALL_INFO,1,2,"\n");

	m_maxAvailBuffers = m_maxCompBuffers = params.find<int>("maxBuffers",32);
	m_maxNumWindows =params.find<int>("maxNumWindows",32);
	m_cmdFuncTbl.resize( NicCmd::NumCmds);
	m_cmdFuncTbl[NicCmd::InitWindow] = &RvmaNicSubComponent::initWindow;
	m_cmdFuncTbl[NicCmd::CloseWindow] = &RvmaNicSubComponent::closeWindow;
	m_cmdFuncTbl[NicCmd::PostBuffer] = &RvmaNicSubComponent::postBuffer;
	m_cmdFuncTbl[NicCmd::PostOneTimeBuffer] = &RvmaNicSubComponent::postOneTimeBuffer;
	m_cmdFuncTbl[NicCmd::WinGetEpoch] = &RvmaNicSubComponent::getEpoch;
	m_cmdFuncTbl[NicCmd::WinIncEpoch] = &RvmaNicSubComponent::incEpoch;
	m_cmdFuncTbl[NicCmd::WinGetBufPtrs] = &RvmaNicSubComponent::getBufPtrs;
	m_cmdFuncTbl[NicCmd::Put] = &RvmaNicSubComponent::put;
	m_cmdFuncTbl[NicCmd::Mwait] = &RvmaNicSubComponent::mwait;
}

void RvmaNicSubComponent::setup()
{
    char buffer[100];
    snprintf(buffer,100,"@t:%d:Aurora::RVMA::Nic::@p():@l ", getNodeNum());
    m_dbg.setPrefix(buffer);
}

void RvmaNicSubComponent::setNumCores( int num ) {
	NicSubComponent::setNumCores( num );
	m_coreTbl.resize( getNumCores() );
	for ( int i = 0; i < m_coreTbl.size(); i++ ) {
		m_coreTbl[i].m_windowTbl.resize( m_maxNumWindows );
		for ( int j = 0; j < m_maxNumWindows; j++ ) {
				m_coreTbl[i].m_windowTbl[j].setMaxes( m_maxAvailBuffers, m_maxCompBuffers );
		}
	}
}

void RvmaNicSubComponent::handleEvent( int core, Event* event ) {

	if ( ! m_clocking ) { startClocking(); }

	NicCmd* cmd = static_cast<NicCmd*>(event); 

    m_dbg.debug(CALL_INFO,3,2,"core=%d %s\n",core, m_cmdName[cmd->type]);
	(this->*m_cmdFuncTbl[cmd->type])( core, event );
}

void RvmaNicSubComponent::initWindow( int coreNum, Event* event )
{
	InitWindowCmd* cmd = static_cast<InitWindowCmd*>(event);
	Core& core = m_coreTbl[coreNum];
	int window = -1;
	for ( int i = 0; i < core.m_windowTbl.size(); i++ ) {
		if ( ! core.m_windowTbl[i].isActive() ) {
			core.m_windowTbl[i].setActive( cmd->addr, cmd->threshold, cmd->type ); 
			window = i;
			break;
		}
	}
    m_dbg.debug(CALL_INFO,2,2,"core=%d windowSlot=%d winAddr=%" PRIx64 " threshold=%zu type=%d\n",window, coreNum, cmd->addr,cmd->threshold,cmd->type);
	sendResp( coreNum, new InitWindowResp(window) );	
	delete event;
}

void RvmaNicSubComponent::closeWindow( int coreNum, Event* event )
{
	CloseWindowCmd* cmd = static_cast<CloseWindowCmd*>(event);
	Core& core = m_coreTbl[coreNum];
	int retval = -2;
	if (  cmd->window < core.m_windowTbl.size() ) {
		if ( ! core.m_windowTbl[cmd->window].isActive() ) {
			retval = -1;
		} else {
			core.m_windowTbl[cmd->window].close();	
			retval = 0;
		}
	}
	sendResp( coreNum, new RetvalResp(retval) );	
	delete event;
}

void RvmaNicSubComponent::incEpoch( int coreNum, Event* event )
{
	Core& core = m_coreTbl[coreNum];
	int retval = -2;
	WinIncEpochCmd* cmd = static_cast<WinIncEpochCmd*>(event);
    m_dbg.debug(CALL_INFO,2,1,"core=%d window=%d\n",coreNum, cmd->window);
	if (  cmd->window < core.m_windowTbl.size() ) {
		retval = core.m_windowTbl[cmd->window].incEpoch( );
	}
	sendResp( coreNum, new RetvalResp(retval) );	
}

void RvmaNicSubComponent::getEpoch( int coreNum, Event* event )
{
	Core& core = m_coreTbl[coreNum];
	int retval = -2;
	WinGetEpochCmd* cmd = static_cast<WinGetEpochCmd*>(event);
    m_dbg.debug(CALL_INFO,2,1,"core=%d window=%d\n",coreNum, cmd->window);
	if (  cmd->window < core.m_windowTbl.size() ) {
		int epoch;
		retval = core.m_windowTbl[cmd->window].getEpoch( &epoch );
	}
	sendResp( coreNum, new WinGetEpochResp(retval) );	
}

void RvmaNicSubComponent::getBufPtrs( int coreNum, Event* event )
{
	Core& core = m_coreTbl[coreNum];
	int retval = -2;
	WinGetBufPtrsCmd* cmd = static_cast<WinGetBufPtrsCmd*>(event);
    m_dbg.debug(CALL_INFO,2,1,"core=%d window=%d\n",coreNum, cmd->window);
	WinGetBufPtrsResp* resp = new WinGetBufPtrsResp();
	if (  cmd->window < core.m_windowTbl.size() ) {
		core.m_windowTbl[cmd->window].getBufPtrs( resp->completions, cmd->max );
	}
	sendResp( coreNum, resp );	
}

void RvmaNicSubComponent::postBuffer( int coreNum, Event* event )
{
	Core& core = m_coreTbl[coreNum];

	PostBufferCmd* cmd = static_cast<PostBufferCmd*>(event);

	int ret;
	if (  cmd->window < core.m_windowTbl.size() ) {
		ret = core.m_windowTbl[cmd->window].postBuffer( cmd->addr, cmd->size, cmd->completion );
	} else {
		m_dbg.fatal(CALL_INFO,-1,"node %d, failed window %d out of range\n", getNodeNum(), cmd->window );
	}

    m_dbg.debug(CALL_INFO,2,1,"core=%d windowSlot=%d winAddr=0x%" PRIx64 " buffAddr=0x%" PRIx64 " size=%zu completion=%p numAvailBuffers=%d\n",
				coreNum, cmd->window, core.m_windowTbl[cmd->window].getWinAddr(), cmd->addr.getSimVAddr(),
				cmd->size, cmd->completion, core.m_windowTbl[cmd->window].numAvailBuffers());

	if ( ret != 0 ) {
		m_dbg.fatal(CALL_INFO,-1,"node %d, failed ret=%d\n", getNodeNum(), ret);
	}

	sendCredit( coreNum );	
	
	delete event;
}

void RvmaNicSubComponent::postOneTimeBuffer( int coreNum, Event* event )
{
	Core& core = m_coreTbl[coreNum];
	int window = -1;
	PostOneTimeBufferCmd* cmd = static_cast<PostOneTimeBufferCmd*>(event);

	for ( int i = 0; i < core.m_windowTbl.size(); i++ ) {
		if ( ! core.m_windowTbl[i].isActive() ) {
			core.m_windowTbl[i].setActive( cmd->winAddr, cmd->threshold, cmd->type, true ); 
			window = i;
			break;
		}
	}

    m_dbg.debug(CALL_INFO,2,2,"core=%d windowSlot=%d winAddr=0x%" PRIx64 " threshold=%zu type=%d bufAddr=0x%" PRIx64 " completion=%p\n",
						coreNum, window, cmd->winAddr,cmd->threshold,cmd->type,cmd->bufAddr.getSimVAddr(), cmd->completion );

	if ( -1 == window ) {
		m_dbg.fatal(CALL_INFO,-1,"node %d, failed couldn't allocate window\n", getNodeNum());
	}

	int ret = core.m_windowTbl[window].postBuffer( cmd->bufAddr, cmd->size, cmd->completion );

	if ( ret != 0 ) {
		m_dbg.fatal(CALL_INFO,-1,"node %d, failed ret=%d\n", getNodeNum(), ret);
	}

	sendCredit( coreNum );	
	
	delete event;
}

void RvmaNicSubComponent::put( int coreNum, Event* event )
{
	PutCmd* cmd = static_cast<PutCmd*>(event);

    m_dbg.debug(CALL_INFO,2,1,"core=%d nid=%d pid=%d size=%zu virtAddr=0x%" PRIx64 " offset=%zu srcAddr=0x%" PRIx64 "\n",
			coreNum, cmd->proc.node, cmd->proc.pid, cmd->size, cmd->virtAddr, cmd->offset, cmd->srcAddr.getSimVAddr());

	m_sendQ.push( new SendEntry( coreNum, cmd ) );

	if ( cmd->completion ) { 
		sendResp( coreNum, new RetvalResp(0) );	
	}
}

void RvmaNicSubComponent::mwait( int coreNum, Event* event )
{
	MwaitCmd* cmd = static_cast<MwaitCmd*>(event);
    m_dbg.debug(CALL_INFO,2,1,"core=%d completion %p\n",coreNum, cmd->completion);
	Core& core = m_coreTbl[coreNum];
	assert( ! core.m_mwaitCmd );

	if ( cmd->completion->count ) {
		sendResp( coreNum, new RetvalResp(0) );	
		delete cmd;
	} else {
		core.m_mwaitCmd = cmd;
	}
}

bool RvmaNicSubComponent::clockHandler( Cycle_t cycle ) {

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

	bool stop = ( ! processRecv() && ! processSend( cycle ) );

	if ( stop ) {
		stopClocking(cycle);
	}

	return stop;
}

bool RvmaNicSubComponent::processSend( Cycle_t cycle ) {
	return ! processSendQ(cycle);
}

void RvmaNicSubComponent::handleSelfEvent( Event* e ) {
	SelfEvent* event = static_cast<SelfEvent*>(e);

	if ( ! m_clocking ) { startClocking(); }

	m_selfEventQ.push( event );
}

void RvmaNicSubComponent::handleSelfEvent( SelfEvent* event ) {

	m_dbg.debug(CALL_INFO,2,1,"type %d\n",event->type);
	switch ( event->type ) {
	  case SelfEvent::TxLatency:
		processSendPktStart( event->pkt, event->sendEntry, event->length, event->lastPkt  );
		break;

	  case SelfEvent::TxDmaDone:
		processSendPktFini( event->pkt, event->sendEntry, event->lastPkt  );
		break;

	  case SelfEvent::RxLatency:
		processRecvPktStart(event->pkt);
		break;

	  case SelfEvent::RxDmaDone:
		processRecvPktFini( event->pid, event->window, event->pkt, event->length, event->rvmaOffset );
		break;

	  default:
		assert(0);
	}
	delete event;
}

bool RvmaNicSubComponent::processSendQ( Cycle_t cycle ) {

	if ( m_sendQ.empty() || m_sendStartBusy || m_pendingNetReq ) {
		return false;
	}

	SendEntry& entry = *m_sendQ.front();

	NetworkPkt* pkt = new NetworkPkt( getPktSize() );

	pkt->setDestPid( entry.destPid() );

	Hermes::RVMA::VirtAddr rvmaAddr = entry.virtAddr();
	uint64_t rvmaOffset = entry.offset();
	pkt->payloadPush( &rvmaAddr, sizeof( rvmaAddr ) );
	pkt->payloadPush( &rvmaOffset, sizeof( rvmaOffset ) );

	int bytesLeft = pkt->pushBytesLeft();
	int payloadSize = entry.remainingBytes() <  bytesLeft ? entry.remainingBytes() : bytesLeft;

	pkt->payloadPush( entry.dataAddr(), payloadSize );

	m_dbg.debug(CALL_INFO,2,1,"core=%d destNid=%d destPid=%d payloadSize %d\n",entry.getSrcCore(), entry.getDestNode(), entry.destPid(),  payloadSize);
	entry.decRemainingBytes( payloadSize );

	pkt->setSrcNid( getNodeNum() );
	pkt->setSrcPid( entry.getSrcCore() );

	m_sendStartBusy = true;

	m_selfLink->send( m_txLatency, new SelfEvent( pkt, &entry, payloadSize, 0 == entry.remainingBytes() ) );

	if ( 0 == entry.remainingBytes() ) {
		m_dbg.debug(CALL_INFO,2,1,"done %p %p\n",pkt, &entry);
		m_sendQ.pop();
	}

	return false;
}

void RvmaNicSubComponent::processSendPktStart( NetworkPkt* pkt, SendEntry* entry, size_t length, bool lastPkt ) {

	m_dbg.debug(CALL_INFO,2,1,"pkt send start latency complete\n");

	SelfEvent* event = new SelfEvent( pkt, entry, lastPkt );

	if ( m_sendDmaPending ) {
		m_dbg.debug(CALL_INFO,2,1,"pkt DMA blocked\n");
		assert( NULL == m_sendDmaBlockedEvent );
		event->length = length;
		m_sendDmaBlockedEvent = event;
		m_dbg.debug(CALL_INFO,2,1,"have blocked dma request %d %zu\n",m_sendDmaBlockedEvent->type,m_sendDmaBlockedEvent->length);
	} else {
		m_sendStartBusy = false;
		m_sendDmaPending = true;
		m_selfLink->send( calcFromHostBW_Latency( length ), event );
	}
}

void RvmaNicSubComponent::processSendPktFini( NetworkPkt* pkt, SendEntry* entry, bool lastPkt ) {

	m_dbg.debug(CALL_INFO,2,1,"pkt send DMA latency complete\n");

	if ( m_sendStartBusy ) {
		m_sendStartBusy = false;
	}

	SimpleNetwork::Request* req = makeNetReq( pkt, entry->getDestNode() );

	if ( lastPkt ) {
		m_dbg.debug(CALL_INFO,2,1,"send entry done\n");
		int srcCore = entry->getSrcCore();
		if ( NULL == entry->getCmd().completion ) {
			sendResp( srcCore, new RetvalResp(0) );
		} else { 
			entry->getCmd().completion->count = entry->getCmd().size;
			Core& core = m_coreTbl[srcCore];
			if ( core.m_mwaitCmd && core.m_mwaitCmd->completion == entry->getCmd().completion ) {
				m_dbg.debug(CALL_INFO,2,1,"completed put matches\n");
				sendResp( srcCore, new RetvalResp(0) );	
				delete core.m_mwaitCmd;
				core.m_mwaitCmd = NULL;
			}
		}
		delete entry;
	}

	if ( sendNetReq( req ) ) {
		netReqSent();
	} else {
		m_dbg.debug(CALL_INFO,2,1,"blocking on network TX pendingNetReq\n");
		m_pendingNetReq = req;
	}
}

void RvmaNicSubComponent::netReqSent(  )
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

bool RvmaNicSubComponent::sendNetReq( SimpleNetwork::Request* req )
{
	if ( getNetworkLink().spaceToSend( m_vc, req->size_in_bits ) ) {
		getNetworkLink().send( req, m_vc );
		return true;
	}
	return false;
}

SimpleNetwork::Request* RvmaNicSubComponent::makeNetReq( NetworkPkt* pkt, int destNode )
{
	m_dbg.debug(CALL_INFO,3,1,"send packet\n");
	SimpleNetwork::Request* req = new SimpleNetwork::Request();
	req->dest = destNode;
	req->src = getNodeNum();
	req->size_in_bits = pkt->payloadSize()*8;
	req->vn = m_vc;
	req->givePayload( pkt );
	return req;
}

bool RvmaNicSubComponent::processRecv() {
	if ( m_recvStartBusy ) {
		return false;
	}

	Interfaces::SimpleNetwork::Request* req = getNetworkLink().recv( m_vc );
	if ( req ) {
		NetworkPkt* pkt = static_cast<NetworkPkt*>(req->takePayload());
		m_dbg.debug(CALL_INFO,3,1,"got network packet\n");
		m_selfLink->send( m_rxLatency, new SelfEvent( pkt ) );
		m_recvStartBusy = true;
		delete req;
	}
	return false;
}

void RvmaNicSubComponent::processRecvPktStart( NetworkPkt* pkt )
{
	Buffer* buffer = NULL;
	Hermes::RVMA::VirtAddr rvmaAddr;
	uint64_t rvmaOffset;
	pkt->payloadPop( &rvmaAddr,sizeof( rvmaAddr ) );
	pkt->payloadPop( &rvmaOffset,sizeof( rvmaOffset ) );
	int destPid = pkt->getDestPid();
	size_t length = pkt->popBytesLeft();

	m_dbg.debug(CALL_INFO,2,1,"srcNid=%d srcPid=%d core=%d rvmaAddr=0x%" PRIx64 " offset=%" PRIu64 " length=%zu\n",
			pkt->getSrcNid(), pkt->getSrcPid(),
			pkt->getDestPid(), rvmaAddr, rvmaOffset, length );

	assert( destPid < m_coreTbl.size() );

	Core& core = m_coreTbl[destPid];

	int window = core.findWindow( rvmaAddr );
	if ( window == -1 ) {
		m_dbg.fatal(CALL_INFO,-1,"node %d, couldn't find window for virtaddr 0x%" PRIx64 "\n", getNodeNum(), rvmaAddr);
	} else {
		m_dbg.debug(CALL_INFO,2,1,"found window %d for virtAddr 0x%" PRIx64 "\n",window,rvmaAddr);
	}

	SelfEvent* event = new SelfEvent( destPid, window, pkt, length, rvmaOffset );

	if ( m_recvDmaPending ) {
		m_dbg.debug(CALL_INFO,2,1,"pkt DMA blocking\n");
		assert( NULL == m_recvDmaBlockedEvent );
		m_recvDmaBlockedEvent = event;
	} else {
		m_selfLink->send( calcToHostBW_Latency( length ), event );
		m_recvStartBusy = false;
		m_recvDmaPending = true;
	}
}

void RvmaNicSubComponent::processRecvPktFini( int destPid, int window, NetworkPkt* pkt, size_t length, uint64_t rvmaOffset ) 
{
	Core& core = m_coreTbl[destPid];
	m_dbg.debug(CALL_INFO,2,1,"DMA finished core=%d window=%d length=%zu offset=%" PRIu64 "\n",destPid, window, length, rvmaOffset);

	Buffer* buffer = core.m_windowTbl[window].recv( rvmaOffset, pkt->payload(), length );
	if ( NULL == buffer ) {
		m_dbg.fatal(CALL_INFO,-1,"node %d, couldn't find buffer for window %d\n", getNodeNum(), window);
	}
	delete pkt;

	if ( m_recvStartBusy ) {
		m_recvStartBusy = false;
	}

	if ( buffer && buffer->filled ) { 
		m_dbg.debug(CALL_INFO,2,1,"buffer filled nBytes=%zu virtAddr=0x%" PRIx64 " count=%zu\n",
										buffer->nBytes, buffer->completion->addr.getSimVAddr(), buffer->completion->count);

		m_dbg.debug(CALL_INFO,2,1,"window=%d availBuffers=%d\n",window, core.m_windowTbl[window].numAvailBuffers() );

		if ( core.m_mwaitCmd ) {
			m_dbg.debug(CALL_INFO,2,1,"currently waiting %p %p\n", core.m_mwaitCmd->completion, buffer->completion );
			if ( core.m_mwaitCmd->completion == buffer->completion  ) {
				m_dbg.debug(CALL_INFO,2,1,"completed buffer matches\n");
				sendResp( destPid, new RetvalResp(0) );	
				delete core.m_mwaitCmd;
				core.m_mwaitCmd = NULL;
			}
		}

		if ( core.m_windowTbl[window].isOneTime() ) {
			m_dbg.debug(CALL_INFO,2,1,"close windowSlot=%d\n",window);
			core.m_windowTbl[window].close();	
		}
	}

	if ( m_recvDmaBlockedEvent ) {
		m_selfLink->send( calcToHostBW_Latency(length), m_recvDmaBlockedEvent );
		m_recvDmaBlockedEvent = NULL;
	} else {
		m_recvDmaPending = false;
	}
}
