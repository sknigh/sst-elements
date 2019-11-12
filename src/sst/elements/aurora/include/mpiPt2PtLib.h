// Copyright 2013-2018 NTESS. Under the terms
// of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.
//
// Copyright (c) 2013-2018, NTESS
// All rights reserved.
//
// Portions are copyright of other developers:
// See the file CONTRIBUTORS.TXT in the top level directory
// the distribution for more information.
//
// This file is part of the SST software package. For license
// information, see the LICENSE file in the top level directory of the
// distribution.


#ifndef COMPONENTS_AURORA_INCLUDE_MPI_PT2PT_LIB_H
#define COMPONENTS_AURORA_INCLUDE_MPI_PT2PT_LIB_H

#include "include/hostLib.h"
#include "sst/elements/hermes/mpiPt2Pt.h"

#define CALL_INFO_LAMBDA     __LINE__, __FILE__

#define MPI_DBG_MASK_MSG_LVL1 (1<<1)
#define MPI_DBG_MASK_MSG_LVL2 (1<<2)
#define MPI_DBG_MASK_MSG_LVL3 (1<<3)

namespace SST {
namespace Aurora {

class MpiPt2Pt : public Hermes::Mpi::Interface {

  public:

	MpiPt2Pt(Component* owner, Params& params) : Interface(owner), m_memcpyLatency(0)
	{
		m_shortMsgLength = params.find<size_t>("shortMsgLength",4096);
		m_numRecvBuffers = params.find<int>("numRecvBuffers",32);
		m_numSendBuffers = params.find<int>("numSendBuffers",32);
		m_sendBuffList.resize( m_numSendBuffers ); 

    	std::ostringstream tmp;
    	tmp << this << "-AuroraMpiPt2PtLibSelfLink";
    	m_selfLink = owner->configureSelfLink(tmp.str(), "1 ns", new Event::Handler<MpiPt2Pt>(this,&MpiPt2Pt::selfLinkHandler));

    	Params modParams;
		m_misc = dynamic_cast< Hermes::Misc::Interface*>( loadAnonymousSubComponent<Hermes::Interface>( "aurora.misc", "", 0, ComponentInfo::SHARE_NONE, modParams ) );
    	assert(m_misc);

		int defaultLatency = params.find<int>("defaultLatency",0);
		m_initLatency = params.find<int>("initLatency",defaultLatency);
		m_sendLatency = params.find<int>("sendLatency",defaultLatency);
		m_recvLatency = params.find<int>("recvLatency",defaultLatency);
		m_isendLatency = params.find<int>("isendLatency",defaultLatency);
		m_irecvLatency = params.find<int>("irecvLatency",defaultLatency);
		m_testLatency = params.find<int>("testLatency",defaultLatency);
		m_testallLatency = params.find<int>("testallLatency",defaultLatency);	
		m_testanyLatency = params.find<int>("testanyLatency",defaultLatency);
		m_matchLatency = params.find<int>("matchLatency",0);
		double bw = params.find<UnitAlgebra>("memcpyBandwidth","0").getRoundedValue();
		if ( bw ) {
			m_memcpyLatency = ((1.0/bw) * 1000000000.0); 
		}
	}

	void setup() {
		m_misc->setup();
	}

	void setOS( Hermes::OS* os ) {
        m_os = static_cast<Host*>(os);
        m_misc->setOS(m_os);
    }

	void init( int* numRanks, int* myRank, Hermes::Callback* callback ) {
		*myRank = os().getWorldRank();
		*numRanks = os().getWorldNumRanks();
		Hermes::Callback* cb = new Hermes::Callback(std::bind( &MpiPt2Pt::_init, this, numRanks, myRank, callback ) );
		m_selfLink->send( m_initLatency,new SelfEvent(cb) );
	}

	void send(const Hermes::MemAddr& buf, int count, Mpi::DataType dataType, int dest, int tag,
        Hermes::Mpi::Comm comm, Hermes::Callback* callback )
	{
		m_dbg.debug(CALL_INFO,1,1,"buf=0x%" PRIx64 " count=%d dataSize=%d dest=%d tag=%d comm=%d\n",
			buf.getSimVAddr(),count,Mpi::sizeofDataType( dataType ), dest, tag, comm );

		Callback* cb = new Callback( std::bind( &MpiPt2Pt::_send, this, buf, count, dataType, dest, tag, comm, callback ) );
		m_selfLink->send( m_sendLatency,new SelfEvent(cb) );
	}

