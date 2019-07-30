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
#include "rvmaNicCmds.h"
#include "rvmaNicResp.h"
#include "rvmaNetworkPkt.h"

using namespace SST::Snotra::RVMA;
using namespace SST::Interfaces;

const char* RvmaNicSubComponent::m_cmdName[] = {
    FOREACH_CMD(GENERATE_CMD_STRING)
};

int Snotra::NicSubComponent::m_pktSize = 0;

RvmaNicSubComponent::RvmaNicSubComponent( Component* owner, Params& params ) : NicSubComponent(owner), m_vc(0),
	m_firstActiveDMAslot(0), m_firstAvailDMAslot(0), m_activeDMAslots(0)
{

   if ( params.find<bool>("print_all_params",false) ) {
        printf("Snotra::RVMA::RvmaNicSubComponent()\n");
        params.print_all_params(std::cout);
    }

    m_dbg.init("@t:Snotra::RVMA::Nic::@p():@l ", params.find<uint32_t>("verboseLevel",0),
            params.find<uint32_t>("verboseMask",0), Output::STDOUT );

    m_dbg.debug(CALL_INFO,1,2,"\n");

	m_maxBuffers = params.find<int>("maxBuffers",32);
	m_maxNumWindows =params.find<int>("maxNumWindows",32);
	m_cmdFuncTbl.resize( NicCmd::NumCmds);
	m_cmdFuncTbl[NicCmd::InitWindow] = &RvmaNicSubComponent::initWindow;
	m_cmdFuncTbl[NicCmd::CloseWindow] = &RvmaNicSubComponent::closeWindow;
	m_cmdFuncTbl[NicCmd::PostBuffer] = &RvmaNicSubComponent::postBuffer;
	m_cmdFuncTbl[NicCmd::WinGetEpoch] = &RvmaNicSubComponent::getEpoch;
	m_cmdFuncTbl[NicCmd::WinIncEpoch] = &RvmaNicSubComponent::incEpoch;
	m_cmdFuncTbl[NicCmd::WinGetBufPtrs] = &RvmaNicSubComponent::getBufPtrs;
	m_cmdFuncTbl[NicCmd::Put] = &RvmaNicSubComponent::put;
	m_cmdFuncTbl[NicCmd::Mwait] = &RvmaNicSubComponent::mwait;

	m_dmaSlots.resize( params.find<int>("numDmaSlots",32) );

    m_selfLink = owner->configureSelfLink("Nic::selfLink", "1 ns",
       new Event::Handler<RvmaNicSubComponent>(this,&RvmaNicSubComponent::handleSelfEvent));
    assert( m_selfLink );
}

void RvmaNicSubComponent::setup()
{
    char buffer[100];
    snprintf(buffer,100,"@t:%d:Snotra::RVMA::Nic::@p():@l ", getNodeNum());
    m_dbg.setPrefix(buffer);
}

void RvmaNicSubComponent::setNumCores( int num ) {
	NicSubComponent::setNumCores( num );
	m_coreTbl.resize( getNumCores() );
	for ( int i = 0; i < m_coreTbl.size(); i++ ) {
		m_coreTbl[i].m_windowTbl.resize( m_maxNumWindows );
	}
}

void RvmaNicSubComponent::handleEvent( int core, Event* event ) {

	NicCmd* cmd = static_cast<NicCmd*>(event); 
    m_dbg.debug(CALL_INFO,2,2,"%s\n",m_cmdName[cmd->type]);
	(this->*m_cmdFuncTbl[cmd->type])( core, event );
}

void RvmaNicSubComponent::initWindow( int coreNum, Event* event )
{
	InitWindowCmd* cmd = static_cast<InitWindowCmd*>(event);
    m_dbg.debug(CALL_INFO,1,2,"addr=%" PRIx64 " threshold=%zu type=%d\n",cmd->addr,cmd->threshold,cmd->type);
	Core& core = m_coreTbl[coreNum];
	int window = -1;
	for ( int i = 0; i < core.m_windowTbl.size(); i++ ) {
		if ( ! core.m_windowTbl[i].isActive() ) {
			core.m_windowTbl[i].setActive( cmd->addr, cmd->threshold, cmd->type ); 
			window = i;
			break;
		}
	}
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
    m_dbg.debug(CALL_INFO,2,1,"window=%d\n",cmd->window);
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
    m_dbg.debug(CALL_INFO,2,1,"window=%d\n",cmd->window);
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
    m_dbg.debug(CALL_INFO,2,1,"window=%d\n",cmd->window);
	WinGetBufPtrsResp* resp = new WinGetBufPtrsResp();
	if (  cmd->window < core.m_windowTbl.size() ) {
		core.m_windowTbl[cmd->window].getBufPtrs( resp->completions, cmd->max );
	}
	sendResp( coreNum, resp );	
}

