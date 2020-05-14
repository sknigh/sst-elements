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

PCIE_Link::PCIE_Link(ComponentId_t id, Params &params) :  MemLinkBase(id, params), m_dllpOutQmax(0), m_tlpOutQmax(0), m_linkIdle(true), m_oldestAck(0)
{
    // Output for debug
    char buffer[100];
    snprintf(buffer,100,"@t:%s:PCIE_Link::@p():@l ",getName().c_str());
    m_dbg.init( buffer,
        params.find<int>("debug_level", 0),
        params.find<int>("debug_mask",0), 
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

	m_creditThreshold = params.find<double>("creditThreshold",0.5);

	m_maxMtuLength = 128;

	m_maxHdrCredits = params.find<uint32_t>("numHdrCredits",128);
	m_maxDataCredits = params.find<uint32_t>("numDataCredits",2048);
	m_numHdrCredits.resize(2);
	m_numDataCredits.resize(2);

	for ( size_t i = 0; i < 2; i++ ) {
		m_numHdrCredits[i].resize(3);
		m_numDataCredits[i].resize(3);
	}

	for ( size_t i = 0; i < 3; i++ ) {
		m_numHdrCredits[RX][i] = 0;
		m_numDataCredits[RX][i] = 0;
		m_numHdrCredits[TX][i] = m_maxHdrCredits;
		m_numDataCredits[TX][i] = m_maxDataCredits;
	}

    m_tlpHdrLength = params.find<uint32_t>("tlpHdrLength", 16);
    m_tlpSeqLength = params.find<uint32_t>("tlpSeqLength", 2);
    m_dllpHdrLength = params.find<uint32_t>("dlpHdrLength", 6);
    m_lcrcLength = params.find<uint32_t>("lcrcLength", 4);

	// The PCI spec specifies the AckNak_LATENCY_TIMER as
	// ( Max Payload + TLP overhead ) * AckFactor
	// ------------------------------------------ + InternalDelay
	//            LinkBandwidth
	// The model currently does use internalDelay
	// The AckFactor varies based on link bandwidth, width and MaxMtu
	// AckFactor of 3 is used for MTU=128 and numLinks=16 and all frequencies
	uint32_t  maxTlpBytes= m_tlpSeqLength + m_tlpHdrLength + m_maxMtuLength + m_lcrcLength;
	m_ackTimeout = (calcNumClocks(maxTlpBytes * 3) * m_latPerByte_ps)/1000;

	m_mtuLength = params.find<size_t>( "mtuLength", 256 );
}

void PCIE_Link::handleEvent(SST::Event *ev){
	LinkEvent* le = static_cast<LinkEvent*>(ev);
	if ( m_inQ.empty() ) {
		SimTime_t latency = calcNumClocks(le->pktLen); 
		m_dbg.debug(CALL_INFO,1,0,"start InQ timer %" PRIu64"\n",latency);
		m_selfLink->send( latency, new SelfEvent( SelfEvent::InQ ) );
	} else {
		m_dbg.debug(CALL_INFO,1,0,"enque\n");
	}
	m_inQ.push( le );
}


void PCIE_Link::selfLinkHandler(SST::Event *ev){

	SelfEvent* se = static_cast<SelfEvent*>(ev);

	if ( se->type == SelfEvent::OutQ ) {

		m_dbg.debug(CALL_INFO,1,0,"OutQ timer fired\n");

		LinkEvent* le = NULL;

		if ( ! m_dllpAckOutQ.empty() ) {
			m_dbg.debug(CALL_INFO,1,0,"send DLLP ACK\n");
			le = m_dllpAckOutQ.front();
			m_dllpAckOutQ.pop();
		} else if ( ! m_dllpFcOutQ.empty() ) {
			m_dbg.debug(CALL_INFO,1,0,"send DLLP FC\n");
			le = m_dllpFcOutQ.front();
			m_dllpFcOutQ.pop();
		} else if ( ! m_tlpOutQ.empty() ) {
			m_dbg.debug(CALL_INFO,1,0,"send TLP\n");
			le = m_tlpOutQ.front();
			m_tlpOutQ.pop();
		}
		if ( le ) {
			SimTime_t latency = calcNumClocks(le->pktLen); 
			m_link->send( 1, le );
			m_dbg.debug(CALL_INFO,1,0,"start OutQ timer %" PRIu64"\n",latency);
			m_selfLink->send( latency, new SelfEvent( SelfEvent::OutQ ) );
		} else {
			m_linkIdle = true;
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
	m_dbg.debug( CALL_INFO,1,0,"(%s) Received: '%s'\n", getName().c_str(), ev->getBriefString().c_str());

    switch ( ev->getCmd() ) {

        case Command::PutM: // Memory write
			ev->setFlag(MemEvent::F_NORESPONSE);
			break;

        case Command::GetX: // Memory read (unless NONCACHEABLE), then this is a write
        case Command::GetS: // Memory read (unless NONCACHEABLE), then this is a write 
        case Command::PrWrite:
			if ( isRead( static_cast<MemEvent*>(ev) ) ) {
				m_dbg.debug( CALL_INFO,1,0,"Write\n");
				m_req[ev->getID()] = ev;
			} else {
				m_dbg.debug( CALL_INFO,1,0,"Write\n");
				MemEventBase* resp = ev->makeResponse();
				m_dbg.debug( CALL_INFO,1,0,"(%s) Received: '%s'\n", getName().c_str(), resp->getBriefString().c_str());
				(*recvHandler)( resp );
			}
			break;

        case Command::PrRead:
			// NOTE that this event is not ours, the receiver will delete it
			m_dbg.debug( CALL_INFO,1,0,"Read\n");
			m_req[ev->getID()] = ev;
            break;

			m_dbg.debug( CALL_INFO,1,0,"WriteResp\n");
			break;

        case Command::GetXResp: // PrWrite or GetX  Resp
        case Command::GetSResp: // PrRead or GetS  Resp
			m_dbg.debug( CALL_INFO,1,0,"Response\n");
			if ( ev->queryFlag(MemEvent::F_NONCACHEABLE) ) {
				delete ev;
				return;
			}
			break;

        default:
           assert(0);
     }

	consumeTxCredits(ev);

	size_t length = calcPktLength( ev );
	pushLinkEvent( new LinkEvent(ev,length) );
}

void PCIE_Link::recvPkt( LinkEvent* le ) {

	if ( le->type == LinkEvent::Type::DLLP ) {
			
		switch( le->dllpType ) {
		  case LinkEvent::DLLP_type::ACK:
			m_dbg.debug(CALL_INFO,1,0,"got DLLP\n");
			break;

		  case LinkEvent::DLLP_type::CREDIT:
			m_dbg.debug(CALL_INFO,1,0,"got CREDIT\n");
			addTxCredits( le );
			break;
		}
		delete le;
		return;
	}

	MemEventBase* meb = le->memEventBase;
	delete le;
	MemEventBase* req;

	m_dbg.debug( CALL_INFO,1,0,"(%s) Received: '%s'\n", getName().c_str(), meb->getBriefString().c_str());

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

	if ( m_oldestAck == 0 ) {
		m_oldestAck = getCurrentSimTimeNano();
	}

	addRxCredits( static_cast<MemEvent*>(meb) );

	(*recvHandler)(meb);
}

void PCIE_Link::pushLinkEvent( LinkEvent* le )
{
	// if the PCI link is idle send the packet and send the timer event
	// to signal when packet has been transmitted
	if ( m_linkIdle ) {
		SimTime_t latency = calcNumClocks(le->pktLen); 
		m_dbg.debug(CALL_INFO,1,0,"start OutQ timer %" PRIu64"\n",latency);
		m_selfLink->send( latency, new SelfEvent( SelfEvent::OutQ ) );
		m_link->send(1,le);
		m_linkIdle = false;
	} else {
		if ( le->type == LinkEvent::TLP ) {
			m_tlpOutQ.push(le);
			if ( m_tlpOutQ.size() > m_tlpOutQmax ) {
				m_tlpOutQmax = m_tlpOutQ.size();
			}
		} else {
			if ( le->dllpType == LinkEvent::ACK ) {
				m_dllpAckOutQ.push(le);
			} else {
				m_dllpFcOutQ.push(le);
			}
		}
	}
}

/* 
 * Called by parent on a clock 
 * Returns whether anything sent this cycle
 */
bool PCIE_Link::clock() {

	if ( ! m_nackEventQ.empty() ) {
		m_dbg.debug( CALL_INFO,1,0,"(%s) Send To Parent: '%s'\n", getName().c_str(), m_nackEventQ.front()->getBriefString().c_str());
		(*recvHandler)( m_nackEventQ.front() );
		m_nackEventQ.pop();
	}

	SimTime_t now = getCurrentSimTimeNano();

	if ( m_oldestAck &&  m_oldestAck + m_ackTimeout < now ) {
		m_dbg.debug(CALL_INFO,1,0,"queue ACK DLLP\n");
		pushLinkEvent( new LinkEvent( LinkEvent::DLLP_type::ACK, m_dllpHdrLength ) );
		m_oldestAck = 0;
	}

	if ( m_numHdrCredits[RX][P] > m_maxHdrCredits * m_creditThreshold || m_numDataCredits[RX][P] > m_maxDataCredits * m_creditThreshold ) {
		m_dbg.debug(CALL_INFO,1,0,"queue Post FC DLLP\n");
		pushLinkEvent( new LinkEvent( LinkEvent::PH, m_numHdrCredits[RX][P], m_numDataCredits[RX][P], m_dllpHdrLength ) );
		resetCredits( RX, P );
	}
	if ( m_numHdrCredits[RX][NP] > m_maxHdrCredits * m_creditThreshold || m_numDataCredits[RX][NP] > m_maxDataCredits * m_creditThreshold ) {
		m_dbg.debug(CALL_INFO,1,0,"queue Non-Post FC DLLP\n");
		pushLinkEvent( new LinkEvent( LinkEvent::NPH, m_numHdrCredits[RX][NP], m_numDataCredits[RX][NP], m_dllpHdrLength ) );
		resetCredits( RX, NP );
	}
	if ( m_numHdrCredits[RX][CPL] > m_maxHdrCredits * m_creditThreshold || m_numDataCredits[RX][CPL] > m_maxDataCredits * m_creditThreshold ) {
		m_dbg.debug(CALL_INFO,1,0,"queue CPL FC DLLP\n");
		pushLinkEvent( new LinkEvent( LinkEvent::CPLH, m_numHdrCredits[RX][CPL], m_numDataCredits[RX][CPL], m_dllpHdrLength ) );
		resetCredits( RX, CPL );
	}
    return false;
}
