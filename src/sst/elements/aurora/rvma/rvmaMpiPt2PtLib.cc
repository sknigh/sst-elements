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

RvmaMpiPt2PtLib::RvmaMpiPt2PtLib( Component* owner, Params& params) : MpiPt2Pt(owner,params)
{

	if ( params.find<bool>("print_all_params",false) ) {
		printf("Aurora::RVMA::RvmaMpiPt2PtLib()\n");
		params.print_all_params(std::cout);
	}

	m_dbg.init("@t:Aurora::RVMA::RvmaMpiPt2PtLib::@p():@l ", params.find<uint32_t>("verboseLevel",0),
			params.find<uint32_t>("verboseMask",0), Output::STDOUT );

	m_windowAddr = 0xF00D;
	m_numRecvBuffers = params.find<int>("numRecvBuffers",32); 
	m_numSendBuffers = 1;

    m_dbg.debug(CALL_INFO,1,2,"\n");

	Params rvmaParams =  params.find_prefix_params("rvmaLib.");

	m_rvma = dynamic_cast< Hermes::RVMA::Interface*>( loadAnonymousSubComponent<Interface>( "aurora.rvmaLib", "", 0, ComponentInfo::SHARE_NONE, rvmaParams ) );
	assert(m_rvma);
}

void RvmaMpiPt2PtLib::init( int* numRanks, int* myRank, Hermes::Callback* callback ) {
	
	*myRank = os().getWorldRank();
	*numRanks = os().getWorldNumRanks();
	m_dbg.debug(CALL_INFO,1,2,"numRanks=%d myRank=%d\n",*numRanks,*myRank);
	Callback* cb = new Callback( std::bind( &RvmaMpiPt2PtLib::mallocSendBuffers, this, callback, std::placeholders::_1 ) );
	rvma().initWindow( m_windowAddr, 1, Hermes::RVMA::EpochType::Op, &m_windowId, cb ); 
}

void RvmaMpiPt2PtLib::isend( const Hermes::Mpi::MemAddr& buf, int count, Hermes::Mpi::DataType dataType, int dest, int tag,
		Hermes::Mpi::Comm comm, Hermes::Mpi::Request* request, Hermes::Callback* callback ) 
{
	m_dbg.debug(CALL_INFO,1,2,"buf=0x%" PRIx64 " count=%d dataSize=%d dest=%d tag=%d comm=%d\n",
			buf.getSimVAddr(),count,Mpi::sizeofDataType( dataType ), dest, tag, comm );

	Hermes::Callback* x = new Hermes::Callback([=]( int retval ) {
		m_dbg.debug(CALL_INFO_LAMBDA,"isend",1,2,"returning\n");
		assert( m_callback == NULL );
		m_callback = callback;
		m_retval = retval;
		m_selfLink->send(0,NULL);
	});	

	request->type = Hermes::Mpi::Request::Send;
	SendEntry* entry = new SendEntry( buf, count, dataType, dest, tag, comm, request );
	request->entry = entry;

	m_dbg.debug(CALL_INFO,1,2,"request=%p entry=%p\n",request,request->entry);

	m_postedSends.push( entry );

	makeProgress(x);
}

