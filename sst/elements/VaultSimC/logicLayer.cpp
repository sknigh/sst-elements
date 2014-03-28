// Copyright 2009-2011 Sandia Corporation. Under the terms
// of Contract DE-AC04-94AL85000 with Sandia Corporation, the U.S.
// Government retains certain rights in this software.
// 
// Copyright (c) 2012, Sandia Corporation
// All rights reserved.
// 
// This file is part of the SST software package. For license
// information, see the LICENSE file in the top level directory of the
// distribution.

#include <sst_config.h>
#include "sst/core/serialization.h"
#include <logicLayer.h>

#include <sst/core/interfaces/stringEvent.h>
#include <sst/core/interfaces/memEvent.h>
#include <sst/core/link.h>
#include <sst/core/params.h>

using namespace SST::Interfaces;

#define DBG( fmt, args... )m_dbg.write( "%s():%d: "fmt, __FUNCTION__, __LINE__, ##args)
//typedef  VaultCompleteFn; 

logicLayer::logicLayer( ComponentId_t id, Params& params ) :
  IntrospectedComponent( id ), memOps(0)
{
  dbg.init("@R:LogicLayer::@p():@l " + getName() + ": ", 0, 0, (Output::output_location_t)params.find_integer("debug", 0));
  dbg.output(CALL_INFO, "making logicLayer\n");

  std::string frequency = "2.2 GHz";
  frequency = params.find_string("clock", "2.2 Ghz");

  int ident = params.find_integer("llID", -1);
  if (-1 == ident) {
    _abort(logicLayer::logicLayer, "no llID defined\n");
  }
  llID = ident;

  bwlimit = params.find_integer( "bwlimit", -1 );
  if (-1 == bwlimit ) {
    _abort(logicLayer::logicLayer, 
	   " no <bwlimit> tag defined for logiclayer\n");
  }

  int mask = params.find_integer( "LL_MASK", -1 );
  if ( -1 == mask ) {
    _abort(logicLayer::logicLayer, 
	   " no <LL_MASK> tag defined for logiclayer\n");
  }
  LL_MASK = mask;

  bool terminal = params.find_integer("terminal", 0);

  int numVaults = params.find_integer("vaults", -1);
  if ( -1 != numVaults) {
    // connect up our vaults
    for (int i = 0; i < numVaults; ++i) {
      char bus_name[50];
      snprintf(bus_name, 50, "bus_%d", i);
      memChan_t *chan = configureLink( bus_name, "1 ns" );
      if (chan) {
	m_memChans.push_back(chan);
	dbg.output(" connected %s\n", bus_name);
      } else {
	_abort(logicLayer::logicLayer, 
	       " could not find %s\n", bus_name);
      }
    }
    printf(" Connected %d Vaults\n", numVaults);
  } else {
    _abort(logicLayer::logicLayer, 
	   " no <vaults> tag defined for LogicLayer\n");
  }

  // connect chain
  toCPU = configureLink( "toCPU");
  if (!terminal) {
    toMem = configureLink( "toMem");
  } else {
    toMem = 0;
  }

  registerClock( frequency, new Clock::Handler<logicLayer>(this, &logicLayer::clock) );

  dbg.output(CALL_INFO, "made logicLayer %d %p %p\n", llID, toMem, toCPU);
}

int logicLayer::Finish() 
{
  printf("Logic Layer %d completed %lld ops\n", llID, memOps);
  return 0;
}

