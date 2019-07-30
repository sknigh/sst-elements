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

#ifndef COMPONENTS_AURORA_INFO_H
#define COMPONENTS_AURORA_INFO_H

#include "group.h"
#include "sst/elements/hermes/mpiPt2Pt.h"

using namespace Hermes;

namespace SST {
namespace Aurora {

class Info {
  public:
    Info() : m_currentGroupID(Mpi::CommWorld+1) {}
	~Info() {
    	std::map<Mpi::Comm, Group*>::iterator iter; 
		for ( iter = m_groupMap.begin(); iter != m_groupMap.end(); ++iter ) {
			delete (*iter).second;	
		}
	}

    enum GroupType { Dense, Identity, NetMap }; 
    Mpi::Comm newGroup( Mpi::Comm groupID, GroupType type = Dense ) {

        assert( m_groupMap.find( groupID ) == m_groupMap.end() );
        switch( type) {
          case Dense:
            m_groupMap[groupID] = new DenseGroup();
            break;
          case Identity:
            m_groupMap[groupID] = new IdentityGroup();
            break;
          case NetMap:
            m_groupMap[groupID] = new NetMapGroup();
            break;
        }
        return groupID;
    }

    Mpi::Comm newGroup( GroupType type = Dense ) {
        return newGroup( genGroupID(), type );
    }
    void delGroup( Mpi::Comm group ) {
        delete m_groupMap[ group ];
        m_groupMap.erase( group );
    }

    Group* getGroup( Mpi::Comm group ) {
		if ( m_groupMap.empty() ) return NULL;
        return m_groupMap[group];
    } 

    int worldRank() {
        if ( m_groupMap.empty() ) {
            return -1;
        } else {
            return m_groupMap[Mpi::CommWorld]->getMyRank();
        }
    }

  private: 

    Mpi::Comm genGroupID() {
        while ( m_groupMap.find(m_currentGroupID) != m_groupMap.end() ) {
            ++m_currentGroupID;
        }
        return m_currentGroupID;
    }

    std::map<Mpi::Comm, Group*> m_groupMap;
    Mpi::Comm    m_currentGroupID;
};

}
}
#endif
