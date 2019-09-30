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
using namespace SST::Interfaces;

const char* RvmaNicSubComponent::m_cmdName[] = {
    FOREACH_CMD(GENERATE_CMD_STRING)
};

RvmaNicSubComponent::RvmaNicSubComponent( ComponentId_t id, Params& params ) : NicSubComponent(id, params ), m_vc(0),
	m_firstActiveDMAslot(0), m_firstAvailDMAslot(0), m_activeDMAslots(0)
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

	m_dmaSlots.resize( params.find<int>("numDmaSlots",32) );
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

    m_dbg.debug(CALL_INFO,3,2,"%s\n",m_cmdName[cmd->type]);
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
    m_dbg.debug(CALL_INFO,2,2,"windowSlot=%d winAddr=%" PRIx64 " threshold=%zu type=%d\n",window, cmd->addr,cmd->threshold,cmd->type);
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

	PostBufferCmd* cmd = static_cast<PostBufferCmd*>(event);

	int ret;
	if (  cmd->window < core.m_windowTbl.size() ) {
		ret = core.m_windowTbl[cmd->window].postBuffer( cmd->addr, cmd->size, cmd->completion );
	} else {
		m_dbg.fatal(CALL_INFO,-1,"node %d, failed window %d out of range\n", getNodeNum(), cmd->window );
	}

    m_dbg.debug(CALL_INFO,2,1,"windowSlot=%d winAddr=0x%" PRIx64 " buffAddr=0x%" PRIx64 " size=%zu completion=%p numAvailBuffers=%d\n",
				cmd->window, core.m_windowTbl[cmd->window].getWinAddr(), cmd->addr.getSimVAddr(),
				cmd->size, cmd->completion, core.m_windowTbl[cmd->window].numAvailBuffers());

	if ( ret != 0 ) {
		m_dbg.fatal(CALL_INFO,-1,"node %d, failed ret=%d\n", getNodeNum(), ret);
	}

	sendResp( coreNum, new RetvalResp(ret) );	
	
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

    m_dbg.debug(CALL_INFO,2,2,"windowSlot=%d winAddr=0x%" PRIx64 " threshold=%zu type=%d bufAddr=0x%" PRIx64 " completion=%p\n",
						window, cmd->winAddr,cmd->threshold,cmd->type,cmd->bufAddr.getSimVAddr(), cmd->completion );

	if ( -1 == window ) {
		m_dbg.fatal(CALL_INFO,-1,"node %d, failed couldn't allocate window\n", getNodeNum());
	}

	int ret = core.m_windowTbl[window].postBuffer( cmd->bufAddr, cmd->size, cmd->completion );

	if ( ret != 0 ) {
		m_dbg.fatal(CALL_INFO,-1,"node %d, failed ret=%d\n", getNodeNum(), ret);
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

	if ( cmd->completion ) { 
		sendResp( coreNum, new RetvalResp(0) );	
	}
}

void RvmaNicSubComponent::mwait( int coreNum, Event* event )
{
	MwaitCmd* cmd = static_cast<MwaitCmd*>(event);
    m_dbg.debug(CALL_INFO,2,1,"completion %p\n",cmd->completion);
	Core& core = m_coreTbl[coreNum];
	assert( ! core.m_mwaitCmd );

	if ( cmd->completion->count ) {
//	if ( core.checkCompleted( cmd ) ) {
		sendResp( coreNum, new RetvalResp(0) );	
		delete cmd;
	} else {
		core.m_mwaitCmd = cmd;
	}
}

bool RvmaNicSubComponent::clockHandler( Cycle_t cycle ) {

    m_dbg.debug(CALL_INFO,3,1,"\n");

	bool stop = ( ! processRecv() && ! processSend( cycle ) );

	if ( stop ) {
		stopClocking(cycle);
	}

	return stop;
}

bool RvmaNicSubComponent::processSend( Cycle_t cycle ) {
	return processDMAslots() || ! processSendQ(cycle);
}

void RvmaNicSubComponent::handleSelfEvent( Event* e ) {
	SelfEvent* event = static_cast<SelfEvent*>(e);
	m_dbg.debug(CALL_INFO,3,1,"slot %d\n",event->slot);

	if ( ! m_clocking ) { startClocking(); }

	m_dmaSlots[event->slot].setReady();	
	event->entry->incCompletedDMAs();
	if ( event->entry->done() ) {
		delete event->entry;
	}
}

