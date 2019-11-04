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

  private:
	size_t getMsgHdrSize() { return sizeof(MsgHdr); }
	Hermes::RVMA::Interface& rvma() { return *m_rvma; }

	void _init( int* numRanks, int* myRank, Hermes::Callback* );
    void _isend( const Hermes::Mpi::MemAddr& buf, int count, Hermes::Mpi::DataType, int dest, int tag, Hermes::Mpi::Comm, Hermes::Mpi::Request*, Hermes::Callback* );
    void _irecv( const Hermes::Mpi::MemAddr& buf, int count, Hermes::Mpi::DataType ,int src, int tag, Hermes::Mpi::Comm, Hermes::Mpi::Request*, Hermes::Callback* );

    struct RvmaEntryData {
		Hermes::RVMA::VirtAddr rvmaAddr;
		Hermes::RVMA::Completion completion;
    };

	struct SendEntry : public SendEntryBase {
        SendEntry( const Hermes::Mpi::MemAddr& buf, int count, Hermes::Mpi::DataType dataType, int dest, int tag,
                Hermes::Mpi::Comm comm, Hermes::Mpi::Request* request ) : SendEntryBase( buf, count, dataType, dest, tag, comm, request) {}
		RvmaEntryData extra;
	};

    struct RecvEntry : public RecvEntryBase {
        RecvEntry( const Hermes::Mpi::MemAddr& buf, int count, Hermes::Mpi::DataType dataType, int src, int tag,
                Hermes::Mpi::Comm comm, Hermes::Mpi::Request* request ) : RecvEntryBase( buf, count, dataType, src, tag, comm, request) {}
		RvmaEntryData extra;
	};

    typedef TestEntryBase TestEntry;
    typedef TestallEntryBase TestallEntry;
    typedef TestanyEntryBase TestanyEntry;

	struct MsgHdr : public MsgHdrBase {
		Hermes::ProcAddr procAddr;
		enum Type { Match, Go } type;
		Hermes::RVMA::VirtAddr rvmaAddr;
		SendEntry* sendEntry;
	};

	void processTest( TestBase*, int retval );
	void processMsg( Hermes::RVMA::Completion*, Hermes::Callback* );
	void sendMsg( Hermes::ProcAddr, Hermes::MemAddr&, size_t length, int* handle, Hermes::Callback* );
	void postRecvBuffer( Hermes::Callback*, int count, int retval );
	void repostRecvBuffer( Hermes::MemAddr, Hermes::Callback* );
	void processSendEntry( Hermes::Callback*, SendEntryBase*  );
	void processRecvQ(Hermes::Callback*, int );
	void processMatch( const Hermes::MemAddr& msg, RecvEntryBase* entry, Hermes::Callback* callback );
	void makeProgress( Hermes::Callback* );
	Hermes::MemAddr checkUnexpected( RecvEntry* );

	void processGoMsg( Hermes::MemAddr msg, Hermes::Callback* callback );
	void waitForLongGo( Hermes::Callback*, SendEntry*, int retval );
	void poll( TestBase* );
	void recvCheckMatch( RecvEntry* entry, Hermes::Callback* callback );

	Hermes::RVMA::VirtAddr m_windowAddr;
	Hermes::RVMA::Window   m_windowId;
	std::queue<Hermes::RVMA::Completion*> m_completionQ;

	Hermes::RVMA::Interface* m_rvma;

	std::deque< Hermes::MemAddr > m_unexpectedRecvs;

	std::queue<RecvEntry*> m_pendingLongEntry;
	std::deque<SendEntry*> m_pendingLongPut;
	int m_longPutWinAddr;
};

}
}
}

#endif
