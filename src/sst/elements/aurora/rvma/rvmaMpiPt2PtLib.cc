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


#include <exception>
#include <sst_config.h>
#include "rvmaMpiPt2PtLib.h"

using namespace SST::Aurora::RVMA;
using namespace Hermes;

#define CALL_INFO_LAMBDA     __LINE__, __FILE__

RvmaMpiPt2PtLib::RvmaMpiPt2PtLib( Component* owner, Params& params) : MpiPt2Pt(owner,params), m_pendingLongEntry(NULL), m_longPutWinAddr(1) 
{
	if ( params.find<bool>("print_all_params",false) ) {
		printf("Aurora::RVMA::RvmaMpiPt2PtLib()\n");
		params.print_all_params(std::cout);
	}

	m_dbg.init("@t:Aurora::RVMA::RvmaMpiPt2PtLib::@p():@l ", params.find<uint32_t>("verboseLevel",0),
			params.find<uint32_t>("verboseMask",0), Output::STDOUT );

	m_windowAddr = 0xF00D;

    m_dbg.debug(CALL_INFO,1,2,"\n");

	Params rvmaParams =  params.find_prefix_params("rvmaLib.");

	m_rvma = dynamic_cast< Hermes::RVMA::Interface*>( loadAnonymousSubComponent<Hermes::Interface>( "aurora.rvmaLib", "", 0, ComponentInfo::SHARE_NONE, rvmaParams ) );
	assert(m_rvma);
}

void RvmaMpiPt2PtLib::_init( int* numRanks, int* myRank, Hermes::Callback* callback ) {
	m_dbg.debug(CALL_INFO,1,2,"numRanks=%d myRank=%d\n",*numRanks,*myRank);
	Callback* cb = new Callback( std::bind( &RvmaMpiPt2PtLib::mallocSendBuffers, this, callback, std::placeholders::_1 ) );
	rvma().initWindow( m_windowAddr, 1, Hermes::RVMA::EpochType::Op, &m_windowId, cb ); 
}

void RvmaMpiPt2PtLib::_isend( const Hermes::Mpi::MemAddr& buf, int count, Hermes::Mpi::DataType dataType, int dest, int tag,
		Hermes::Mpi::Comm comm, Hermes::Mpi::Request* request, Hermes::Callback* callback ) 
{
	m_dbg.debug(CALL_INFO,1,2,"buf=0x%" PRIx64 " count=%d dataSize=%d dest=%d tag=%d comm=%d\n",
			buf.getSimVAddr(),count,Mpi::sizeofDataType( dataType ), dest, tag, comm );

	Hermes::Callback* x = new Hermes::Callback([=]( int retval ) {
		m_dbg.debug(CALL_INFO_LAMBDA,"isend",1,2,"returning\n");

		m_selfLink->send(0,new SelfEvent(callback,retval) );
	});	

	request->type = Hermes::Mpi::Request::Send;
	SendEntry* entry = new SendEntry( buf, count, dataType, dest, tag, comm, request );
	request->entry = entry;

	m_dbg.debug(CALL_INFO,1,2,"request=%p entry=%p\n",request,request->entry);

	m_postedSends.push( entry );

	makeProgress(x);
}

