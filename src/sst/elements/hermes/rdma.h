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

#ifndef _H_HERMES_RDMA_INTERFACE
#define _H_HERMES_RDMA_INTERFACE

#include <assert.h>

#include "hermes.h"

namespace SST {
namespace Hermes {
namespace RDMA {


typedef Hermes::Callback Callback;

typedef uint64_t Addr; 
typedef uint16_t RecvBufId;
typedef uint16_t RqId;
typedef uint16_t MemRegionId;

struct Status {
	Hermes::ProcAddr procAddr;
	size_t length;
	Hermes::MemAddr addr;
};

class Interface : public Hermes::Interface {
  public:
    Interface( ComponentId_t id ) : Hermes::Interface(id) {}
    Interface( Component* parent ) : Hermes::Interface(parent) {}
	virtual ~Interface() {}
	virtual void createRQ( RDMA::RqId, Callback* ) = 0;
	virtual void postRecv( RDMA::RqId, Hermes::MemAddr&, size_t length, RDMA::RecvBufId*, Callback* ) = 0;
	virtual void send( Hermes::ProcAddr, RDMA::RqId, Hermes::MemAddr& src, size_t length, Callback* ) = 0;
	virtual void checkRQ( RDMA::RqId, RDMA::Status*, bool blocking, Callback* ) = 0;
	virtual void registerMem( Hermes::MemAddr& addr, size_t length, MemRegionId*, Callback* ) = 0;
	virtual void read( Hermes::ProcAddr, Hermes::MemAddr& destAddr, RDMA::Addr srcAddr, size_t length, Callback* ) = 0; 
	virtual void write( Hermes::ProcAddr, RDMA::Addr destAddr, Hermes::MemAddr& srcAddr, size_t length, Callback* ) = 0; 
};

}
}
}

#endif

