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


class WriteTarget : public Base {
  public:
 	WriteTarget(EmberRdmaTestGenerator* obj, Params& params ) : obj(*obj), m_state(0) {
		m_writeLen = params.find<size_t>( "arg.msgLength", 0x400 ); 
		m_rqId = params.find<size_t>( "arg.recvQid", 100 ); 
	} 

	bool generate( std::queue<EmberEvent*>& evQ ) {
		bool ret = false;
		switch ( m_state ) { 
		  case 0:
			obj.misc().malloc( evQ, &m_sendBuf.addr(), sizeof(MsgHdr), true);
			obj.misc().malloc( evQ, &m_recvBuf.addr(), sizeof(MsgHdr), true);
			obj.misc().malloc( evQ, &m_rdmaBuf, m_writeLen, true);
			obj.rdma().createRQ( evQ, m_rqId );
			++m_state;
			break;

		  case 1:
			ret = state1( evQ );
			++m_state;
			break;

		  case 2:
			obj.verbose(CALL_INFO,1,MOTIF_MASK,"WriteTarget: checkRQ\n");
			obj.rdma().checkRQ( evQ, m_rqId, &m_status, true );
			++m_state;
			break;

		  case 3:
			obj.output("WriteTarget: write is done srcNid=%d srcPid=%d length=%zu bufId=0x%" PRIx64 "\n",
                   m_status.procAddr.node, m_status.procAddr.pid, m_status.length, m_status.addr.getSimVAddr());
			checkBuf( m_rdmaBuf.getBacking(), m_writeLen );
			ret = true;
			++m_state;
			break;
		}
		return ret;
	}
		
  private:

	bool state1( std::queue<EmberEvent*>& evQ ) 
	{
		Hermes::RDMA::Addr addr = m_rdmaBuf.getSimVAddr(); 
		RDMA::ProcAddr procAddr;
		procAddr.pid = 0;
		procAddr.node = ( obj.m_node_num + 1 ) % obj.m_num_nodes;

		obj.verbose(CALL_INFO,1,MOTIF_MASK,"WriteTarget: postRecv\n");
		obj.rdma().postRecv( evQ, m_rqId, m_recvBuf.addr(), sizeof(MsgHdr), &m_bufId ); 
		obj.rdma().registerMem( evQ, m_rdmaBuf, m_writeLen, &m_memId );

		m_sendBuf.push( &addr, sizeof( addr ) );

		obj.verbose(CALL_INFO,1,MOTIF_MASK,"WriteTarget: RDMA buff addr=0x%" PRIx64 "\n",m_rdmaBuf.getSimVAddr() );
		obj.rdma().send( evQ, procAddr, m_rqId, m_sendBuf.addr(), sizeof(MsgHdr) );
		return false;
	}

	int m_state;

	size_t m_writeLen;
	Hermes::MemAddr m_rdmaBuf;
	FifoBuf m_sendBuf;
	FifoBuf m_recvBuf;

	RDMA::RqId m_rqId;
	RDMA::Status m_status;
	RDMA::RecvBufId m_bufId;
	RDMA::MemRegionId m_memId;

	EmberRdmaTestGenerator& obj;
};
