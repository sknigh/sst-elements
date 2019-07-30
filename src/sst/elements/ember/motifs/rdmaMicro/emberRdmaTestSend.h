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


class Send : public Base {
  public:
 	Send(EmberRdmaTestGenerator* obj, Params& params ) : obj(*obj), m_state(0) {
		m_bufLen = params.find<size_t>( "arg.msgLength", 0x400 ); 
		m_rqId = params.find<size_t>( "arg.recvQid", 100 ); 
	} 

	bool generate( std::queue<EmberEvent*>& evQ ) {
		bool ret = false;
		switch ( m_state ) { 
		  case 0:
			obj.verbose(CALL_INFO,1,MOTIF_MASK,"malloc sendBuf %zu\n",m_bufLen);
			obj.misc().malloc( evQ, &m_sendBuf, m_bufLen);
			obj.verbose(CALL_INFO,1,MOTIF_MASK,"malloc recvBuf %zu\n",m_bufLen);
			obj.misc().malloc( evQ, &m_recvBuf, m_bufLen);

			obj.verbose(CALL_INFO,1,MOTIF_MASK,"createRQ\n");
			obj.rdma().createRQ( evQ, m_rqId );
			++m_state;
			break;
		  case 1:
			obj.verbose(CALL_INFO,1,MOTIF_MASK,"postRecv\n");
			obj.rdma().postRecv( evQ, m_rqId, m_recvBuf, m_bufLen, &m_bufId ); 

			m_procAddr.pid = 0;
			m_procAddr.node = ( obj.m_node_num + 1 ) % obj.m_num_nodes;
			obj.verbose(CALL_INFO,1,MOTIF_MASK,"send\n");
			obj.rdma().send( evQ, m_procAddr, m_rqId, m_sendBuf, m_bufLen );
			++m_state;
			break;

		  case 2:
			obj.verbose(CALL_INFO,1,MOTIF_MASK,"checkRQ\n");
			obj.rdma().checkRQ( evQ, m_rqId, &m_status, true );
			++m_state;
			break;

		  case 3:
			obj.output("Send done srcNid=%d srcPid=%d length=%zu bufId=0x%" PRIx64 "\n",
					m_status.procAddr.node,m_status.procAddr.pid, m_status.length,m_status.addr.getSimVAddr());
			ret = true; 
		}
		return ret;
	}
		
  private:
	int m_state;
	Hermes::MemAddr m_sendBuf;
	Hermes::MemAddr m_recvBuf;
	size_t m_bufLen;
	RDMA::RqId m_rqId;
	RDMA::ProcAddr m_procAddr;
	RDMA::Status m_status;
	RDMA::RecvBufId m_bufId;

	EmberRdmaTestGenerator& obj;
};
