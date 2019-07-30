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


struct MsgHdr {
	uint64_t addr;
	size_t length;
};

class Send2 : public Base {
  public:
 	Send2(EmberRdmaTestGenerator* obj, Params& params ) : obj(*obj), m_state(0) {
		m_msgLen = params.find<size_t>( "arg.msgLength", 0x400 ); 
		m_rqId = params.find<size_t>( "arg.recvQid", 100 ); 
		m_shortMsgLen = params.find<size_t>( "arg.shortMsgLength", 1024 ); 

		if ( m_msgLen > m_shortMsgLen ) {
			m_bufLen = sizeof(MsgHdr);
		} else {
			m_bufLen = sizeof(MsgHdr) + m_msgLen;
		}
	} 

	bool generate( std::queue<EmberEvent*>& evQ ) {
		bool ret = false;
		switch ( m_state ) { 
		  case 0:
			{
				obj.verbose(CALL_INFO,1,MOTIF_MASK,"Send: malloc sendBuf %zu\n", m_bufLen );
				obj.misc().malloc( evQ, &m_sendBuf.addr(), m_bufLen, true );
				obj.verbose(CALL_INFO,1,MOTIF_MASK,"Send: malloc recvBuf %zu\n", m_bufLen);
				obj.misc().malloc( evQ, &m_recvBuf.addr(), m_bufLen, true);
				if ( m_msgLen > m_shortMsgLen ) {
					obj.verbose(CALL_INFO,1,MOTIF_MASK,"Send: malloc RDMA Buf %zu\n", m_msgLen);
					obj.misc().malloc( evQ, &m_rdmaBuf, m_msgLen, true);
				}
			}

			obj.verbose(CALL_INFO,1,MOTIF_MASK,"Send: createRQ\n");
			obj.rdma().createRQ( evQ, m_rqId );
			++m_state;
			break;

		  case 1:
			obj.verbose(CALL_INFO,1,MOTIF_MASK,"Send: postRecv\n");
			obj.rdma().postRecv( evQ, m_rqId, m_recvBuf.addr(), m_bufLen, &m_bufId ); 

			m_procAddr.pid = 0;
			m_procAddr.node = ( obj.m_node_num + 1 ) % obj.m_num_nodes;

			obj.verbose(CALL_INFO,1,MOTIF_MASK,"Send:  send addr=0x%" PRIx64 " backing=%p\n",
					m_sendBuf.addr().getSimVAddr(), m_sendBuf.addr().getBacking());

			{ 
				size_t length = m_msgLen;
				m_sendBuf.push( &length, sizeof( length ) );
				if ( m_msgLen <= m_shortMsgLen ) { 
					m_sendBuf.push( NULL, m_msgLen );
					initSendBuf( m_sendBuf.addr().getBacking(sizeof(length)), m_msgLen );
					obj.verbose(CALL_INFO,1,MOTIF_MASK,"Send: short message length=%zu\n", length );
				} else {
					obj.rdma().registerMem( evQ, m_rdmaBuf, m_msgLen, &m_memId );

					Hermes::RDMA::Addr addr = m_rdmaBuf.getSimVAddr(); 
					m_sendBuf.push( &addr, sizeof( addr ) );
					initSendBuf( m_rdmaBuf.getBacking(), m_msgLen );
					obj.verbose(CALL_INFO,1,MOTIF_MASK,"Send: long message addr=0x%" PRIx64 " length=%zu\n",addr, length );
				}
				obj.rdma().send( evQ, m_procAddr, m_rqId, m_sendBuf.addr(), m_sendBuf.length() );
			}

			if ( m_msgLen > m_shortMsgLen ) {
				obj.verbose(CALL_INFO,1,MOTIF_MASK,"Send; checkRQ\n");
				obj.rdma().checkRQ( evQ, m_rqId, &m_status, true );
			}

			++m_state;
			break;

		  case 2:
			if ( m_msgLen > m_shortMsgLen ) {
				obj.output("Send: long message done srcNid=%d srcPid=%d length=%zu bufId=0x%" PRIx64 "\n",
					m_status.procAddr.node,m_status.procAddr.pid, m_status.length,m_status.addr.getSimVAddr());
			} else {
				obj.output("Send: short message done\n");
			}
			ret = true; 
		}
		return ret;
	}
		
  private:

	void initSendBuf(void* ptr, size_t length ) {
		unsigned char* buf = (unsigned char*) ptr;
		for ( int i = 0; i < length; i++ ) {
			buf[i]=i;
		}
	}	

	size_t m_bufLen;
	size_t m_shortMsgLen;
	int m_state;
	Hermes::MemAddr m_rdmaBuf;
	FifoBuf m_sendBuf;
	FifoBuf m_recvBuf;
	size_t m_msgLen;
	RDMA::RqId m_rqId;
	RDMA::ProcAddr m_procAddr;
	RDMA::Status m_status;
	RDMA::RecvBufId m_bufId;
	RDMA::MemRegionId m_memId;

	EmberRdmaTestGenerator& obj;
};
