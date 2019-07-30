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


#ifndef COMPONENTS_SNOTRA_RVMA_LIB_H
#define COMPONENTS_SNOTRA_RVMA_LIB_H

#include <vector>
#include "sst/elements/hermes/rvma.h"
#include "host/host.h"
#include "rvma/rvmaNicCmds.h"
#include "rvma/rvmaNicResp.h"

namespace SST {
namespace Snotra {
namespace RVMA {

class Lib : public Hermes::RVMA::Interface
{
  public:
    SST_ELI_REGISTER_SUBCOMPONENT(
        Lib,
        "Snotra",
        "RVMAlib",
        SST_ELI_ELEMENT_VERSION(1,0,0),
        "",
        ""
    )

    SST_ELI_DOCUMENT_PARAMS(
        {"verboseLevel","Sets the level of debug verbosity",""},
        {"verboseMask","Sets the debug mask",""},
    )

	Lib( Component*, Params& );
	~Lib() {} 

	std::string getName()        { return "Rvma"; }

	void setup();
	void setOS( Hermes::OS* os ) { m_os = static_cast<Host*>(os); }

	void initWindow( Hermes::RVMA::VirtAddr addr, size_t threshold, Hermes::RVMA::EpochType type, 
			Hermes::RVMA::Window* window, Hermes::Callback* );
	void closeWindow( Hermes::RVMA::Window, Hermes::Callback* );
	void winIncEpoch( Hermes::RVMA::Window, Hermes::Callback* );
    void winGetEpoch( Hermes::RVMA::Window, int*, Hermes::Callback* );
    void winGetBufPtrs( Hermes::RVMA::Window, Hermes::RVMA::Completion* notification_ptrs, int count, Hermes::Callback* );
    void put( Hermes::MemAddr, size_t size, Hermes::RVMA::ProcAddr dest, Hermes::RVMA::VirtAddr, size_t offset, Hermes::Callback* );
	void postBuffer( Hermes::MemAddr, size_t size, Hermes::RVMA::Completion* completion , Hermes::RVMA::Window, Hermes::Callback* );
	void mwait(  Hermes::RVMA::Completion* completion, Hermes::Callback* );

  private:

	int retvalFini( Event* event ) {
		RetvalResp* resp = static_cast<RetvalResp*>(event);
		m_dbg.debug(CALL_INFO,1,2,"retval %d\n", resp->retval);
		return resp->retval;
	}

	int winGetBufPtrsFini( Hermes::RVMA::Completion* ptr, Event* event ) {

		WinGetBufPtrsResp* resp = static_cast<WinGetBufPtrsResp*>(event);
		m_dbg.debug(CALL_INFO,1,2,"count %zu\n", resp->completions.size());

		for ( int i = 0; i < resp->completions.size(); i++ ) {
			ptr[i] = resp->completions[i];
		}
		return resp->completions.size();
	}

	int initWindowFini( Hermes::RVMA::Window* window, Event* event ) {
		InitWindowResp* resp = static_cast<InitWindowResp*>(event);
		m_dbg.debug(CALL_INFO,1,2,"window %d\n", resp->window);
		*window = resp->window;
		return 0;
	}

	int winGetEpochFini( int* epoch, Event* event ) {
		WinGetEpochResp* resp = static_cast<WinGetEpochResp*>(event);
		m_dbg.debug(CALL_INFO,1,2,"window %d\n", resp->epoch);
		*epoch = resp->epoch;
		return 0;
	}

	void selfLinkHandler( Event* event ) {
		m_dbg.debug(CALL_INFO,1,2,"\n");

		if ( ! event ) {
			if ( m_state == Enter ) {

				if ( host().cmdQ().isBlocked() ) {
					m_dbg.debug(CALL_INFO,1,2,"blocked on cmd queue\n");
        			host().cmdQ().setWakeup( 
						[=](){ 
							m_dbg.debug(CALL_INFO,1,2,"cmd queue wakeup\n");
							pushNicCmd( m_nicCmd );
							m_nicCmd = NULL;
						}
					);
				} else {
					m_dbg.debug(CALL_INFO,1,2,"pushed cmd to queue\n");
					pushNicCmd( m_nicCmd );
					m_nicCmd = NULL;
				}	

			} else if ( m_state == Return ) {
				m_dbg.debug(CALL_INFO,1,2,"return to motif\n");
				(*m_hermesCallback)(m_retval);
				delete m_hermesCallback;
				m_state = Idle;
			} else {
				assert(0);
			}
		}
	}

	void pushNicCmd( NicCmd* cmd ) {
		m_dbg.debug(CALL_INFO,1,2,"\n");
		host().cmdQ().push( cmd );
		if ( ! isCmdBlocking( cmd->type ) ) {
			m_dbg.debug(CALL_INFO,1,2,"non-blocking cmd\n");
			doReturn( 0 );
		} else {
			m_dbg.debug(CALL_INFO,1,2,"blocking cmd \n");
			host().respQ().setWakeup( 
				[=]( Event* event ){
					m_dbg.debug(CALL_INFO,1,2,"wakeup event=%p\n",event );
					doReturn( m_finiCallback( event ) );
					delete event;
				}  
			);
		}
	}

	void doEnter( NicCmd* cmd, Hermes::Callback* callback ) {
		m_dbg.debug(CALL_INFO,1,2,"%d\n",cmd->type);
		assert( m_state == Idle );
		m_pendingCmd = cmd->type;
		m_nicCmd = cmd; 
		m_hermesCallback = callback;
		m_state = Enter;
		schedDelay( calcEnterDelay(cmd->type) );
	}

	void doReturn( int retval ) {
		m_dbg.debug(CALL_INFO,1,2," nicCmd %d\n",m_pendingCmd);
		assert( m_state == Enter );
		m_state = Return;
		m_retval = retval;
		schedDelay( calcReturnDelay(m_pendingCmd) );
	}

	void schedDelay( SimTime_t delay, Event* event = NULL ) {
		m_dbg.debug(CALL_INFO,1,2,"delay=%" PRIu64 "\n",delay);
		m_selfLink->send( delay, event ); 
	}

	SimTime_t calcEnterDelay( NicCmd::Type type )  { return m_enterDelay[type]; }
	SimTime_t calcReturnDelay( NicCmd::Type type ) { return m_returnDelay[type]; }
	bool isCmdBlocking( NicCmd::Type type )        { return m_blockingCmd[type]; } 

	Host& host() { return *m_os; }

	enum State { Idle, Enter, Return } m_state;
	Link*	m_selfLink;
	Output 	m_dbg;
	Host*	m_os;

	Hermes::Callback* m_hermesCallback;
	NicCmd* m_nicCmd;
	NicCmd::Type m_pendingCmd;

	int m_retval;

	std::vector<SimTime_t> m_enterDelay;
	std::vector<SimTime_t> m_returnDelay;
	std::vector<bool> m_blockingCmd;

	typedef std::function<int(Event*)> Callback;

	Callback m_finiCallback;
};

}
}
}

#endif
