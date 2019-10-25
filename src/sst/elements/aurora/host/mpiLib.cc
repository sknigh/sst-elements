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


#include <sst_config.h>
#include "mpiLib.h"

using namespace SST::Aurora;
using namespace Hermes;

#define CALL_INFO_LAMBDA     __LINE__, __FILE__


MpiLib::MpiLib( ComponentId_t id, Params& params) : Interface(id), m_retFunctor(NULL), m_pt2pt(NULL)
{

    if ( params.find<bool>("print_all_params",false) ) {
        printf("Aurora::MpiLib()\n");
        params.print_all_params(std::cout);
    }

    m_dbg.init("@t:Aurora::MpiLib::@p():@l ", 
			params.find<uint32_t>("verboseLevel",0), 
			params.find<uint32_t>("verboseMask",0),
			Output::STDOUT );

    Params pt2ptParams =  params.find_prefix_params("pt2pt.");

	std::string moduleName = pt2ptParams.find<std::string>("module");

	m_dbg.debug(CALL_INFO,1,0,"pt2pt module name %s\n",moduleName.c_str());

    m_pt2pt = dynamic_cast< Hermes::Mpi::Interface*>( loadAnonymousSubComponent<Hermes::Interface>( moduleName, "",0, ComponentInfo::SHARE_NONE, pt2ptParams ) );
    assert(m_pt2pt);

	std::ostringstream tmp;
    tmp << this << "-AuroraMpiLibSelfLink";
   	m_selfLink = configureSelfLink(tmp.str(), "1 ns", new Event::Handler<MpiLib>(this,&MpiLib::selfLinkHandler));

	m_barrier = new Collectives::Barrier(*m_pt2pt);
}

void MpiLib::recv(const Hermes::MemAddr& addr, uint32_t count, MP::PayloadDataType dtype, MP::RankID source, uint32_t tag, MP::Communicator group,
        MP::MessageResponse* resp, MP::Functor* functor )
{
	m_dbg.debug(CALL_INFO,1,2,"buf=0x%" PRIx64 " count=%d dtype=%d source=%d tag=%d group=%d\n",
			addr.getSimVAddr(),count,convertDataType( dtype ),source,tag,group);

	Callback* cb = new Callback;

	*cb = [=](int retval) {
		m_dbg.debug(CALL_INFO_LAMBDA,"recv",1,2,"return to motif, count=%d dtype=%d source=%d tag=%d\n",
					count,convertDataType( dtype ),source,tag);
		// 
		//  We need to convert m_statue to resp
		//
		if ( (*functor)(retval) ) {
			delete functor;
		}	
	};

	assert( group == MP::GroupWorld );
	m_pt2pt->recv( addr, count, convertDataType( dtype ), source, tag, Mpi::CommWorld, &m_status, cb );
}

void MpiLib::send(const Hermes::MemAddr& addr, uint32_t count, MP::PayloadDataType dtype, MP::RankID dest, uint32_t tag,
            MP::Communicator group, MP::Functor* functor )
{
	m_dbg.debug(CALL_INFO,1,2,"buf=0x%" PRIx64 " count=%d dtype=%d dest=%d tag=%d group=%d\n",
			addr.getSimVAddr(),count,convertDataType( dtype ),dest,tag,group);

	Callback* cb = new Callback;

	*cb = [=](int retval) {
		m_dbg.debug(CALL_INFO_LAMBDA,"send",1,2,"return to motif, count=%d dtype=%d dest=%d tag=%d\n",
				count,convertDataType( dtype ),dest,tag);
		if ( (*functor)(retval) ) {
			delete functor;
		}	
	};

	assert( group == MP::GroupWorld );
	m_pt2pt->send( addr, count, convertDataType( dtype ), dest, tag, Mpi::CommWorld, cb );
}

void MpiLib::isend(const Hermes::MemAddr& addr, uint32_t count, MP::PayloadDataType dtype,
        MP::RankID dest, uint32_t tag, MP::Communicator group, MP::MessageRequest* req, MP::Functor* functor ) 
{
	Callback* cb = new Callback;

	*cb = [=](int retval) {
		m_dbg.debug(CALL_INFO_LAMBDA,"isend",1,2,"return to motif, count=%d dtype=%d dest=%d tag=%d\n",
				count,convertDataType( dtype ),dest,tag);
		if ( (*functor)(retval) ) {
			delete functor;
		}
	};

	assert( group == MP::GroupWorld );
	m_pt2pt->isend( addr, count, convertDataType( dtype ), dest, tag, Mpi::CommWorld, (Mpi::Request*)req, cb );
}

void MpiLib::irecv(const Hermes::MemAddr& addr, uint32_t count, MP::PayloadDataType dtype,
        MP::RankID source, uint32_t tag, MP::Communicator group, MP::MessageRequest* req, MP::Functor* functor) 
{
	Callback* cb = new Callback;

	*cb = [=](int retval) {
		m_dbg.debug(CALL_INFO_LAMBDA,"irecv",1,2,"return to motif, count=%d dtype=%d source=%d tag=%d\n",
					count,convertDataType( dtype ),source,tag);
		if ( (*functor)(retval) ) {
			delete functor;
		}
	};

	assert( group == MP::GroupWorld );
	m_pt2pt->irecv( addr, count, convertDataType( dtype ), source, tag, Mpi::CommWorld, (Mpi::Request*)req, cb );
}

void MpiLib::waitall( int count, MP::MessageRequest req[], MP::MessageResponse* resp[], MP::Functor* functor )
{
	Callback* cb = new Callback;

	Mpi::Status* status = new Mpi::Status[count];
	int* flag = new int;

	*cb = [=](int retval) {
		m_dbg.debug(CALL_INFO_LAMBDA,"waitall",1,2,"return to motif\n");

		//
		//  We need to convert m_statue to resp
		//
		delete[] status;
		delete flag;
		if ( (*functor)(retval) ) {
			delete functor;
		}
	};
	m_pt2pt->testall( count, (Mpi::Request*)req, flag, status, true, cb );
};
