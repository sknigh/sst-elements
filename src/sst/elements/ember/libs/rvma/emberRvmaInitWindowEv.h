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


#ifndef _H_EMBER_RVMA_INIT_WINDOW_EVENT
#define _H_EMBER_RVMA_INIT_WINDOW_EVENT

#include "emberRvmaEvent.h"

namespace SST {
namespace Ember {

class EmberRvmaInitWindowEvent : public EmberRvmaEvent {

public:
	EmberRvmaInitWindowEvent( RVMA::Interface& api, Output* output,
		RVMA::VirtAddr addr, size_t threshold , RVMA::EpochType type, RVMA::Window* window ) :
		EmberRvmaEvent( api, output ), m_addr(addr), m_threshold(threshold), m_type(type), m_window(window) {}

	~EmberRvmaInitWindowEvent() {}

    std::string getName() { return "RvmaInitWindow"; }

    void issue( uint64_t time, RVMA::Callback* callback ) {
        EmberEvent::issue( time );
        m_api.initWindow( m_addr, m_threshold, m_type, m_window, callback );
    }

private:
	RVMA::VirtAddr m_addr;
	size_t m_threshold;
    RVMA::EpochType m_type;
	RVMA::Window* m_window;
};

}
}

#endif
