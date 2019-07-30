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


#ifndef _H_EMBER_LIBS_RDMA_CHECK_RQ_EVENT
#define _H_EMBER_LIBS_RDMA_CHECK_RQ_EVENT

#include "emberRdmaEvent.h"

namespace SST {
namespace Ember {

class EmberRdmaCheckRqEvent : public EmberRdmaEvent {

public:
	EmberRdmaCheckRqEvent( RDMA::Interface& api, Output* output,
			RDMA::RqId rqId, RDMA::Status* status, bool blocking, int* retval ) :
		EmberRdmaEvent( api, output, retval ), rqId(rqId), status(status), blocking(blocking) {}

	~EmberRdmaCheckRqEvent() {}

    std::string getName() { return "RdmaCheckRQ"; }

    void issue( uint64_t time, RDMA::Callback* callback ) {
        EmberEvent::issue( time );
        api.checkRQ( rqId, status, blocking, callback );
    }

private:
	RDMA::RqId rqId;
	RDMA::Status* status;
	bool blocking;
};

}
}

#endif
