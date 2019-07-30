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


class Recv2 : public Base {
  public:
 	Recv2(EmberRdmaTestGenerator* obj, Params& params ) : obj(*obj), m_state(0) {
        m_msgLen = params.find<size_t>( "arg.msgLength", 0x400 );
        m_rqId = params.find<size_t>( "arg.recvQid", 100 );
        m_shortMsgLen = params.find<size_t>( "arg.shortMsgLength", 1024 );

        if ( m_msgLen > m_shortMsgLen ) {
            m_bufLen = sizeof(MsgHdr);
        } else {
            m_bufLen = sizeof(MsgHdr) + m_msgLen;
        }
	} 

	size_t m_msgLen;
	size_t m_shortMsgLen;
	bool generate( std::queue<EmberEvent*>& evQ ) {
		bool ret = false;
		switch ( m_state ) { 
		  case 0:
			obj.verbose(CALL_INFO,1,MOTIF_MASK,"Recv: malloc sendBuf %zu\n",m_bufLen);
			obj.misc().malloc( evQ, &m_sendBuf.addr(), m_bufLen, true );
			obj.verbose(CALL_INFO,1,MOTIF_MASK,"Recv: malloc recvdBuf %zu\n",m_bufLen);
			obj.misc().malloc( evQ, &m_recvBuf.addr(), m_bufLen, true );

			if ( m_msgLen > m_shortMsgLen ) {
				obj.verbose(CALL_INFO,1,MOTIF_MASK,"Recv: malloc RDMA buf %zu\n",m_msgLen);
				obj.misc().malloc( evQ, &m_rdmaBuf, m_msgLen, true );
			}

			obj.verbose(CALL_INFO,1,MOTIF_MASK,"Recv: createRQ\n");
			obj.rdma().createRQ( evQ, m_rqId );
			++m_state;
			break;

		  case 1:

			obj.verbose(CALL_INFO,1,MOTIF_MASK,"Recv: postRecv addr=0x%" PRIx64" backing=%p\n",
						m_recvBuf.addr().getSimVAddr(), m_recvBuf.addr().getBacking());
			obj.rdma().postRecv( evQ, m_rqId, m_recvBuf.addr(), m_bufLen, &m_bufId ); 
			obj.verbose(CALL_INFO,1,MOTIF_MASK,"Recv: checkRQ\n");
			obj.rdma().checkRQ( evQ, m_rqId, &m_status, true );
			++m_state;
			break;

		  case 2:
			obj.output("Recv: srcNid=%d srcPid=%d length=%zu bufId=0x%" PRIx64 "\n",
					m_status.procAddr.node,m_status.procAddr.pid, m_status.length,m_status.addr.getSimVAddr());
			{
				size_t length;
				m_recvBuf.pop(&length,sizeof( length ) );
				if ( length <= m_shortMsgLen ) {
					obj.output("Recv: received short message with %zu bytes\n", length );
					checkBuf( m_recvBuf.addr().getBacking(8), m_msgLen );
				} else {
					Hermes::RDMA::Addr srcAddr;
					m_recvBuf.pop(&srcAddr,sizeof( srcAddr ) );
					obj.output("Recv: received long message with %zu bytes srcAddr=0x%" PRIx64 "\n", length, srcAddr);

					obj.rdma().registerMem( evQ, m_rdmaBuf, m_msgLen, &m_memId );
					obj.rdma().read( evQ, m_status.procAddr, m_rdmaBuf, srcAddr, length ); 
					obj.verbose(CALL_INFO,1,MOTIF_MASK,"send\n");
					obj.rdma().send( evQ, m_status.procAddr, m_rqId, m_sendBuf.addr(), m_bufLen );
				}	
			}
			++m_state;
			break;

		  case 3:

			if ( m_msgLen > m_shortMsgLen ) {
				checkBuf( m_rdmaBuf.getBacking(), m_msgLen );
			}
			obj.output("Recv: done\n");
			ret = true; 
		}
		return ret;
	}
	void checkBuf( void* ptr, size_t length ) {
		unsigned char* buf = (unsigned char*) ptr;
		for ( int i = 0; i < length; i++ ) {
			if ( buf[i] != i % 256 ) {
				printf("buf[%d] = %d\n",i , buf[i]);
			}
		}
	}

  private:
	int m_state;
	Hermes::MemAddr m_rdmaBuf;
	FifoBuf m_sendBuf;
	FifoBuf m_recvBuf;
	size_t m_bufLen;
	RDMA::RqId m_rqId;
	RDMA::ProcAddr m_procAddr;
	RDMA::Status m_status;
	RDMA::RecvBufId m_bufId;
	RDMA::MemRegionId m_memId;

	EmberRdmaTestGenerator& obj;
};
