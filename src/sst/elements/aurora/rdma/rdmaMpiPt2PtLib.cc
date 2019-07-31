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
#include "rdmaMpiPt2PtLib.h"

using namespace SST::Aurora::RDMA;
using namespace Hermes;

#define CALL_INFO_LAMBDA     __LINE__, __FILE__


RdmaMpiPt2PtLib::RdmaMpiPt2PtLib( Component* owner, Params& params) : MpiPt2Pt(owner,params)
{

	if ( params.find<bool>("print_all_params",false) ) {
		printf("Aurora::RDMA::RdmaMpiPt2PtLib()\n");
		params.print_all_params(std::cout);
	}

	m_dbg.init("@t:Aurora::RDMA::RdmaMpiPt2PtLib::@p():@l ", params.find<uint32_t>("verboseLevel",0),
			params.find<uint32_t>("verboseMask",0), Output::STDOUT );

	m_shortMsgLength = params.find<size_t>("shortMsgLength",4096);
	m_numRecvBuffers = params.find<int>("numRecvBuffers",32); 
	m_numSendBuffers = 1;
	m_rqId = 0xF00D;
    m_dbg.debug(CALL_INFO,1,2,"\n");

	Params rdmaParams =  params.find_prefix_params("rdmaLib.");

	m_rdma = dynamic_cast< Hermes::RDMA::Interface*>( loadAnonymousSubComponent<Hermes::Interface>( "aurora.rdmaLib", "", 0, ComponentInfo::SHARE_NONE, rdmaParams ) );

	assert(m_rdma);
}

void RdmaMpiPt2PtLib::init( int* numRanks, int* myRank, Hermes::Callback* callback ) {
	
	*myRank = os().getWorldRank();
	*numRanks = os().getWorldNumRanks();
	m_dbg.debug(CALL_INFO,1,2,"numRanks=%d myRank=%d\n",*numRanks,*myRank);
	Callback* cb = new Callback( std::bind( &RdmaMpiPt2PtLib::mallocSendBuffers, this, callback, std::placeholders::_1 ) );
	rdma().createRQ( m_rqId, cb ); 
}

void RdmaMpiPt2PtLib::isend( const Hermes::Mpi::MemAddr& buf, int count, Hermes::Mpi::DataType dataType, int dest, int tag,
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

	size_t bytes = entry->count * Mpi::sizeofDataType( entry->dataType ); 
	if ( bytes > m_shortMsgLength ) {

		Callback* cb = new Callback( [=](int) {
			makeProgress(x);
		});	
				
		rdma().registerMem( entry->buf, bytes, &entry->extra.memId, cb );
	} else {

		makeProgress( x );
	}
}

void RdmaMpiPt2PtLib::irecv( const Hermes::Mpi::MemAddr& buf, int count, Hermes::Mpi::DataType dataType, int src, int tag,
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

	Hermes::RDMA::Status* status = checkUnexpected( entry );

	if ( status ) {
		m_dbg.debug(CALL_INFO,1,2,"found unexpected\n");
		processMatch( *status, entry, x );
	} else { 

		m_dbg.debug(CALL_INFO,1,2,"post recv\n");
		m_postedRecvs.push_back( entry );

		size_t bytes = entry->count * Mpi::sizeofDataType( entry->dataType ); 
		if ( bytes > m_shortMsgLength ) {

			Callback* cb = new Callback( [=](int) {
				makeProgress(x);
			});
				
			rdma().registerMem( entry->buf, bytes, &entry->extra.memId, cb );
		} else {
			makeProgress( x );
		}
	}
}

void RdmaMpiPt2PtLib::test( Hermes::Mpi::Request* request, Hermes::Mpi::Status* status, bool blocking, Hermes::Callback* callback )
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
	Callback* cb = new Callback( std::bind( &RdmaMpiPt2PtLib::processTest, this, entry, std::placeholders::_1 ) );

	makeProgress(cb);
}

void RdmaMpiPt2PtLib::testall( int count, Mpi::Request* request, int* flag, Mpi::Status* status, bool blocking, Callback* callback )
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

	Callback* cb = new Callback( std::bind( &RdmaMpiPt2PtLib::processTest, this, entry, std::placeholders::_1 ) );
	makeProgress(cb);
}

void RdmaMpiPt2PtLib::testany( int count, Mpi::Request* request, int* indx, int* flag, Mpi::Status* status, bool blocking, Callback* callback )
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

	Callback* cb = new Callback( std::bind( &RdmaMpiPt2PtLib::processTest, this, entry, std::placeholders::_1 ) );
	makeProgress(cb);
}

