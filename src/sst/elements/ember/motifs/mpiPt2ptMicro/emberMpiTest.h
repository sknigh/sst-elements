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


#ifndef _H_EMBER_MOTIF_MPI_PT2PT_TEST
#define _H_EMBER_MOTIF_MPI_PT2PT_TEST

#include "embergen.h"
#include "emberOsLib.h"
#include "libs/emberMpiPt2PtLib.h"
#include "libs/misc.h"

namespace SST {
namespace Ember {

class EmberMpiPt2PtTestGenerator : public EmberGenerator {

  public:
    SST_ELI_REGISTER_SUBCOMPONENT(
        EmberMpiPt2PtTestGenerator,
        "ember",
        "MpiPt2PtTestMotif",
        SST_ELI_ELEMENT_VERSION(1,0,0),
        "MPI Test",
        "SST::Ember::EmberGenerator"
    )

    SST_ELI_DOCUMENT_PARAMS()

	EmberMpiPt2PtTestGenerator(SST::Component* owner, Params& params) :
		EmberGenerator(owner, params, "MpiPt2PtTest" )
	{ 
        m_mpi = static_cast<EmberMpiPt2PtLib*>(getLib("mpiPt2Pt"));
        assert(m_mpi);
        m_mpi->initOutput( &getOutput() );

        m_miscLib = static_cast<EmberMiscLib*>(getLib("misc"));
        assert(m_miscLib);
        m_miscLib->initOutput( &getOutput() );

        m_osLib = new EmberOsLib();
        assert(m_osLib);
        m_osLib->initOutput( &getOutput() );

		std::string test = params.find<std::string>("arg.test","Msg");
		
		output("%s() running test=`%s`\n",__func__,test.c_str());
		if ( 0 == test.compare("BasicNonBlocking") ) { 
			m_rank0 = new Isend(this, params);
			m_rank1 = new Irecv(this, params);
		} else {
			output("%s() can't find test %s\n",__func__,test.c_str());
			assert(0);
		}

		m_generate = &EmberMpiPt2PtTestGenerator::init;

	}

	void completed(const SST::Output* output, uint64_t time ) {}
    bool generate( std::queue<EmberEvent*>& evQ)  {
        return (this->*m_generate)( evQ );
    }

  private:


	class Base {
	  public:
    		virtual bool generate( std::queue<EmberEvent*>& evQ) = 0;
	};

#include "emberMpiIsend.h"
#include "emberMpiIrecv.h"

    bool init( std::queue<EmberEvent*>& evQ) {
        misc().getNodeNum( evQ, &m_node_num );
        misc().getNumNodes( evQ, &m_num_nodes );
        m_generate = &EmberMpiPt2PtTestGenerator::init2;
        return false;
    }

    bool init2( std::queue<EmberEvent*>& evQ) {

		setVerbosePrefix( m_node_num );
        if ( 0 == m_node_num  ) {
            m_generate = &EmberMpiPt2PtTestGenerator::send;
        } else if ( 1 == m_node_num  ) {
            m_generate = &EmberMpiPt2PtTestGenerator::recv;
        }
        return (this->*m_generate)( evQ );
    }

    bool send( std::queue<EmberEvent*>& evQ ) {
        return m_rank0->generate( evQ );
    }
    bool recv( std::queue<EmberEvent*>& evQ ) {
        return m_rank1->generate( evQ );
    }

    EmberMpiPt2PtLib& mpi() { return *m_mpi; }
    EmberMiscLib& misc() { return *m_miscLib; }
    EmberOsLib& os()     { return *m_osLib; }

    int m_node_num;
    int m_num_nodes;

    EmberMpiPt2PtLib* m_mpi;
    EmberMiscLib* m_miscLib;
    EmberOsLib*   m_osLib;

	Base* m_rank0;
	Base* m_rank1;

	typedef bool (EmberMpiPt2PtTestGenerator::*GenerateFuncPtr)( std::queue<EmberEvent*>& evQ );
	GenerateFuncPtr  m_generate;
};

}
}

#endif
