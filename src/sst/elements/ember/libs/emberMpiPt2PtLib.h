// Copyright 2009-2018 NTESS. Under the terms

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

#ifndef _H_EMBER_MPI_PT2PT_LIB
#define _H_EMBER_MPI_PT2PT_LIB

#include "libs/emberLib.h"
#include "sst/elements/hermes/mpiPt2Pt.h"
#include "mpiPt2Pt/emberMpiInitEv.h"
#include "mpiPt2Pt/emberMpiIsendEv.h"
#include "mpiPt2Pt/emberMpiIrecvEv.h"
#include "mpiPt2Pt/emberMpiTestEv.h"
#include "mpiPt2Pt/emberMpiTestallEv.h"
#include "mpiPt2Pt/emberMpiTestanyEv.h"

using namespace Hermes;

namespace SST {
namespace Ember {

class EmberMpiPt2PtLib : public EmberLib {
  
  public:

    SST_ELI_REGISTER_MODULE(
        EmberMpiPt2PtLib,
        "ember",
        "mpiPt2PtLib",
        SST_ELI_ELEMENT_VERSION(1,0,0),
        "",
        "SST::Ember::EmberMpiPt2PtLib"
    )

    typedef std::queue<EmberEvent*> Queue;

	EmberMpiPt2PtLib( Params& params ) {}

	void init( Queue& q, int* numRanks, int* myRank, int* retval = NULL ) {
		m_output->verbose(CALL_INFO, 2, 0,"\n");
        q.push( new EmberMpiInitEvent( api(), m_output, numRanks, myRank, retval ) );
	}

	void isend( Queue& q, Mpi::MemAddr& buf, int count, Mpi::DataType dataType, int dest, int tag, Mpi::Comm comm, Mpi::Request* request, int* retval = NULL ) 
	{
    	m_output->verbose(CALL_INFO, 2, 0,"\n");
    	q.push( new EmberMpiIsendEvent( api(), m_output, buf, count, dataType, dest, tag, comm, request, retval ) );
	}

	void irecv( Queue& q, Mpi::MemAddr& buf, int count, Mpi::DataType dataType, int src, int tag, Mpi::Comm comm, Mpi::Request* request, int* retval = NULL ) 
	{
    	m_output->verbose(CALL_INFO, 2, 0,"\n");
    	q.push( new EmberMpiIrecvEvent( api(), m_output, buf, count, dataType, src, tag, comm, request, retval ) );
	}

	void wait( Queue& q, Mpi::Request* request, Mpi::Status* status, int* retval = NULL ) {
    	m_output->verbose(CALL_INFO, 2, 0,"\n");
		q.push( new EmberMpiTestEvent( api(), m_output, request, status, true, retval ) );
	}

	void waitall( Queue& q, int count, Mpi::Request* request, int* flag, Mpi::Status* status, int* retval = NULL ) {
    	m_output->verbose(CALL_INFO, 2, 0,"\n");
		q.push( new EmberMpiTestallEvent( api(), m_output, count, request, flag, status, true, retval ) );
	}

	void waitany( Queue& q, int count, Mpi::Request* request, int* indx, int* flag, Mpi::Status* status, int* retval = NULL ) {
    	m_output->verbose(CALL_INFO, 2, 0,"\n");
		q.push( new EmberMpiTestanyEvent( api(), m_output, count, request, indx, flag, status, true, retval ) );
	}

	void test( Queue& q, Mpi::Request* request, Mpi::Status* status, int* retval = NULL ) {
    	m_output->verbose(CALL_INFO, 2, 0,"\n");
		q.push( new EmberMpiTestEvent( api(), m_output, request, status, false, retval ) );
	}

	void testall( Queue& q, int count, Mpi::Request* request, int* flag, Mpi::Status* status, int* retval = NULL ) {
    	m_output->verbose(CALL_INFO, 2, 0,"\n");
		q.push( new EmberMpiTestallEvent( api(), m_output, count, request, flag, status, false, retval ) );
	}

	void testany( Queue& q, int count, Mpi::Request* request, int* indx, int* flag, Mpi::Status* status, int* retval = NULL ) {
    	m_output->verbose(CALL_INFO, 2, 0,"\n");
		q.push( new EmberMpiTestanyEvent( api(), m_output, count, request, indx, flag, status, false, retval ) );
	}


  private:
	Mpi::Interface& api() { return *static_cast<Mpi::Interface*>(m_api); } 
};

}
}

#endif