	void isend( const Hermes::Mpi::MemAddr& buf, int count, Hermes::Mpi::DataType dataType, int dest, int tag,
		Hermes::Mpi::Comm comm, Hermes::Mpi::Request* request, Hermes::Callback* callback )
	{
		Hermes::Callback* cb = new Hermes::Callback(std::bind( &MpiPt2Pt::_isend, this, buf, count, dataType, dest, tag, comm, request, callback ) );
		m_selfLink->send( m_isendLatency,new SelfEvent(cb) );
	}

	void recv(const Hermes::MemAddr& buf, int count, Mpi::DataType dataType, int src, int tag,
        Hermes::Mpi::Comm comm, Hermes::Mpi::Status* status, Hermes::Callback* callback )
	{
		m_dbg.debug(CALL_INFO,1,1,"buf=0x%" PRIx64 " count=%d dataSize=%d src=%d tag=%d comm=%d\n",
			buf.getSimVAddr(),count, Mpi::sizeofDataType( dataType ), src, tag, comm );
		Callback* cb = new Hermes::Callback( std::bind( &MpiPt2Pt::_recv, this, buf, count, dataType, src, tag, comm, status, callback ) );
		m_selfLink->send( m_recvLatency,new SelfEvent(cb) );
	}

	void irecv( const Hermes::Mpi::MemAddr& buf, int count, Hermes::Mpi::DataType dataType, int src, int tag,
		Hermes::Mpi::Comm comm, Hermes::Mpi::Request* request, Hermes::Callback* callback )
	{
		Hermes::Callback* cb = new Hermes::Callback(std::bind( &MpiPt2Pt::_irecv, this, buf, count, dataType, src, tag, comm, request, callback ) );
		m_selfLink->send( m_irecvLatency,new SelfEvent(cb) );
	}

	void test( Hermes::Mpi::Request* request, Hermes::Mpi::Status* status, bool blocking, Hermes::Callback* callback )
	{
		Hermes::Callback* cb = new Hermes::Callback(std::bind( &MpiPt2Pt::_test, this, request, status, blocking, callback ) );
		m_selfLink->send( m_testLatency,new SelfEvent(cb) );
	}

	void testall( int count, Mpi::Request* request, int* flag, Mpi::Status* status, bool blocking, Callback* callback )
	{
		Hermes::Callback* cb = new Hermes::Callback(std::bind( &MpiPt2Pt::_testall, this, count, request, flag, status, blocking, callback ) );
		m_selfLink->send( m_testallLatency,new SelfEvent(cb) );
	}

	void testany( int count, Mpi::Request* request, int* indx, int* flag, Mpi::Status* status, bool blocking, Callback* callback )
	{
		Hermes::Callback* cb = new Hermes::Callback(std::bind( &MpiPt2Pt::_testany, this, count, request, indx, flag, status, blocking, callback ));
		m_selfLink->send( m_testanyLatency,new SelfEvent(cb) );
	}

  protected:

	uint64_t calcMemcpyLatency( size_t bytes ) {
		m_dbg.debug(CALL_INFO,1,1,"memcpyLatency for %zu bytes is %" PRIu64 " ns.\n",bytes,  (uint64_t) bytes * m_memcpyLatency);
		return ((double)bytes * m_memcpyLatency);
	}

	virtual void _init( int* numRanks, int* myRank, Hermes::Callback* callback ) = 0;
	virtual void _isend( const Hermes::Mpi::MemAddr& buf, int count, Hermes::Mpi::DataType dataType, int dest, int tag,
		Hermes::Mpi::Comm comm, Hermes::Mpi::Request* request, Hermes::Callback* callback ) = 0;
	virtual void _irecv( const Hermes::Mpi::MemAddr& buf, int count, Hermes::Mpi::DataType dataType, int src, int tag,
		Hermes::Mpi::Comm comm, Hermes::Mpi::Request* request, Hermes::Callback* callback ) = 0;

