// Copyright 2009-2018 NTESS. Under the terms

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

#ifndef _H_EMBER_RVMA_LIB
#define _H_EMBER_RVMA_LIB

#include <sst/elements/hermes/rvma.h>
#include "libs/emberLib.h"
#include "rvma/emberRvmaInitWindowEv.h"
#include "rvma/emberRvmaCloseWindowEv.h"
#include "rvma/emberRvmaPostBufferEv.h"
#include "rvma/emberRvmaWinIncEpochEv.h"
#include "rvma/emberRvmaWinGetEpochEv.h"
#include "rvma/emberRvmaWinGetBufPtrsEv.h"
#include "rvma/emberRvmaPutEv.h"
#include "rvma/emberRvmaMwaitEv.h"

using namespace Hermes;

namespace SST {
namespace Ember {

class EmberRvmaLib : public EmberLib {
  
  public:

    SST_ELI_REGISTER_MODULE(
        EmberRvmaLib,
        "ember",
        "rvmaLib",
        SST_ELI_ELEMENT_VERSION(1,0,0),
        "",
        "SST::Ember::EmberRvmaLib"
    )

    typedef std::queue<EmberEvent*> Queue;

	EmberRvmaLib( Params& params ) {}

	inline void initWindow( Queue&, RVMA::VirtAddr, size_t, RVMA::EpochType, RVMA::Window* ); 
	inline void closeWindow( Queue&, RVMA::Window ); 
	inline void postBuffer( Queue&, Hermes::MemAddr, size_t, Hermes::RVMA::Completion*, RVMA::Window, int* retval = NULL);
	inline void winIncEpoch( Queue&, RVMA::Window, int* retval = NULL );
	inline void winGetEpoch( Queue&, RVMA::Window, int* epoch, int* retval = NULL );
	inline void winGetBufPtrs( Queue&, RVMA::Window, Hermes::RVMA::Completion*, int count, int* retval = NULL );
	inline void put( Queue&, Hermes::MemAddr srcAddr, size_t, Hermes::ProcAddr, RVMA::VirtAddr, 
			size_t offset, Hermes::RVMA::Completion*, int* retval = NULL );
	inline void mwait( Queue&, Hermes::RVMA::Completion* completion, int* retval = NULL );

  private:
	RVMA::Interface& api() { return *static_cast<RVMA::Interface*>(m_api); } 
};

void EmberRvmaLib::initWindow( Queue& q, RVMA::VirtAddr addr, size_t threshold , RVMA::EpochType type, RVMA::Window* window )
{
	m_output->verbose(CALL_INFO, 2, 0,"\n");
	q.push( new EmberRvmaInitWindowEvent( api(), m_output, addr, threshold, type, window ) );
}

void EmberRvmaLib::closeWindow( Queue& q, RVMA::Window window )
{
	m_output->verbose(CALL_INFO, 2, 0,"\n");
	q.push( new EmberRvmaCloseWindowEvent( api(), m_output, window ) );
}

void EmberRvmaLib::winIncEpoch( Queue& q, RVMA::Window window, int* retval )
{
	m_output->verbose(CALL_INFO, 2, 0,"\n");
	q.push( new EmberRvmaWinIncEpochEvent( api(), m_output, window, retval ) );
}

void EmberRvmaLib::winGetEpoch( Queue& q, RVMA::Window window, int* epoch, int* retval )
{
	m_output->verbose(CALL_INFO, 2, 0,"\n");
	q.push( new EmberRvmaWinGetEpochEvent( api(), m_output, window, epoch, retval ) );
}

void EmberRvmaLib::winGetBufPtrs( Queue& q, RVMA::Window window, Hermes::RVMA::Completion* ptrs, int count, int* retval )
{
	m_output->verbose(CALL_INFO, 2, 0,"\n");
	q.push( new EmberRvmaWinGetBufPtrsEvent( api(), m_output, window, ptrs, count, retval ) );
}

void EmberRvmaLib::postBuffer( Queue& q, Hermes::MemAddr addr, size_t size,
		Hermes::RVMA::Completion* completion, RVMA::Window window, int* retval )
{
	m_output->verbose(CALL_INFO, 2, 0,"\n");
	q.push( new EmberRvmaPostBufferEvent( api(), m_output, addr, size, completion, window, retval ) );
}

void EmberRvmaLib::put( Queue& q, Hermes::MemAddr srcAddr, size_t size, Hermes::ProcAddr destProc,
						RVMA::VirtAddr virtAddr, size_t offset, Hermes::RVMA::Completion* completion, int* retval )
{
	m_output->verbose(CALL_INFO, 2, 0,"\n");
	q.push( new EmberRvmaPutEvent( api(), m_output, srcAddr, size, destProc, virtAddr, offset, completion, retval ) );
}

void EmberRvmaLib::mwait( Queue& q, Hermes::RVMA::Completion* completion, int* retval  )
{
	m_output->verbose(CALL_INFO, 2, 0,"\n");
	q.push( new EmberRvmaMwaitEvent( api(), m_output, completion, retval ) );
}

}
}

#endif
