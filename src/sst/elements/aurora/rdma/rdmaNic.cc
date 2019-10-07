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
   	m_vc(0), m_firstActiveDMAslot(0), m_firstAvailDMAslot(0), m_activeDMAslots(0), m_streamIdCnt(0), m_availRecvDmaEngines(0), m_clockCnt(0)
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

	m_dmaSlots.resize( params.find<int>("numDmaSlots",32) );
	m_availRecvDmaEngines =  params.find<int>("numDmaSlots",32 );
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

	if ( ! m_clocking ) { startClocking(); } 

	NicCmd* cmd = static_cast<NicCmd*>(event); 
    m_dbg.debug(CALL_INFO,2,EVENT_DEBUG_MASK,"%s\n",m_cmdName[cmd->type]);
	assert(cmd->type < NicCmd::NumCmds);
	(this->*m_cmdFuncTbl[cmd->type])( core, event );
}

bool RdmaNicSubComponent::clockHandler( Cycle_t cycle ) {

	++m_clockCnt;

	bool stop = true;

	if ( m_availRecvDmaEngines ) {
		if ( processRecv() ) {
    		m_dbg.debug(CALL_INFO,1,2,"nothing to receive\n");
		} else {
			stop = false;	
		}
	}

	int ret1 = processRdmaSendQ(cycle);
	int ret2 = processDMAslots();

	if ( ret1 == 0 && ret2 == 0 ) {
    	m_dbg.debug(CALL_INFO,1,2,"no send work\n");
	} else if ( ret1 == 2 || ret2 == 2 ) {
    	m_dbg.debug(CALL_INFO,1,2,"blocked on send\n");
	} else {
		stop = false;	
    	m_dbg.debug(CALL_INFO,1,2,"check send work next clock\n");
	}

	if ( processSendQ(cycle) ) {
    	m_dbg.debug(CALL_INFO,1,2,"processSendQ blocked\n");
	} else {
		stop = false;	
	}	

	if (stop) {
		stopClocking( cycle );
	}
	return stop; 
}

void RdmaNicSubComponent::handleSelfEvent( Event* e ) {

	SelfEvent& event = *static_cast<SelfEvent*>(e);

  	m_dbg.debug(CALL_INFO,1,2,"\n");

	if ( ! m_clocking ) { startClocking(); } 

	Callback& callback = *event.callback;
	callback();
	delete event.callback;
	delete e;
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

    m_sendQ.push( new MsgSendEntry( m_streamIdCnt++, coreNum, cmd ) );

    sendResp( coreNum, new RetvalResp(0) );
}

