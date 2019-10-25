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


#include "sst_config.h"

#include "nic/nic.h"
#include "nic/nicSubComponent.h"

using namespace SST;
using namespace SST::Aurora;

NicSubComponent::NicSubComponent( ComponentId_t id, Params params ) : SubComponent(id),  m_clocking(false), m_totalIdleCycles(0), m_vc(0) {

    UnitAlgebra    m_clockRate = params.find<UnitAlgebra>("clock","100Mhz");
	SST::UnitAlgebra busBandwidth = params.find<SST::UnitAlgebra>( "busBandwidth", "15GB/s" );

	m_toHostLatency = params.find<SimTime_t>("toHostLatency",0);
	m_rxLatency = params.find<SimTime_t>("rxLatency",0);
	m_txLatency = params.find<SimTime_t>("txLatency",0);

	m_toHostBandwidth = params.find<SST::UnitAlgebra>( "toHostBandwidth", busBandwidth ).getRoundedValue();
	m_fromHostBandwidth = params.find<SST::UnitAlgebra>( "fromHostBandwidth", busBandwidth ).getRoundedValue();

	m_selfLink = configureSelfLink("Nic::selfLink", "1 ns",
		new Event::Handler<NicSubComponent>(this,&NicSubComponent::handleSelfEvent));
	assert( m_selfLink );

	m_dbg.verbose(CALL_INFO,1,1,"clockRate=%s\n", m_clockRate.toString().c_str());
	if ( m_clockRate.getRoundedValue() ) {
		m_clockHandler = new Clock::Handler<NicSubComponent>(this,&NicSubComponent::clockHandler);
		m_timeConverter = registerClock( m_clockRate, m_clockHandler );
		m_clocking = true;
	}
}

void NicSubComponent::stopClocking( Cycle_t cycle ) {
	assert( m_clocking == true );

	m_dbg.debug(CALL_INFO,2,2,"stop clocking cycle %" PRIu64 " %s\n",cycle, 
			getNetworkLink().requestToReceive( m_vc ) ? "pkt avail":"no pkt" );

	m_nic->enableNetNotifier();
	m_clocking = false;
	m_stopCycle = cycle;
}

void NicSubComponent::startClocking() {
	m_nic->disableNetNotifier();
	Cycle_t cycle = _startClocking();
	m_totalIdleCycles += getNextClockCycle(m_timeConverter) - m_stopCycle;

	m_dbg.debug(CALL_INFO,2,2,"start clocking cycle %" PRIu64 "\n",cycle);
}
