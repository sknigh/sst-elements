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

#include <sst_config.h>
#include "host.h"

#include "host/misc.h"

#include "include/nicEvents.h"

using namespace SST;
using namespace SST::Aurora;

Host::Host( ComponentId_t owner, Params& params ) :
	OS( owner, params ), m_rank(-1), m_nodeNum(-1), m_mallocAddr( 0x1000 )
{
	if ( params.find<bool>("print_all_params",false) ) {
        printf("Aurora::Host::Host()\n");
        params.print_all_params(std::cout);
	}

	m_dbg.init("@t:Aurora::Host::@p():@l ", params.find<uint32_t>("verboseLevel",0),
			params.find<uint32_t>("verboseMask",0), Output::STDOUT );

	m_numNodes = params.find<int>("numNodes",0);

	m_dbg.debug(CALL_INFO,1,2,"numNodes=%d\n",m_numNodes);

	m_nicLink = configureLink( params.find<std::string>("portName","nic"),
            "1 ns", new Event::Handler<Host>(this,&Host::handleEvent) );
	assert( m_nicLink );

	m_nicCmdQ = new NicCmdQueue( m_nicLink, params.find<int>("m_maxNicQdepth",32) );
	m_nicRespQ = new NicRespQueue();


	m_netMapSize = params.find<int>("netMapSize",-1);
    assert(m_netMapSize > -1 );

    if ( m_netMapSize > 0 ) {

        int netId = params.find<int>("netId",-1);
        int netMapId = params.find<int>("netMapId",-1);

        if ( -1 == netMapId ) {
            netMapId = netId;
        }

        m_dbg.debug(CALL_INFO,1,2,"netId=%d netMapId=%d netMapSize=%d\n",
            netId, netMapId, m_netMapSize );

        m_netMapName = params.find<std::string>( "netMapName" );
        assert( ! m_netMapName.empty() );

        m_sreg = getGlobalSharedRegion( m_netMapName,
                    m_netMapSize*sizeof(int), new SharedRegionMerger());

        if ( 0 == params.find<int>("coreId",0) ) {
            m_sreg->modifyArray( netMapId, netId );
        }

        m_sreg->publish();
    }
}

void Host::_componentInit( unsigned int phase ) {
	m_dbg.debug(CALL_INFO,1,2,"phase=%d\n",phase);	

    if ( 1 == phase ) {
        NicInitEvent* ev = static_cast<NicInitEvent*>(m_nicLink->recvInitData());
        assert( ev );
        m_nodeNum = ev->nodeNum;
        m_coreId = ev->coreId;
        m_numCores = ev->numCores;
        delete ev;

		m_rank = m_nodeNum * m_numCores + m_coreId;

        char buffer[100];
        snprintf(buffer,100,"@t:%d:%d:Aurora::Host::@p():@l ", m_nodeNum, m_coreId );
        m_dbg.setPrefix( buffer );

        m_dbg.debug(CALL_INFO,1,2,"we are nic=%d core=%d\n", m_nodeNum, m_coreId );
    }
}

void Host::_componentSetup()
{
    m_dbg.debug(CALL_INFO,1,1,"nodeId %d numCores %d, coreNum %d\n", m_nodeNum, m_numCores, m_coreId);

    if ( m_netMapSize > 0 ) {
        Group* group = m_info.getGroup( m_info.newGroup( Mpi::CommWorld, Info::NetMap ) );

    	m_dbg.debug(CALL_INFO,1,1,"netMapSize=%d\n", m_netMapSize);
        group->initMapping( m_sreg->getPtr<const int*>(), m_netMapSize, m_numCores );

        int nid = m_nodeNum;

        for ( int i =0; i < group->getSize(); i++ ) {
            m_dbg.debug(CALL_INFO,1,2,"rank %d\n", i);
            if ( nid == group->getMapping( i ) ) {
              	m_dbg.debug(CALL_INFO,1,2,"nid %d core %d -> rank %d\n", nid, m_coreId, (nid * m_numCores) + m_coreId );
               	group->setMyRank( (nid * m_numCores) + m_coreId );
				break;
            }
        }

        m_dbg.debug(CALL_INFO,1,2,"nid %d, numRanks %u, myRank %u \n",
                                nid, group->getSize(),group->getMyRank() );
    }

    char buffer[100];
    snprintf(buffer,100,"@t:%#x:%d:Hades::@p():@l ", m_nodeNum, m_rank);
    m_dbg.setPrefix(buffer);
}

void Host::handleEvent( Event* ev )
{
	m_dbg.debug(CALL_INFO,2,0,"\n");
	NicEvent* event = static_cast<NicEvent*>(ev);
	switch( event->type ) {
	  case NicEvent::Credit:
		// we don't currently use this case
		assert(0);	
		m_dbg.debug(CALL_INFO,1,0,"\n");
		m_nicCmdQ->consumed();
		break;	
	  case NicEvent::Payload:
		m_nicRespQ->push( event->payload );
		m_nicCmdQ->consumed();
		break;	
	}
	delete ev;
}