void RdmaMpiPt2PtLib::processTest( TestBase* waitEntry, int ) 
{
	m_dbg.debug(CALL_INFO,1,2,"\n");
   	if ( waitEntry->isDone() ) {

		m_dbg.debug(CALL_INFO,1,2,"entry done\n");
		(*waitEntry->callback)(0);
		delete waitEntry;

	} else if ( waitEntry->blocking )  {
		m_dbg.debug(CALL_INFO,1,2,"blocking\n");
		Callback* cb = new Callback( [=](int retval ){
			m_dbg.debug(CALL_INFO_LAMBDA,"processTest",1,2,"return from blocking checkRQ %s\n",retval ? "message ready":"no message");
			assert( retval == 1 );
		    Callback* cb = new Callback( std::bind( &RdmaMpiPt2PtLib::processTest, this, waitEntry, std::placeholders::_1 ) );
			checkMsgAvail( cb, retval );
		});
		rdma().checkRQ( m_rqId, &m_rqStatus, true, cb );
	} else {
		m_dbg.debug(CALL_INFO,1,2,"entry not done\n");
		(*waitEntry->callback)(0);
		delete waitEntry;
	}		
}

void RdmaMpiPt2PtLib::makeProgress( Hermes::Callback* callback )
{
	m_dbg.debug(CALL_INFO,1,2,"\n");
	Callback* cb = new Callback( std::bind( &RdmaMpiPt2PtLib::processSendQ, this, callback, std::placeholders::_1 ) );

	processRecvQ( cb, 0 ); 
}

void RdmaMpiPt2PtLib::processRecvQ( Hermes::Callback* callback, int retval  )
{
	m_dbg.debug(CALL_INFO,1,2,"\n");
	Callback* cb = new Callback( std::bind( &RdmaMpiPt2PtLib::checkMsgAvail, this, callback, std::placeholders::_1 ) );
	rdma().checkRQ( m_rqId, &m_rqStatus, false, cb );
}

void RdmaMpiPt2PtLib::checkMsgAvail( Hermes::Callback* callback, int retval ) {
    if ( 1 == retval ) {
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
}

void RdmaMpiPt2PtLib::processSendQ( Hermes::Callback* callback, int retval )
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

void RdmaMpiPt2PtLib::processSendEntry( Hermes::Callback* callback, SendEntry* entry ) {

	size_t bytes = entry->count * Mpi::sizeofDataType( entry->dataType ); 

	MsgHdr* hdr = (MsgHdr*) m_sendBuff.getBacking();
	hdr->srcRank = os().getMyRank(entry->comm);
	hdr->tag = entry->tag;
	hdr->count = entry->count;
	hdr->dataType = entry->dataType;
	hdr->comm = entry->comm;
	hdr->type = MsgHdr::Match;
	m_dbg.debug(CALL_INFO,1,2,"tag=%d rank=%d comm=%d count=%d type=%d\n",hdr->tag,hdr->srcRank,hdr->comm,hdr->count,hdr->type);

    Hermes::Callback* cb;

	Hermes::ProcAddr procAddr = getProcAddr<SendEntry>(entry);
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

    	*cb = std::bind( &RdmaMpiPt2PtLib::waitForLongAck, this, callback, entry, std::placeholders::_1 );
		
		bytes = 0;
	}
	sendMsg( procAddr, m_sendBuff, sizeof(MsgHdr) + bytes, cb );
}

void RdmaMpiPt2PtLib::waitForLongAck( Hermes::Callback* callback, SendEntry* entry, int retval )
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

void RdmaMpiPt2PtLib::sendMsg( Hermes::ProcAddr& procAddr, Hermes::MemAddr& addr, size_t length, Hermes::Callback* callback )
{
	m_dbg.debug(CALL_INFO,1,2,"destNid=%d destPid=%d addr=0x%" PRIx64 "length=%zu\n",
				procAddr.node, procAddr.pid, addr.getSimVAddr(), length  );
	rdma().send( procAddr, m_rqId, addr, length, callback );
}

void RdmaMpiPt2PtLib::processMsg( Hermes::RDMA::Status& status, Hermes::Callback* callback ) {
    m_dbg.debug(CALL_INFO,1,2,"got message from node=%d pid=%d \n",status.procAddr.node, status.procAddr.pid );
	
    MsgHdr* hdr = (MsgHdr*) status.addr.getBacking();
	if( hdr->type == MsgHdr::Ack ) {
		m_dbg.debug(CALL_INFO,1,2,"Ack key=%p\n",hdr->key);
		SendEntry* entry = (SendEntry*) hdr->key;
		entry->doneFlag = true;

		repostRecvBuffer( status.addr, callback );
		return;
	}

    RecvEntry* entry =  findPostedRecv( hdr );
	if ( entry ) {
		processMatch( status, entry, callback );
	} else {
		m_dbg.debug(CALL_INFO,1,2,"unexpected recv\n");

		m_unexpectedRecvs.push_back( new Hermes::RDMA::Status(status) );

		(*callback)(0);
		delete callback;	
	}	
}

