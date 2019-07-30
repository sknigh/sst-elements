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


#ifndef _H_EMBER_LIBS_MPI_PT2PT_TESTALL_EVENT
#define _H_EMBER_LIBS_MPI_PT2PT_TESTALL_EVENT

#include "emberMpiPt2PtEvent.h"

namespace SST {
namespace Ember {

class EmberMpiTestallEvent : public EmberMpiPt2PtEvent {

public:
	EmberMpiTestallEvent( Mpi::Interface& api, Output* output,
		    int count, Hermes::Mpi::Request* request, int* flag, Hermes::Mpi::Status* status, bool blocking, int* retval ) :
		EmberMpiPt2PtEvent( api, output, retval ), count(count), request(request), flag(flag), status(status), blocking(blocking) {}

	~EmberMpiTestallEvent() {}

    std::string getName() { return "MpiTestall"; }

    void issue( uint64_t time, Mpi::Callback* callback ) {
        EmberEvent::issue( time );
        api.testall( count, request, flag, status, blocking, callback );
    }

private:
	int count;
	Hermes::Mpi::Request* request;
	int* flag;
	Hermes::Mpi::Status*  status;
	bool blocking;
};

}
}

#endif
