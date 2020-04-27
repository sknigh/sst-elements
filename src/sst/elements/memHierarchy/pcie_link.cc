// Copyright 2013-2019 NTESS. Under the terms
// of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.
//
// Copyright (c) 2013-2019, NTESS
// All rights reserved.
//
// Portions are copyright of other developers:
// See the file CONTRIBUTORS.TXT in the top level directory
// the distribution for more information.
//
// This file is part of the SST software package. For license
// information, see the LICENSE file in the top level directory of the
// distribution.

#include <array>
#include <sst_config.h>

#include <sst/core/simulation.h>
#include <sst/core/timeConverter.h>

#include "sst/elements/memHierarchy/pcie_link.h"

using namespace SST;
using namespace SST::MemHierarchy;

/* Constructor */

PCIE_Link::PCIE_Link(ComponentId_t id, Params &params) :  MemLinkBase(id, params) {

    // Output for debug
    char buffer[100];
    snprintf(buffer,100,"@t:%s:PCIE_Link::@p():@l ",getName().c_str());
    m_dbg.init( buffer,
        10,//params.find<int>("debug_level", 0),
        -1,//params.find<int>("debug_mask",0), 
        (Output::output_location_t) 1 );

	m_link = configureLink("link", new Event::Handler<PCIE_Link>(this, &PCIE_Link::handleEvent));
	assert(m_link);

	m_linkWidthBytes = params.find<int>("linkWidthInBytes", 1 );
    UnitAlgebra link_bw = params.find<UnitAlgebra>("linkBandwidth", "1GB/s");
    if ( !link_bw.hasUnits("B/s") ) {
        m_dbg.fatal(CALL_INFO,-1,"PCIE_Link: link_bw must be specified in either "
                           "b/s or B/s: %s\n",link_bw.toStringBestSI().c_str());
    }
	UnitAlgebra link_clock = link_bw / UnitAlgebra("1B");

	m_selfLink = configureSelfLink( getName() + "selfLink", getTimeConverter(link_clock), 
                                              new Event::Handler<PCIE_Link>(this,&PCIE_Link::selfLinkHandler));

	m_link->setDefaultTimeBase(  getTimeConverter(link_clock) );

	m_latPerByte_ps = m_selfLink->getDefaultTimeBase()->getFactor();

	m_dbg.debug(CALL_INFO,1,1,"link BW=%s latPerByte_ps=%" PRIu64 "\n",link_bw.toString().c_str(),m_latPerByte_ps);

    m_tlpHdrLength = params.find<uint32_t>("tlpHdrLength", 16);
    m_tlpSeqLength = params.find<uint32_t>("tlpSeqLength", 2);
    m_dllpHdrLength = params.find<uint32_t>("dlpHdrLength", 6);
    m_lcrcLength = params.find<uint32_t>("lcrcLength", 4);

    m_fc_numPH = params.find<uint32_t>("numPH",32);
    m_fc_numPD = params.find<uint32_t>("numPD",32);
    m_fc_numNPH = params.find<uint32_t>("numNPH",32);
    m_fc_numNPD = params.find<uint32_t>("numNPD",32);
    m_fc_numCPLH = params.find<uint32_t>("numCPLH",32);
    m_fc_numCPLD = params.find<uint32_t>("numCPLD",32);
}

void PCIE_Link::init(unsigned int phase) {
}

void PCIE_Link::setup() {
}

void PCIE_Link::handleEvent(SST::Event *ev){
	LinkEvent* le = static_cast<LinkEvent*>(ev);
	m_dbg.debug(CALL_INFO,1,0,"\n");
	if ( m_inQ.empty() ) {
		SimTime_t latency = calcNumClocks(le->pktLen); 
		m_dbg.debug(CALL_INFO,1,0,"start InQ timer %" PRIu64"\n",latency);
		m_selfLink->send( latency, new SelfEvent( SelfEvent::InQ ) );
	}
	m_inQ.push( le );
}


void PCIE_Link::selfLinkHandler(SST::Event *ev){

	SelfEvent* se = static_cast<SelfEvent*>(ev);

	if ( se->type == SelfEvent::OutQ ) {

		m_dbg.debug(CALL_INFO,1,0,"OutQ timer fired\n");

		m_outQ.pop();
		if ( ! m_outQ.empty() ) {
			SimTime_t latency = calcNumClocks(m_outQ.front()->pktLen); 
			m_link->send( 1, m_outQ.front() );
			m_dbg.debug(CALL_INFO,1,0,"start OutQ timer %" PRIu64"\n",latency);
			m_selfLink->send( latency, new SelfEvent( SelfEvent::OutQ ) );
		}

	} else if ( se->type == SelfEvent::InQ ) {
		m_dbg.debug(CALL_INFO,1,0,"InQ timer fired\n");

		recvPkt( m_inQ.front() );
		m_inQ.pop();
		if ( ! m_inQ.empty() ) {
			SimTime_t latency = calcNumClocks(m_inQ.front()->pktLen); 
			m_dbg.debug(CALL_INFO,1,0,"start InQ timer %" PRIu64"\n",latency);
			m_selfLink->send( latency, new SelfEvent( SelfEvent::InQ ) );
		}

	} else {
		assert(0);
	}
	delete se;
}

/* Send event to memNIC */
void PCIE_Link::send(MemEventBase *ev) {
	m_dbg.debug(CALL_INFO,1,0,"ev=%p\n",ev);

	size_t length = calcPktLength( ev );
	pushLinkEvent( new LinkEvent(ev,length) );	

	if ( ! static_cast<MemEvent*>(ev)->isResponse() ) {
		// NOTE that this event is not ours, the receiver will delete it
		m_req[ev->getID()] = ev;
	}
}

void PCIE_Link::pushLinkEvent( LinkEvent* le )
{
	// if the outQ is empty thats mean xmit is idle
	// send the packet and send the timer event
	// to signal when packet has been transmitted
	if ( m_outQ.empty() ) {
		SimTime_t latency = calcNumClocks(le->pktLen); 
		m_dbg.debug(CALL_INFO,1,0,"start OutQ timer %" PRIu64"\n",latency);
		m_selfLink->send( latency, new SelfEvent( SelfEvent::OutQ ) );
		m_link->send(1,le);
	}
	m_outQ.push(le);
}
void PCIE_Link::recvPkt( LinkEvent* le ) {

	if ( le->type == LinkEvent::Type::DLLP ) {
			
		m_dbg.debug(CALL_INFO,1,0,"got DLLP\n");
		delete le;
		return;
	}

	MemEventBase* meb = le->memEventBase;
	delete le;
	MemEventBase* req;

	m_dbg.debug(CALL_INFO,1,0,"process InQ %p\n",meb);

	if ( static_cast<MemEvent*>(meb)->isResponse() ) {
			
		m_dbg.debug(CALL_INFO,1,0,"process InQ response\n");

		try {
			req = m_req.at(meb->getResponseToID());
		} catch (const std::out_of_range& oor) {
			assert(0);
        }
		// NOTE that this m_req is not ours, the receiver deleted it
		m_req.erase( meb->getResponseToID() );
	}

	pushLinkEvent( new LinkEvent( LinkEvent::Type::DLLP, m_dllpHdrLength ) );	

	(*recvHandler)(meb);
}


/* 
 * Called by parent on a clock 
 * Returns whether anything sent this cycle
 */
bool PCIE_Link::clock() {
    return false;
}
