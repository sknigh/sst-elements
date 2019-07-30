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


#include <sst_config.h>
#include "rvmaLib.h"

using namespace SST::Snotra::RVMA;
using namespace Hermes;


Lib::Lib( Component* owner, Params& params) :
	Interface(owner), m_nicCmd(NULL), m_state( Idle )
{
	if ( params.find<bool>("print_all_params",false) ) {
        printf("Snotra::RVMA::Lib()\n");
        params.print_all_params(std::cout);
    }

    m_dbg.init("@t:Snotra::RVMA::Lib::@p():@l ", params.find<uint32_t>("verboseLevel",0),
            params.find<uint32_t>("verboseMask",0), Output::STDOUT );

    m_dbg.debug(CALL_INFO,1,2,"\n");

	m_enterDelay.resize( NicCmd::NumCmds );
	m_returnDelay.resize( NicCmd::NumCmds );
	m_blockingCmd.resize( NicCmd::NumCmds );

	for ( int i = 0; i < NicCmd::NumCmds; i++ ) {
		m_enterDelay[i] = 10;
		m_returnDelay[i] = 10;
		m_blockingCmd[i] = true;
	}	

	m_selfLink = configureSelfLink("LibSelfLink", "1 ns", new Event::Handler<Lib>(this,&Lib::selfLinkHandler));
}

void Lib::setup() 
{
    char buffer[100];
    snprintf(buffer,100,"@t:%d:%d:Snotra::RVMA::Lib::@p():@l ",
                    m_os->getNodeNum(), m_os->getRank());
    m_dbg.setPrefix(buffer);
}

void Lib::initWindow( Hermes::RVMA::VirtAddr addr, size_t threshold, Hermes::RVMA::EpochType type,
            Hermes::RVMA::Window* window, Hermes::Callback* callback ) 
{
	m_dbg.debug(CALL_INFO,1,2,"\n");
	NicCmd* cmd = new InitWindowCmd( addr, threshold, type );
	m_finiCallback = std::bind( &Lib::initWindowFini, this, window, std::placeholders::_1 );

	doEnter( cmd, callback );
}

void Lib::closeWindow( Hermes::RVMA::Window window, Hermes::Callback* callback ) 
{
	m_dbg.debug(CALL_INFO,1,2,"\n");
	NicCmd* cmd = new CloseWindowCmd( window );
	m_finiCallback = std::bind( &Lib::retvalFini, this, std::placeholders::_1 );

	doEnter( cmd, callback );
}

void Lib::winIncEpoch( Hermes::RVMA::Window window, Hermes::Callback* callback )
{
	m_dbg.debug(CALL_INFO,1,2,"\n");
	NicCmd* cmd = new WinIncEpochCmd( window );
	m_finiCallback = std::bind( &Lib::retvalFini, this, std::placeholders::_1 );

	doEnter( cmd, callback );
}

void Lib::winGetEpoch( Hermes::RVMA::Window window, int* epoch, Hermes::Callback* callback )
{
	m_dbg.debug(CALL_INFO,1,2,"\n");
	NicCmd* cmd = new WinGetEpochCmd( window );
	m_finiCallback = std::bind( &Lib::winGetEpochFini, this, epoch, std::placeholders::_1 );

	doEnter( cmd, callback );
}

void Lib::winGetBufPtrs( Hermes::RVMA::Window window, Hermes::RVMA::Completion* ptr,
				int count, Hermes::Callback* callback )
{
	m_dbg.debug(CALL_INFO,1,2,"count=%d\n",count);
	NicCmd* cmd = new WinGetBufPtrsCmd( window, count );
	m_finiCallback = std::bind( &Lib::winGetBufPtrsFini, this, ptr, std::placeholders::_1 );

	doEnter( cmd, callback );
}

void Lib::postBuffer( Hermes::MemAddr addr, size_t size, Hermes::RVMA::Completion* completion, 
			Hermes::RVMA::Window window, Hermes::Callback* callback )
{
	m_dbg.debug(CALL_INFO,1,2,"window=%d size=%zu completion=%p\n",window,size,completion);

	NicCmd* cmd = new PostBufferCmd( addr, size, completion, window );

	m_finiCallback = std::bind( &Lib::retvalFini, this, std::placeholders::_1 );

	doEnter( cmd, callback );
}

void Lib::put( Hermes::MemAddr srcAddr, size_t size, Hermes::RVMA::ProcAddr dest,
				Hermes::RVMA::VirtAddr virtAddr, size_t offset, Hermes::Callback* callback )
{
	m_dbg.debug(CALL_INFO,1,2,"size=%zu destNode=%d destPid=%d virtAddr=0x%" PRIx64 " offset=%zu\n",
						size, dest.node, dest.pid, virtAddr, offset );

	NicCmd* cmd = new PutCmd( srcAddr, size, dest, virtAddr, offset );

	m_finiCallback = std::bind( &Lib::retvalFini, this, std::placeholders::_1 );

	doEnter( cmd, callback );
}

void Lib::mwait( Hermes::RVMA::Completion* completion, Hermes::Callback* callback )
{
	m_dbg.debug(CALL_INFO,1,2,"completion %p\n",completion);

	NicCmd* cmd = new MwaitCmd( completion );

	m_finiCallback = std::bind( &Lib::retvalFini, this, std::placeholders::_1 );

	doEnter( cmd, callback );
}

