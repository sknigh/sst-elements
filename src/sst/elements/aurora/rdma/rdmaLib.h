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


#ifndef COMPONENTS_AURORA_RDMA_LIB_H
#define COMPONENTS_AURORA_RDMA_LIB_H

#include <vector>
#include <queue>
#include "sst/elements/hermes/rdma.h"
#include "include/hostLib.h"

#include "rdma/rdmaNicCmds.h"
#include "rdma/rdmaNicResp.h"

namespace SST {
namespace Aurora {
namespace RDMA {

class RdmaLib : public HostLib< Hermes::RDMA::Interface, NicCmd, RetvalResp >
{
  public:
    SST_ELI_REGISTER_SUBCOMPONENT_DERIVED(
        RdmaLib,
        "aurora",
        "rdmaLib",
        SST_ELI_ELEMENT_VERSION(1,0,0),
        "",
        SST::Hermes::Interface
    )

    SST_ELI_DOCUMENT_PARAMS(
        {"verboseLevel","Sets the level of debug verbosity",""},
        {"verboseMask","Sets the debug mask",""},
    )

	RdmaLib( ComponentId_t, Params& );
	~RdmaLib() { } 

	std::string getName()        { return "Rdma"; }

    void createRQ( Hermes::RDMA::RqId rqId, Hermes::Callback* callback ) { 
		dbg().debug(CALL_INFO,1,2,"\n");
		setRetvalCallback();	
    	doEnter( new CreateRqCmd( rqId ), callback );
	}

    void postRecv( Hermes::RDMA::RqId rqId, Hermes::MemAddr& addr, size_t length,
		   	Hermes::RDMA::RecvBufId* bufId, Hermes::Callback* callback ) 
	{
		dbg().debug(CALL_INFO,1,2,"\n");
		setFiniCallback(  new Callback(std::bind( &RdmaLib::postRecvFini, this, bufId, std::placeholders::_1 )) );	
		doEnter( new PostRecvCmd( rqId, addr, length ), callback );
	}

    void send( Hermes::ProcAddr proc, Hermes::RDMA::RqId rqId, const Hermes::MemAddr& src, size_t length, int* handle, Hermes::Callback* callback ) {
		dbg().debug(CALL_INFO,1,2,"\n");
		setRetvalCallback();	
		doEnter( new SendCmd( proc, rqId, src, length, handle ), callback );
	}

    void checkRQ( Hermes::RDMA::RqId rqId, Hermes::RDMA::Status* status, bool blocking, Hermes::Callback* callback ) {

		setFiniCallback( new Callback( std::bind( &RdmaLib::checkRqFini, this, status, std::placeholders::_1 ) ) );
		doEnter( new CheckRqCmd( rqId, blocking ), callback );
	}

    void registerMem( Hermes::MemAddr& addr, size_t length, Hermes::RDMA::MemRegionId* id, Hermes::Callback* callback ) {
		dbg().debug(CALL_INFO,1,2,"\n");
		setFiniCallback( new Callback(std::bind( &RdmaLib::registerMemFini, this, id, std::placeholders::_1 ) ) );	
    	doEnter( new RegisterMemCmd( addr, length ), callback );
	}

	void read( Hermes::ProcAddr proc, Hermes::MemAddr& destAddr, Hermes::RDMA::Addr srcAddr, size_t length, Hermes::Callback* callback ) 
	{
		dbg().debug(CALL_INFO,1,2,"\n");
		setRetvalCallback();	
    	doEnter( new ReadCmd( proc, destAddr, srcAddr, length ), callback );
	}

    void write( Hermes::ProcAddr proc, Hermes::RDMA::Addr  destAddr, Hermes::MemAddr& srcAddr, size_t length, Hermes::Callback* callback ) {
		dbg().debug(CALL_INFO,1,2,"\n");
		setRetvalCallback();	
    	doEnter( new WriteCmd( proc, destAddr, srcAddr, length ), callback );
	}

  private:

	int postRecvFini( Hermes::RDMA::RecvBufId* bufId, Event* event ) {
		PostRecvResp* resp = static_cast<PostRecvResp*>(event);
		dbg().debug(CALL_INFO,1,2,"bufId=%d retval=%d\n", (int) resp->bufId, resp->retval);
		if ( bufId ) {
			*bufId = resp->bufId;
		}
        return resp->retval;
    }

	int checkRqFini( Hermes::RDMA::Status* status, Event* event ) {
		CheckRqResp* resp = static_cast<CheckRqResp*>(event);
		if ( status ) {
			*status = resp->status;
		}
		return resp->retval;
	}

	int registerMemFini( Hermes::RDMA::MemRegionId* id, Event* event ) {
		RegisterMemResp* resp = static_cast<RegisterMemResp*>(event);
		dbg().debug(CALL_INFO,1,2,"retval=%d\n", (int)resp->id);
		*id = resp->id;
        return resp->retval;
	}
};

}
}
}

#endif