void RvmaMpiPt2PtLib::_irecv( const Hermes::Mpi::MemAddr& buf, int count, Hermes::Mpi::DataType dataType, int src, int tag,
		Hermes::Mpi::Comm comm, Hermes::Mpi::Request* request, Hermes::Callback* callback ) 
{
	m_dbg.debug(CALL_INFO,1,2,"buf=0x%" PRIx64 " count=%d dataSize=%d src=%d tag=%d comm=%d\n",
			buf.getSimVAddr(),count,Mpi::sizeofDataType( dataType ), src, tag, comm );

	Hermes::Callback* x = new Hermes::Callback([=]( int retval ) {
		m_dbg.debug(CALL_INFO_LAMBDA,"irecv",1,2,"returning\n");
		m_selfLink->send(0, new SelfEvent(callback,retval) );
	});	

	request->type = Hermes::Mpi::Request::Recv;
	RecvEntry* entry = new RecvEntry( buf, count, dataType, src, tag, comm, request );
	request->entry = entry;

	m_dbg.debug(CALL_INFO,1,2,"request=%p entry=%p\n",request,request->entry);

	size_t bytes = entry->count * Mpi::sizeofDataType( entry->dataType ); 
	if ( bytes > m_shortMsgLength ) {
		Callback* cb = new Callback( [=](int) {
			m_dbg.debug(CALL_INFO_LAMBDA,"irecv",1,2,"back from postOneTimeBuffer\n");
			recvCheckMatch( entry, x );
		});
		entry->extra.rvmaAddr = m_longPutWinAddr << 16; 
		++m_longPutWinAddr;
		if (0 == m_longPutWinAddr) {
			++m_longPutWinAddr;
		}
		m_longPutWinAddr &= 0xffff;
		m_dbg.debug(CALL_INFO,1,2,"postOneTimeBuffer rvmaAddr=0x%" PRIx64 " \n", entry->extra.rvmaAddr);
		rvma().postOneTimeBuffer( entry->extra.rvmaAddr, bytes, Hermes::RVMA::EpochType::Byte, entry->buf, bytes, &entry->extra.completion, cb );
	} else {
		recvCheckMatch( entry, x );
	}
}

void RvmaMpiPt2PtLib::recvCheckMatch( RecvEntry* entry, Hermes::Callback* callback )
{
	try {
		Hermes::MemAddr addr = checkUnexpected( entry );
		m_dbg.debug(CALL_INFO,1,2,"found unexpected\n");
		processMatch( addr, entry, callback );
	} 
	catch ( int error )
	{
		m_dbg.debug(CALL_INFO,1,2,"post recv\n");
		m_postedRecvs.push_back( entry );
		makeProgress( callback );
	}
}

void RvmaMpiPt2PtLib::processTest( TestBase* waitEntry, int ) 
{
	m_dbg.debug(CALL_INFO,1,2,"\n");

	if ( m_pendingLongEntry )  {
		m_dbg.debug(CALL_INFO,1,2,"waiting for long message\n");

		Callback* cb = new Callback( [=](int retval ){
			m_dbg.debug(CALL_INFO_LAMBDA,"processTest",1,2,"return from long message mwait\n");
			m_pendingLongEntry->doneFlag = true;
			m_pendingLongEntry = NULL;
			processTest( waitEntry, 0 );
		});
		
	 	m_dbg.debug(CALL_INFO,1,2,"calling mwait completion=%p\n", &m_pendingLongEntry->extra.completion );
		rvma().mwait( &m_pendingLongEntry->extra.completion, cb );

   	} else if ( waitEntry->isDone() ) {

		m_dbg.debug(CALL_INFO,1,2,"entry done, return\n");
		(*waitEntry->callback)(0);
		delete waitEntry;

	} else if ( waitEntry->blocking )  {
		m_dbg.debug(CALL_INFO,1,2,"blocking\n");

		if ( m_pendingLongPut.empty() ) {
			Hermes::RVMA::Completion* completion = m_completionQ.front();
			m_completionQ.pop();

			Callback* cb = new Callback( [=](int retval ){
				m_dbg.debug(CALL_INFO_LAMBDA,"processTest",1,2,"return from mwait\n");
				assert( retval == 0 );
		    	Callback* cb = new Callback( std::bind( &RvmaMpiPt2PtLib::processTest, this, waitEntry, std::placeholders::_1 ) );
        		processMsg( completion, cb );
			});

			m_dbg.debug(CALL_INFO,1,2,"call mwait completion=%p\n",completion);
			rvma().mwait( completion, cb );

		} else {

			poll( waitEntry );
		}

	} else {
		m_dbg.debug(CALL_INFO,1,2,"entry not done\n");
		(*waitEntry->callback)(0);
		delete waitEntry;
	}		
}

