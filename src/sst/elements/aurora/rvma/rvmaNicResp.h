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

#ifndef COMPONENTS_AURORA_RVMA_NIC_RESP_H
#define COMPONENTS_AURORA_RVMA_NIC_RESP_H

#include "sst/elements/hermes/rvma.h"

namespace SST {
namespace Aurora {
namespace RVMA {

#define FOREACH_RESP(NAME) \
        NAME(Retval)   \
        NAME(InitWindow)   \
        NAME(WinGetEpoch)   \
        NAME(WinGetBufPtrs)   \
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


	NotSerializable(NicResp);
};


class RetvalResp : public NicResp {
  public:
	RetvalResp( int retval ) :
		NicResp(Retval), retval(retval) {}

	NotSerializable(RetvalResp);

	int retval;
};

class InitWindowResp : public NicResp {
  public:
	InitWindowResp( Hermes::RVMA::Window window ) :
		NicResp(InitWindow), window(window) {}

	Hermes::RVMA::Window window;

	NotSerializable(InitWindowResp);
};

class WinGetEpochResp : public NicResp {
  public:
	WinGetEpochResp( int epoch ) :
		NicResp(WinGetEpoch), epoch(epoch) {}

	int epoch;

	NotSerializable(WinGetEpochResp);
};

class WinGetBufPtrsResp : public NicResp {
  public:
	WinGetBufPtrsResp( ) :
		NicResp(WinGetBufPtrs)  {}

	std::vector< Hermes::RVMA::Completion > completions;

	NotSerializable(WinGetEpochResp);
};

}
}
}

#endif
