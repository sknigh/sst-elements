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

#ifndef COMPONENTS_AURORA_NIC_SUB_COMPONENT_H
#define COMPONENTS_AURORA_NIC_SUB_COMPONENT_H

#include <sst/core/subcomponent.h>
#include <sst/core/link.h>
#include <sst/core/interfaces/simpleNetwork.h>
#include <sst/core/params.h>
#include "include/nicEvents.h"

namespace SST {
namespace Aurora {

class Nic;
class NicSubComponent : public SubComponent {

  public:

    SST_ELI_REGISTER_SUBCOMPONENT_API(SST::Aurora::NicSubComponent)

    NicSubComponent( Component* owner ) : SubComponent(owner) {assert(0);}
    NicSubComponent( ComponentId_t, Params );

    virtual void setup() {}
    virtual void finish() {
#if 0
		Cycle_t cycle  = getNextClockCycle(m_timeConverter);
		printf("total cycles %" PRIu64 ", idle cycles %" PRIu64 " idle %.1f%%\n", cycle, m_totalIdleCycles,  ((double) m_totalIdleCycles / cycle ) * 100);
#endif
	}

	virtual void init( Nic* nic ) { m_nic = nic; } 
	void setNetworkLink( Interfaces::SimpleNetwork* link ) { m_netLink= link; } 
	virtual void setNumCores( int num ) { m_toCoreLinks.resize(num); }
	void setCoreLink( int core, Link* link ) { m_toCoreLinks[core] = link; }
	int getNumCores() { return m_toCoreLinks.size(); }
	void setPktSize( int size ) { m_pktSize = size; } 

	Interfaces::SimpleNetwork& getNetworkLink( ) { return *m_netLink; }

	void sendResp( int core, Event* event ) {
		m_toCoreLinks[core]->send(m_toHostLatency,new NicEvent( event, NicEvent::Payload ) ); 
	}

	void sendCredit( int core ) {
		m_toCoreLinks[core]->send(m_toHostLatency,new NicEvent( NULL,  NicEvent::Credit ) ); 
	}

	virtual void setNodeNum( int num ) { m_nodeNum = num; }
	virtual int getNodeNum( ) { return m_nodeNum; }
	virtual void handleEvent( int core, Event* ) = 0;
	virtual bool clockHandler( Cycle_t ) { assert(0); }
	virtual void handleSelfEvent( Event* ) { assert(0); }
	static int getPktSize() { return m_pktSize; }

    void networkReady( int vc ) {
        m_dbg.debug(CALL_INFO,1,2,"\n");
        _startClocking();
    }

  protected:

	SimTime_t calcToHostBW_Latency( size_t length ) {
        m_dbg.debug(CALL_INFO,1,2,"length=%zu BW=%f latency=%" PRIu64 "\n",
					length, m_toHostBandwidth, (SimTime_t) ( ( (double) length / m_toHostBandwidth ) * 1000000000.0 ) );
		return ( ( (double) length / m_toHostBandwidth ) * 1000000000.0 ); 
    }

	SimTime_t calcFromHostBW_Latency( size_t length ) {
		m_dbg.debug(CALL_INFO,1,2,"length=%zu BW=%f latency=%" PRIu64 "\n",
					length, m_fromHostBandwidth, (SimTime_t) ( ( (double) length / m_fromHostBandwidth ) * 1000000000.0 ) );
		return ( ( (double) length / m_fromHostBandwidth ) * 1000000000.0 ); 
    }

    void stopClocking( Cycle_t cycle );
    void startClocking();
    Cycle_t _startClocking() {

        if ( m_clocking != false ) {
            m_dbg.debug(CALL_INFO,1,2,"\n");
            assert(0);
        }
        m_clocking = true;
        return reregisterClock( m_timeConverter, m_clockHandler );
    }

	TimeConverter* m_timeConverter;	

	Link*	m_selfLink;
	bool	m_clocking;
	Output	m_dbg;

	SimTime_t m_toHostLatency;	
	SimTime_t m_rxLatency;	
	SimTime_t m_txLatency;
	double	m_toHostBandwidth;
	double	m_fromHostBandwidth;

  private:
    std::vector<Link*>          		m_toCoreLinks;
	Clock::Handler<NicSubComponent>*	m_clockHandler;
	Interfaces::SimpleNetwork*  		m_netLink;

	int 	m_nodeNum;
	Nic* 	m_nic;

	Cycle_t m_totalIdleCycles;
	Cycle_t m_stopCycle;

	static int m_pktSize;
};

}
}

#endif