void RvmaMpiPt2PtLib::irecv( const Hermes::Mpi::MemAddr& buf, int count, Hermes::Mpi::DataType dataType, int src, int tag,
		Hermes::Mpi::Comm comm, Hermes::Mpi::Request* request, Hermes::Callback* callback ) 
{

	m_dbg.debug(CALL_INFO,1,2,"buf=0x%" PRIx64 " count=%d dataSize=%d src=%d tag=%d comm=%d\n",
			buf.getSimVAddr(),count,Mpi::sizeofDataType( dataType ), src, tag, comm );

	Hermes::Callback* x = new Hermes::Callback([=]( int retval ) {
		m_dbg.debug(CALL_INFO_LAMBDA,"irecv",1,2,"returning\n");
		assert( m_callback == NULL );
		m_callback = callback;
		m_retval = retval;
		m_selfLink->send(0,NULL);
	});	

	request->type = Hermes::Mpi::Request::Recv;
	RecvEntry* entry = new RecvEntry( buf, count, dataType, src, tag, comm, request );
	request->entry = entry;

	m_dbg.debug(CALL_INFO,1,2,"request=%p entry=%p\n",request,request->entry);

	try {
		Hermes::MemAddr addr = checkUnexpected( entry );
		m_dbg.debug(CALL_INFO,1,2,"found unexpected\n");
		processMatch( addr, entry, x );
	} 
	catch ( int error )
	{
		m_dbg.debug(CALL_INFO,1,2,"post recv\n");
		m_postedRecvs.push_back( entry );

		size_t bytes = entry->count * Mpi::sizeofDataType( entry->dataType ); 
		if ( bytes > m_shortMsgLength ) {

			Callback* cb = new Callback( [=](int) {
				m_dbg.debug(CALL_INFO_LAMBDA,"irecv",1,2,"back from initWindow\n");
				Callback* cb = new Callback( [=](int) {
					m_dbg.debug(CALL_INFO_LAMBDA,"irecv",1,2,"back from postBuffer\n");
					makeProgress(x);
				});
				rvma().postBuffer( entry->buf, bytes, &entry->extra.completion, entry->extra.winId, cb );
			});
				
			rvma().initWindow( entry->buf.getSimVAddr(), bytes, Hermes::RVMA::EpochType::Byte, &entry->extra.winId, cb );
		} else {
			makeProgress( x );
		}
	}
}

void RvmaMpiPt2PtLib::test( Hermes::Mpi::Request* request, Hermes::Mpi::Status* status, bool blocking, Hermes::Callback* callback )
{
	m_dbg.debug(CALL_INFO,1,2,"type=%s blocking=%s\n",request->type==Mpi::Request::Send?"Send":"Recv", blocking?"yes":"no");

	Hermes::Callback* x  = new Hermes::Callback([=]( int retval ) {
		m_dbg.debug(CALL_INFO_LAMBDA,"test",1,2,"returning\n");
		assert( m_callback == NULL );
		m_callback = callback;
		m_retval = retval;
		m_selfLink->send(0,NULL);
	});	

	TestEntry* entry = new TestEntry( request, status, blocking, x );
	Callback* cb = new Callback( std::bind( &RvmaMpiPt2PtLib::processTest, this, entry, std::placeholders::_1 ) );

	makeProgress(cb);
}

void RvmaMpiPt2PtLib::testall( int count, Mpi::Request* request, int* flag, Mpi::Status* status, bool blocking, Callback* callback )
{
	Hermes::Callback* x  = new Hermes::Callback([=]( int retval ) {
		m_dbg.debug(CALL_INFO_LAMBDA,"testall",1,2,"returning\n");
		assert( m_callback == NULL );
		m_callback = callback;
		m_retval = retval;
		m_selfLink->send(0,NULL);
	});	

	m_dbg.debug(CALL_INFO,1,2,"count=%d blocking=%s\n",count,blocking?"yes":"no");

	for ( int i = 0; i < count; i++ ) {
		m_dbg.debug(CALL_INFO,1,2,"request[%d].type=%s\n",i,request[i].type==Mpi::Request::Send?"Send":"Recv");
	}
	TestallEntry* entry = new TestallEntry( count, request, flag, status, blocking, x );

	Callback* cb = new Callback( std::bind( &RvmaMpiPt2PtLib::processTest, this, entry, std::placeholders::_1 ) );
	makeProgress(cb);
}

void RvmaMpiPt2PtLib::testany( int count, Mpi::Request* request, int* indx, int* flag, Mpi::Status* status, bool blocking, Callback* callback )
{
	Hermes::Callback* x  = new Hermes::Callback([=]( int retval ) {
		m_dbg.debug(CALL_INFO_LAMBDA,"testany",1,2,"returning\n");
		assert( m_callback == NULL );
		m_callback = callback;
		m_retval = retval;
		m_selfLink->send(0,NULL);
	});	

	m_dbg.debug(CALL_INFO,1,2,"count=%d\n",count);
	for ( int i = 0; i < count; i++ ) {
		m_dbg.debug(CALL_INFO,1,2,"request[%d]type=%s\n",i,request[i].type==Mpi::Request::Send?"Send":"Recv");
	}
	TestanyEntry* entry = new TestanyEntry( count, request, indx, flag, status, blocking, x );

	Callback* cb = new Callback( std::bind( &RvmaMpiPt2PtLib::processTest, this, entry, std::placeholders::_1 ) );
	makeProgress(cb);
}

