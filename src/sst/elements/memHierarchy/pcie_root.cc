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
#include "pcie_link.h"

using namespace SST::MemHierarchy;

PCIE_Root::PCIE_Root(ComponentId_t id, Params &params) : Component(id), m_privateMemOffset(0), m_lastSend(0)
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

    // Memory region - overwrite with what we got if we got some
    bool found;
    bool gotRegion = false;
    uint64_t addrStart = params.find<uint64_t>("addr_range_start", 0, gotRegion);
    uint64_t addrEnd = params.find<uint64_t>("addr_range_end", (uint64_t) - 1, found);
    gotRegion |= found;
    string ilSize = params.find<std::string>("interleave_size", "0B", found);
    gotRegion |= found;
    string ilStep = params.find<std::string>("interleave_step", "0B", found);
    gotRegion |= found;

    // Ensure SI units are power-2 not power-10 - for backward compability
    fixByteUnits(ilSize);
    fixByteUnits(ilStep);

    if (!UnitAlgebra(ilSize).hasUnits("B")) {
        dbg.fatal(CALL_INFO, -1, "Invalid param(%s): interleave_size - must be specified in bytes with units (SI units OK). For example, '1KiB'. You specified '%s'\n",
                getName().c_str(), ilSize.c_str());
    }

    if (!UnitAlgebra(ilStep).hasUnits("B")) {
        dbg.fatal(CALL_INFO, -1, "Invalid param(%s): interleave_step - must be specified in bytes with units (SI units OK). For example, '1KiB'. You specified '%s'\n",
                getName().c_str(), ilSize.c_str());
    }

    m_region.start = addrStart;
    m_region.end = addrEnd;
    m_region.interleaveSize = UnitAlgebra(ilSize).getRoundedValue();
    m_region.interleaveStep = UnitAlgebra(ilStep).getRoundedValue();

	printf("%s start=%" PRIu64 " stop=%" PRIx64 " iLSize=%" PRIu64 " iLStep=%" PRIu64 "\n",getName().c_str(),
						m_region.start, m_region.end, m_region.interleaveSize, m_region.interleaveStep );

	m_memLink->setRegion(m_region);

    m_clockLink = m_memLink->isClocked();
    m_memLink->setRecvHandler( new Event::Handler<PCIE_Root>(this, &PCIE_Root::handleTargetEvent));
    m_memLink->setName(getName());

    // Clock handler
    std::string clockFreq = params.find<std::string>("clock", "1GHz");
    m_clockHandler = new Clock::Handler<PCIE_Root>(this, &PCIE_Root::clock);
    m_clockTC = registerClock( clockFreq, m_clockHandler );

    m_opRate = registerStatistic<uint64_t>("opRate");
    m_opLatencyRead = registerStatistic<uint64_t>("opLatencyRead");
    m_opLatencyWrite = registerStatistic<uint64_t>("opLatencyWrite");
    m_bytesRead = registerStatistic<uint64_t>("bytesRead");
    m_bytesWritten = registerStatistic<uint64_t>("bytesWritten");
}

void PCIE_Root::handleTargetEvent(SST::Event* event) {

    MemEvent *me = static_cast<MemEvent*>(event);

    dbg.debug( CALL_INFO,1,0,"(%s) Received on memLink: '%s'\n", getName().c_str(), me->getBriefString().c_str());

	if (me->isAddrGlobal()) {
        me->setBaseAddr(translateToLocal(me->getBaseAddr()));
        me->setAddr(translateToLocal(me->getAddr()));
    }

    if ( me->isResponse() ) {
        bool last = handleResponseFromHost( me );
        if ( me->getCmd() == Command::NACK ) {
            handleNackFromHost( me, last );
        } else {
            m_toDevEventQ.push(me);
        }
    } else {
        m_toDevEventQ.push(me);
    }
}

bool PCIE_Root::handleResponseFromHost(MemEvent* me)
{
	bool last = false;
    try {
        auto& q = m_devOriginPendingMap.at(me->getAddr());
        auto iter = q.begin();
        int pos = 0;
        for ( ; iter != q.end(); ++iter ) {
            if ( *iter == me->getID() ) {
                q.erase(iter);
                break;
            }
            ++pos;
        }
		last = q.size() == pos;

        dbg.debug( CALL_INFO,1,0,"addr=%" PRIx64 " erased event %d of %zu\n", me->getAddr(), pos, q.size() );

        if ( q.empty() ) {
            m_devOriginPendingMap.erase(me->getAddr());
        }

    } catch (const std::out_of_range& oor) {
        out.fatal(CALL_INFO,-1,"Can't find request\n");
    }
	return last;
}

void PCIE_Root::handleNackFromHost(MemEvent* me, bool last )
{
	MemEvent* orig =  me->getNACKedEvent();

    if ( last || isRead( orig ) ) {
        dbg.debug( CALL_INFO,1,0,"(%s) Resend on memLink: '%s'\n", getName().c_str(), orig->getBriefString().c_str());
		auto& q = m_devOriginPendingMap.at(orig->getAddr());
        q.push_back( orig->getID() );
        m_toHostEventQ.push( orig );
    } else {
        dbg.debug(CALL_INFO,1,0,"drop request: %s %s\n",  getName().c_str(), orig->getBriefString().c_str() );
        delete orig;
    }
    delete me;
}

