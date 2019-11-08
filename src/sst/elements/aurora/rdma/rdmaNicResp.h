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

#ifndef COMPONENTS_AURORA_RDMA_NIC_RESP_H
#define COMPONENTS_AURORA_RDMA_NIC_RESP_H

#include <sst/core/event.h>
#include "sst/elements/hermes/rdma.h"

namespace SST {
namespace Aurora {
namespace RDMA {

#define FOREACH_RESP(NAME) \
        NAME(Retval)   \
        NAME(PostRecv)   \
        NAME(CheckRQ)   \
        NAME(RegisterMem)   \
        NAME(NumResps)  \

#define GENERATE_RESP_ENUM(ENUM) ENUM,
#define GENERATE_RESP_STRING(STRING) #STRING,

class NicResp : public SST::Event {
  public:

	enum Type { 
        FOREACH_RESP(GENERATE_RESP_ENUM)
    } type;

    NicResp() : Event() {}
	NicResp( Type type ) : Event(), type( type ) {}
	NicResp( Type type, int retval ) : Event(), type( type ), retval(retval) {}

	int retval;
	NotSerializable(SST::Aurora::RDMA::NicResp);
};


class RetvalResp : public NicResp {
  public:
	RetvalResp( int retval ) :
		NicResp(Retval,retval) {}

	NotSerializable(SST::Aurora::RDMA::RetvalResp);
};

class PostRecvResp : public NicResp {
  public:
	PostRecvResp( Hermes::RDMA::RecvBufId bufId, int retval = 0 ) :
		NicResp(PostRecv,retval), bufId(bufId) {}

	Hermes::RDMA::RecvBufId bufId;

	NotSerializable(SST::Aurora::RDMA::PostRecvResp);
};

class CheckRqResp : public NicResp {
  public:
	CheckRqResp( Hermes::RDMA::Status& status, int retval = 0 ) :
		NicResp(CheckRQ,retval), status(status) {}

	Hermes::RDMA::Status status;

	NotSerializable(SST::Aurora::RDMA::CheckRqResp);
};

class RegisterMemResp : public NicResp {
  public:
	RegisterMemResp( Hermes::RDMA::MemRegionId id, int retval = 0 ) :
		NicResp(RegisterMem,retval), id(id) {}

	Hermes::RDMA::MemRegionId id;

	NotSerializable(SST::Aurora::RDMA::CheckRqResp);
};

}
}
}

#endif