void RvmaMpiPt2PtLib::processTest( TestBase* waitEntry, int ) 
{
	m_dbg.debug(CALL_INFO,1,2,"\n");
	assert(0);
   	if ( waitEntry->isDone() ) {

		m_dbg.debug(CALL_INFO,1,2,"entry done\n");
		(*waitEntry->callback)(0);
		delete waitEntry;

	} else if ( waitEntry->blocking )  {
		m_dbg.debug(CALL_INFO,1,2,"blocking\n");
		Callback* cb = new Callback( [=](int retval ){
			m_dbg.debug(CALL_INFO_LAMBDA,"processTest",1,2,"return from blocking checkRQ %s\n",retval ? "message ready":"no message");
			assert( retval == 1 );
		    Callback* cb = new Callback( std::bind( &RvmaMpiPt2PtLib::processTest, this, waitEntry, std::placeholders::_1 ) );
			checkMsgAvail( cb, retval );
		});
#if 0
		rvma().mwait( &m_completion, true, cb );
#endif
	} else {
		m_dbg.debug(CALL_INFO,1,2,"entry not done\n");
		(*waitEntry->callback)(0);
		delete waitEntry;
	}		
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
	Callback* cb = new Callback( std::bind( &RvmaMpiPt2PtLib::checkMsgAvail, this, callback, std::placeholders::_1 ) );
#if 0
	rvma().mwait( &m_completion, false, cb );
#endif
}

void RvmaMpiPt2PtLib::checkMsgAvail( Hermes::Callback* callback, int retval ) {

#if 0
    if ( m_completion == retval ) {
    	m_dbg.debug(CALL_INFO,1,2,"message avail\n");

		Hermes::Callback* cb = new Hermes::Callback;
		*cb = [=](int) {
			processRecvQ( callback, 0 );
		};
        processMsg( m_rqStatus, cb );
    } else { 
    	m_dbg.debug(CALL_INFO,1,2,"no message\n");
		(*callback)(0);
		delete callback;
	}
#endif
}

void RvmaMpiPt2PtLib::processSendQ( Hermes::Callback* callback, int retval )
{
	if ( ! m_postedSends.empty() ) { 
		SendEntry* entry = m_postedSends.front(); 
    	m_dbg.debug(CALL_INFO,1,2,"have entry\n");

		Hermes::Callback* cb = new Hermes::Callback;
		*cb = [=](int) {
    		m_dbg.debug(CALL_INFO_LAMBDA,"processSendQ",1,2,"callback\n");
			if ( entry->isDone() ) {
    			m_dbg.debug(CALL_INFO,1,2,"send entry is done\n");
				m_postedSends.pop();
			}
			processSendQ( callback, 0 );
		};

		processSendEntry( cb, entry );

	} else {
    	m_dbg.debug(CALL_INFO,1,2,"call callback\n");
		(*callback)(0);
		delete callback;
	}
}

