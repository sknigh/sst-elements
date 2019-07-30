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


#ifndef _H_EMBER_LIBS_MPI_PT2PT_INIT_EVENT
#define _H_EMBER_LIBS_MPI_PT2PT_INIT_EVENT

#include "emberMpiPt2PtEvent.h"

namespace SST {
namespace Ember {

class EmberMpiInitEvent : public EmberMpiPt2PtEvent {

public:
	EmberMpiInitEvent( Mpi::Interface& api, Output* output, int* numRanks, int* myRank, int* retval ) :
		EmberMpiPt2PtEvent( api, output, retval ), numRanks(numRanks), myRank(myRank) {}

	~EmberMpiInitEvent() {}

    std::string getName() { return "MpiInit"; }

    void issue( uint64_t time, Mpi::Callback* callback ) {
        EmberEvent::issue( time );
        api.init( numRanks, myRank, callback );
    }

private:
	int* numRanks;
	int* myRank;
};

}
}

#endif
