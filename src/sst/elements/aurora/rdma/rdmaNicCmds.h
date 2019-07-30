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

#ifndef COMPONENTS_AURORA_RDMA_NIC_CMDS_H
#define COMPONENTS_AURORA_RDMA_NIC_CMDS_H

#include "sst/elements/hermes/rdma.h"

namespace SST {
namespace Aurora {
namespace RDMA {

#define FOREACH_CMD(NAME) \
        NAME(CreateRQ) \
        NAME(PostRecv) \
        NAME(Send) \
        NAME(CheckRQ) \
        NAME(RegisterMem) \
        NAME(Read) \
        NAME(Write) \
        NAME(NumCmds) \

#define GENERATE_CMD_ENUM(ENUM) ENUM,
#define GENERATE_CMD_STRING(STRING) #STRING,

class NicCmd : public SST::Event {
  public:

	enum Type { 
        FOREACH_CMD(GENERATE_CMD_ENUM)
    } type;

    NicCmd() : Event() {}
	NicCmd( Type type ) : Event(), type( type ) {}

    NotSerializable(SST::Aurora::RDMA::NicCmd);
};

class CreateRqCmd : public NicCmd {
  public:
	CreateRqCmd( Hermes::RDMA::RqId rqId ) :
		NicCmd( CreateRQ ), rqId(rqId) {}
	Hermes::RDMA::RqId rqId;

    NotSerializable(SST::Aurora::RDMA::CreateRqCmd);
};

class PostRecvCmd : public NicCmd {
  public:
	PostRecvCmd( Hermes::RDMA::RqId rqId, Hermes::MemAddr& addr, size_t length ) :
		NicCmd( PostRecv ), rqId(rqId), addr(addr), length(length) {}
	Hermes::RDMA::RqId rqId;
	Hermes::MemAddr addr;
	size_t length;
    NotSerializable(SST::Aurora::RDMA::PostRecvCmd);
};

class SendCmd : public NicCmd {
  public:
	SendCmd( Hermes::ProcAddr proc, Hermes::RDMA::RqId rqId, Hermes::MemAddr& src, size_t length ) :
		NicCmd( Send ), proc(proc), rqId(rqId), src(src), length(length){} 

	Hermes::ProcAddr proc;
	Hermes::RDMA::RqId rqId;
	Hermes::MemAddr src;
	size_t length;
    NotSerializable(SST::Aurora::RDMA::SendCmd);
};

class CheckRqCmd : public NicCmd {
  public:
	CheckRqCmd( Hermes::RDMA::RqId rqId, bool blocking ) :
		NicCmd( CheckRQ ), rqId(rqId), blocking(blocking) {}

	Hermes::RDMA::RqId rqId;
	bool blocking;
    NotSerializable(SST::Aurora::RDMA::CheckRqCmd);
};

class RegisterMemCmd : public NicCmd {
  public:
	RegisterMemCmd( Hermes::MemAddr& addr, size_t length  ) :
		NicCmd( RegisterMem ), addr(addr), length(length) {}

	Hermes::MemAddr addr;
	size_t length;
    NotSerializable(SST::Aurora::RDMA::RegisterMemCmd);
};

class ReadCmd : public NicCmd {
  public:
	ReadCmd( Hermes::ProcAddr proc, Hermes::MemAddr& destAddr, Hermes::RDMA::Addr srcAddr, size_t length ) : 
		NicCmd( Read ), proc(proc), destAddr(destAddr), srcAddr(srcAddr), length(length) {}

	Hermes::ProcAddr proc;
   	Hermes::MemAddr destAddr;
	Hermes::RDMA::Addr srcAddr;
   	size_t length;
    NotSerializable(SST::Aurora::RDMA::ReadCmd);
};

class WriteCmd : public NicCmd {
  public:
	WriteCmd( Hermes::ProcAddr proc, Hermes::RDMA::Addr destAddr, Hermes::MemAddr& srcAddr, size_t length ) : 
		NicCmd( Write ), proc(proc), destAddr(destAddr), srcAddr(srcAddr), length(length) {}

	Hermes::ProcAddr proc;
	Hermes::RDMA::Addr destAddr;
   	Hermes::MemAddr srcAddr;
   	size_t length;
    NotSerializable(SST::Aurora::RDMA::WriteCmd);
};

}
}
}

#endif
