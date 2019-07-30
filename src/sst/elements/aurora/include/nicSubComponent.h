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
#include "include/nicEvents.h"

namespace SST {
namespace Aurora {

class NicSubComponent : public SubComponent {

  public:
    NicSubComponent( Component* owner ) : SubComponent(owner) {}
    virtual void setup() {}
    virtual void finish() {}

	virtual void init() {} 
	void setNetworkLink( Interfaces::SimpleNetwork* link ) { m_netLink= link; } 
	virtual void setNumCores( int num ) { m_toCoreLinks.resize(num); }
	void setCoreLink( int core, Link* link ) { m_toCoreLinks[core] = link; }
	int getNumCores() { return m_toCoreLinks.size(); }
	void setPktSize( int size ) { m_pktSize = size; } 


	Interfaces::SimpleNetwork& getNetworkLink( ) { return *m_netLink; }
	void sendResp( int core, Event* event ) {
		m_toCoreLinks[core]->send(0,new NicEvent( event) ); 
	}

	virtual void setNodeNum( int num ) { m_nodeNum = num; }
	virtual int getNodeNum( ) { return m_nodeNum; }
	virtual void handleEvent( int core, Event* ) = 0;
	virtual bool clockHandler( Cycle_t ) { assert(0); }
	static int getPktSize() { return m_pktSize; }

	virtual bool recvNotify( int vc ) { assert(0); } 
	virtual bool sendNotify( int vc ) { assert(0); } 

  private:
    std::vector<Link*>          m_toCoreLinks;
	Interfaces::SimpleNetwork*  m_netLink;
	int m_nodeNum;
	static int m_pktSize;
};

}
}

#endif