void RvmaNicSubComponent::postBuffer( int coreNum, Event* event )
{
	Core& core = m_coreTbl[coreNum];
	int ret = -2;
	PostBufferCmd* cmd = static_cast<PostBufferCmd*>(event);
    m_dbg.debug(CALL_INFO,2,1,"window=%d size=%zu\n",cmd->window, cmd->size);
	if (  cmd->window < core.m_windowTbl.size() ) {
		if ( core.m_windowTbl[cmd->window].totalBuffers() < m_maxBuffers ) {
			ret = core.m_windowTbl[cmd->window].postBuffer( cmd->addr, cmd->size, cmd->completion );
		}
	}
	sendResp( coreNum, new RetvalResp(ret) );	
	
	delete event;
}

void RvmaNicSubComponent::put( int coreNum, Event* event )
{
	PutCmd* cmd = static_cast<PutCmd*>(event);

    m_dbg.debug(CALL_INFO,2,1,"nid=%d pid=%d size=%zu virtAddr=0x%" PRIx64 " offset=%zu srcAddr=0x%" PRIx64 "\n",
			cmd->proc.node,cmd->proc.pid,cmd->size,cmd->virtAddr,cmd->offset, cmd->srcAddr.getSimVAddr());
	m_sendQ.push( new SendEntry( coreNum, cmd ) );
	m_dbg.debug(CALL_INFO,2,1,"\n");

	sendResp( coreNum, new RetvalResp(0) );	
}

void RvmaNicSubComponent::mwait( int coreNum, Event* event )
{
	MwaitCmd* cmd = static_cast<MwaitCmd*>(event);
    m_dbg.debug(CALL_INFO,2,1,"completion %p\n",cmd->completion);
	Core& core = m_coreTbl[coreNum];
	assert( ! core.m_mwaitCmd );
	core.m_mwaitCmd =  cmd;
}


bool RvmaNicSubComponent::clockHandler( Cycle_t cycle ) {

	processDMAslots();
	processSendQ(cycle);
	processRecv();

	return false;
}

void RvmaNicSubComponent::handleSelfEvent( Event* e ) {
	SelfEvent* event = static_cast<SelfEvent*>(e);
	m_dbg.debug(CALL_INFO,2,1,"slot %d\n",event->slot);

	m_dmaSlots[event->slot].setReady();	
	event->entry->incCompletedDMAs();
	if ( event->entry->done() ) {
		delete event->entry;
	}
}


void RvmaNicSubComponent::processSendQ( Cycle_t cycle ) {

	if ( ! m_sendQ.empty() && m_activeDMAslots < m_dmaSlots.size() ) {
		SendEntry& entry = *m_sendQ.front();

		int slot = m_firstAvailDMAslot;

		m_firstAvailDMAslot = (m_firstAvailDMAslot + 1) % m_dmaSlots.size();
		m_dbg.debug(CALL_INFO,2,1,"slot %d\n",slot);

		++m_activeDMAslots;

		NetworkPkt* pkt = new NetworkPkt( getPktSize() );

		pkt->setDestPid( entry.destPid() );
		pkt->setOp( RVMA );

		Hermes::RVMA::VirtAddr rvmaAddr = entry.virtAddr();
		uint64_t rvmaOffset = entry.offset();  
		pkt->payloadPush( &rvmaAddr, sizeof( rvmaAddr ) );
		pkt->payloadPush( &rvmaOffset, sizeof( rvmaOffset ) );

		int bytesLeft = pkt->pushBytesLeft();
		m_dbg.debug(CALL_INFO,2,1,"bytes left %d\n",bytesLeft);
		int payloadSize = entry.remainingBytes() <  bytesLeft ? entry.remainingBytes() : bytesLeft;  

		pkt->payloadPush( NULL, payloadSize );

		m_dbg.debug(CALL_INFO,2,1,"payloadSize %d\n",payloadSize);
		entry.decRemainingBytes( payloadSize );

		pkt->setSrcNid( getNodeNum() );
		m_dmaSlots[slot].init( pkt, entry.getDestNode() ); 

		SimTime_t delay = 10; 

		m_selfLink->send( delay, new SelfEvent( slot, m_sendQ.front() ) );

		if ( 0 == entry.remainingBytes() ) {
			m_dbg.debug(CALL_INFO,2,1,"send entry DMA requests done\n");
			m_sendQ.pop();
		}
	}	
}

