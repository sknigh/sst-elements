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

#ifndef _H_EMBER_RDMA_LIB
#define _H_EMBER_RDMA_LIB

#include "libs/emberLib.h"
#include "libs/rdma/emberRdmaCreateRqEv.h"
#include "libs/rdma/emberRdmaPostRecvEv.h"
#include "libs/rdma/emberRdmaSendEv.h"
#include "libs/rdma/emberRdmaCheckRqEv.h"
#include "libs/rdma/emberRdmaRegisterMemEv.h"
#include "libs/rdma/emberRdmaReadEv.h"
#include "libs/rdma/emberRdmaWriteEv.h"

using namespace Hermes;

namespace SST {
namespace Ember {

class EmberRdmaLib : public EmberLib {
  
  public:

    SST_ELI_REGISTER_MODULE(
        EmberRdmaLib,
        "ember",
        "rdmaLib",
        SST_ELI_ELEMENT_VERSION(1,0,0),
        "",
        "SST::Ember::EmberRdmaLib"
    )

    typedef std::queue<EmberEvent*> Queue;

	EmberRdmaLib( Params& params ) {}

	inline void createRQ( Queue& q, RDMA::RqId rqId, int* retval = NULL ) {
		q.push( new EmberRdmaCreateRqEvent( api(), m_output, rqId, retval ) );
	}
	inline void postRecv( Queue& q, RDMA::RqId id, Hermes::MemAddr addr, size_t length, RDMA::RecvBufId* bufId, int* retval = NULL) {
		q.push( new EmberRdmaPostRecvEvent( api(), m_output, id, addr, length, bufId, retval  ) );
	}
	inline void send( Queue& q, RDMA::ProcAddr procAddr, RDMA::RqId rqId, Hermes::MemAddr src, size_t length, int* retval = NULL ) {
		q.push( new EmberRdmaSendEvent( api(), m_output, procAddr, rqId, src, length, retval ) );
	}
	inline void checkRQ( Queue& q, RDMA::RqId rqId, RDMA::Status* status, bool blocking, int* retval = NULL ) {
		q.push( new EmberRdmaCheckRqEvent( api(), m_output, rqId, status, blocking, retval ) );
	}
	inline void registerMem( Queue& q, Hermes::MemAddr addr, size_t length, RDMA::MemRegionId* id, int* retval = NULL ) {
		q.push( new EmberRdmaRegisterMemEvent( api(), m_output, addr, length, id, retval ) );
	}
	inline void read( Queue& q, RDMA::ProcAddr procAddr, Hermes::MemAddr destAddr, RDMA::Addr srcAddr, size_t length, int* retval = NULL ) {
		q.push( new EmberRdmaReadEvent( api(), m_output, procAddr, destAddr, srcAddr, length, retval  ) );
	}
	inline void write( Queue& q, RDMA::ProcAddr procAddr, RDMA::Addr destAddr, Hermes::MemAddr srcAddr, size_t length, int* retval = NULL ) {
		q.push( new EmberRdmaWriteEvent( api(), m_output, procAddr, destAddr, srcAddr, length, retval  ) );
	}

  private:
	RDMA::Interface& api() { return *static_cast<RDMA::Interface*>(m_api); } 
};

}
}

#endif