	void _send(const Hermes::MemAddr& buf, int count, Mpi::DataType dataType, int dest, int tag,
        Hermes::Mpi::Comm comm, Hermes::Callback* callback )
	{
    	Callback *cb = new Callback( [=](int) {
        	m_dbg.debug(CALL_INFO_LAMBDA,"send",2,2,"back from isend\n");
        	Callback* cb = new Callback( [=](int retval ) {
				m_dbg.debug(CALL_INFO_LAMBDA,"send",1,1,"return to motif, count=%d dtype=%d dest=%d tag=%d\n", count,dataType,dest,tag);
            	(*callback)(retval);
            	delete callback;
        	});

        	test( &m_request, &m_status, true, cb );
    	});

    	assert( comm == Mpi::CommWorld );
    	isend( buf, count, dataType, dest, tag, Mpi::CommWorld, &m_request, cb );
	}

	void _recv(const Hermes::MemAddr& buf, int count, Mpi::DataType dataType, int src, int tag,
        Hermes::Mpi::Comm comm, Hermes::Mpi::Status* status, Hermes::Callback* callback )
	{
    	Callback* cb = new Callback( [=](int) {
        	m_dbg.debug(CALL_INFO_LAMBDA,"recv",2,2,"back from irecv\n");
        	Callback* cb = new Callback( [=]( int retval ) {
				m_dbg.debug(CALL_INFO_LAMBDA,"recv",1,1,"return to motif, count=%d dtype=%d src=%d tag=%d\n", count,dataType,src,tag);
            	(*callback)(retval);
            	delete callback;
        	});

        	test( &m_request, &m_status, true, cb );
    	});

    	assert( comm == Mpi::CommWorld );
    	irecv( buf, count, dataType, src, tag, Mpi::CommWorld, &m_request, cb );
	}

	void _test( Hermes::Mpi::Request* request, Hermes::Mpi::Status* status, bool blocking, Hermes::Callback* callback )
	{
		m_dbg.debug(CALL_INFO,1,1,"type=%s blocking=%s\n",(*request)->type==Mpi::RequestData::Send?"Send":"Recv", blocking?"yes":"no");

		Hermes::Callback* x  = new Hermes::Callback([=]( int retval ) {
			m_dbg.debug(CALL_INFO_LAMBDA,"test",1,1,"returning\n");
			m_selfLink->send(0,new SelfEvent(callback,retval) );
		});

		TestEntryBase* entry = new TestEntryBase( request, status, blocking, x );
		Callback* cb = new Callback( std::bind( &MpiPt2Pt::processTest, this, entry, std::placeholders::_1 ) );

		makeProgress(cb);
	}

	void _testall( int count, Mpi::Request* request, int* flag, Mpi::Status* status, bool blocking, Callback* callback )
	{
		Hermes::Callback* x  = new Hermes::Callback([=]( int retval ) {
			m_dbg.debug(CALL_INFO_LAMBDA,"testall",1,1,"returning\n");
			m_selfLink->send(0,new SelfEvent(callback,retval) );
		});

		m_dbg.debug(CALL_INFO,1,1,"count=%d blocking=%s\n",count,blocking?"yes":"no");

		for ( int i = 0; i < count; i++ ) {
			m_dbg.debug(CALL_INFO,1,1,"request[%d].type=%s\n",i,request[i]->type==Mpi::RequestData::Send?"Send":"Recv");
		}
		TestallEntryBase* entry = new TestallEntryBase( count, request, flag, status, blocking, x );

		Callback* cb = new Callback( std::bind( &MpiPt2Pt::processTest, this, entry, std::placeholders::_1 ) );

		makeProgress(cb);
	}

	void _testany( int count, Mpi::Request* request, int* indx, int* flag, Mpi::Status* status, bool blocking, Callback* callback )
	{
		Hermes::Callback* x  = new Hermes::Callback([=]( int retval ) {
			m_dbg.debug(CALL_INFO_LAMBDA,"testany",1,1,"returning\n");
			m_selfLink->send(0,new SelfEvent(callback,retval));
		});

		m_dbg.debug(CALL_INFO,1,1,"count=%d\n",count);
		for ( int i = 0; i < count; i++ ) {
			m_dbg.debug(CALL_INFO,1,1,"request[%d]type=%s\n",i,request[i]->type==Mpi::RequestData::Send?"Send":"Recv");
		}
		TestanyEntryBase* entry = new TestanyEntryBase( count, request, indx, flag, status, blocking, x );

		Callback* cb = new Callback( std::bind( &MpiPt2Pt::processTest, this, entry, std::placeholders::_1 ) );

		makeProgress(cb);
	}