bool RvmaNicSubComponent::processSendQ( Cycle_t cycle ) {

	if ( ! m_sendQ.empty() && m_activeDMAslots < m_dmaSlots.size() ) {
		SendEntry& entry = *m_sendQ.front();

		int slot = m_firstAvailDMAslot;

		m_firstAvailDMAslot = (m_firstAvailDMAslot + 1) % m_dmaSlots.size();
		m_dbg.debug(CALL_INFO,2,1,"slot %d\n",slot);

		++m_activeDMAslots;

		NetworkPkt* pkt = new NetworkPkt( getPktSize() );

		pkt->setDestPid( entry.destPid() );

		Hermes::RVMA::VirtAddr rvmaAddr = entry.virtAddr();
		uint64_t rvmaOffset = entry.offset();  
		pkt->payloadPush( &rvmaAddr, sizeof( rvmaAddr ) );
		pkt->payloadPush( &rvmaOffset, sizeof( rvmaOffset ) );

		int bytesLeft = pkt->pushBytesLeft();
		m_dbg.debug(CALL_INFO,2,1,"bytes left %d\n",bytesLeft);
		int payloadSize = entry.remainingBytes() <  bytesLeft ? entry.remainingBytes() : bytesLeft;  

		pkt->payloadPush( entry.dataAddr(), payloadSize );

		m_dbg.debug(CALL_INFO,2,1,"destNid=%d destPid=%d payloadSize %d\n",entry.getDestNode(), entry.destPid(),  payloadSize);
		entry.decRemainingBytes( payloadSize );

		pkt->setSrcNid( getNodeNum() );
		pkt->setSrcPid( entry.getSrcCore() );
		m_dmaSlots[slot].init( pkt, entry.getDestNode() ); 

		SimTime_t delay = 0; 

		m_selfLink->send( delay, new SelfEvent( slot, m_sendQ.front() ) );

		if ( 0 == entry.remainingBytes() ) {
			m_dbg.debug(CALL_INFO,2,1,"send entry done\n");
			int srcCore = entry.getSrcCore();
			if ( NULL == entry.getCmd().completion ) {
				sendResp( srcCore, new RetvalResp(0) );	
			} else { 
				entry.getCmd().completion->count = entry.getCmd().size; 
				Core& core = m_coreTbl[srcCore];
				if ( core.m_mwaitCmd && core.m_mwaitCmd->completion == entry.getCmd().completion ) {
					m_dbg.debug(CALL_INFO,2,1,"completed put matches\n");
					sendResp( srcCore, new RetvalResp(0) );	
					delete core.m_mwaitCmd;
					core.m_mwaitCmd = NULL;
				}
			}
			m_sendQ.pop();
		}
	}	
	return m_activeDMAslots == m_dmaSlots.size() || m_sendQ.empty();	
}

int RvmaNicSubComponent::processDMAslots() 
{
	if ( m_activeDMAslots && m_dmaSlots[m_firstActiveDMAslot].ready() ) {
		m_dbg.debug(CALL_INFO,3,1,"packet is ready\n");
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
			return true;
		} else {
			m_dbg.debug(CALL_INFO,3,1,"can't send packet because network is busy\n");
			return false;
		}
	}	
	return false;
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

	m_dbg.debug(CALL_INFO,2,1,"srcNid=%d srcPid=%d destPid=%d rvmaAddr=0x%" PRIx64 " offset=%" PRIu64 " length=%zu\n",
			pkt->getSrcNid(), pkt->getSrcPid(),
			pkt->getDestPid(), rvmaAddr, rvmaOffset, length );

	assert( destPid < m_coreTbl.size() );

	Core& core = m_coreTbl[destPid];

	int window = core.findWindow( rvmaAddr );

	if ( window == -1 ) {
		m_dbg.fatal(CALL_INFO,-1,"node %d, couldn't find window for virtaddr 0x%" PRIx64 "\n", getNodeNum(), rvmaAddr);
	} else {
		m_dbg.debug(CALL_INFO,2,1,"found window %d for virtAddr 0x%" PRIx64 "\n",window,rvmaAddr);

		buffer = core.m_windowTbl[window].recv( rvmaOffset, pkt->payload(), length );

		if ( NULL == buffer ) {
			m_dbg.fatal(CALL_INFO,-1,"node %d, couldn't find buffer for window %d\n", getNodeNum(), window);
		} 
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
		} else {
#if 0
			if ( ! core.m_windowTbl[window].isOneTime() ) {
				core.saveCompleted( buffer );
			}
#endif
		}

		if ( core.m_windowTbl[window].isOneTime() ) {
			m_dbg.debug(CALL_INFO,2,1,"close windowSlot=%d\n",window);
			core.m_windowTbl[window].close();	
		}
	}

	delete pkt;
}
