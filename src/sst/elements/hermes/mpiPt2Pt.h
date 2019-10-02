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

#ifndef _H_HERMES_MpiPt2Pt_INTERFACE
#define _H_HERMES_MpiPt2Pt_INTERFACE

#include <assert.h>

#include "hermes.h"

namespace SST {
namespace Hermes {
namespace Mpi {

typedef Hermes::Callback Callback;
typedef int Comm;

struct Status {
	int rank;
	int tag;
};

class RequestData {
  protected:
	RequestData() {}
	RequestData( const RequestData & ) {}
	RequestData &operator=(const RequestData& ) { assert(0); }  
  public:
	virtual ~RequestData() = default; 
};

struct Request {
	enum Type { Send, Recv } type;
	RequestData *entry;
};

enum DataType { Char, Int, Long, Float, Double, Complex };
typedef Hermes::MemAddr MemAddr;

const int CommWorld = 0; 
const int AnyTag = -1;
const int AnySrc = -1;

static inline   unsigned sizeofDataType( Mpi::DataType type ) {
        switch( type ) {
        case Mpi::Char:
            return sizeof( char );
        case Mpi::Int:
            return sizeof( int );
        case Mpi::Long:
            return sizeof( long );
        case Mpi::Double:
            return sizeof( double);
        case Mpi::Float:
            return sizeof( float );
        case Mpi::Complex:
            return sizeof( double ) * 2;
        default:
            assert(0);
        }
    }

class Interface : public Hermes::Interface {
  public:
    SST_ELI_REGISTER_SUBCOMPONENT_API(SST::Hermes::Mpi::Interface)

    Interface( Component* parent ) : Hermes::Interface(parent) {}
    Interface( ComponentId_t id ) : Hermes::Interface(id) {}
	virtual ~Interface() {}
	virtual void init( int* numRanks, int* myRank, Callback* ) = 0;

    virtual void send( const Mpi::MemAddr&, int count, Mpi::DataType, int dest, int tag, Mpi::Comm,Callback* ) = 0;
    virtual void recv( const Mpi::MemAddr&, int count, Mpi::DataType, int src, int tag, Mpi::Comm, Mpi::Status*, Callback* ) = 0;
	virtual void isend( const Mpi::MemAddr& buf, int count, Mpi::DataType, int dest, int tag, Mpi::Comm, Mpi::Request*, Callback* ) = 0;
	virtual void irecv( const Mpi::MemAddr& buf, int count, Mpi::DataType ,int src, int tag, Mpi::Comm, Mpi::Request*, Callback* ) = 0;
	virtual void test( Mpi::Request*, Mpi::Status*, bool blocking, Callback* ) = 0;
	virtual void testall( int count, Mpi::Request*, int* flag, Mpi::Status*, bool blocking, Callback* ) = 0;
	virtual void testany( int count, Mpi::Request*, int* indx, int* flag, Mpi::Status*, bool blocking, Callback* ) = 0;
};

}
}
}

#endif