	class SelfEvent : public SST::Event { 
	  public:
		SelfEvent( Hermes::Callback* callback, int retval = 0 ) : callback(callback), retval(retval) {} 
 		Hermes::Callback* callback;
		int retval;
		NotSerializable(SelfEvent);
	}; 

    class __attribute__ ((packed)) MsgHdrBase {
	  protected:
		MsgHdrBase() {}
		MsgHdrBase( const MsgHdrBase& ){}
		MsgHdrBase &operator=(const MsgHdrBase& ) { assert(0); } 
 	  public:
        int srcRank;
        int tag;
        int count;
		Hermes::Mpi::DataType dataType;
        Hermes::Mpi::Comm comm;
    };

	struct SendBuf {
		int handle;
		Hermes::MemAddr* buf;
	};

    class Entry : public Hermes::Mpi::RequestData {
      public:
        Entry( const Hermes::Mpi::MemAddr& buf, int count, Hermes::Mpi::DataType dataType, int tag,
                Hermes::Mpi::Comm comm, Hermes::Mpi::Request* request ) :
            buf(buf), count(count), dataType(dataType), tag(tag), comm(comm), request(request), doneFlag(false), sendBuf(NULL)
        { }

        Hermes::Mpi::Status status;
        bool isDone() { return doneFlag; }
        bool doneFlag;
        Hermes::Mpi::MemAddr buf;
        int count;
        Hermes::Mpi::DataType dataType;
        int tag;
        Hermes::Mpi::Comm comm;
        Hermes::Mpi::Request* request;
        SendBuf* sendBuf;
    };

    class SendEntryBase : public Entry {
      public:
        SendEntryBase( const Hermes::Mpi::MemAddr& buf, int count, Hermes::Mpi::DataType dataType, int dest, int tag,
                Hermes::Mpi::Comm comm, Hermes::Mpi::Request* request ) : Entry( buf, count, dataType, tag, comm, request), dest(dest) {}
        int dest;
    };

    class RecvEntryBase : public Entry {
      public:
        RecvEntryBase( const Hermes::Mpi::MemAddr& buf, int count, Hermes::Mpi::DataType dataType, int src, int tag,
                Hermes::Mpi::Comm comm, Hermes::Mpi::Request* request ) : Entry( buf, count, dataType, tag, comm, request), src(src) {}
        int src;
    };

    class TestBase {
      public:
        TestBase( bool blocking, Hermes::Callback* callback ) : blocking(blocking), callback(callback) {}
        virtual ~TestBase() {
            delete callback;
        }

        virtual bool isDone() = 0;

        Hermes::Callback* callback;
        bool blocking;
    };

    class TestEntryBase : public TestBase {
      public:
        TestEntryBase( Hermes::Mpi::Request* request, Hermes::Mpi::Status* status, bool blocking, Hermes::Callback* callback ) :
            TestBase( blocking, callback), request(request), status(status) {}

        ~TestEntryBase() {
            delete *request;
            *request = NULL;
        }

        bool isDone() {
            Entry* entry = dynamic_cast<Entry*>(*request);
            *status = entry->status;
            return entry->isDone();
        }
        Hermes::Mpi::Request* request;
        Hermes::Mpi::Status* status;
    };

    class TestallEntryBase : public TestBase {
      public:
        TestallEntryBase( int count, Hermes::Mpi::Request* request, int* flag, Hermes::Mpi::Status* status, bool blocking, Hermes::Callback* callback ) :
            TestBase( blocking, callback ), count(count), request(request), flag(flag), status(status), done(0) { *flag = false; }


        ~TestallEntryBase() {
            for ( int i = 0; i < count; i++ ) {
                delete request[i];
                request[i] = NULL;
            }
        }
        bool isDone() {

            for ( int i = 0; i < count; i++ ) {
                Entry* entry = dynamic_cast<Entry*>(request[i]);
                if ( entry && entry->isDone() ) {
                    status[i] = entry->status;
                    request[i] = NULL;
                    delete entry;
                    ++done;
                }
            }
            *flag = done == count;

            return done == count;
        }

        int done;
        int count;
        Hermes::Mpi::Request* request;
        int* flag;
        Hermes::Mpi::Status* status;
    };