void RvmaMpiPt2PtLib::processSendEntry( Hermes::Callback* callback, SendEntry* entry ) {

	size_t bytes = entry->count * Mpi::sizeofDataType( entry->dataType ); 

	MsgHdr* hdr = (MsgHdr*) m_sendBuff.getBacking();
	hdr->srcRank = os().getMyRank(entry->comm);
	hdr->tag = entry->tag;
	hdr->count = entry->count;
	hdr->dataType = entry->dataType;
	hdr->comm = entry->comm;
#if 0
	hdr->type = MsgHdr::Match;
	m_dbg.debug(CALL_INFO,1,2,"tag=%d rank=%d comm=%d count=%d type=%d\n",hdr->tag,hdr->srcRank,hdr->comm,hdr->count,hdr->type);
#endif

	assert(0);
    Hermes::Callback* cb;

	Hermes::ProcAddr procAddr = getProcAddr(entry);
#if 0
	if ( bytes <= m_shortMsgLength ) {
		m_dbg.debug(CALL_INFO,1,2,"short message\n");
		if ( entry->buf.getBacking() ) {
			void* payload = entry->buf.getBacking(); 
			memcpy( m_sendBuff.getBacking(sizeof(MsgHdr)), payload, bytes ); 
		}	
		entry->doneFlag = true;
		cb = callback;

	} else {
		m_dbg.debug(CALL_INFO,1,2,"long message key=%p\n",entry);

		hdr->readAddr = entry->buf.getSimVAddr();
		hdr->key = entry;

		cb = new Hermes::Callback;

    	*cb = std::bind( &RvmaMpiPt2PtLib::waitForLongAck, this, callback, entry, std::placeholders::_1 );
		
		bytes = 0;
	}
#endif
	sendMsg( procAddr, m_sendBuff, sizeof(MsgHdr) + bytes, cb );
}

#if 0
void RvmaMpiPt2PtLib::waitForLongAck( Hermes::Callback* callback, SendEntry* entry, int retval )
{
	m_dbg.debug(CALL_INFO_LAMBDA,"processSendQ",1,2,"\n");

	Hermes::Callback* cb = new Hermes::Callback;
	*cb = [=](int retval ){

		m_dbg.debug(CALL_INFO_LAMBDA,"processSendQ",1,2,"back from checkRQ\n");
   		Hermes::Callback* cb = new Hermes::Callback;
		*cb = [=](int retval ){ 
			m_dbg.debug(CALL_INFO_LAMBDA,"processSendQ",1,2,"back from checkMsgAvail\n");
			if ( entry->isDone() ) {
    			m_dbg.debug(CALL_INFO_LAMBDA,"processSendQ",1,2,"long send done\n");
				(*callback)(0);
				delete callback;				
			} else {
    			m_dbg.debug(CALL_INFO_LAMBDA,"processSendQ",1,2,"call waitForLongAck again\n");
				waitForLongAck( callback, entry, 0 );
			}
		};

		assert( retval == 1 );
		checkMsgAvail( cb, retval );
	};
	rdma().checkRQ( m_rqId, &m_rqStatus, true, cb );
}
#endif

void RvmaMpiPt2PtLib::sendMsg( Hermes::ProcAddr& procAddr, Hermes::MemAddr& addr, size_t length, Hermes::Callback* callback )
{
	m_dbg.debug(CALL_INFO,1,2,"destNid=%d destPid=%d addr=0x%" PRIx64 "length=%zu\n",
				procAddr.node, procAddr.pid, addr.getSimVAddr(), length  );
	assert(0);
#if 0
	rdma().send( procAddr, m_rqId, addr, length, callback );
#endif
}

void RvmaMpiPt2PtLib::processMsg( Hermes::MemAddr& msg, Hermes::Callback* callback ) {
	
    MsgHdr* hdr = (MsgHdr*) msg.getBacking();
    m_dbg.debug(CALL_INFO,1,2,"got message from node=%d pid=%d \n",hdr->procAddr.node, hdr->procAddr.pid );

	assert(0);
#if 0
	if( hdr->type == MsgHdr::Ack ) {
		m_dbg.debug(CALL_INFO,1,2,"Ack key=%p\n",hdr->key);
		SendEntry* entry = (SendEntry*) hdr->key;
		entry->doneFlag = true;

		repostRecvBuffer( status.addr, callback );
		return;
	}
#endif

    RecvEntry* entry =  findPostedRecv( hdr );
	if ( entry ) {
		processMatch( msg, entry, callback );
	} else {
		m_dbg.debug(CALL_INFO,1,2,"unexpected recv\n");

		m_unexpectedRecvs.push_back( msg );

		(*callback)(0);
		delete callback;	
	}	
}

