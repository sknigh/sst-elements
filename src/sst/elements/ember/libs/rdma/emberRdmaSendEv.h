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


#ifndef _H_EMBER_LIBS_RDMA_SEND_EVENT
#define _H_EMBER_LIBS_RDMA_SEND_EVENT

#include "emberRdmaEvent.h"

namespace SST {
namespace Ember {

class EmberRdmaSendEvent : public EmberRdmaEvent {

public:
	EmberRdmaSendEvent( RDMA::Interface& api, Output* output,
			RDMA::ProcAddr& procAddr, RDMA::RqId rqId, Hermes::MemAddr& src, size_t length, int* retval ) :
		EmberRdmaEvent( api, output, retval ), procAddr(procAddr), rqId(rqId), srcAddr(src), length(length) {
			m_output->debug(CALL_INFO, 2, EVENT_MASK, "node=%d pid=%d rdId=%d addr=0x%" PRIx64 " backing=%p length=%zu\n",
			   	procAddr.node, procAddr.pid, (int) rqId, srcAddr.getSimVAddr(), srcAddr.getBacking(), length );
		}

	~EmberRdmaSendEvent() {}

    std::string getName() { return "RdmaSend"; }

    void issue( uint64_t time, RDMA::Callback* callback ) {
        EmberEvent::issue( time );
		m_output->debug(CALL_INFO, 2, EVENT_MASK, "%s node=%d pid=%d rdId=%d addr=0x%" PRIx64 " backing=%p length=%zu\n",
			   	getName().c_str(), procAddr.node, procAddr.pid, (int) rqId, srcAddr.getSimVAddr(), srcAddr.getBacking(), length );

        api.send( procAddr, rqId, srcAddr, length, callback );
    }

private:
	RDMA::ProcAddr procAddr;
	RDMA::RqId rqId;
	Hermes::MemAddr srcAddr;
	size_t length;
};

}
}

#endif
