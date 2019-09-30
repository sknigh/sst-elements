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
#include <sst/core/component.h>
#include <sst/core/params.h>

#include "nic.h"
#include "include/nicEvents.h"

using namespace SST;
using namespace SST::Aurora;
using namespace SST::Interfaces;

int Aurora::NicSubComponent::m_pktSize = 0;

Nic::Nic(ComponentId_t id, Params &params) :
    Component( id ), m_nicSC(NULL)
{
	if ( params.find<bool>("print_all_params",false) ) { 
		printf("Aurora::Nic::Nic()\n");
		params.print_all_params(std::cout);
	}

    m_nodeId = params.find<int>("nid", -1);
    assert( m_nodeId != -1 );

    char buffer[100];
    snprintf(buffer,100,"@t:%d:Nic::@p():@l ",m_nodeId);

    m_dbg.init(buffer,
        params.find<uint32_t>("verboseLevel",0),
        params.find<uint32_t>("verboseMask",-1),
        Output::STDOUT);

	int pktSize = params.find<SST::UnitAlgebra>( "packetSize" ).getRoundedValue();

    if ( params.find<SST::UnitAlgebra>( "packetSize" ).hasUnits( "b" ) ) {
        pktSize /= 8;
	}

    UnitAlgebra input_buf_size = params.find<SST::UnitAlgebra>("input_buf_size" );
    UnitAlgebra output_buf_size = params.find<SST::UnitAlgebra>("output_buf_size" );
    UnitAlgebra link_bw = params.find<SST::UnitAlgebra>("link_bw" );

    m_dbg.verbose(CALL_INFO,1,1,"id=%d input_buf_size=%s output_buf_size=%s link_bw=%s packetSize=%d\n", m_nodeId,
            input_buf_size.toString().c_str(), output_buf_size.toString().c_str(), link_bw.toString().c_str(), pktSize);

    m_linkControl = loadUserSubComponent<Interfaces::SimpleNetwork>( "rtrLink", ComponentInfo::SHARE_NONE, 0 );
    assert( m_linkControl );

	m_recvNotifyFunctor = new SimpleNetwork::Handler<Nic>(this,&Nic::recvNotify );
	m_sendNotifyFunctor = new SimpleNetwork::Handler<Nic>(this,&Nic::sendNotify );

    m_linkControl->initialize(params.find<std::string>("rtrPortName","rtr"), link_bw, 2, input_buf_size, output_buf_size );

	Params p = params.find_prefix_params("nicSubComponent.");
    m_nicSC = loadAnonymousSubComponent<NicSubComponent>( params.find<std::string>("nicSubComponent"), "",0, ComponentInfo::SHARE_NONE, p );
    assert( m_nicSC );

	m_nicSC->setNodeNum( m_nodeId );
	m_nicSC->setNetworkLink( m_linkControl );

	std::string portName = params.find<std::string>("corePortName","core");
	int numCores = params.find<int>("numCores",1);

	m_nicSC->setNumCores( numCores );
	m_nicSC->setPktSize( pktSize );

	for ( int i = 0; i < numCores; i++ ) {
        std::ostringstream tmp;
        tmp << i;

		m_dbg.verbose(CALL_INFO,1,1,"portName=%s\n", (portName + tmp.str() ).c_str() );

		Link* link = configureLink( portName + tmp.str(), "1 ns", new Event::Handler<Nic,int>( this, &Nic::handleCoreEvent, i ) );
		assert(link);
		m_nicSC->setCoreLink( i, link );
		m_toCoreLinks.push_back( link );
	}
	m_nicSC->init(this);

	m_dbg.verbose(CALL_INFO,1,1,"numberOfCores=%d\n", numCores);

#if 0
	m_clockRate = params.find<UnitAlgebra>("clock");
	m_dbg.verbose(CALL_INFO,1,1,"clockRate=%s\n", m_clockRate.toString().c_str());

	if ( m_clockRate.getRoundedValue() ) {
    	m_clockHandler = new Clock::Handler<Nic>(this,&Nic::clockHandler);
	}
#endif

}

void Nic::init( unsigned int phase )
{
	m_dbg.debug(CALL_INFO,2,1,"phase=%d\n",phase);
    m_linkControl->init(phase);
	if ( 0 == phase ) {
		for ( size_t i = 0; i < m_toCoreLinks.size(); i++ ) {
			m_toCoreLinks[i]->sendInitData( new NicInitEvent( m_nodeId, i, m_toCoreLinks.size() ) );
		}
	}
}