void RvmaNicSubComponent::processDMAslots() 
{
	if ( m_activeDMAslots && m_dmaSlots[m_firstActiveDMAslot].ready() ) {
		m_dbg.debug(CALL_INFO,2,1,"packet is ready\n");
		DMAslot& slot = m_dmaSlots[m_firstActiveDMAslot]; 
		NetworkPkt* pkt = slot.pkt();

		int sizeInBits = pkt->payloadSize()*8;

		if ( getNetworkLink().spaceToSend( m_vc, sizeInBits ) ) { 
			m_dbg.debug(CALL_INFO,2,1,"send packet\n");
			SimpleNetwork::Request* req = new SimpleNetwork::Request();	
			req->dest = slot.getDestNode();
			req->src = getNodeNum();
			req->size_in_bits = sizeInBits; 
			req->vn = m_vc;
			req->givePayload( pkt );
			getNetworkLink().send( req, m_vc );
			slot.setIdle();
			m_firstActiveDMAslot = (m_firstActiveDMAslot + 1) % m_dmaSlots.size();
			--m_activeDMAslots;
		} else {
			m_dbg.debug(CALL_INFO,2,1,"can't send packet because network is busy\n");
		}
	}	
}	

void RvmaNicSubComponent::processRecv() 
{
	SimpleNetwork::Request* req = getNetworkLink().recv( m_vc );
	if ( req ) {
		NetworkPkt* pkt = static_cast<NetworkPkt*>(req->takePayload());

		Protocol op = (Protocol) pkt->getOp();
		switch ( op ) {
		  case RVMA:
			processRVMA( pkt );
			break;
		  default:
			assert(0);
		}
		delete req;
	}
}

void RvmaNicSubComponent::processRVMA( NetworkPkt* pkt ) 
{
	Buffer* buffer = NULL;
	Hermes::RVMA::VirtAddr rvmaAddr;
	uint64_t rvmaOffset;
	pkt->payloadPop( &rvmaAddr,sizeof( rvmaAddr ) );
	pkt->payloadPop( &rvmaOffset,sizeof( rvmaOffset ) );
	int destPid = pkt->getDestPid();
	size_t length = pkt->popBytesLeft();

	m_dbg.debug(CALL_INFO,2,1,"destPid=%d rvmaAddr=0x%" PRIx64 " offset=%" PRIu64 " length=%zu\n",
			pkt->getDestPid(), rvmaAddr, rvmaOffset, length );

	assert( destPid < m_coreTbl.size() );

	Core& core = m_coreTbl[destPid];

	int window = core.findWindow( rvmaAddr );

	if ( window == -1 ) {
		printf("couldn't find window for virtaddr 0x%" PRIx64 "\n", rvmaAddr);
	} else {
		m_dbg.debug(CALL_INFO,2,1,"found window %d for virtAddr 0x%" PRIx64 "\n",window,rvmaAddr);
		buffer = core.m_windowTbl[window].recv( rvmaOffset, pkt->payload(), length );
	}

	if ( buffer ) { 
		m_dbg.debug(CALL_INFO,2,1,"epoch complete\n");
		if ( core.m_mwaitCmd ) {
			m_dbg.debug(CALL_INFO,2,1,"currently waiting\n");
			if ( core.m_mwaitCmd->completion == buffer->completion  ) {
				m_dbg.debug(CALL_INFO,2,1,"completed buffer matches\n");
				sendResp( m_activeRecvCore, new RetvalResp(0) );	
				delete core.m_mwaitCmd;
				core.m_mwaitCmd = NULL;
			}
		}
	}

	delete pkt;
}
