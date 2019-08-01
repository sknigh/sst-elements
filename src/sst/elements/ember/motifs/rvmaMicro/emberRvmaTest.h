// Copyright 2009-2018 NTESS. Under the terms
// of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.
//
// Copyright (c) 2009-2018, NTESS
// All rights reserved.
//
// Portions are copyright of other developers:
// See the file CONTRIBUTORS.TXT in the top level directory
// the distribution for more information.
//
// This file is part of the SST software package. For license
// information, see the LICENSE file in the top level directory of the
// distribution.


#ifndef _H_EMBER_MOTIF_RVMA_TEST
#define _H_EMBER_MOTIF_RVMA_TEST

#include "embergen.h"
#include "libs/emberRvmaLib.h"
#include "libs/misc.h"

namespace SST {
namespace Ember {

class EmberRvmaTestGenerator : public EmberGenerator {

public:
    SST_ELI_REGISTER_SUBCOMPONENT_DERIVED(
        EmberRvmaTestGenerator,
        "ember",
        "RvmaTestMotif",
        SST_ELI_ELEMENT_VERSION(1,0,0),
        "RVMA Test",
        SST::Ember::EmberRvmaTestGenerator
    )

    SST_ELI_DOCUMENT_PARAMS()

	EmberRvmaTestGenerator(SST::Component* owner, Params& params) : EmberGenerator( owner, params,"") {}
	EmberRvmaTestGenerator(SST::ComponentId_t id, Params& params) :
		EmberGenerator( id, params, "RvmaTest" ), m_recvState(0), m_rvma(NULL), m_miscLib(NULL)
	{ 
		int putBufLen = m_recvBufLen = params.find<size_t>( "arg.bufferSize", 0x400 ); 
		m_virtAddr = params.find<uint64_t>( "arg.virtAddr",0xbeef000000000000);
		m_numRecvBufs = params.find<int>("arg.numRecvBufs", 5 );

		m_completions.resize(m_numRecvBufs);
		m_bufPtrs.resize(m_numRecvBufs);

		m_generate = &EmberRvmaTestGenerator::init;
		m_send = new Send(this, putBufLen, m_virtAddr);
	}

	void setup() {
        m_rvma = static_cast<EmberRvmaLib*>(getLib("rvma"));
        assert(m_rvma);
        m_rvma->initOutput( &getOutput() );
        m_miscLib = static_cast<EmberMiscLib*>(getLib("misc"));
        assert(m_miscLib);
        m_miscLib->initOutput( &getOutput() );
	}

	class Send {
	  public:
	 	Send(EmberRvmaTestGenerator* obj, int len, RVMA::VirtAddr addr ) : 
			obj(*obj), m_state(0), m_putBufLen(len), m_virtAddr(addr)  {} 

		bool generate( std::queue<EmberEvent*>& evQ ) {
			bool ret = false;
			switch ( m_state ) { 
			  case 0:
				printf("%s() malloc\n",__func__);
				obj.misc().malloc( evQ, &m_putBuf, m_putBufLen );
				++m_state;
				break;

			  case 1:
				m_destProc.node=( obj.m_node_num + 1) % obj.m_num_nodes;
				m_destProc.pid = 0;
				printf("%s() put\n",__func__);
				obj.rvma().put( evQ, m_putBuf, m_putBufLen/2, m_destProc, m_virtAddr, 0  ); 
				obj.rvma().put( evQ, m_putBuf, m_putBufLen/2, m_destProc, m_virtAddr, m_putBufLen/2 ); 
				++m_state;
				break;

			  case 2:
				printf("%s() done\n",__func__);
				ret = true; 
			}
			return ret;
		}

	  private:
		RVMA::VirtAddr m_virtAddr;
		ProcAddr m_destProc;
		int m_state;
		EmberRvmaTestGenerator& obj;
		Hermes::MemAddr m_putBuf;
		size_t m_putBufLen;
	};

