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


class Irecv : public Base {
  public:
 	Irecv(EmberMpiPt2PtTestGenerator* obj, Params& params ) : obj(*obj), m_state(0) {
		m_bufLen = params.find<size_t>( "arg.msgLength", 0x400 ); 
		m_tag = 0xf00d;
	} 

	bool generate( std::queue<EmberEvent*>& evQ ) {
		bool ret = false;
        switch ( m_state ) {
          case 0:
			obj.mpi().init( evQ, &m_numRanks, &m_myRank );
            obj.verbose(CALL_INFO,1,MOTIF_MASK,"malloc recvdBuf %zu\n",m_bufLen);
            obj.misc().malloc( evQ, &m_recvBuf, m_bufLen);
            ++m_state;
            break;

          case 1:
			m_src = (m_myRank + 1) % m_numRanks;
            obj.output("Irecv: numRanks=%d myRank=%d srcRank=%d\n",m_numRanks,m_myRank,m_src);
            obj.mpi().irecv( evQ, m_recvBuf, m_bufLen, Hermes::Mpi::Char, m_src, m_tag, Mpi::CommWorld, &m_request, &m_irecvRetval );
            obj.mpi().wait( evQ, &m_request, &m_status, &m_waitRetval );
            ++m_state;
            break;

          case 2:
            obj.output("Irecv: done irecv retval=%d, wait retval=%d srcRank=%d tag=%d\n",m_irecvRetval,m_waitRetval,m_status.rank,m_status.tag);
            ret = true;
		}
		return ret;
	}

  private:
	int m_numRanks;
	int m_myRank;
	int m_state;
    int m_src;
    int m_tag;
    size_t m_bufLen;
    Mpi::Request m_request;
    Mpi::Status m_status;
    Hermes::MemAddr m_recvBuf;
    int m_irecvRetval;
    int m_waitRetval;

	EmberMpiPt2PtTestGenerator& obj;
};