void logicLayer::init(unsigned int phase) {
  // tell the bus (or whaterver) that we are here. only the first one
  // in the chain should report, so every one sends towards the cpu,
  // but only the first one will arrive.
  if ( !phase ) {
    toCPU->sendInitData(new SST::Interfaces::StringEvent("SST::Interfaces::MemEvent"));
  }

  // rec data events from the direction of the cpu
  SST::Event *ev = NULL;
  while ( (ev = toCPU->recvInitData()) != NULL ) {
    MemEvent *me = dynamic_cast<MemEvent*>(ev);
    if ( me ) {
      /* Push data to memory */
      if ( me->getCmd() == WriteReq ) {
	//printf("Memory received Init Command: of size 0x%x at addr 0x%lx\n", me->getSize(), me->getAddr() );
	uint32_t chunkSize = (1 << VAULT_SHIFT);
	if (me->getSize() > chunkSize) {
	  // may need to break request up in to 256 byte chunks (minimal
	  // vault width)
	  int numNewEv = (me->getSize() / chunkSize) + 1;
	  uint8_t *inData = &(me->getPayload()[0]);
	  SST::Interfaces::Addr addr = me->getAddr();
	  for (int i = 0; i < numNewEv; ++i) {
	    // make new event
	    MemEvent *newEv = new MemEvent(this, addr, me->getCmd());
	    // set size and payload
	    if (i != (numNewEv - 1)) {
	      newEv->setSize(chunkSize);
	      newEv->setPayload(chunkSize, inData);
	      inData += chunkSize;
	      addr += chunkSize;
	    } else {
	      uint32_t remain = me->getSize() - (chunkSize * (numNewEv - 1));
	      newEv->setSize(remain);
	      newEv->setPayload(remain, inData);
	    }
	    // sent to where it needs to go
	    if (isOurs(newEv->getAddr())) {
	      // send to the vault
	      unsigned int vaultID = 
		(newEv->getAddr() >> VAULT_SHIFT) % m_memChans.size();
	      m_memChans[vaultID]->sendInitData(newEv);      
	    } else {
	      // send down the chain
	      toMem->sendInitData(newEv);
	    }
	  }
	  delete ev;
	} else {
	  if (isOurs(me->getAddr())) {
	    // send to the vault
	    unsigned int vaultID = (me->getAddr() >> 8) % m_memChans.size();
	    m_memChans[vaultID]->sendInitData(me);      
	  } else {
	    // send down the chain
	    toMem->sendInitData(ev);
	  }
	}
      } else {
	printf("Memory received unexpected Init Command: %d\n", me->getCmd() );
      }
    }
  }

}


bool logicLayer::clock( Cycle_t current )
{
  SST::Event* e = 0;

  int tm[2] = {0,0}; //recv send
  int tc[2] = {0,0};

  // check for events from the CPU
  while((tc[0] < bwlimit) && (e = toCPU->recv())) {
    MemEvent *event  = dynamic_cast<MemEvent*>(e);
    dbg.output(CALL_INFO, "LL%d got req for %p (%lld %d)\n", llID, 
	       (void*)event->getAddr(), event->getID().first, event->getID().second);
    if (event == NULL) {
      _abort(logicLayer::clock, "logic layer got bad event\n");
    }

    tc[0]++;
    if (isOurs(event->getAddr())) {
      // it is ours!
      unsigned int vaultID = (event->getAddr() >> VAULT_SHIFT) % m_memChans.size();
      dbg.output(CALL_INFO, "ll%d sends %p to vault @ %lld\n", llID, event, 
		 current);
      m_memChans[vaultID]->send(event);      
    } else {
      // it is not ours
      if (toMem) {
	toMem->send( event );
	tm[1]++;
	dbg.output(CALL_INFO, "ll%d sends %p to next\n", llID, event);
      } else {
	//printf("ll%d not sure what to do with %p...\n", llID, event);
      }
    }
  }

  // check for events from the memory chain
  if (toMem) {
    while((tm[0] < bwlimit) && (e = toMem->recv())) {
      MemEvent *event  = dynamic_cast<MemEvent*>(e);
      if (event == NULL) {
	_abort(logicLayer::clock, "logic layer got bad event\n");
      }

      tm[0]++;
      // pass along to the CPU
      dbg.output(CALL_INFO, "ll%d sends %p towards cpu (%lld %d)\n", 
		 llID, event, event->getID().first, event->getID().second);
      toCPU->send( event );
      tc[1]++;
    }
  }
	
  // check for incoming events from the vaults
  for (memChans_t::iterator i = m_memChans.begin(); 
       i != m_memChans.end(); ++i) {
    memChan_t *m_memChan = *i;
    while ((e = m_memChan->recv())) {
      MemEvent *event  = dynamic_cast<MemEvent*>(e);
      if (event == NULL) {
        _abort(logicLayer::clock, "logic layer got bad event from vaults\n");
      }
      dbg.output(CALL_INFO, "ll%d got an event %p from vault @ %lld, sends "
		 "towards cpu\n", llID, event, current);
      
      // send to CPU
      memOps++;
      toCPU->send( event );
      tc[1]++;
    }    
  }

  if (tm[0] > bwlimit || 
      tm[1] > bwlimit || 
      tc[0] > bwlimit || 
      tc[1] > bwlimit) {
    dbg.output(CALL_INFO, "ll%d Bandwdith: %d %d %d %d\n", 
	       llID, tm[0], tm[1], tc[0], tc[1]);
  }

  return false;
}

extern "C" {
	Component* create_logicLayer( SST::ComponentId_t id,  SST::Params& params )
	{
		return new logicLayer( id, params );
	}
}

