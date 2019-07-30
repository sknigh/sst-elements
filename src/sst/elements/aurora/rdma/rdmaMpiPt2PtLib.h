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
    SST_ELI_REGISTER_SUBCOMPONENT(
        RdmaMpiPt2PtLib,
        "aurora",
        "rdmaMpiPt2PtLib",
        SST_ELI_ELEMENT_VERSION(1,0,0),
        "",
        ""
    )

    SST_ELI_DOCUMENT_PARAMS(
        {"verboseLevel","Sets the level of debug verbosity",""},
        {"verboseMask","Sets the debug mask",""},
    )

	RdmaMpiPt2PtLib( Component*, Params& );
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

	void init( int* numRanks, int* myRank, Hermes::Callback* );
    void isend( const Hermes::Mpi::MemAddr& buf, int count, Hermes::Mpi::DataType, int dest, int tag, Hermes::Mpi::Comm, Hermes::Mpi::Request*, Hermes::Callback* );
    void irecv( const Hermes::Mpi::MemAddr& buf, int count, Hermes::Mpi::DataType ,int src, int tag, Hermes::Mpi::Comm, Hermes::Mpi::Request*, Hermes::Callback* );
    void test( Hermes::Mpi::Request*, Hermes::Mpi::Status*, bool blocking, Hermes::Callback* );
	virtual void testall( int count, Mpi::Request*, int* flag, Mpi::Status*, bool blocking, Callback* );
	virtual void testany( int count, Mpi::Request*, int* indx, int* flag, Mpi::Status*, bool blocking, Callback* );

  private:

	struct MsgHdr : public MsgHdrBase {
		enum Type { Match, Ack } type;
		Hermes::RDMA::Addr readAddr;
		void* key;
	};

	struct RdmaEntryData {
		Hermes::RDMA::MemRegionId memId;
	};

	typedef SendEntryBase<RdmaEntryData> SendEntry;
	typedef RecvEntryBase<RdmaEntryData> RecvEntry;
	typedef TestEntryBase<RdmaEntryData> TestEntry;
	typedef TestallEntryBase<RdmaEntryData> TestallEntry;
	typedef TestanyEntryBase<RdmaEntryData> TestanyEntry;

	void waitForLongAck( Hermes::Callback*, SendEntry*, int retval );
	void sendMsg( Hermes::ProcAddr&, Hermes::MemAddr&, size_t length, Hermes::Callback* );
	void processTest( TestBase*, int retval );
	void checkMsgAvail( Hermes::Callback*, int retval );
	void processMsg( Hermes::RDMA::Status&, Hermes::Callback* );
	RecvEntry* findPostedRecv( MsgHdr* );
	void mallocSendBuffers( Hermes::Callback*, int retval );
	void mallocRecvBuffers( Hermes::Callback*, int retval );
	void postRecvBuffer( Hermes::Callback*, int count, int retval );
	void repostRecvBuffer( Hermes::MemAddr, Hermes::Callback* );
	void processSendEntry( Hermes::Callback*, SendEntry*  );

	Hermes::RDMA::Status* checkUnexpected( RecvEntry* );
	void processSendQ(Hermes::Callback*, int );
	void processRecvQ(Hermes::Callback*, int );

	void processMatch( Hermes::RDMA::Status& status, RecvEntry* entry, Hermes::Callback* callback );

	void makeProgress( Hermes::Callback* );
	Hermes::RDMA::Interface& rdma() { return *m_rdma; }

	std::deque< RecvEntry* > m_postedRecvs;
	std::queue< SendEntry* > m_postedSends;

	Hermes::RDMA::Interface* m_rdma;

	Hermes::RDMA::RqId m_rqId;
	Hermes::RDMA::Status m_rqStatus;
	Hermes::MemAddr m_recvBuff;
	Hermes::MemAddr m_sendBuff;
	int m_numRecvBuffers;
	int m_numSendBuffers;

	std::deque< Hermes::RDMA::Status* > m_unexpectedRecvs;
};

}
}
}

#endif
