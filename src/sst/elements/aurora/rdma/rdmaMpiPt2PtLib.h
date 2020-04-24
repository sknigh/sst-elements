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


#ifndef COMPONENTS_AURORA_RDMA_MPI_PT2PT_LIB_H
#define COMPONENTS_AURORA_RDMA_MPI_PT2PT_LIB_H

#include <queue>

#include "sst/elements/hermes/rdma.h"
#include "sst/elements/hermes/miscapi.h"
#include "include/mpiPt2PtLib.h"
#include "include/hostLib.h"

namespace SST {
namespace Aurora {
namespace RDMA {

class RdmaMpiPt2PtLib : public MpiPt2Pt
{
  public:
    SST_ELI_REGISTER_SUBCOMPONENT_DERIVED(
        RdmaMpiPt2PtLib,
        "aurora",
        "rdmaMpiPt2PtLib",
        SST_ELI_ELEMENT_VERSION(1,0,0),
        "",
        SST::Hermes::Mpi::Interface
    )

    SST_ELI_DOCUMENT_PARAMS(
        {"verboseLevel","Sets the level of debug verbosity",""},
        {"verboseMask","Sets the debug mask",""},
    )

	RdmaMpiPt2PtLib( ComponentId_t, Params& );
	~RdmaMpiPt2PtLib() {} 

	std::string getName()        { return "RdmaMpiPt2Pt"; }

    void setup() {
        char buffer[100];
        snprintf(buffer,100,"@t:%d:%d:Aurora::RDMA::RdmaMpiPt2PtLib::@p():@l ", os().getNodeNum(), os().getRank());
        m_dbg.setPrefix(buffer);

		MpiPt2Pt::setup();
		m_rdma->setup();
    }

    void setOS( Hermes::OS* os ) { 
		MpiPt2Pt::setOS(os);
		m_rdma->setOS(os);
   	}

	void _init( int* numRanks, int* myRank, Hermes::Callback* );
    void _isend( const Hermes::Mpi::MemAddr& buf, int count, Hermes::Mpi::DataType, int dest, int tag, Hermes::Mpi::Comm, Hermes::Mpi::Request*, Hermes::Callback* );
    void _irecv( const Hermes::Mpi::MemAddr& buf, int count, Hermes::Mpi::DataType ,int src, int tag, Hermes::Mpi::Comm, Hermes::Mpi::Request*, Hermes::Callback* );

  private:

	size_t getMsgHdrSize() { return sizeof(MsgHdr); }
	Hermes::RDMA::Interface& rdma() { return *m_rdma; }

	struct __attribute__ ((packed)) MsgHdr : public MsgHdrBase {
		Hermes::ProcAddr pad; // Not used by RDMA but here so header is the same size of RVMA 
		enum Type { Match, Ack } type;
		Hermes::RDMA::Addr readAddr;
		void* key;
	};

	struct RdmaEntryData {
		Hermes::RDMA::MemRegionId memId;
	};

	struct SendEntry : public SendEntryBase {
		SendEntry( const Hermes::Mpi::MemAddr& buf, int count, Hermes::Mpi::DataType dataType, int dest, int tag,
                Hermes::Mpi::Comm comm, Hermes::Mpi::Request* request ) : SendEntryBase( buf, count, dataType, dest, tag, comm, request) {}
		RdmaEntryData extra;
	};

	struct RecvEntry : public RecvEntryBase {
		RecvEntry( const Hermes::Mpi::MemAddr& buf, int count, Hermes::Mpi::DataType dataType, int src, int tag,
				Hermes::Mpi::Comm comm, Hermes::Mpi::Request* request ) : RecvEntryBase( buf, count, dataType, src, tag, comm, request) {}
		RdmaEntryData extra;
	};

	typedef TestEntryBase TestEntry;
	typedef TestallEntryBase TestallEntry;
	typedef TestanyEntryBase TestanyEntry;

	void processTest( TestBase*, int retval );
	void processMsg( Hermes::RDMA::Status&, Hermes::Callback* );
	void sendMsg( Hermes::ProcAddr, const Hermes::MemAddr&, size_t length, int* handle, Hermes::Callback* );
	void postRecvBuffer( Hermes::Callback*, int count, int retval );
	void repostRecvBuffer( Hermes::MemAddr, Hermes::Callback* );
	void processRecvQ(Hermes::Callback*, int );
	void processMatch( const Hermes::RDMA::Status status, RecvEntryBase* entry, Hermes::Callback* callback );
	void makeProgress( Hermes::Callback* );
	Hermes::RDMA::Status* checkUnexpected( RecvEntryBase* );

	void waitForLongAck( Hermes::Callback*, SendEntry*, int retval );
	void checkMsgAvail( Hermes::Callback*, int retval );
	void processSendEntry( Hermes::Callback*, SendEntryBase* );

	Hermes::RDMA::Interface*	m_rdma;
	Hermes::RDMA::RqId 			m_rqId;
	Hermes::RDMA::Status 		m_rqStatus;

	std::deque< Hermes::RDMA::Status* > m_unexpectedRecvs;
};

}
}
}

#endif