    bool recv( std::queue<EmberEvent*>& evQ) {
		bool ret = false;
		switch ( m_recvState ) {
		  case 0:
			printf("%s() malloc\n",__func__);
			misc().malloc( evQ, &m_recvBuf, m_recvBufLen * m_numRecvBufs);
			++m_recvState;
			break;

		  case 1:
			printf("%s():%d nodeNum=%d numNodes=%d buffer=0x%" PRIx64 "\n",
					__func__,__LINE__,m_node_num,m_num_nodes, m_recvBuf.getSimVAddr() );
			enQ_getTime( evQ, &m_start );
			rvma().initWindow( evQ, m_virtAddr, m_recvBufLen, RVMA::Byte, &m_window );
			enQ_getTime( evQ, &m_stop );
			++m_recvState;
			break;

		  case 2:
			printf("%s() window=%d latency=%" PRIu64 "\n",__func__,m_window, m_stop-m_start);
			for ( int i = 0; i < m_numRecvBufs; i++ ) {
				printf("postBuffer addr 0x%" PRIx64 "\n", m_recvBuf.getSimVAddr(i*m_recvBufLen) );
				rvma().postBuffer( evQ, m_recvBuf.offset(i*m_recvBufLen), m_recvBufLen, &m_completions[i], m_window, &m_retval );
			}
			++m_recvState;
			break;

		  case 3:
			printf("%s() mwait\n",__func__);
			rvma().mwait( evQ, &m_completions[0] ); 
			printf("%s() getEpoch\n",__func__);
			rvma().winGetEpoch( evQ, m_window, &m_epoch, &m_getEpochRetval );
			printf("%s() incEpoch\n",__func__);
			rvma().winIncEpoch( evQ, m_window, &m_incEpochRetval );
			printf("%s() getBufPtrs\n",__func__);
			rvma().winGetBufPtrs( evQ, m_window, &m_bufPtrs[0], m_numRecvBufs, &m_getBufPtrsRetval );
			printf("%s() closeWindow\n",__func__);
			rvma().closeWindow( evQ, m_window );
			++m_recvState;
			break;

		  case 4:
			printf("numBufPtrs=%d\n", m_getBufPtrsRetval );
			for ( int i = 0; i < m_getBufPtrsRetval; i++ ) {
				printf("bufPtrs[%d] count=%zu addr=0x%" PRIx64 "\n",i,m_bufPtrs[i].count,m_bufPtrs[i].addr.getSimVAddr());
			}
			printf("count=%zu addr=0x%" PRIx64 "\n",m_completions[0].count,m_completions[0].addr.getSimVAddr());
			printf("count=%zu addr=0x%" PRIx64 "\n",m_completions[1].count,m_completions[1].addr.getSimVAddr());
			printf("getEpoch %d retval=%d\n",m_epoch,m_getEpochRetval);
			printf("incEpoch retval %d\n",m_incEpochRetval);
			ret = true;
		}
		return ret;
	}

	void completed(const SST::Output* output, uint64_t time ) {}

    bool generate( std::queue<EmberEvent*>& evQ)  {
		return (this->*m_generate)( evQ );
	}

    bool init( std::queue<EmberEvent*>& evQ) {
		misc().getNodeNum( evQ, &m_node_num );
       	misc().getNumNodes( evQ, &m_num_nodes );
		m_generate = &EmberRvmaTestGenerator::init2;
		return false;
	}

    bool init2( std::queue<EmberEvent*>& evQ) {
		if ( 0 == m_node_num  ) {
			m_generate = &EmberRvmaTestGenerator::send;
		} else if ( 1 == m_node_num  ) {
			m_generate = &EmberRvmaTestGenerator::recv;
		}
		return (this->*m_generate)( evQ );
	}

   	bool send( std::queue<EmberEvent*>& evQ) {
		return m_send->generate( evQ );
	}

  private:

	Send* m_send;

	int m_getBufPtrsRetval;
	int m_numRecvBufs;
	int m_incEpochRetval;
	int m_getEpochRetval;
	int m_epoch;
	int m_recvState;
	uint64_t m_start;	
	uint64_t m_stop;	
	RVMA::Window m_window;
	Hermes::MemAddr m_recvBuf;
	size_t m_recvBufLen;
	std::vector<Hermes::RVMA::Completion> m_completions;
	std::vector<Hermes::RVMA::Completion> m_bufPtrs;
	int m_count;
	RVMA::VirtAddr m_virtAddr;
	int	m_retval;

	typedef bool (EmberRvmaTestGenerator::*GenerateFuncPtr)( std::queue<EmberEvent*>& evQ );
	GenerateFuncPtr  m_generate;

protected:
    EmberRvmaLib& rvma() { return *m_rvma; }
    EmberMiscLib& misc() { return *m_miscLib; }

    int m_node_num;
    int m_num_nodes;

private:
    EmberRvmaLib* m_rvma;
    EmberMiscLib* m_miscLib;
};

}
}

#endif
