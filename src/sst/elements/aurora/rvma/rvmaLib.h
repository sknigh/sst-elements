// Copyright 2013-2018 NTESS. Under the terms
// of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.
//
// Copyright (c) 2013-2018, NTESS
// All rights reserved.
//
// Portions are copyright of other developers:
// See the file CONTRIBUTORS.TXT in the top level directory
// the distribution for more information.
//
// This file is part of the SST software package. For license
// information, see the LICENSE file in the top level directory of the
// distribution.


#ifndef COMPONENTS_AURORA_RVMA_LIB_H
#define COMPONENTS_AURORA_RVMA_LIB_H

#include <vector>
#include "sst/elements/hermes/rvma.h"
#include "include/hostLib.h"

#include "rvma/rvmaNicCmds.h"
#include "rvma/rvmaNicResp.h"

namespace SST {
namespace Aurora {
namespace RVMA {

class RvmaLib : public HostLib< Hermes::RVMA::Interface, NicCmd, RetvalResp > 
{
  public:
    SST_ELI_REGISTER_SUBCOMPONENT_DERIVED(
        RvmaLib,
        "aurora",
        "rvmaLib",
        SST_ELI_ELEMENT_VERSION(1,0,0),
        "",
        SST::Hermes::Interface
    )

    SST_ELI_DOCUMENT_PARAMS(
        {"verboseLevel","Sets the level of debug verbosity",""},
        {"verboseMask","Sets the debug mask",""},
    )

	RvmaLib( ComponentId_t, Params& );
	~RvmaLib() {} 

	std::string getName()        { return "Rvma"; }
	std::string getType()        { return "rvma"; }

	void initWindow( Hermes::RVMA::VirtAddr addr, size_t threshold, Hermes::RVMA::EpochType type, 
			Hermes::RVMA::Window* window, Hermes::Callback* callback )
	{
    	dbg().debug(CALL_INFO,1,2,"\n");
    	NicCmd* cmd = new InitWindowCmd( addr, threshold, type );
    	setFiniCallback( new Callback(std::bind( &RvmaLib::initWindowFini, this, window, std::placeholders::_1 ) ));

    	doEnter( cmd, callback );
	}
	
	void closeWindow( Hermes::RVMA::Window window, Hermes::Callback* callback )
	{
    	dbg().debug(CALL_INFO,1,2,"\n");
    	NicCmd* cmd = new CloseWindowCmd( window );
		setRetvalCallback();
    	doEnter( cmd, callback );
	}

	void winIncEpoch( Hermes::RVMA::Window window, Hermes::Callback* callback)
	{
    	dbg().debug(CALL_INFO,1,2,"\n");
    	NicCmd* cmd = new WinIncEpochCmd( window );
    	setRetvalCallback();
		doEnter( cmd, callback );
	}

    void winGetEpoch( Hermes::RVMA::Window window, int* epoch, Hermes::Callback* callback ) 
	{
		dbg().debug(CALL_INFO,1,2,"\n");
		NicCmd* cmd = new WinGetEpochCmd( window );
		setFiniCallback( new Callback( std::bind( &RvmaLib::winGetEpochFini, this, epoch, std::placeholders::_1 ) ) );
		doEnter( cmd, callback );
	}

    void winGetBufPtrs( Hermes::RVMA::Window window, Hermes::RVMA::Completion* ptr, int count, Hermes::Callback* callback )
	{
		dbg().debug(CALL_INFO,1,2,"count=%d\n",count);
		NicCmd* cmd = new WinGetBufPtrsCmd( window, count );
    	setFiniCallback( new Callback( std::bind( &RvmaLib::winGetBufPtrsFini, this, ptr, std::placeholders::_1 ) ) );
		doEnter( cmd, callback );
	}
	
	void postBuffer( Hermes::MemAddr addr, size_t size, Hermes::RVMA::Completion* completion,
				Hermes::RVMA::Window window, Hermes::Callback* callback )
	{
		dbg().debug(CALL_INFO,1,2,"window=%d size=%zu completion=%p\n",window,size,completion);
		NicCmd* cmd = new PostBufferCmd( addr, size, completion, window );
		setRetvalCallback();
		doEnter( cmd, callback );
	}

	void postOneTimeBuffer( Hermes::RVMA::VirtAddr vAddr, size_t threshold, Hermes::RVMA::EpochType type,
				Hermes::MemAddr bufAddr, size_t size, Hermes::RVMA::Completion* completion, Hermes::Callback* callback )  
	{
		dbg().debug(CALL_INFO,1,2,"\n");
		NicCmd* cmd = new PostOneTimeBufferCmd( vAddr, threshold, type, bufAddr, size, completion );
		setRetvalCallback();
		doEnter( cmd, callback );
	}

    void put( const Hermes::MemAddr& srcAddr, size_t size, Hermes::ProcAddr dest, Hermes::RVMA::VirtAddr virtAddr,
				size_t offset, Hermes::RVMA::Completion* comp, int* handle, Hermes::Callback* callback )
	{
		dbg().debug(CALL_INFO,1,2,"size=%zu destNode=%d destPid=%d virtAddr=0x%" PRIx64 " offset=%zu\n",
                        size, dest.node, dest.pid, virtAddr, offset );
		NicCmd* cmd = new PutCmd( srcAddr, size, dest, virtAddr, offset, comp, handle );
		setRetvalCallback();
		doEnter( cmd, callback );
	}

	void mwait(  Hermes::RVMA::Completion* completion, bool blocking, Hermes::Callback* callback )
	{
		dbg().debug(CALL_INFO,1,2,"completion %p\n",completion);
		NicCmd* cmd = new MwaitCmd( completion, blocking );
		setRetvalCallback();
		doEnter( cmd, callback );
	}

  private:

	int winGetBufPtrsFini( Hermes::RVMA::Completion* ptr, Event* event ) {

		WinGetBufPtrsResp* resp = static_cast<WinGetBufPtrsResp*>(event);
		dbg().debug(CALL_INFO,1,2,"count %zu\n", resp->completions.size());

		for ( int i = 0; i < resp->completions.size(); i++ ) {
			ptr[i] = resp->completions[i];
		}
		return resp->completions.size();
	}

	int initWindowFini( Hermes::RVMA::Window* window, Event* event ) {
		InitWindowResp* resp = static_cast<InitWindowResp*>(event);
		dbg().debug(CALL_INFO,1,2,"window %d\n", resp->window);
		*window = resp->window;
		return 0;
	}

	int winGetEpochFini( int* epoch, Event* event ) {
		WinGetEpochResp* resp = static_cast<WinGetEpochResp*>(event);
		dbg().debug(CALL_INFO,1,2,"window %d\n", resp->epoch);
		*epoch = resp->epoch;
		return 0;
	}
};

}
}
}

#endif
