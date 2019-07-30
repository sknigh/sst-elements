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


#ifndef _H_EMBER_RVMA_WIN_GET_EPOCH_EVENT
#define _H_EMBER_RVMA_WIN_GET_EPOCH_EVENT

#include "emberRvmaEvent.h"

namespace SST {
namespace Ember {

class EmberRvmaWinGetEpochEvent : public EmberRvmaEvent {

public:
	EmberRvmaWinGetEpochEvent( RVMA::Interface& api, Output* output, RVMA::Window window, int* epoch, int* retval ) :
		EmberRvmaEvent( api, output, retval ), m_window(window), m_epoch(epoch) {}

	~EmberRvmaWinGetEpochEvent() {}

    std::string getName() { return "RvmaWinGetEpoch"; }

    void issue( uint64_t time, RVMA::Callback* callback ) {
        EmberEvent::issue( time );
        m_api.winGetEpoch( m_window, m_epoch, callback );
    }

private:
	RVMA::Window m_window;
	int* m_epoch;
};

}
}

#endif
