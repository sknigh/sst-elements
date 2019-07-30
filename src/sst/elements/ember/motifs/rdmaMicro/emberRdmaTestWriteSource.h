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


class WriteSource : public Base {
  public:
 	WriteSource(EmberRdmaTestGenerator* obj, Params& params ) : obj(*obj), m_state(0) {
        m_writeLen = params.find<size_t>( "arg.msgLength", 0x400 );
        m_rqId = params.find<size_t>( "arg.recvQid", 100 );
	} 

	size_t m_msgLen;
	size_t m_shortMsgLen;
	bool generate( std::queue<EmberEvent*>& evQ ) {
		bool ret = false;
		switch ( m_state ) { 
		  case 0:
			obj.misc().malloc( evQ, &m_sendBuf.addr(), sizeof(MsgHdr), true );
			obj.misc().malloc( evQ, &m_recvBuf.addr(), sizeof(MsgHdr), true );
			obj.misc().malloc( evQ, &m_rdmaBuf, m_writeLen, true );
			obj.rdma().createRQ( evQ, m_rqId );

			++m_state;
			break;

		  case 1:
			printf("WriteSource: recvBuf addr=0x%" PRIx64 " backing=%p\n",m_recvBuf.addr().getSimVAddr(), m_recvBuf.addr().getBacking() );
			initBuf( m_rdmaBuf.getBacking(), m_writeLen); 
			obj.rdma().postRecv( evQ, m_rqId, m_recvBuf.addr(), sizeof(MsgHdr), &m_bufId );
			++m_state;
			break;

		  case 2:
			obj.rdma().checkRQ( evQ, m_rqId, &m_status, true );
			++m_state;
			break;

		  case 3:
			obj.output("WriteSource: message from srcNid=%d srcPid=%d length=%zu bufId=0x%" PRIx64 "\n",
                   m_status.procAddr.node, m_status.procAddr.pid, m_status.length, m_status.addr.getSimVAddr());
			{
				Hermes::RDMA::Addr destAddr;	
				m_recvBuf.pop(&destAddr,sizeof( destAddr ) );
				printf("WriteSource: destAddr=0x%" PRIx64 "\n",destAddr);
				obj.rdma().write( evQ, m_status.procAddr, destAddr, m_rdmaBuf, m_writeLen ); 
				obj.rdma().send( evQ, m_status.procAddr, m_rqId, m_sendBuf.addr(), sizeof(MsgHdr) );
			}
			++m_state;
			break;

		  case 4:
			printf("WriteSource: Done\n");
			ret = true;
			break;
		}
		return ret;
	}

  private:

	int m_state;
	Hermes::MemAddr m_rdmaBuf;
	FifoBuf m_sendBuf;
	FifoBuf m_recvBuf;
	size_t m_writeLen;
	RDMA::RqId m_rqId;
	RDMA::ProcAddr m_procAddr;
	RDMA::Status m_status;
	RDMA::RecvBufId m_bufId;
	RDMA::MemRegionId m_memId;

	EmberRdmaTestGenerator& obj;
};
