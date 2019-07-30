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

#ifndef COMPONENTS_AURORA_NIC_RESP_QUEUE_H
#define COMPONENTS_AURORA_NIC_RESP_QUEUE_H

namespace SST {
namespace Aurora {

class NicRespQueue {
  public:
	  typedef std::function<void( Event* )> Callback;

	  NicRespQueue() : m_callback(NULL) {}

	  void push( Event* event ) {
		  //printf("%s():%d\n",__func__,__LINE__);
		  if ( m_callback ) {
			  m_callback( event );
			  m_callback = NULL;
		  }
	  }

	  void setWakeup( Callback callback ) {
		  //printf("%s():%d\n",__func__,__LINE__);
		  m_callback = callback;
	  }

  private:
	Callback m_callback;
};

}
}

#endif
