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


#ifndef _H_EMBER_RVMA_POST_BUFFER_EVENT
#define _H_EMBER_RVMA_POST_BUFFER_EVENT

#include "emberRvmaEvent.h"

namespace SST {
namespace Ember {

class EmberRvmaPostBufferEvent : public EmberRvmaEvent {

public:
	EmberRvmaPostBufferEvent( RVMA::Interface& api, Output* output,
		Hermes::MemAddr addr, size_t size, Hermes::RVMA::Completion* completion , RVMA::Window window, int* retval ) :
		EmberRvmaEvent( api, output, retval ), m_addr(addr), m_size(size), m_completion(completion), m_window(window) {}

	~EmberRvmaPostBufferEvent() {}

    std::string getName() { return "RvmaPostBuffer"; }

    void issue( uint64_t time, RVMA::Callback* callback ) {
        EmberEvent::issue( time );
        m_api.postBuffer( m_addr, m_size, m_completion, m_window, callback );
    }

private:
	Hermes::MemAddr m_addr;
	size_t m_size;
	Hermes::RVMA::Completion* m_completion;
	RVMA::Window m_window;
};

}
}

#endif