void RvmaMpiPt2PtLib::poll( TestBase* waitEntry )
{
	m_dbg.debug(CALL_INFO,1,2,"polling for completions\n");
	std::deque<SendEntry*>::iterator iter = m_pendingLongPut.begin();
	for ( ; iter != m_pendingLongPut.end(); ++iter ) {
		if ( (*iter)->extra.completion.count ) {
			m_dbg.debug(CALL_INFO,1,2,"send is complete\n");
			(*iter)->doneFlag = true;
			m_pendingLongPut.erase(iter);
			processTest( waitEntry, 0);
			return;
		} 
	} 

	if ( m_completionQ.front()->count ) {
		m_dbg.debug(CALL_INFO,1,2,"have a new message\n");

		Hermes::RVMA::Completion* completion = m_completionQ.front();
		m_completionQ.pop();
		
    	Callback* cb = new Callback( std::bind( &RvmaMpiPt2PtLib::processTest, this, waitEntry, std::placeholders::_1 ) );
		processMsg( completion, cb ); 
		return;
	}

	Callback* cb = new Callback( [=](int retval ){
		m_dbg.debug(CALL_INFO_LAMBDA,"processTest",1,2,"return from poll wait\n");
		poll(waitEntry);
	});

	m_selfLink->send( 100,new SelfEvent(cb,0) );
}

void RvmaMpiPt2PtLib::makeProgress( Hermes::Callback* callback )
{
	m_dbg.debug(CALL_INFO,1,2,"\n");
	Callback* cb = new Callback( std::bind( &RvmaMpiPt2PtLib::processSendQ, this, callback, std::placeholders::_1 ) );

	processRecvQ( cb, 0 ); 
}

void RvmaMpiPt2PtLib::processRecvQ( Hermes::Callback* callback, int retval  )
{
	m_dbg.debug(CALL_INFO,1,2,"\n");
	Hermes::RVMA::Completion* completion = m_completionQ.front();	

	if ( 0 != completion->addr.getSimVAddr() ) {
		Hermes::Callback* cb = new Hermes::Callback;
		*cb = [=](int) {
			m_dbg.debug(CALL_INFO_LAMBDA,"processRecvQ",1,2,"call processRecvQ()\n");
			processRecvQ( callback, 0 );
		};

		m_dbg.debug(CALL_INFO,1,2,"message available\n");
		m_completionQ.pop();
		processMsg( completion, cb );
	} else {
		m_dbg.debug(CALL_INFO,1,2,"no message available, return\n");
		(*callback)(0);
		delete callback;
	}	
}

void RvmaMpiPt2PtLib::processSendEntry( Hermes::Callback* callback, SendEntryBase* _entry ) {

	SendEntry* entry = dynamic_cast<SendEntry*>(_entry);
	size_t bytes = entry->count * Mpi::sizeofDataType( entry->dataType ); 

	MsgHdr* hdr = (MsgHdr*) m_sendBuff.getBacking();
	hdr->srcRank = os().getMyRank(entry->comm);
	hdr->tag = entry->tag;
	hdr->count = entry->count;
	hdr->dataType = entry->dataType;
	hdr->comm = entry->comm;
	hdr->type = MsgHdr::Match;
	m_dbg.debug(CALL_INFO,1,2,"tag=%d rank=%d comm=%d count=%d msgtype=%d\n",hdr->tag,hdr->srcRank,hdr->comm,hdr->count,hdr->type);

	Hermes::ProcAddr procAddr = getProcAddr(entry);

	hdr->procAddr.node =  os().getNodeNum(); 
	hdr->procAddr.pid =  procAddr.pid;

	m_dbg.debug(CALL_INFO,1,2,"node=%d pid=%d\n",procAddr.node,procAddr.pid);
	if ( bytes <= m_shortMsgLength ) {
		m_dbg.debug(CALL_INFO,1,2,"short message\n");
		if ( entry->buf.getBacking() ) {
			void* payload = entry->buf.getBacking(); 
			memcpy( m_sendBuff.getBacking(sizeof(MsgHdr)), payload, bytes ); 
		}	
		entry->doneFlag = true;

	} else {

		m_dbg.debug(CALL_INFO,1,2,"long message key=%p\n",entry);
		hdr->sendEntry = entry;
		bytes = 0;
	}
	sendMsg( procAddr, m_sendBuff, sizeof(MsgHdr) + bytes, callback );
}

