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


#ifndef COMPONENTS_AURORA_NIC_H
#define COMPONENTS_AURORA_NIC_H

#include <sst/core/component.h>
#include <sst/core/output.h>
#include <sst/core/interfaces/simpleNetwork.h>
#include <sst/core/link.h>
#include "include/nicSubComponent.h"

namespace SST {
namespace Aurora {

class Nic : public SST::Component  {

  public:
    SST_ELI_REGISTER_COMPONENT(
        Nic,
        "aurora",
        "nic",
        SST_ELI_ELEMENT_VERSION(1,0,0),
        "",
        COMPONENT_CATEGORY_SYSTEM
        )
    SST_ELI_DOCUMENT_PARAMS(
        { "nid", "node id on network", "-1"},
        { "verboseLevel", "Sets the output verbosity of the component", "0"},
        { "verboseMask", "Sets the output mask of the component", "-1"},
		{ "numCores", "Sets number of cores", "1"},
        { "packetSize", "Sets the size of the network packet in bytes", "64"},
        { "input_buf_size", "Sets the buffer size of the link connected to the router", "128"},
        { "output_buf_size", "Sets the buffer size of the link connected to the router", "128"},
        { "link_bw", "Sets the bandwidth of link connected to the router", "500Mhz"},
		{ "module", "Sets the link control module", "merlin.linkcontrol"},
        { "rtrPortName", "Port connected to the router", "rtr"},
        { "corePortName", "Port connected to the core", "core"},
	)

    SST_ELI_DOCUMENT_PORTS(
        {"rtr", "Port connected to the router", {}},
        {"core%(num_vNics)d", "Ports connected to the network driver", {}},
    )
 
  public:
	Nic(ComponentId_t, Params& );
	~Nic() {}
	void setup() { if ( m_nicSC ) m_nicSC->setup(); }
	void finish() { if ( m_nicSC ) m_nicSC->finish(); }

	void init( unsigned int phase );

	void registerRecvNotify() {
		m_linkControl->setNotifyOnReceive( m_recvNotifyFunctor );
	}
	void registerSendNotify() {
		m_linkControl->setNotifyOnSend( m_sendNotifyFunctor );
	}


  private:

	void handleCoreEvent( Event* ev, int core ) { 
    	m_nicSC->handleEvent( core, ev );
	}

    bool recvNotify(int vc) {
        m_dbg.debug(CALL_INFO,2,1,"network event available vc=%d\n",vc);
		return m_nicSC->recvNotify( vc );
    }

    bool sendNotify(int vc) {
        m_dbg.debug(CALL_INFO,2,1,"network event available vc=%d\n",vc);
		return m_nicSC->sendNotify( vc );
    }


    SST::Interfaces::SimpleNetwork::Handler<Nic>* m_recvNotifyFunctor;
    SST::Interfaces::SimpleNetwork::Handler<Nic>* m_sendNotifyFunctor;
	Interfaces::SimpleNetwork*  m_linkControl;
	NicSubComponent*			m_nicSC;

	std::vector<Link*>          m_toCoreLinks;

	Output	m_dbg;
	int		m_numCores;
	int		m_nodeId;
};

}
}

#endif
