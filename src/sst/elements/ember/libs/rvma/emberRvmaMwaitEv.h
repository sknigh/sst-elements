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


#ifndef _H_EMBER_RVMA_MWAIT_EVENT
#define _H_EMBER_RVMA_MWAIT_EVENT

#include "emberRvmaEvent.h"

namespace SST {
namespace Ember {

class EmberRvmaMwaitEvent : public EmberRvmaEvent {

public:
	EmberRvmaMwaitEvent( RVMA::Interface& api, Output* output, Hermes::RVMA::Completion* completion, int* retval ) :
		EmberRvmaEvent( api, output ), m_completion(completion) {}

	~EmberRvmaMwaitEvent() {}

    std::string getName() { return "RvmaMwait"; }

    void issue( uint64_t time, RVMA::Callback* callback ) {
        EmberEvent::issue( time );
        m_api.mwait( m_completion, callback );
    }

private:
	Hermes::RVMA::Completion* m_completion;
};

}
}

#endif
