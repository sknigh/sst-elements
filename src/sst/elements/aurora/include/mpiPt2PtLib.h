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

namespace SST {
namespace Aurora {

class MpiPt2Pt : public Hermes::Mpi::Interface {

  public:

	MpiPt2Pt(Component* owner, Params& params) : Interface(owner) 
	{
		m_shortMsgLength = params.find<size_t>("shortMsgLength",4096);

    	std::ostringstream tmp;
    	tmp << this << "-AuroraMpiPt2PtLibSelfLink";
    	m_selfLink = owner->configureSelfLink(tmp.str(), "1 ns", new Event::Handler<MpiPt2Pt>(this,&MpiPt2Pt::selfLinkHandler));

    	Params modParams;
		m_misc = dynamic_cast< Hermes::Misc::Interface*>( loadAnonymousSubComponent<Hermes::Interface>( "aurora.misc", "", 0, ComponentInfo::SHARE_NONE, modParams ) );
    	assert(m_misc);
	}

	void setup() {
		m_misc->setup();
	}

	void setOS( Hermes::OS* os ) {
        m_os = static_cast<Host*>(os);
        m_misc->setOS(m_os);
    }

	void send(const Hermes::MemAddr& buf, int count, Mpi::DataType dataType, int dest, int tag,
        Hermes::Mpi::Comm comm, Hermes::Callback* callback )
	{
    	m_dbg.debug(CALL_INFO,1,2,"buf=0x%" PRIx64 " count=%d dataSize=%d dest=%d tag=%d comm=%d\n",
            buf.getSimVAddr(),count,Mpi::sizeofDataType( dataType ), dest, tag, comm );

    	Callback *cb = new Callback( [=](int) {
        	m_dbg.debug(CALL_INFO_LAMBDA,"send",2,2,"back from isend\n");
        	Callback* cb = new Callback( [=](int retval ) {
            	m_dbg.debug(CALL_INFO_LAMBDA,"send",1,2,"return to motif, count=%d dtype=%d dest=%d tag=%d\n",
                    count,dataType,dest,tag);
            	(*callback)(retval);
            	delete callback;
        	});

        	test( &m_request, &m_status, true, cb );
    	});

    	assert( comm == Mpi::CommWorld );
    	isend( buf, count, dataType, dest, tag, Mpi::CommWorld, &m_request, cb );
	}

	void recv(const Hermes::MemAddr& buf, int count, Mpi::DataType dataType, int src, int tag,
        Hermes::Mpi::Comm comm, Hermes::Mpi::Status* status, Hermes::Callback* callback )
	{
    	m_dbg.debug(CALL_INFO,1,2,"buf=0x%" PRIx64 " count=%d dataSize=%d src=%d tag=%d comm=%d\n",
            buf.getSimVAddr(),count, Mpi::sizeofDataType( dataType ), src, tag, comm );

    	Callback* cb = new Callback( [=](int) {
        	m_dbg.debug(CALL_INFO_LAMBDA,"recv",2,2,"back from irecv\n");
        	Callback* cb = new Callback( [=]( int retval ) {
            	m_dbg.debug(CALL_INFO_LAMBDA,"recv",1,2,"return to motif, count=%d dtype=%d src=%d tag=%d\n", count,dataType,src,tag);
            	(*callback)(retval);
            	delete callback;
        	});

        	test( &m_request, &m_status, true, cb );
    	});

    	assert( comm == Mpi::CommWorld );
    	irecv( buf, count, dataType, src, tag, Mpi::CommWorld, &m_request, cb );
	}

  protected:

	template< class ENTRY >
    Hermes::ProcAddr getProcAddr( ENTRY* entry ) {
        Hermes::ProcAddr procAddr;
        procAddr.node = os().calcDestNid( entry->dest, entry->comm );
        procAddr.pid = os().calcDestPid( entry->dest, entry->comm );
        return procAddr;
    }

	template < class HDR, class ENTRY >
	bool checkMatch( HDR* hdr, ENTRY* entry )
	{
    	m_dbg.debug(CALL_INFO,1,2,"hdr tag=%d src=%d comm=%d count=%d datatype=%d\n",hdr->tag,hdr->srcRank,hdr->comm,hdr->count,hdr->dataType);
    	m_dbg.debug(CALL_INFO,1,2,"entry tag=%d src=%d comm=%d count=%d datatype=%d\n",entry->tag,entry->src,entry->comm,entry->count,entry->dataType);
    	if ( entry->tag != Hermes::Mpi::AnyTag && entry->tag != hdr->tag ) {
        	m_dbg.debug(CALL_INFO,1,2,"tag no match posted=%d %d\n",entry->tag,hdr->tag);
        	return false;
    	}
    	if ( entry->src != Hermes::Mpi::AnySrc && entry->src != hdr->srcRank ) {
        	m_dbg.debug(CALL_INFO,1,2,"rank no match posted=%d %d\n",entry->src,hdr->srcRank);
        	return false;
    	}
    	if ( entry->comm != hdr->comm ) {
        	m_dbg.debug(CALL_INFO,1,2,"comm no match posted=%d %d\n",entry->comm,hdr->comm);
        	return false;
    	}
    	if ( entry->count != hdr->count ) {
        	m_dbg.debug(CALL_INFO,1,2,"count no match posted=%d %d\n",entry->count,hdr->count);
        	return false;
    	}
    	if ( entry->dataType != hdr->dataType ) {
        	m_dbg.debug(CALL_INFO,1,2,"dataType no match posted=%d %d\n",entry->dataType,hdr->dataType);
        	return false;
    	}
    	return true;
	}


