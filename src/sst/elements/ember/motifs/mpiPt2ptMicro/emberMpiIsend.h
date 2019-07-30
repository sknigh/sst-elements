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


class Isend : public Base {
  public:
 	Isend(EmberMpiPt2PtTestGenerator* obj, Params& params ) : obj(*obj), m_state(0) {
		m_bufLen = params.find<size_t>( "arg.msgLength", 0x400 ); 
		m_tag = 0xf00d;
	} 

	bool generate( std::queue<EmberEvent*>& evQ ) {
		bool ret = false;
		switch ( m_state ) { 
		  case 0:
			obj.mpi().init( evQ, &m_numRanks, &m_myRank );
			obj.verbose(CALL_INFO,1,MOTIF_MASK,"malloc sendBuf %zu\n",m_bufLen);
			obj.misc().malloc( evQ, &m_sendBuf, m_bufLen);
			++m_state;
			break;

		  case 1:
			m_dest = ( m_myRank + 1 ) % m_numRanks;
			obj.output("Isend: numRanks=%d myRank=%d destRank=%d\n",m_numRanks,m_myRank,m_dest);
			obj.mpi().isend( evQ, m_sendBuf, m_bufLen, Hermes::Mpi::Char, m_dest, m_tag, Mpi::CommWorld, &m_request, &m_isendRetval );
			obj.mpi().wait( evQ, &m_request, &m_status, &m_waitRetval );
			++m_state;
			break;

		  case 2:
			obj.output("Isend: done retval=%d retval=%d\n",m_isendRetval,m_waitRetval);
			ret = true; 
		}
		return ret;
	}
		
  private:
	int m_numRanks;
	int m_myRank;
	int m_state;
	int m_dest;
	int m_tag;
	size_t m_bufLen;
	Mpi::Request m_request;
	Mpi::Status m_status;
	Hermes::MemAddr m_sendBuf;
	int m_isendRetval;
	int m_waitRetval;

	EmberMpiPt2PtTestGenerator& obj;
};