void RvmaMpiPt2PtLib::sendMsg( Hermes::ProcAddr& procAddr, Hermes::MemAddr& addr, size_t length, Hermes::Callback* callback )
{
	m_dbg.debug(CALL_INFO,1,2,"destNid=%d destPid=%d addr=0x%" PRIx64 " length=%zu\n",
				procAddr.node, procAddr.pid, addr.getSimVAddr(), length  );
	rvma().put( addr, length, procAddr, m_windowAddr, 0, NULL, callback );
}

void RvmaMpiPt2PtLib::processMsg( Hermes::RVMA::Completion* completion, Hermes::Callback* callback ) {
	
	MemAddr msg = completion->addr;
	m_dbg.debug(CALL_INFO,1,2,"completion=%p vaddr=0x%" PRIx64 " backing=%p\n",completion, completion->addr.getSimVAddr(), completion->addr.getBacking() );

    MsgHdr* hdr = (MsgHdr*) msg.getBacking();
	m_dbg.debug(CALL_INFO,1,2,"completion=%p hdr=%p\n",completion,hdr);
	delete completion;

    m_dbg.debug(CALL_INFO,1,2,"got message from node=%d pid=%d \n",hdr->procAddr.node, hdr->procAddr.pid );

	if( hdr->type == MsgHdr::Go ) {
		processGoMsg( msg, callback );
		m_dbg.debug(CALL_INFO,1,2,"\n");
		return;
	}

	auto foo = [=]( RecvEntryBase* entry ) {

		if ( entry ) {
			processMatch( msg, entry, callback );
		} else {
			m_dbg.debug(CALL_INFO,1,2,"unexpected recv\n");

			m_unexpectedRecvs.push_back( msg );

			(*callback)(0);
			delete callback;	
		}	
	};

	findPostedRecv( hdr, foo );

	m_dbg.debug(CALL_INFO,1,2,"\n");
}

void RvmaMpiPt2PtLib::processGoMsg( Hermes::MemAddr msg, Hermes::Callback* callback ) {

    MsgHdr* hdr = (MsgHdr*) msg.getBacking();
	m_dbg.debug(CALL_INFO,1,2,"from nid=%d pid=%d rvmaAddr=%" PRIx64 "\n",hdr->procAddr.node, hdr->procAddr.pid, hdr->rvmaAddr);

   	SendEntry* entry =  hdr->sendEntry;
	entry->extra.rvmaAddr = hdr->rvmaAddr;

	size_t bytes = entry->count * Mpi::sizeofDataType( entry->dataType ); 
	Hermes::ProcAddr procAddr = getProcAddr(entry);

	Hermes::Callback* cb  = new Hermes::Callback([=]( int retval ) {
		repostRecvBuffer( msg, callback );
	});

	rvma().put( entry->buf, bytes, procAddr, entry->extra.rvmaAddr, 0, &entry->extra.completion, cb );

	m_pendingLongPut.push_back(entry);
}

