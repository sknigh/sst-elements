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


#ifndef COMPONENTS_AURORA_HOST_COLLECTIVES_BARRIER_H
#define COMPONENTS_AURORA_HOST_COLLECTIVES_BARRIER_H

#include "./tree.h"
#include "./base.h"
#include "sst/elements/hermes/mpiPt2Pt.h"

namespace SST {
namespace Aurora {
namespace Collectives { 

class Barrier : public Base {
  public:
    Barrier( Mpi::Interface& api ) : Base( api ), m_tag( -10 ) {}
	inline void start( Tree* tree, Hermes::Mpi::Comm comm, Hermes::Callback* callback );
	inline void leaf() ;
	inline void sendDown( int );
	inline void postUp( int );
	inline void waitUp();
	inline void waitDown();
  private:
	Tree* m_tree;
	Hermes::Callback* m_callback;
	Hermes::Mpi::Comm m_comm;
	std::vector<Hermes::Mpi::Request> m_request;
	std::vector<Hermes::Mpi::Status> m_status;
	int m_tag;
	int m_flag;
};

void Barrier::start( Tree* tree, Hermes::Mpi::Comm comm, Hermes::Callback* callback )
{
	m_tree = tree;
	m_callback = callback;
	m_comm = comm;
	m_request.resize( m_tree->numChildren() );

	if ( m_tree->numChildren()  ) {
		m_status.resize( m_tree->numChildren() );
		postUp( m_tree->numChildren() );		
	} else {
		m_status.resize( 1 );
		leaf();
	}
}

void Barrier::sendDown( int count ) {
	Hermes::MemAddr addr;
    Hermes::Callback* cb;
	--count;
	if ( count ) {
 		cb = new Hermes::Callback;
    	*cb = [=](int retval ) {
			sendDown( count );		
		};
	} else {
		cb = m_callback;
	} 
	api().send( addr ,0, Mpi::Char, m_tree->children(count), m_tag, m_comm, cb ); 
}

void Barrier::postUp( int count ) {

	Hermes::MemAddr addr;
    Hermes::Callback* cb = new Hermes::Callback;
	--count;
	if ( count ) {
    	*cb = [=](int retval ) {
			postUp( count );		
		};
	} else {
    	*cb = [=](int retval ) {
			waitUp();
		};
	} 
	api().irecv( addr ,0, Mpi::Char, m_tree->children(count), m_tag, m_comm, &m_request[count], cb ); 
}

void Barrier::waitUp( ) {

    Hermes::Callback* cb = new Hermes::Callback;
    *cb = [=](int retval) {
		if ( m_tree->myPe() == m_tree->parent() ) {
			sendDown( m_tree->numChildren() );
		} else {
    		Hermes::Callback* cb = new Hermes::Callback;
    		*cb = [=](int retval) {
				sendDown( m_tree->numChildren() );
			};
			Hermes::MemAddr addr;
			api().send( addr ,0, Mpi::Char, m_tree->parent(), m_tag, m_comm, cb ); 
		} 	
	};

	api().testall( m_tree->numChildren(), &m_request[0], &m_flag, &m_status[0], true, cb );		
}

void Barrier::leaf( ) {
	
	Hermes::MemAddr addr;

    Hermes::Callback* cb = new Hermes::Callback;
    *cb = [=](int retval ) {
		// get response from parent;
		api().recv( addr ,0, Mpi::Char, m_tree->parent(), m_tag, m_comm, &m_status[0], m_callback ); 
    };

	// send to parrent
	api().send( addr ,0,Mpi::Char, m_tree->parent(), m_tag, m_comm, cb ); 
}

}
}
}

#endif

