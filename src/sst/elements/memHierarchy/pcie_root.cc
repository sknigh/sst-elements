// Copyright 2009-2019 NTESS. Under the terms
// of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.
//
// Copyright (c) 2009-2019, NTESS
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
#include <sst/core/params.h>
#include <sst/core/simulation.h>

#include "pcie_root.h"

using namespace SST::MemHierarchy;

PCIE_Root::PCIE_Root(ComponentId_t id, Params &params) : Component(id), m_maxToCpu(0), m_maxToDev(0), m_numDropped(0),m_numPrWrites(0), m_numPrReads(0)
{

    // Output for debug
    char buffer[100];
    snprintf(buffer,100,"@t:PCIE_Root::@p():@l ");
    dbg.init( buffer, 
        params.find<int>("debug_level", 0),
        params.find<int>("debug_mask",0),
        (Output::output_location_t)params.find<int>("debug", 0));

    m_memLink = loadUserSubComponent<MemLinkBase>("cpulink");

    string link_lat         = params.find<std::string>("direct_link_latency", "10 ns");

    if (!m_memLink && isPortConnected("direct_link")) {
        Params linkParams = params.find_prefix_params("cpulink.");
        linkParams.insert("port", "direct_link");
        linkParams.insert("latency", link_lat, false);
        linkParams.insert("accept_region", "1", false);
        m_memLink = loadAnonymousSubComponent<MemLinkBase>("memHierarchy.MemLink", "cpulink", 0, ComponentInfo::SHARE_PORTS | ComponentInfo::INSERT_STATS, linkParams);
    } else if (!m_memLink) {

        if (!isPortConnected("network")) {
            out.fatal(CALL_INFO,-1,"%s, Error: No connected port detected. Connect 'direct_link' or 'network' port.\n", getName().c_str());
        }

        Params nicParams = params.find_prefix_params("memNIC.");
        nicParams.insert("group", "4", false);
        nicParams.insert("accept_region", "1", false);

        if (isPortConnected("network_ack") && isPortConnected("network_fwd") && isPortConnected("network_data")) {
            nicParams.insert("req.port", "network");
            nicParams.insert("ack.port", "network_ack");
            nicParams.insert("fwd.port", "network_fwd");
            nicParams.insert("data.port", "network_data");
            m_memLink = loadAnonymousSubComponent<MemLinkBase>("memHierarchy.MemNICFour", "cpulink", 0, ComponentInfo::SHARE_PORTS | ComponentInfo::INSERT_STATS, nicParams);
        } else {
            nicParams.insert("port", "network");
            m_memLink = loadAnonymousSubComponent<MemLinkBase>("memHierarchy.MemNIC", "cpulink", 0, ComponentInfo::SHARE_PORTS | ComponentInfo::INSERT_STATS, nicParams);
        }
    }

	Params linkParams = params.find_prefix_params("pcilink.");
    m_pciLink = loadUserSubComponent<MemLinkBase>("pcilink");
    m_pciLink->setRecvHandler( new Event::Handler<PCIE_Root>(this, &PCIE_Root::handlePCI_linkEvent));

	m_ioBaseAddr = params.find<uint64_t>( "addr_range_start", 0x100000000 );
	m_ioLength = params.find<size_t>( "addr_range_length", 0x100000000 );

    m_region.start = m_ioBaseAddr;
    m_region.end = m_ioBaseAddr + m_ioLength;
    m_region.interleaveSize = 0;
    m_region.interleaveStep = 0;
	m_memLink->setRegion(m_region);

    m_clockLink = m_memLink->isClocked();
    m_memLink->setRecvHandler( new Event::Handler<PCIE_Root>(this, &PCIE_Root::handleTargetEvent));
    m_memLink->setName(getName());

    // Clock handler
    std::string clockFreq = params.find<std::string>("clock", "1GHz");
    m_clockHandler = new Clock::Handler<PCIE_Root>(this, &PCIE_Root::clock);
    m_clockTC = registerClock( clockFreq, m_clockHandler );
}