void RvmaMpiPt2PtLib::processMatch( const Hermes::MemAddr& msg, RecvEntryBase* _entry, Hermes::Callback* callback ) {

	RecvEntry* entry = dynamic_cast< RecvEntry* >( _entry );
    MsgHdr* hdr = (MsgHdr*) msg.getBacking();
	m_dbg.debug(CALL_INFO,1,2,"node=%d pid=%d\n",hdr->procAddr.node,hdr->procAddr.pid);

	entry->status.rank = hdr->srcRank;
	entry->status.tag = hdr->tag; 
	size_t bytes = entry->count * Mpi::sizeofDataType( entry->dataType ); 
	if ( bytes <= m_shortMsgLength ) {
		m_dbg.debug(CALL_INFO,1,2,"found posted short recv bytes=%zu\n",bytes);
		void* buf = entry->buf.getBacking();
		if ( buf ) {  
			void* payload = msg.getBacking(sizeof(MsgHdr)); 
			memcpy( buf, payload, bytes ); 
		}
		entry->doneFlag = true;

		repostRecvBuffer( msg, callback );
	} else {

		m_dbg.debug(CALL_INFO,1,2,"found posted long recv bytes=%zu\n", bytes);

		MsgHdr* goMsg = (MsgHdr*) m_sendBuff.getBacking();
		goMsg->type = MsgHdr::Go;
		goMsg->rvmaAddr = entry->extra.rvmaAddr;
		goMsg->procAddr.node = os().getNodeNum();
		goMsg->procAddr.pid = os().getPid();
		goMsg->sendEntry = hdr->sendEntry;

		m_dbg.debug(CALL_INFO,1,2,"send GoMsg to nid=%d pid=%d rvmaAddr=%" PRIx64 "\n",hdr->procAddr.node, hdr->procAddr.pid,goMsg->rvmaAddr);

       Hermes::Callback* cb  = new Hermes::Callback([=]( int retval ) {
            repostRecvBuffer( msg, callback );
        });

		sendMsg( hdr->procAddr, m_sendBuff, sizeof(MsgHdr), cb );
		m_pendingLongEntry = entry;
	}
}

Hermes::MemAddr RvmaMpiPt2PtLib::checkUnexpected( RecvEntry* entry ) {
	std::deque< Hermes::MemAddr >::iterator iter = m_unexpectedRecvs.begin();
	for ( ; iter != m_unexpectedRecvs.end(); ++iter ) {
    	MsgHdr* hdr = (MsgHdr*) (*iter).getBacking();
		if ( checkMatch( hdr, entry ) ) {
			Hermes::MemAddr retval = *iter;
			m_unexpectedRecvs.erase(iter);
			return retval;
		}
	}
	throw -1;
}

void RvmaMpiPt2PtLib::postRecvBuffer( Hermes::Callback* callback, int count, int retval ) {

    --count;
    Hermes::Callback* cb = new Hermes::Callback;
    if ( count > 0 ) {
        *cb = std::bind( &RvmaMpiPt2PtLib::postRecvBuffer, this, callback, count, std::placeholders::_1 );
    } else {
        cb = callback;
    }
    size_t length = m_shortMsgLength + sizeof(MsgHdr);
    Hermes::MemAddr addr = m_recvBuff.offset( count * length );

	Hermes::RVMA::Completion* completion = new Hermes::RVMA::Completion;
    m_dbg.debug(CALL_INFO,1,2,"completion=%p count=%d retval=%d vaddr=0x%" PRIx64 " backing=%p\n", completion, count,retval, addr.getSimVAddr(), addr.getBacking());
	m_completionQ.push( completion );
    rvma().postBuffer( addr, length, completion, m_windowId, cb );
}

void RvmaMpiPt2PtLib::repostRecvBuffer( Hermes::MemAddr addr, Hermes::Callback* callback ) {

   	size_t length = m_shortMsgLength + sizeof(MsgHdr);
	int pos = (addr.getSimVAddr() - m_recvBuff.getSimVAddr()) / length; 
	Hermes::RVMA::Completion* completion = new Hermes::RVMA::Completion;
    m_dbg.debug(CALL_INFO,1,2,"completion=%p vaddr=0x%" PRIx64 " backing=%p\n",completion, addr.getSimVAddr(), addr.getBacking());
	m_completionQ.push( completion );
    rvma().postBuffer( addr, length, completion, m_windowId, callback );
}