	 Hermes::Misc::Interface& misc() { return *m_misc; }

	class SelfEvent : public SST::Event { 
	  public:
		SelfEvent( Hermes::Callback* callback, int retval ) : callback(callback), retval(retval) {} 
 		Hermes::Callback* callback;
		int retval;
		NotSerializable(SelfEvent);
	}; 
	void selfLinkHandler( Event* e ) {
		SelfEvent* event = static_cast<SelfEvent*>(e);
		m_dbg.debug(CALL_INFO,1,1,"\n");
		(*(event->callback))( event->retval );
		delete e;
	}

    Link* m_selfLink;

    struct MsgHdrBase {
        int srcRank;
        int tag;
        int count;
		Hermes::Mpi::DataType dataType;
        Hermes::Mpi::Comm comm;
    };

	template < class T >
    class Entry {
      public:
        Entry( const Hermes::Mpi::MemAddr& buf, int count, Hermes::Mpi::DataType dataType, int tag,
                Hermes::Mpi::Comm comm, Hermes::Mpi::Request* request ) :
            buf(buf), count(count), dataType(dataType), tag(tag), comm(comm), request(request), doneFlag(false)
        {
            //printf("%s() %p\n",__func__,this);
        }
        ~Entry() {
            //printf("%s() %p\n",__func__,this);
        }

        Hermes::Mpi::Status status;
        bool isDone() { return doneFlag; }
        bool doneFlag;
        Hermes::Mpi::MemAddr buf;
        int count;
        Hermes::Mpi::DataType dataType;
        int tag;
        Hermes::Mpi::Comm comm;
        Hermes::Mpi::Request* request;
		T	extra;
    };

	template< class T >
    class SendEntryBase : public Entry<T> {
      public:
        SendEntryBase( const Hermes::Mpi::MemAddr& buf, int count, Hermes::Mpi::DataType dataType, int dest, int tag,
                Hermes::Mpi::Comm comm, Hermes::Mpi::Request* request ) : Entry<T>( buf, count, dataType, tag, comm, request), dest(dest) {}
        int dest;
    };

	template< class T >
    class RecvEntryBase : public Entry<T> {
      public:
        RecvEntryBase( const Hermes::Mpi::MemAddr& buf, int count, Hermes::Mpi::DataType dataType, int src, int tag,
                Hermes::Mpi::Comm comm, Hermes::Mpi::Request* request ) : Entry<T>( buf, count, dataType, tag, comm, request), src(src) {}
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

	template< class T >
    class TestEntryBase : public TestBase {
      public:
        TestEntryBase( Hermes::Mpi::Request* request, Hermes::Mpi::Status* status, bool blocking, Hermes::Callback* callback ) :
            TestBase( blocking, callback), request(request), status(status) {}

        ~TestEntryBase() {
            Entry<T>* entry = (Entry<T>*) request->entry;
            delete entry;
            request->entry = NULL;
        }

        bool isDone() {
            Entry<T>* entry = (Entry<T>*) request->entry;
            *status = entry->status;

            return entry->isDone();
        }
        Hermes::Mpi::Request* request;
        Hermes::Mpi::Status* status;
    };

	template< class T >
    class TestallEntryBase : public TestBase {
      public:
        TestallEntryBase( int count, Hermes::Mpi::Request* request, int* flag, Hermes::Mpi::Status* status, bool blocking, Hermes::Callback* callback ) :
            TestBase( blocking, callback ), count(count), request(request), flag(flag), status(status), done(0) { *flag = false; }


        ~TestallEntryBase() {
            for ( int i = 0; i < count; i++ ) {
                Entry<T>* entry = (Entry<T>*) request[i].entry;
                delete entry;
                request[i].entry = NULL;
            }
        }
        bool isDone() {

            for ( int i = 0; i < count; i++ ) {
                Entry<T>* entry = (Entry<T>*) request[i].entry;
                if ( entry && entry->isDone() ) {
                    status[i] = entry->status;
                    request[i].entry = NULL;
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

	template< class T >
    class TestanyEntryBase : public TestBase {
      public:
        TestanyEntryBase( int count, Hermes::Mpi::Request* request, int* index, int* flag, Hermes::Mpi::Status* status, bool blocking, Hermes::Callback* callback ) :
            TestBase(blocking,callback), count(count), request(request), index(index), flag(flag), status(status) { *flag = false; }

        bool isDone() {
            int done = 0;
            for ( int i = 0; i < count; i++ ) {

                Entry<T>* entry = (Entry<T>*) request[i].entry;
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

    Output  m_dbg;

	size_t m_shortMsgLength;


    Hermes::Mpi::Request m_request;
    Hermes::Mpi::Status  m_status;
    Host&   os() { return *m_os; }

  private:
    Host*   m_os;
	Hermes::Misc::Interface* m_misc;
};

}
}

#endif
