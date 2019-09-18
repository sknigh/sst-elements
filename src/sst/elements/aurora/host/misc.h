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


#ifndef COMPONENTS_AURORA_MISC_H
#define COMPONENTS_AURORA_MISC_H

#include "sst/elements/hermes/miscapi.h"
#include "host/host.h"

namespace SST {
namespace Aurora {

class Misc : public Hermes::Misc::Interface
{
  public:
  public:
    SST_ELI_REGISTER_SUBCOMPONENT_DERIVED(
        Misc,
        "aurora",
        "misc",
        SST_ELI_ELEMENT_VERSION(1,0,0),
        "",
        SST::Aurora::Misc 
    )

    Misc(Component* owner, Params&) : Interface(owner) {}
    Misc(ComponentId_t id, Params&) : Interface(id), m_os(NULL), m_callback(NULL) { 
		std::ostringstream tmp;
		tmp << this << "-AuroraMiscSelfLink";
		//printf("Misc::%s() %s\n",__func__,tmp.str().c_str());
		m_selfLink = configureSelfLink(tmp.str(), "1 ns", new Event::Handler<Misc>(this,&Misc::selfLinkHandler));
	}
    ~Misc() {}

    virtual void setup() {}
    virtual void finish() {}
    virtual std::string getName() { return "Misc"; }
    virtual std::string getType() { return "misc"; }

    virtual void setOS( Hermes::OS* os ) {
        m_os = static_cast<Host*>(os);
    }

    void getNumNodes( int* ptr, Hermes::Callback* callback ) { 
		assert( m_callback == NULL );
        *ptr = m_os->getNumNodes();
		m_callback = callback;
		m_selfLink->send(0,NULL);
    }

    void getNodeNum( int* ptr, Hermes::Callback* callback) { 
		assert( m_callback == NULL );
    	*ptr = m_os->getNodeNum();
		m_callback = callback;
		m_selfLink->send(0,NULL);
    }

    void malloc( Hermes::MemAddr* addr, size_t length, bool backed, Hermes::Callback* callback) { 
		//printf("Misc::%s() length=%zu\n",__func__,length);
		assert( m_callback == NULL );
    	m_os->malloc( addr, length, backed );
		m_callback = callback;
		m_selfLink->send(0,NULL);
    }

  private:

	void selfLinkHandler( Event* event ) {
		//printf("Misc::%s()\n",__func__);
		Hermes::Callback* tmp = m_callback;
		m_callback = NULL;
		(*tmp)(0);
		delete tmp;
	}

    Host*      m_os;
	Link* m_selfLink;
	Hermes::Callback* m_callback;
};

}
}

#endif
