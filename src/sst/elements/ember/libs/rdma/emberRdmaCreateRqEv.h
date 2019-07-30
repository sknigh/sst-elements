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


#ifndef _H_EMBER_LIBS_RDMA_CREATE_RQ_EVENT
#define _H_EMBER_LIBS_RDMA_CREATE_RQ_EVENT

#include "emberRdmaEvent.h"

namespace SST {
namespace Ember {

class EmberRdmaCreateRqEvent : public EmberRdmaEvent {

public:
	EmberRdmaCreateRqEvent( RDMA::Interface& api, Output* output, 
			RDMA::RqId rqId, int* retval ) :
		EmberRdmaEvent( api, output, retval ), rqId(rqId) {}

	~EmberRdmaCreateRqEvent() {}

    std::string getName() { return "RdmaCreateRQ"; }

    void issue( uint64_t time, RDMA::Callback* callback ) {
        EmberEvent::issue( time );
        api.createRQ( rqId, callback );
    }

private:
	RDMA::RqId rqId;
};

}
}

#endif
