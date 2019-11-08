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

#ifndef COMPONENTS_AURORA_NIC_CMD_QUEUE_H
#define COMPONENTS_AURORA_NIC_CMD_QUEUE_H

namespace SST {
namespace Aurora {

class NicCmdQueue {
  public:
	typedef std::function<void()> Callback;

	NicCmdQueue( Link* link, int size, int delay ) : m_link(link), m_size(size), m_current(0), m_delay(delay), m_callback(NULL) { } 

	void consumed() { 
		--m_current; 
		if ( m_callback ) {
			(*m_callback)();
			delete m_callback;
			m_callback = NULL;
		}
	}

	bool isBlocked() {
		return m_current == m_size;
	}

	void push( Event* event, bool delay ) {
		if ( delay ) {
			m_link->send( m_delay, event );
		} else {
			m_link->send( 0, event );
		}
		++m_current;
	}

	void setWakeup( Callback* callback ) {
		assert( NULL == m_callback );
		m_callback = callback;
	}

  private:

	Link* m_link;

	int m_delay;
	int m_current;
	int m_size;
	Callback* m_callback;
};

}
}

#endif