void PCIE_Root::handleTargetEvent(SST::Event* event) {

    MemEvent *me = static_cast<MemEvent*>(event);

    dbg.debug( CALL_INFO,1,0,"(%s) Received on memLink: '%s'\n", getName().c_str(), me->getBriefString().c_str());
	
	if ( me->isResponse() ) {
        try {
            auto& q = m_pendingMap.at(me->getAddr());
            auto iter = q.begin();
            int pos = 0;
            for ( ; iter != q.end(); ++iter ) {
                if ( *iter == me->getID() ) {
                    q.erase(iter);
                    break;
                }
                ++pos;
            }
            dbg.debug( CALL_INFO,1,0,"pos=%d\n",pos);

            if ( me->getCmd() ==  Command::NACK ) {
                MemEvent* orig =  me->getNACKedEvent();
                if ( Command::GetS == orig->getCmd() || Command::PrRead == orig->getCmd() || q.size() == pos ) { 
                    dbg.debug( CALL_INFO,1,0,"(%s) Resend on memLink: '%s'\n", getName().c_str(), orig->getBriefString().c_str());
                    q.push_back( orig->getID() );
                    m_toCpuEventQ.push( orig );
                } else {
                    ++m_numDropped;
                    dbg.debug(CALL_INFO,1,0,"drop request: %s %s, %d %zu\n",  getName().c_str(), orig->getBriefString().c_str(), pos, q.size() );
                    delete orig;
                }
                delete me;
                return;
            } else {
                dbg.debug( CALL_INFO,1,0,"(%s) Clear: '%s'\n", getName().c_str(), me->getBriefString().c_str());
            }

            if ( q.empty() ) {
                m_pendingMap.erase(me->getAddr());
            }

        } catch (const std::out_of_range& oor) {
            out.fatal(CALL_INFO,-1,"Can't find request\n");
        }
    }

    m_toDevEventQ.push(me);
}

void PCIE_Root::handlePCI_linkEvent(SST::Event* event) {
    MemEvent *me = static_cast<MemEvent*>(event);
	if ( ! me->isResponse() ) {
		me->setDst(m_memLink->findTargetDestination(me->getAddr()));
		me->setSrc(getName());
		m_pendingMap[me->getAddr()].push_back( me->getID() );
	}
   	dbg.debug( CALL_INFO,1,0,"(%s) Send on memLink: '%s'\n", getName().c_str(), me->getBriefString().c_str());
	switch ( me->getCmd() ) {
		case Command::PrWrite:
			++m_numPrWrites;
			break;
		case Command::PrRead:
			++m_numPrReads;
			break;
		default:
			break;
	}

	m_toCpuEventQ.push( me );
}

bool PCIE_Root::clock(Cycle_t cycle) {

    bool unclock = m_pciLink->clock();
    unclock = m_memLink->clock();

	if ( m_toCpuEventQ.size() > m_maxToCpu ) {
		++m_maxToCpu;
	}
	if ( m_toDevEventQ.size() > m_maxToDev ) {
		++m_maxToDev;
	}
	if ( ! m_toCpuEventQ.empty() ) {
		m_memLink->send( m_toCpuEventQ.front() );
		m_toCpuEventQ.pop();
	}

	if ( ! m_toDevEventQ.empty() && m_pciLink->spaceToSend( m_toDevEventQ.front() ) ) {
		m_pciLink->send( m_toDevEventQ.front() );
		m_toDevEventQ.pop();
	}

    return false;
}

void PCIE_Root::init(unsigned int phase) {

    m_memLink->init(phase);
    m_pciLink->init(phase);

    m_region = m_memLink->getRegion(); // This can change during init, but should stabilize before we start receiving init data

    /* Inherit region from our source(s) */
    if (!phase) {
        /* Announce our presence on link */
		int requestWidth = 8;
        m_memLink->sendInitData(new MemEventInitCoherence(getName(), Endpoint::Memory, true, false, requestWidth, false));
    }

    while (MemEventInit *ev = m_memLink->recvInitData()) {
        if (ev->getCmd() == Command::NULLCMD) {
            if (ev->getInitCmd() == MemEventInit::InitCommand::Coherence) {
                MemEventInitCoherence * mEv = static_cast<MemEventInitCoherence*>(ev);
                if ( m_cacheLineSize == 0 ) {
                    m_cacheLineSize = mEv->getLineSize();
                }
                assert( m_cacheLineSize == mEv->getLineSize() ); 
            }
            delete ev;
        } else {
            assert(0);
        }
    }
}

void PCIE_Root::setup(void) {
    m_memLink->setup();
    m_pciLink->setup();
}


void PCIE_Root::finish(void) {
    m_memLink->finish();
    m_pciLink->finish();
}
