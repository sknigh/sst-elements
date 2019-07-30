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


#ifndef COMPONENTS_AURORA_HOST_H
#define COMPONENTS_AURORA_HOST_H

#include <sst/core/output.h>
#include <sst/core/sharedRegion.h>

#include "sst/elements/hermes/hermes.h"
#include "host/nicCmdQueue.h"
#include "host/nicRespQueue.h"
#include "include/info.h"

namespace SST {
namespace Aurora {

class Host : public Hermes::OS 
{
  public:
    SST_ELI_REGISTER_SUBCOMPONENT(
        Host,
        "aurora",
        "host",
        SST_ELI_ELEMENT_VERSION(1,0,0),
        "",
        ""
    )

	Host(Component*, Params&);
	~Host() {}

	int getRank()    { 
		m_dbg.debug(CALL_INFO,1,2,"rank=%d\n",m_rank);
		return m_rank; 
	}

	int getNodeNum() { 
		m_dbg.debug(CALL_INFO,1,2,"nodeNum=%d\n",m_nodeNum);
		return m_nodeNum; 
	}

	int getNumNodes() {
		m_dbg.debug(CALL_INFO,1,2,"numNodes=%d\n",m_numNodes);
		return m_numNodes;
	}

	NicCmdQueue& cmdQ() {
		return *m_nicCmdQ;
	}

	NicRespQueue& respQ() {
		return *m_nicRespQ;
	}

	void malloc( Hermes::MemAddr* addr, size_t length, bool backed ) {
		m_dbg.debug(CALL_INFO,1,2,"m_mallocAddr=%" PRIx64 " length=%zu \n",m_mallocAddr,length);
		void* backing = NULL;
		if ( backed ) {
			backing = ::malloc( length );
		}
		*addr = Hermes::MemAddr( m_mallocAddr, backing  ); 
		m_mallocAddr += length; 
		m_mallocAddr += 64;
		m_mallocAddr &= ~(64-1);
	}

	int getWorldNumRanks() {
		int num = m_info.getGroup(Mpi::CommWorld)->getSize();
		m_dbg.debug(CALL_INFO,1,2,"num=%d\n",num);
		return num;
	}

	int getWorldRank() {
		int rank = m_info.worldRank();
		m_dbg.debug(CALL_INFO,1,2,"rank=%d\n",rank);
		return rank;
	}	

	int calcDestNid( int rank, Mpi::Comm comm ) {
		assert( comm == Mpi::CommWorld );

 		int nid = m_info.getGroup(comm)->getMapping( rank ) / m_numCores;


		m_dbg.debug(CALL_INFO,1,2,"comm=%d rank=%d -> nid=%d\n",comm,rank,nid);
		return nid;
	}

	int calcDestPid( int rank, Mpi::Comm comm ) {
		assert( comm == Mpi::CommWorld );

 		int pid = m_info.getGroup(comm)->getMapping( rank ) % m_numCores;
		m_dbg.debug(CALL_INFO,1,2,"pid=%d\n",pid);
		return pid ;
	}
	int getMyRank( Mpi::Comm comm ) {
		assert( comm == Mpi::CommWorld );

		int rank = m_info.getGroup(comm)->getMyRank();
		m_dbg.debug(CALL_INFO,1,2,"rank=%d\n",rank);
		return rank;
	}

	NicCmdQueue* m_nicCmdQ;
  private:

	void _componentInit( unsigned int phase ); 
	void _componentSetup( ); 
	void handleEvent( Event* ev );

	NicRespQueue* m_nicRespQ;

	Link*	m_nicLink;
    Output	m_dbg;

	int m_nodeNum;
	int m_numNodes;

	int m_coreId;
	int m_numCores;

	int m_rank;
	uint64_t m_mallocAddr;

	SharedRegion* m_sreg;
	int m_netMapSize;
	std::string m_netMapName;

	Info                m_info;
};

}
}

#endif
