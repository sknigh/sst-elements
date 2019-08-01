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


#ifndef _H_EMBER_RVMA_PUT_EVENT
#define _H_EMBER_RVMA_PUT_EVENT

#include "emberRvmaEvent.h"

namespace SST {
namespace Ember {

class EmberRvmaPutEvent : public EmberRvmaEvent {

public:
	EmberRvmaPutEvent( RVMA::Interface& api, Output* output,
		Hermes::MemAddr srcAddr, size_t size, Hermes::ProcAddr destProc,
                        RVMA::VirtAddr virtAddr, size_t offset, int* retval ) :
		EmberRvmaEvent( api, output, retval ), m_srcAddr(srcAddr), m_size(size), m_destProc(destProc), 
		m_virtAddr(virtAddr), m_offset(offset) {}

	~EmberRvmaPutEvent() {}

    std::string getName() { return "RvmaPut"; }

    void issue( uint64_t time, RVMA::Callback* callback ) {
        EmberEvent::issue( time );
        m_api.put( m_srcAddr, m_size, m_destProc, m_virtAddr, m_offset, callback );
    }

private:
	Hermes::MemAddr m_srcAddr;
	size_t m_size;
	Hermes::ProcAddr m_destProc;
	RVMA::VirtAddr m_virtAddr;
	size_t m_offset;
};

}
}

#endif