void RvmaMpiPt2PtLib::processMatch( Hermes::MemAddr& msg, RecvEntry* entry, Hermes::Callback* callback ) {
    MsgHdr* hdr = (MsgHdr*) msg.getBacking();

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

	    Hermes::Callback* cb = new Hermes::Callback;
		*cb = [=](int retval ){
			m_dbg.debug(CALL_INFO_LAMBDA,"processMsg",1,2,"back from read, send ACK\n");

			entry->doneFlag = true;

			assert(0);
#if 0
			Hermes::ProcAddr procAddr = hdr->procAddr;

   			MsgHdr* ackHdr = (MsgHdr*) m_sendBuff.getBacking();
			ackHdr->type = MsgHdr::Ack;
			ackHdr->key = hdr->key; 
	    	Hermes::Callback* cb = new Hermes::Callback;
			*cb = [=](int retval ){
				repostRecvBuffer( status.addr, callback );
			};

			sendMsg( procAddr, m_sendBuff, sizeof(MsgHdr), cb );
#endif
		};

		assert(0);
#if 0
		rdma().read( status.procAddr, entry->buf, hdr->readAddr, bytes, cb );
#endif
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

RvmaMpiPt2PtLib::RecvEntry* RvmaMpiPt2PtLib::findPostedRecv( MsgHdr* hdr ) {
    std::deque< RecvEntry* >::iterator iter = m_postedRecvs.begin();
    for ( ; iter != m_postedRecvs.end(); ++iter ) {
		if ( checkMatch( hdr, *iter ) ) {
    		RecvEntry* entry = *iter;
			m_postedRecvs.erase( iter );
			return entry;
		}
    }
	return NULL;
}

void RvmaMpiPt2PtLib::mallocSendBuffers( Hermes::Callback* callback, int retval ) {
    size_t length = m_numSendBuffers * ( m_shortMsgLength + sizeof(MsgHdr) );
    m_dbg.debug(CALL_INFO,1,2,"length=%zu\n",length);

    Hermes::Callback* cb = new Hermes::Callback;
    *cb = std::bind( &RvmaMpiPt2PtLib::mallocRecvBuffers, this, callback, std::placeholders::_1 );
    misc().malloc( &m_sendBuff, length, true, cb );
}

void RvmaMpiPt2PtLib::mallocRecvBuffers( Hermes::Callback* callback, int retval ) {
    size_t length = m_numRecvBuffers *  ( m_shortMsgLength + sizeof(MsgHdr) );
    m_dbg.debug(CALL_INFO,1,2,"length=%zu\n",length);

    Hermes::Callback* cb = new Hermes::Callback;
    *cb = std::bind( &RvmaMpiPt2PtLib::postRecvBuffer, this, callback, m_numRecvBuffers, std::placeholders::_1 );
    misc().malloc( &m_recvBuff, length, true, cb );
}

void RvmaMpiPt2PtLib::postRecvBuffer( Hermes::Callback* callback, int count, int retval ) {
    m_dbg.debug(CALL_INFO,1,2,"count=%d retval=%d\n",count,retval);

    --count;
    Hermes::Callback* cb = new Hermes::Callback;
    if ( count > 0 ) {
        *cb = std::bind( &RvmaMpiPt2PtLib::postRecvBuffer, this, callback, count, std::placeholders::_1 );
    } else {
        cb = callback;
    }
    size_t length = m_shortMsgLength + sizeof(MsgHdr);
    Hermes::MemAddr addr = m_recvBuff.offset( count * length );

    rvma().postBuffer( addr, length, &m_completion, m_windowId, cb );
}

void RvmaMpiPt2PtLib::repostRecvBuffer( Hermes::MemAddr addr, Hermes::Callback* callback ) {
    m_dbg.debug(CALL_INFO,1,2,"\n");

   	size_t length = m_shortMsgLength + sizeof(MsgHdr);
	int pos = (addr.getSimVAddr() - m_recvBuff.getSimVAddr()) / length; 
    rvma().postBuffer( addr, length, &m_completion, m_windowId, callback );
}
