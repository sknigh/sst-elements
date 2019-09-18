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


#ifndef COMPONENTS_AURORA_INCLUDE_HOST_LIB_H
#define COMPONENTS_AURORA_INCLUDE_HOST_LIB_H

#include <vector>
#include "host/host.h"

namespace SST {
namespace Aurora {

template < class Interface, class NicCmd, class RetvalResp >
class HostLib : public Interface 
{
  public:

	HostLib( Component* component, Params& params ) : Interface(component), m_nicCmd(NULL), m_state( Idle ) {

		m_dbg.debug(CALL_INFO,1,2,"\n");

    	if ( params.find<bool>("print_all_params",false) ) {
        	printf("Aurora::HostLib()\n");
        	params.print_all_params(std::cout);
    	}

    	m_dbg.init("@t:Aurora::HostLib::@p():@l ", params.find<uint32_t>("verboseLevel",0),
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

    	m_selfLink = component->configureSelfLink("LibSelfLink", "1 ns", new Event::Handler<HostLib>(this,&HostLib::selfLinkHandler));	
	}
	~HostLib() {} 

	void setup() {
    	char buffer[100];
    	snprintf(buffer,100,"@t:%d:%d:Aurora::HostLib::@p():@l ",
                    m_os->getNodeNum(), m_os->getRank());
    	m_dbg.setPrefix(buffer);
	}	

	void setOS( Hermes::OS* os ) { m_os = static_cast<Host*>(os); }

	Output& dbg() { return m_dbg; }

	void doEnter( NicCmd* cmd, Hermes::Callback* callback ) {
		m_dbg.debug(CALL_INFO,1,2,"%d\n",cmd->type);
		assert( m_state == Idle );
		m_pendingCmd = cmd->type;
		m_nicCmd = cmd; 
		m_hermesCallback = callback;
		m_state = Enter;
		schedDelay( calcEnterDelay(cmd->type) );
	}
	typedef std::function<int(Event*)> Callback;

	void setFiniCallback( Callback* callback ) { m_finiCallback = callback; }
	void setRetvalCallback( ) { m_finiCallback = new Callback( std::bind( &HostLib::retvalFini, this, std::placeholders::_1 ) ); }

  private:

	int retvalFini( Event* event ) {
		RetvalResp* resp = static_cast<RetvalResp*>(event);
		m_dbg.debug(CALL_INFO,1,2,"retval %d\n", resp->retval);
		return resp->retval;
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

			} else {
				assert(0);
			}
		} else {
			m_state = Idle;
			m_dbg.debug(CALL_INFO,1,2,"return\n");
			SelfEvent* e = static_cast<SelfEvent*>(event);
			Hermes::Callback&  callback = *e->callback;
			callback( e->retval );
			delete e->callback;
			delete event;
		}
	}

	class SelfEvent : public Event {
	  public:
		SelfEvent( Hermes::Callback* callback, int retval ) : Event(), callback(callback), retval(retval) {} 
		Hermes::Callback* callback;
		int retval;
		NotSerializable(SelfEvent);
	};

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
					doReturn( (*m_finiCallback)( event ) );
					delete m_finiCallback;
					delete event;
				}  
			);
		}
	}

	void doReturn( int retval ) {
		m_dbg.debug(CALL_INFO,1,2," nicCmd %d\n",m_pendingCmd);
		assert( m_state == Enter );
		m_state = Return;
		schedDelay( calcReturnDelay(m_pendingCmd), new SelfEvent( m_hermesCallback, retval) );
	}

	void schedDelay( SimTime_t delay, Event* event = NULL ) {
		m_dbg.debug(CALL_INFO,1,2,"delay=%" PRIu64 "\n",delay);
		m_selfLink->send( delay, event ); 
	}

	SimTime_t calcEnterDelay( typename NicCmd::Type type )  { return m_enterDelay[type]; }
	SimTime_t calcReturnDelay( typename NicCmd::Type type ) { return m_returnDelay[type]; }
	bool isCmdBlocking( typename NicCmd::Type type )        { return m_blockingCmd[type]; } 

	Host& host() { return *m_os; }

	enum State { Idle, Enter, Return } m_state;
	Link*	m_selfLink;
	Output 	m_dbg;
	Host*	m_os;

	Hermes::Callback* m_hermesCallback;
	NicCmd* m_nicCmd;
	typename NicCmd::Type m_pendingCmd;

	std::vector<SimTime_t> m_enterDelay;
	std::vector<SimTime_t> m_returnDelay;
	std::vector<bool> m_blockingCmd;


	Callback* m_finiCallback;
};

}
}

#endif
