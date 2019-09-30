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


#ifndef COMPONENTS_AURORA_RVMA_MPI_PT2PT_LIB_H
#define COMPONENTS_AURORA_RVMA_MPI_PT2PT_LIB_H

#include <queue>

#include "sst/elements/hermes/rvma.h"
#include "sst/elements/hermes/miscapi.h"
#include "include/mpiPt2PtLib.h"
#include "include/hostLib.h"

namespace SST {
namespace Aurora {
namespace RVMA {

class RvmaMpiPt2PtLib : public MpiPt2Pt
{
  public:
    SST_ELI_REGISTER_SUBCOMPONENT(
        RvmaMpiPt2PtLib,
        "aurora",
        "rvmaMpiPt2PtLib",
        SST_ELI_ELEMENT_VERSION(1,0,0),
        "",
        ""
    )

    SST_ELI_DOCUMENT_PARAMS(
        {"verboseLevel","Sets the level of debug verbosity",""},
        {"verboseMask","Sets the debug mask",""},
    )

	RvmaMpiPt2PtLib( Component*, Params& );
	~RvmaMpiPt2PtLib() {} 

	std::string getName()        { return "RvmaMpiPt2Pt"; }

    void setup() {
        char buffer[100];
        snprintf(buffer,100,"@t:%d:%d:Aurora::RVMA::RvmaMpiPt2PtLib::@p():@l ", os().getNodeNum(), os().getRank());
        m_dbg.setPrefix(buffer);

		MpiPt2Pt::setup();
		m_rvma->setup();
    }

    void setOS( Hermes::OS* os ) { 
		MpiPt2Pt::setOS(os);
		m_rvma->setOS(os);
   	}

	void init( int* numRanks, int* myRank, Hermes::Callback* );
    void isend( const Hermes::Mpi::MemAddr& buf, int count, Hermes::Mpi::DataType, int dest, int tag, Hermes::Mpi::Comm, Hermes::Mpi::Request*, Hermes::Callback* );
    void irecv( const Hermes::Mpi::MemAddr& buf, int count, Hermes::Mpi::DataType ,int src, int tag, Hermes::Mpi::Comm, Hermes::Mpi::Request*, Hermes::Callback* );
    void test( Hermes::Mpi::Request*, Hermes::Mpi::Status*, bool blocking, Hermes::Callback* );
	virtual void testall( int count, Mpi::Request*, int* flag, Mpi::Status*, bool blocking, Callback* );
	virtual void testany( int count, Mpi::Request*, int* indx, int* flag, Mpi::Status*, bool blocking, Callback* );

  private:
	void _init( int* numRanks, int* myRank, Hermes::Callback* );
    void _isend( const Hermes::Mpi::MemAddr& buf, int count, Hermes::Mpi::DataType, int dest, int tag, Hermes::Mpi::Comm, Hermes::Mpi::Request*, Hermes::Callback* );
    void _irecv( const Hermes::Mpi::MemAddr& buf, int count, Hermes::Mpi::DataType ,int src, int tag, Hermes::Mpi::Comm, Hermes::Mpi::Request*, Hermes::Callback* );
    void _test( Hermes::Mpi::Request*, Hermes::Mpi::Status*, bool blocking, Hermes::Callback* );
	void _testall( int count, Mpi::Request*, int* flag, Mpi::Status*, bool blocking, Callback* );
	void _testany( int count, Mpi::Request*, int* indx, int* flag, Mpi::Status*, bool blocking, Callback* );

    struct RvmaEntryData {
		RvmaEntryData() {}
		Hermes::RVMA::VirtAddr rvmaAddr;
		Hermes::RVMA::Completion completion;
    };

    typedef SendEntryBase<RvmaEntryData> SendEntry;
    typedef RecvEntryBase<RvmaEntryData> RecvEntry;
    typedef TestEntryBase<RvmaEntryData> TestEntry;
    typedef TestallEntryBase<RvmaEntryData> TestallEntry;
    typedef TestanyEntryBase<RvmaEntryData> TestanyEntry;

	struct MsgHdr : public MsgHdrBase {
		Hermes::ProcAddr procAddr;
		enum Type { Match, Go } type;
		Hermes::RVMA::VirtAddr rvmaAddr;
		SendEntry* sendEntry;
	};

	void waitForLongGo( Hermes::Callback*, SendEntry*, int retval );
	void sendMsg( Hermes::ProcAddr&, Hermes::MemAddr&, size_t length, Hermes::Callback* );
	void poll( TestBase* );
	void processTest( TestBase*, int retval );
	void processMsg( Hermes::RVMA::Completion*, Hermes::Callback* );

	void findPostedRecv( MsgHdr* , std::function<void(RecvEntry*)> );

	void mallocSendBuffers( Hermes::Callback*, int retval );
	void mallocRecvBuffers( Hermes::Callback*, int retval );
	void postRecvBuffer( Hermes::Callback*, int count, int retval );
	void repostRecvBuffer( Hermes::MemAddr, Hermes::Callback* );
	void processSendEntry( Hermes::Callback*, SendEntry*  );
	void foobar( RecvEntry* entry, Hermes::Callback* callback );

	Hermes::MemAddr checkUnexpected( RecvEntry* );

	void processSendQ(Hermes::Callback*, int );
	void processRecvQ(Hermes::Callback*, int );

	void processMatch( const Hermes::MemAddr& msg, RecvEntry* entry, Hermes::Callback* callback );
	void processGoMsg( Hermes::MemAddr msg, Hermes::Callback* callback );
	void makeProgress( Hermes::Callback* );

	std::deque< RecvEntry* > m_postedRecvs;
	std::queue< SendEntry* > m_postedSends;

	Hermes::RVMA::VirtAddr m_windowAddr;
	Hermes::RVMA::Window   m_windowId;
	std::queue<Hermes::RVMA::Completion*> m_completionQ;

	Hermes::MemAddr m_recvBuff;
	Hermes::MemAddr m_sendBuff;
	int m_numRecvBuffers;
	int m_numSendBuffers;

	std::deque< Hermes::MemAddr > m_unexpectedRecvs;

	Hermes::RVMA::Interface& rvma() { return *m_rvma; }
	Hermes::RVMA::Interface* m_rvma;

	RecvEntry* m_pendingLongEntry;
	std::deque<SendEntry*> m_pendingLongPut;
	int m_longPutWinAddr;
};

}
}
}

#endif