    class TestanyEntryBase : public TestBase {
      public:
        TestanyEntryBase( int count, Hermes::Mpi::Request* request, int* index, int* flag, Hermes::Mpi::Status* status, bool blocking, Hermes::Callback* callback ) :
            TestBase(blocking,callback), count(count), request(request), index(index), flag(flag), status(status) { *flag = false; }

        bool isDone() {
            int done = 0;
            for ( int i = 0; i < count; i++ ) {

                Entry* entry = dynamic_cast<Entry*>( request[i] );
                if ( entry->isDone() ) {
                    *index = i;
                    *flag = true;
                    *status = entry->status;
                    return true;
                }
            }
            return false;
        }

        int count;
        Hermes::Mpi::Request* request;
        int* index;
        int* flag;
        Hermes::Mpi::Status* status;
    };

	Hermes::Misc::Interface& misc() { return *m_misc; }
	virtual void processTest( TestBase* waitEntry, int ) { assert(0); }
	virtual void makeProgress( Hermes::Callback* ) { assert(0); }
	virtual void postRecvBuffer( Hermes::Callback*, int count, int retval ) { assert(0); }
	virtual size_t getMsgHdrSize() { assert(0); }
	virtual void processSendEntry( Hermes::Callback*, SendEntryBase*  ) { assert(0); }

    Hermes::ProcAddr getProcAddr( SendEntryBase* entry ) {
        Hermes::ProcAddr procAddr;
        procAddr.node = os().calcDestNid( entry->dest, entry->comm );
        procAddr.pid = os().calcDestPid( entry->dest, entry->comm );
        return procAddr;
    }

	bool checkMatch( MsgHdrBase* hdr, RecvEntryBase* entry )
	{
		m_dbg.debug(CALL_INFO,1,1,"hdr tag=%#x,%d src=%d comm=%d count=%d datatype=%d\n",hdr->tag,hdr->tag,hdr->srcRank,hdr->comm,hdr->count,hdr->dataType);
		m_dbg.debug(CALL_INFO,1,1,"entry tag=%#x,%d src=%d comm=%d count=%d datatype=%d\n",entry->tag,entry->tag,entry->src,entry->comm,entry->count,entry->dataType);
		if ( entry->tag != Hermes::Mpi::AnyTag && entry->tag != hdr->tag ) {
			m_dbg.debug(CALL_INFO,1,1,"tag no match posted=%#x,%d %#x,%d\n",entry->tag,entry->tag,hdr->tag,hdr->tag);
			return false;
		}
		if ( entry->src != Hermes::Mpi::AnySrc && entry->src != hdr->srcRank ) {
			m_dbg.debug(CALL_INFO,1,1,"rank no match posted=%d %d\n",entry->src,hdr->srcRank);
			return false;
		}
		if ( entry->comm != hdr->comm ) {
			m_dbg.debug(CALL_INFO,1,1,"comm no match posted=%d %d\n",entry->comm,hdr->comm);
			return false;
		}
		if ( entry->count != hdr->count ) {
			m_dbg.debug(CALL_INFO,1,1,"count no match posted=%d %d\n",entry->count,hdr->count);
			return false;
		}
		if ( entry->dataType != hdr->dataType ) {
			m_dbg.debug(CALL_INFO,1,1,"dataType no match posted=%d %d\n",entry->dataType,hdr->dataType);
			return false;
		}
		m_dbg.debug(CALL_INFO,1,1,"match\n");
		return true;
	}

	void selfLinkHandler( Event* e ) {
		SelfEvent* event = static_cast<SelfEvent*>(e);
		m_dbg.debug(CALL_INFO,1,1,"\n");
		(*(event->callback))( event->retval );
		delete e;
	}

	void mallocSendBuffers( Hermes::Callback* callback, int retval ) {
		size_t length = m_numSendBuffers * ( m_shortMsgLength + sizeof(getMsgHdrSize()) );
		m_dbg.debug(CALL_INFO,1,1,"length=%zu\n",length);

		Hermes::Callback* cb = new Hermes::Callback;
		*cb = std::bind( &MpiPt2Pt::mallocRecvBuffers, this, callback, std::placeholders::_1 );
		misc().malloc( &m_sendBuff, length, true, cb );
	}

