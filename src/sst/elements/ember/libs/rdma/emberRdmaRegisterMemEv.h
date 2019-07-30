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


#ifndef _H_EMBER_LIBS_RDMA_REGISTER_MEM_EVENT
#define _H_EMBER_LIBS_RDMA_REGISTER_MEM_EVENT

#include "emberRdmaEvent.h"

namespace SST {
namespace Ember {

class EmberRdmaRegisterMemEvent : public EmberRdmaEvent {

public:
	EmberRdmaRegisterMemEvent( RDMA::Interface& api, Output* output,
			Hermes::MemAddr& addr, size_t length, RDMA::MemRegionId* id, int* retval ) :
		EmberRdmaEvent( api, output, retval ), addr(addr), length(length), id(id) {}

	~EmberRdmaRegisterMemEvent() {}

    std::string getName() { return "RdmaRegisterMem"; }

    void issue( uint64_t time, RDMA::Callback* callback ) {
        EmberEvent::issue( time );
        api.registerMem( addr, length, id, callback );
    }

private:
	 Hermes::MemAddr addr;
	 size_t length;
	 RDMA::MemRegionId* id;
};

}
}

#endif
