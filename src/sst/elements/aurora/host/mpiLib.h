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


#ifndef COMPONENTS_AURORA_HOST_MPILIB_H
#define COMPONENTS_AURORA_HOST_MPILIB_H

#include <sst/core/output.h>

#include "host.h"
#include "sst/elements/hermes/msgapi.h"
#include "sst/elements/hermes/mpiPt2Pt.h"
#include "collectives/tree.h" 
#include "collectives/barrier.h" 

namespace SST {
namespace Aurora {

class MpiLib : public Hermes::MP::Interface
{
  public:
    SST_ELI_REGISTER_SUBCOMPONENT_DERIVED(
        MpiLib,
        "aurora",
        "mpiLib",
        SST_ELI_ELEMENT_VERSION(1,0,0),
        "",
        SST::Aurora::MpiLib 
    )

    SST_ELI_DOCUMENT_PARAMS(
        {"verboseLevel","Sets the level of debug verbosity",""},
        {"verboseMask","Sets the debug mask",""},
    )

    MpiLib( Component* owner, Params& ) : Interface(owner) { assert(0); }
    MpiLib( ComponentId_t, Params& );
    ~MpiLib() {}

    std::string getName()        { return "AuroraMpi"; }
    std::string getType()        { return "mpi"; }

    void setup() {
        char buffer[100];
        snprintf(buffer,100,"@t:%d:%d:Aurora::MpiLib::@p():@l ", m_os->getNodeNum(), m_os->getRank());
        m_dbg.setPrefix(buffer);
        m_pt2pt->setup();
    }

    void setOS( Hermes::OS* os ) {
        m_os = static_cast<Host*>(os);
        assert( m_pt2pt );
        m_pt2pt->setOS(m_os);
    }

	void init(MP::Functor* functor) { 
		m_dbg.debug(CALL_INFO,1,1,"\n");
		Hermes::Callback* cb = new Hermes::Callback;
		*cb = [=](int retval ) {
			m_dbg.debug(CALL_INFO,1,1,"size=%d rank=%d\n",m_size,m_rank);
			m_treeInfo = new Collectives::Tree( m_rank, m_size, 10, 8 );
			if ( (*functor)( retval ) ) {
        		delete functor;
			}
		};
		m_pt2pt->init( &m_size, &m_rank, cb ); 
	}

	void fini(MP::Functor* functor) { 
		m_dbg.debug(CALL_INFO,1,1,"my_parent=%d num_children=%d\n",
				m_treeInfo->parent(),m_treeInfo->numChildren());

		Hermes::Callback* cb = new Hermes::Callback;
		*cb = [=](int retval ) {
			if ( (*functor)( retval ) ) {
        		delete functor;
			}
		};

		m_barrier->start( m_treeInfo, Mpi::CommWorld, cb );
	}

	void rank(MP::Communicator group, MP::RankID* rank, MP::Functor* functor) { 
		m_dbg.debug(CALL_INFO,1,1,"rank=%d\n",m_rank);
		*rank = m_rank;
		assert(m_retFunctor==NULL);
		m_retFunctor = functor;
		m_selfLink->send(0,NULL);
	}
	void size(MP::Communicator group, int* size, MP::Functor* functor) { 
		m_dbg.debug(CALL_INFO,1,1,"size=%d\n",m_size);
		*size= m_size;
		assert(m_retFunctor==NULL);
		m_retFunctor = functor;
		m_selfLink->send(0,NULL);
   	}

	int sizeofDataType( MP::PayloadDataType datatype ) { 
		return Mpi::sizeofDataType(convertDataType(datatype) );
	}

	void recv(const Hermes::MemAddr&, uint32_t count, MP::PayloadDataType, MP::RankID source, uint32_t tag, 
			MP::Communicator, MP::MessageResponse*, MP::Functor*); 
	void send(const Hermes::MemAddr& payload, uint32_t count, MP::PayloadDataType dtype, MP::RankID dest, uint32_t tag,
			MP::Communicator group, MP::Functor* );

  private:

    void selfLinkHandler( Event* event ) {
		assert( m_retFunctor );
		m_dbg.debug(CALL_INFO,1,1,"\n");
		if ( (*m_retFunctor)( 0 ) ) {
       		delete m_retFunctor;
		}
		m_retFunctor = NULL;
	}

	Mpi::DataType convertDataType( MP::PayloadDataType datatype ) {
		switch(datatype) {
		  case Hermes::MP::CHAR:
			return Mpi::Char;
		  case Hermes::MP::INT:
			return Mpi::Int;
		  case Hermes::MP::LONG:
			return Mpi::Long;
		  case Hermes::MP::DOUBLE:
			return Mpi::Double;
		  case Hermes::MP::FLOAT:
			return Mpi::Float;
		  case Hermes::MP::COMPLEX:
		  default:
			assert(0);
		}
	}

	Link* m_selfLink;

	Collectives::Tree* m_treeInfo;
	Collectives::Barrier* m_barrier;

	MP::Functor* m_retFunctor;
    Hermes::Mpi::Interface& mpi() { return *m_pt2pt; }
    Output  m_dbg;
    Host*   m_os;
	Hermes::Mpi::Interface* m_pt2pt;
	int m_size;
	int m_rank;
	Hermes::Mpi::Request m_request;
	Hermes::Mpi::Status  m_status;
};

}
}

#endif