void RdmaMpiPt2PtLib::processMatch( Hermes::RDMA::Status& status, RecvEntry* entry, Hermes::Callback* callback ) {
    MsgHdr* hdr = (MsgHdr*) status.addr.getBacking();

	entry->status.rank = hdr->srcRank;
	entry->status.tag = hdr->tag; 
	size_t bytes = entry->count * Mpi::sizeofDataType( entry->dataType ); 
	if ( bytes <= m_shortMsgLength ) {
		m_dbg.debug(CALL_INFO,1,2,"found posted short recv bytes=%zu\n",bytes);
		void* buf = entry->buf.getBacking();
		if ( buf ) {  
			void* payload = status.addr.getBacking(sizeof(MsgHdr)); 
			memcpy( buf, payload, bytes ); 
		}
		entry->doneFlag = true;

		repostRecvBuffer( status.addr, callback );
	} else {

		m_dbg.debug(CALL_INFO,1,2,"found posted long recv bytes=%zu\n", bytes);

	    Hermes::Callback* cb = new Hermes::Callback;
		*cb = [=](int retval ){
			m_dbg.debug(CALL_INFO_LAMBDA,"processMsg",1,2,"back from read, send ACK\n");

			entry->doneFlag = true;

			Hermes::ProcAddr procAddr = status.procAddr;

   			MsgHdr* ackHdr = (MsgHdr*) m_sendBuff.getBacking();
			ackHdr->type = MsgHdr::Ack;
			ackHdr->key = hdr->key; 
	    	Hermes::Callback* cb = new Hermes::Callback;
			*cb = [=](int retval ){
				repostRecvBuffer( status.addr, callback );
			};

			sendMsg( procAddr, m_sendBuff, sizeof(MsgHdr), cb );
		};

		rdma().read( status.procAddr, entry->buf, hdr->readAddr, bytes, cb );
	}
}

Hermes::RDMA::Status* RdmaMpiPt2PtLib::checkUnexpected( RecvEntry* entry ) {
	std::deque< Hermes::RDMA::Status* >::iterator iter = m_unexpectedRecvs.begin();
	for ( ; iter != m_unexpectedRecvs.end(); ++iter ) {
    	MsgHdr* hdr = (MsgHdr*) (*iter)->addr.getBacking();
		if ( checkMatch<MsgHdr,RecvEntry>( hdr, entry ) ) {
			Hermes::RDMA::Status* retval = *iter;
			m_unexpectedRecvs.erase(iter);
			return retval;
		}
	}
	return NULL;
}

RdmaMpiPt2PtLib::RecvEntry* RdmaMpiPt2PtLib::findPostedRecv( MsgHdr* hdr ) {
    std::deque< RecvEntry* >::iterator iter = m_postedRecvs.begin();
    for ( ; iter != m_postedRecvs.end(); ++iter ) {
		if ( checkMatch<MsgHdr,RecvEntry>( hdr, *iter ) ) {
    		RecvEntry* entry = *iter;
			m_postedRecvs.erase( iter );
			return entry;
		}
    }
	return NULL;
}

void RdmaMpiPt2PtLib::mallocSendBuffers( Hermes::Callback* callback, int retval ) {
    size_t length = m_numSendBuffers * ( m_shortMsgLength + sizeof(MsgHdr) );
    m_dbg.debug(CALL_INFO,1,2,"length=%zu\n",length);

    Hermes::Callback* cb = new Hermes::Callback;
    *cb = std::bind( &RdmaMpiPt2PtLib::mallocRecvBuffers, this, callback, std::placeholders::_1 );
    misc().malloc( &m_sendBuff, length, true, cb );
}

void RdmaMpiPt2PtLib::mallocRecvBuffers( Hermes::Callback* callback, int retval ) {
    size_t length = m_numRecvBuffers *  ( m_shortMsgLength + sizeof(MsgHdr) );
    m_dbg.debug(CALL_INFO,1,2,"length=%zu\n",length);

    Hermes::Callback* cb = new Hermes::Callback;
    *cb = std::bind( &RdmaMpiPt2PtLib::postRecvBuffer, this, callback, m_numRecvBuffers, std::placeholders::_1 );
    misc().malloc( &m_recvBuff, length, true, cb );
}

void RdmaMpiPt2PtLib::postRecvBuffer( Hermes::Callback* callback, int count, int retval ) {
    m_dbg.debug(CALL_INFO,1,2,"count=%d retval=%d\n",count,retval);

    --count;
    Hermes::Callback* cb = new Hermes::Callback;
    if ( count > 0 ) {
        *cb = std::bind( &RdmaMpiPt2PtLib::postRecvBuffer, this, callback, count, std::placeholders::_1 );
    } else {
        cb = callback;
    }
    size_t length = m_shortMsgLength + sizeof(MsgHdr);
    Hermes::MemAddr addr = m_recvBuff.offset( count * length );
    rdma().postRecv( m_rqId, addr, length, NULL, cb );
}

void RdmaMpiPt2PtLib::repostRecvBuffer( Hermes::MemAddr addr, Hermes::Callback* callback ) {
    m_dbg.debug(CALL_INFO,1,2,"\n");

   	size_t length = m_shortMsgLength + sizeof(MsgHdr);
   	rdma().postRecv( m_rqId, addr, length, NULL, callback );
}