	void mallocRecvBuffers( Hermes::Callback* callback, int retval ) {
		size_t length = m_numRecvBuffers *  ( m_shortMsgLength + sizeof(getMsgHdrSize()) );
		m_dbg.debug(CALL_INFO,1,1,"length=%zu\n",length);

		m_dbg.debug(CALL_INFO,1,1,"m_sendBuff vaddr=0x%" PRIx64 " backing=%p\n",m_sendBuff.getSimVAddr(), m_sendBuff.getBacking());
		for ( int i = 0; i < m_numSendBuffers; i++ ) {
			size_t offset = i * ( m_shortMsgLength + sizeof(getMsgHdrSize()) );
			m_sendBuffList[i].buf = new Hermes::MemAddr( m_sendBuff.getSimVAddr(offset), m_sendBuff.getBacking(offset) );
			m_dbg.debug(CALL_INFO,1,1,"m_sendBuff[%d] vaddr=0x%" PRIx64 " backing=%p\n", i, m_sendBuffList[i].buf->getSimVAddr(),  m_sendBuffList[i].buf->getBacking());
			m_sendBuffList[i].handle = 1;
		}

		Hermes::Callback* cb = new Hermes::Callback;
		*cb = std::bind( &MpiPt2Pt::postRecvBuffer, this, callback, m_numRecvBuffers, std::placeholders::_1 );
		misc().malloc( &m_recvBuff, length, true, cb );
	}

	void processSendQ( Hermes::Callback* callback, int retval )
	{
		if ( ! m_postedSends.empty() ) {
			m_dbg.debug(CALL_INFO,1,1,"have entry\n");

			SendEntryBase* entry = m_postedSends.front();
			m_postedSends.pop();

			Hermes::Callback* cb = new Hermes::Callback;
			*cb = [=](int) {
				m_dbg.debug(CALL_INFO_LAMBDA,"processSendQ",1,1,"callback\n");
				processSendQ( callback, 0 );
			};

			processSendEntry( cb, entry );

		} else {
			m_dbg.debug(CALL_INFO,1,1,"call callback\n");
			(*callback)(0);
			delete callback;
		}
	}

	void findPostedRecv( MsgHdrBase* hdr, std::function<void(RecvEntryBase*)> callback ) {

		m_dbg.debug(CALL_INFO,1,1,"\n");

		RecvEntryBase* entry = NULL;
		int delay = 0;
		std::deque< RecvEntryBase* >::iterator iter = m_postedRecvs.begin();
		for ( ; iter != m_postedRecvs.end(); ++iter ) {
			if ( checkMatch( hdr, *iter ) ) {
				entry = *iter;
				m_postedRecvs.erase( iter );
				delay += m_matchLatency;
				m_dbg.debug(CALL_INFO,1,1,"found posted recv\n");
				break;
			}
		}
		Hermes::Callback* cb = new Hermes::Callback( [=](int) { callback(entry); } );
		m_selfLink->send( delay ,new SelfEvent(cb) );
	}

	Output  m_dbg;

	Link* m_selfLink;

	size_t m_shortMsgLength;
	int m_numRecvBuffers;
	int m_numSendBuffers;

	Hermes::MemAddr m_recvBuff;

	SendBuf* allocSendBuf() {

		for ( int i = 0; i < m_sendBuffList.size(); i++ ) {
			if ( 1 == m_sendBuffList[i].handle ) {
				m_dbg.debug(CALL_INFO,1,1,"m_sendBuff[%d] vaddr=0x%" PRIx64 " backing=%p\n", i, m_sendBuffList[i].buf->getSimVAddr(),  m_sendBuffList[i].buf->getBacking() );
				m_sendBuffList[i].handle = 0;
				return &m_sendBuffList[i];
			}
		}
		assert(0);
	}

	Hermes::Mpi::Request m_request;
	Hermes::Mpi::Status  m_status;
	Host&   os() { return *m_os; }

	std::deque< RecvEntryBase* > m_postedRecvs;
	std::queue< SendEntryBase* > m_postedSends;

	int m_initLatency;
	int m_sendLatency;
	int m_recvLatency;
	int m_isendLatency;
	int m_irecvLatency;
	int m_testLatency;
	int m_testallLatency;
	int m_testanyLatency;
	int m_matchLatency;
	double m_memcpyLatency;

  private:
	Host*   m_os;
	Hermes::Misc::Interface* m_misc;

	Hermes::MemAddr m_sendBuff;
	std::vector<SendBuf> m_sendBuffList;
};

}
}

#endif