void RdmaNicSubComponent::checkRQ( int coreNum, Event* event ) {

	Core& core = m_coreTbl[coreNum];
    CheckRqCmd* cmd = static_cast<CheckRqCmd*>(event);

    m_dbg.debug(CALL_INFO,1,2,"rqId=%d blocking=%d\n",(int)cmd->rqId,cmd->blocking);

	Hermes::RDMA::Status status;
	if ( core.validRqId( cmd->rqId ) ) {
		
		assert( core.m_checkRqCmd == NULL );
		bool retval = core.checkRqStatus( cmd->rqId, status );

		if ( ! cmd->blocking || retval ) {
    		m_dbg.debug(CALL_INFO,1,2,"checkRqStatus %s\n",retval? "have message" : "no message");
			if ( retval ) {
				core.popReadyBuf( cmd->rqId );
   				sendResp( coreNum, new CheckRqResp( status, 1) );
			} else {
   				sendResp( coreNum, new CheckRqResp( status, 0) );
			}
			delete cmd;
		} else {
    		m_dbg.debug(CALL_INFO,1,2,"set checkRqCmd\n");
			core.m_checkRqCmd = cmd;
		}

	} else {
   		sendResp( coreNum, new CheckRqResp( status, -1) );
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
	
	NetworkPkt* pkt = new NetworkPkt( getPktSize() );

	pkt->setSrcNid( getNodeNum() );
	pkt->setSrcPid( coreNum );
	pkt->setDestPid( cmd->proc.pid );

	pkt->setProto( RdmaRead );
	pkt->setHead();

	Hermes::RDMA::Addr destAddr = cmd->destAddr.getSimVAddr();

	pkt->payloadPush( &destAddr, sizeof( destAddr ) );
	pkt->payloadPush( &cmd->srcAddr, sizeof( cmd->srcAddr) );
	pkt->payloadPush( &cmd->length, sizeof( cmd->length ) );

	m_rdmaSendQ.push( std::pair<NetworkPkt*,int>( pkt, cmd->proc.node ) );

	delete cmd;
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

	m_sendQ.push( entry );

	delete cmd;
}

int RdmaNicSubComponent::processRdmaSendQ( Cycle_t cycle ) 
{
	if ( ! m_rdmaSendQ.empty() ) {
		NetworkPkt* pkt = m_rdmaSendQ.front().first; 
		int sizeInBits = pkt->payloadSize()*8;
		if ( getNetworkLink().spaceToSend( m_vc, sizeInBits ) ) { 
			SimpleNetwork::Request* req = new SimpleNetwork::Request();	
			req->dest = m_rdmaSendQ.front().second; 
			req->src = getNodeNum();
			req->size_in_bits = sizeInBits; 
			req->vn = m_vc;
			req->givePayload( pkt );
			m_rdmaSendQ.pop();
			m_dbg.debug(CALL_INFO,2,SEND_DEBUG_MASK,"send packet to node=%" PRIu64 "\n",req->dest);
			getNetworkLink().send( req, m_vc );
			return 1;
		} else {
			m_dbg.debug(CALL_INFO,2,1,"can't send packet because network is busy\n");
			return 2;
		}
	}
	return 0;
}

bool RdmaNicSubComponent::processSendQ( Cycle_t cycle ) {

	if ( ! m_sendQ.empty() && m_activeDMAslots < m_dmaSlots.size() ) {
		SendEntry& entry = *m_sendQ.front();

		int slot = m_firstAvailDMAslot;

		m_firstAvailDMAslot = (m_firstAvailDMAslot + 1) % m_dmaSlots.size();
		m_dbg.debug(CALL_INFO,2,SEND_DEBUG_MASK,"slot %d\n",slot);

		++m_activeDMAslots;

		NetworkPkt* pkt = new NetworkPkt( getPktSize() );

		m_dbg.debug(CALL_INFO,2,SEND_DEBUG_MASK,"protocol %s\n",protoNames[entry.pktProtocol()]);
		pkt->setSrcNid( getNodeNum() );
		pkt->setSrcPid( entry.srcPid() );
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

		m_dmaSlots[slot].init( pkt, entry.destNode() ); 

		SimTime_t delay = 0; 

		SendEntry* tmp = &entry;
		Callback* callback = new Callback;
		*callback = [=]() { 
			m_dbg.debug(CALL_INFO_LAMBDA,"processSendQ",2,SEND_DEBUG_MASK,"back from self delay for DMA slot %d\n",slot);
			m_dmaSlots[slot].setReady();	
			tmp->incCompletedDMAs();
			if ( tmp->done() ) {
				m_dbg.debug(CALL_INFO_LAMBDA,"processSendQ",1,SEND_DEBUG_MASK,"delete SendEntry\n");
				delete tmp;
			}
		};
		m_selfLink->send( delay, new SelfEvent( callback ) );

		if ( 0 == entry.remainingBytes() ) {
			m_dbg.debug(CALL_INFO,1,SEND_DEBUG_MASK,"send entry DMA requests done\n");
			m_sendQ.pop();
		}
	}	
	return m_activeDMAslots == m_dmaSlots.size() || m_sendQ.empty();
}

int RdmaNicSubComponent::processDMAslots() 
{
	if ( m_activeDMAslots && m_dmaSlots[m_firstActiveDMAslot].ready() ) {
		m_dbg.debug(CALL_INFO,2,SEND_DEBUG_MASK,"packet is ready\n");
		DMAslot& slot = m_dmaSlots[m_firstActiveDMAslot]; 
		NetworkPkt* pkt = slot.pkt();

		int sizeInBits = pkt->payloadSize()*8;
		if ( getNetworkLink().spaceToSend( m_vc, sizeInBits ) ) { 
			SimpleNetwork::Request* req = new SimpleNetwork::Request();	
			req->dest = slot.getDestNode();
			req->src = getNodeNum();
			req->size_in_bits = sizeInBits; 
			req->vn = m_vc;
			req->givePayload( pkt );
			m_dbg.debug(CALL_INFO,2,SEND_DEBUG_MASK,"send packet to node=%" PRIu64 "\n",req->dest);
			getNetworkLink().send( req, m_vc );
			slot.setIdle();
			m_firstActiveDMAslot = (m_firstActiveDMAslot + 1) % m_dmaSlots.size();
			--m_activeDMAslots;
			return 1;
		} else {
			m_dbg.debug(CALL_INFO,2,SEND_DEBUG_MASK,"can't send packet because network is busy\n");
			return 2;
		}
	}	
	return 0;
}	

void RdmaNicSubComponent::processPkt( NetworkPkt* pkt ) 
{
	m_dbg.debug(CALL_INFO,2,RECV_DEBUG_MASK,"pkt is protocol %s\n",protoNames[pkt->getProto()]);

	switch ( pkt->getProto() ) {
	  case Message:
		processMsg( pkt );
		break;
	  case RdmaRead:
		processRdmaRead( pkt );
		break;
	  case RdmaWrite:
		processRdmaWrite( pkt );
		break;
	}
}


void RdmaNicSubComponent::processMsg( NetworkPkt* pkt )
{
	int destPid = pkt->getDestPid();
	int srcPid = pkt->getSrcPid();
	int srcNid = pkt->getSrcNid();
	size_t length = pkt->popBytesLeft();

	assert( destPid < m_coreTbl.size() );
	Core& core = m_coreTbl[destPid];

	m_dbg.debug(CALL_INFO,1,RECV_DEBUG_MASK,"srcNid=%d srcPid %d destPid %d steramId=%d\n",
			pkt->getSrcNid(), pkt->getSrcPid(), pkt->getDestPid(), pkt->getStreamId() );

	RecvBuf* buffer = NULL;
	if ( pkt->isHead() ) {
		Hermes::RDMA::RqId rqId;
		size_t length;
		pkt->payloadPop( &rqId, sizeof( rqId ) );
		pkt->payloadPop( &length, sizeof( length ) );
		m_dbg.debug(CALL_INFO,1,RECV_DEBUG_MASK,"head rdId=%d length=%zu\n", (int) rqId, length );

		assert ( ! core.activeStream( srcNid, srcPid ) ); 

		buffer = core.findRecvBuf( rqId, length );
		m_dbg.debug(CALL_INFO,1,2,"rqId=%d m_rqs.size=%zu\n", rqId, core.m_rqs[rqId].size() );

		if ( ! buffer ) {
			m_dbg.output("RDMA NIC %d rdId %d no buffer\n", getNodeNum(), (int) rqId);
			assert(0);
		} else {
			buffer->setRecvLength( length );
			buffer->setRqId( rqId );
			buffer->setProc( srcNid, srcPid );
			core.setActiveBuf( srcNid, srcPid, buffer );
		}
	} else {
		buffer = core.findActiveBuf( srcNid, srcPid  );
	}
	--m_availRecvDmaEngines;

	if ( buffer ) {

		if ( buffer->recv( pkt->payload(), pkt->popBytesLeft() ) ) {
			m_dbg.debug(CALL_INFO,1,RECV_DEBUG_MASK,"buffer is done clear active stream\n");
			core.clearActiveStream( srcNid,srcPid);
		}

		Callback* callback = new Callback;
		*callback = [=]() {
			++m_availRecvDmaEngines;
			Core& core = m_coreTbl[destPid];
			m_dbg.debug( CALL_INFO_LAMBDA, "processMsg", 2, RECV_DEBUG_MASK, "DMA done\n");

			if ( buffer->isComplete() ) {
				core.pushReadyBufs( buffer );
				m_dbg.debug( CALL_INFO_LAMBDA, "processMsg", 1, RECV_DEBUG_MASK, "buffer is complete\n");
				if ( core.m_checkRqCmd ) {
					Hermes::RDMA::Status status;
					if ( core.checkRqStatus( buffer->rqId(), status ) ) {
						m_dbg.debug( CALL_INFO_LAMBDA, "processMsg", 1, RECV_DEBUG_MASK, "wake up checkRQ\n");
   						sendResp( destPid, new CheckRqResp( status, 1) );
						core.popReadyBuf( buffer->rqId() );
						delete core.m_checkRqCmd;
						core.m_checkRqCmd = NULL;
						delete buffer;
					}
				}
			}	
			delete pkt;
		};
		m_selfLink->send( 0, new SelfEvent( callback ) );
	} else {
		m_dbg.output("NIC %d dump msg pkt from nid=%d pid=%d\n", getNodeNum(), srcNid, srcPid );
		delete pkt;
	}

}
void RdmaNicSubComponent::processRdmaRead( NetworkPkt* pkt )
{
	int destPid = pkt->getDestPid();
    assert( destPid < m_coreTbl.size() );
    Core& core = m_coreTbl[destPid];

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
		m_sendQ.push( new RdmaWriteEntry( m_streamIdCnt++, pkt->getDestPid(), pkt->getSrcNid(), pkt->getSrcPid(),
								srcMemAddr, destAddr, length, true ) );
	} else {
		m_dbg.output("NIC %d RdmaRead dump pkt from nid=%d pid=%d\n", getNodeNum(), pkt->getSrcNid(), pkt->getSrcPid() );
	}
	delete pkt;
}

void RdmaNicSubComponent::processRdmaWrite( NetworkPkt* pkt )
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
			return;
		}
		
	} else {
		m_dbg.debug(CALL_INFO,2,RECV_DEBUG_MASK,"body pkt\n" );
		assert( core.m_rdmaRecvMap.find( pkt->getStreamId() ) != core.m_rdmaRecvMap.end() );
	}
	--m_availRecvDmaEngines;

	Callback* callback = new Callback;
	*callback = [=]() {
		++m_availRecvDmaEngines;
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
	};

	m_selfLink->send( 0, new SelfEvent( callback ) );
}
