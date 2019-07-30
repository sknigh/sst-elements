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


#ifndef _H_EMBER_LIBS_MPI_PT2PT_IRECV_EVENT
#define _H_EMBER_LIBS_MPI_PT2PT_IRECV_EVENT

#include "emberMpiPt2PtEvent.h"

namespace SST {
namespace Ember {

class EmberMpiIrecvEvent : public EmberMpiPt2PtEvent {

public:
	EmberMpiIrecvEvent( Mpi::Interface& api, Output* output,
			Mpi::MemAddr& buf, int count, Mpi::DataType dataType, int src, int tag, Mpi::Comm comm, Mpi::Request* request, int* retval ) :
		EmberMpiPt2PtEvent( api, output, retval ), buf(buf), count(count), dataType(dataType), src(src), tag(tag), comm(comm), request(request) {}

	~EmberMpiIrecvEvent() {}

    std::string getName() { return "MpiIrecv"; }

    void issue( uint64_t time, Mpi::Callback* callback ) {
        EmberEvent::issue( time );
        api.irecv( buf, count, dataType, src, tag, comm, request, callback );
    }

private:
	Mpi::MemAddr& buf;
	int count;
	Mpi::DataType dataType;
	int src;
	int tag;
   	Mpi::Comm comm;
	Mpi::Request* request; 
};

}
}

#endif
