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

#ifndef _H_HERMES_RVMA_INTERFACE
#define _H_HERMES_RVMA_INTERFACE

#include <assert.h>

#include "hermes.h"

namespace SST {
namespace Hermes {
namespace RVMA {


typedef Hermes::Callback Callback;

enum EpochType { Byte, Op };
typedef int Window;
typedef uint64_t VirtAddr; 

struct Completion {
	Completion() : count(0)  {}
	Hermes::MemAddr addr;
	size_t			count; //can be bytes or operations
};	

class Interface : public Hermes::Interface {
  public:
    Interface( ComponentId_t id ) : Hermes::Interface(id) {}
    Interface( Component* parent ) : Hermes::Interface(parent) {}
	virtual ~Interface() {}
	virtual void initWindow( RVMA::VirtAddr, size_t threshold , RVMA::EpochType, RVMA::Window*, Callback* ) = 0;
	virtual void closeWindow( RVMA::Window, Callback* ) = 0;
	virtual void winIncEpoch( RVMA::Window, Callback* ) = 0;
	virtual void winGetEpoch( RVMA::Window, int*, Callback* ) = 0;
	virtual void winGetBufPtrs( RVMA::Window, Completion*, int count, Callback* ) = 0;
	virtual void put( Hermes::MemAddr, size_t size, ProcAddr dest,  RVMA::VirtAddr, size_t offset, Completion*, Callback* ) = 0;

	virtual void postOneTimeBuffer( RVMA::VirtAddr, size_t threshold, RVMA::EpochType, Hermes::MemAddr, size_t size, Completion*, Callback* ) = 0;

	virtual void postBuffer( Hermes::MemAddr, size_t size, Completion*, RVMA::Window, Callback* ) = 0;
	virtual void mwait( Completion*, Callback* ) = 0;

};

}
}
}

#endif