void PCIE_Root::handlePCI_linkEvent(SST::Event* event)
{
    MemEvent *me = static_cast<MemEvent*>(event);

   	dbg.debug( CALL_INFO,1,0,"(%s) Send on memLink: '%s'\n", getName().c_str(), me->getBriefString().c_str());

	m_toHostEventQ.push( me );
}

bool PCIE_Root::clock(Cycle_t cycle)
{
	// unclocking not currently supported
    bool unclock = m_pciLink->clock();
    unclock = m_memLink->clock();

	if ( ! m_toHostEventQ.empty() ) {
		MemEvent* ev = static_cast<MemEvent*>(m_toHostEventQ.front());

		if ( ev->isResponse() ) {
			auto it = m_hostOriginPendingMap.find( m_toHostEventQ.front()->getID() );
			updateStatistics( it->second.isRead, getCurrentSimTimeNano() - it->second.time );
			m_hostOriginPendingMap.erase( m_toHostEventQ.front()->getID() );
		} else {
			ev->setDst(m_memLink->findTargetDestination(ev->getAddr()));
			ev->setSrc(getName());
			m_devOriginPendingMap[ev->getAddr()].push_back( ev->getID() );
		}

        if (ev->isAddrGlobal()) {
            ev->setBaseAddr(translateToGlobal(ev->getBaseAddr()));
            ev->setAddr(translateToGlobal(ev->getAddr()));
        }

        dbg.debug( CALL_INFO,1,0,"(%s) Send on memLink: '%s'\n", getName().c_str(), m_toHostEventQ.front()->getBriefString().c_str());
		m_memLink->send( m_toHostEventQ.front() );
		m_toHostEventQ.pop();
	}

	if ( ! m_toDevEventQ.empty() && m_pciLink->spaceToSend( m_toDevEventQ.front() ) ) {
		MemEventBase* ev = m_toDevEventQ.front();
		m_toDevEventQ.pop();
		m_pciLink->send( ev );
		SimTime_t now = getCurrentSimTimeNano();

		m_hostOriginPendingMap.insert( std::make_pair(ev->getID(), Entry( ev, now, isRead( static_cast<MemEvent*>(ev) ) ) ) );

		m_opRate->addData( now - m_lastSend );
		m_lastSend = now;
        if ( isRead(static_cast<MemEvent*>(ev)) ) { 
            m_bytesRead->addData( static_cast<MemEvent*>(ev)->getPayload().size() );
        } else {
            m_bytesWritten->addData( static_cast<MemEvent*>(ev)->getPayload().size() );
        }
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
        m_memLink->sendInitData(new MemEventInitCoherence(getName(), Endpoint::Memory, true, false, static_cast<PCIE_Link*>(m_pciLink)->getMtuLength(), false));
    }

    while (MemEventInit *ev = m_memLink->recvInitData()) {
        processInitEvent(ev);
    }
}

void PCIE_Root::processInitEvent( MemEventInit* me ) {
    /* Push data to memory */
    if (Command::GetX == me->getCmd()) {
        me->setAddr(translateToLocal(me->getAddr()));
        dbg.debug( CALL_INFO,1,0,"Memory init %s - Received GetX for %" PRIx64 " size %zu\n", getName().c_str(), me->getAddr(),me->getPayload().size()); 
		assert(0);
#if 0
        Addr addr = me->getAddr();
        if ( isRequestAddressValid(addr) && backing_ ) {
            backing_->set(addr, me->getPayload().size(), me->getPayload());
        }
#endif
    } else if (Command::NULLCMD == me->getCmd()) {
        dbg.debug( CALL_INFO,1,0,"Memory (%s) received init event: %s\n", getName().c_str(), me->getVerboseString().c_str());
    } else {
        dbg.debug( CALL_INFO,1,0,"Memory received unexpected Init Command: %d\n", (int)me->getCmd());
    }

    delete me;
}

/* Translations assume interleaveStep is divisible by interleaveSize */
Addr PCIE_Root::translateToLocal(Addr addr) {
    Addr rAddr = addr;
    if (m_region.interleaveSize == 0) {
        rAddr = rAddr - m_region.start + m_privateMemOffset;
    } else {
        Addr shift = rAddr - m_region.start;
        Addr step = shift / m_region.interleaveStep;
        Addr offset = shift % m_region.interleaveStep;
        rAddr = (step * m_region.interleaveSize) + offset + m_privateMemOffset;
    }
    dbg.debug( CALL_INFO,1,0,"Converting global address 0x%" PRIx64 " to local address 0x%" PRIx64 "\n", addr, rAddr);
    return rAddr;
}

Addr PCIE_Root::translateToGlobal(Addr addr) {
    Addr rAddr = addr - m_privateMemOffset;
    if (m_region.interleaveSize == 0) {
        rAddr += m_region.start;
    } else {
        Addr offset = rAddr % m_region.interleaveSize;
        rAddr -= offset;
        rAddr = rAddr / m_region.interleaveSize;
        rAddr = rAddr * m_region.interleaveStep + offset + m_region.start;
    }
    dbg.debug( CALL_INFO,1,0,"Converting local address 0x%" PRIx64 " to global address 0x%" PRIx64 "\n", addr, rAddr);
    return rAddr;
}

void PCIE_Root::setup(void) {
    m_memLink->setup();
    m_pciLink->setup();
}


void PCIE_Root::finish(void) {
    m_memLink->finish();
    m_pciLink->finish();
}
