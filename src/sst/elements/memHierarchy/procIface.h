// -*- mode: c++ -*-
// Copyright 2009-2019 NTESS. Under the terms
// of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.
//
// Copyright (c) 2009-2019, NTESS
// All rights reserved.
//
// Portions are copyright of other developers:
// See the file CONTRIBUTORS.TXT in the top level directory
// the distribution for more information.
//
// This file is part of the SST software package. For license
// information, see the LICENSE file in the top level directory of the
// distribution.
//

/*
 *  Interface between a processor that implements SimpleMem 
 *  and components that support PrRead/PrWrite/PrLock. 
 *  Currently this includes:
 *      - directory
 *
 *  Processor should load this as a subcomponent
 *  Processor should NOT attempt to use the link directly
 *
 *  This subcomponent interfaces to the on-chip communication fabric
 *  (e.g., bus, network) via a MemLinkBase subcomponent. The user must
 *  load a compatible subcomponent into the 'memlink' subcomponent slot
 *  in the Python. 
 *
 */

#ifndef COMPONENTS_MEMHIERARCHY_MEMORYINTERFACE_PROC
#define COMPONENTS_MEMHIERARCHY_MEMORYINTERFACE_PROC

#include <string>
#include <utility>
#include <map>
#include <queue>

#include <sst/core/sst_types.h>
#include <sst/core/link.h>
#include <sst/core/interfaces/simpleMem.h>
#include <sst/core/output.h>

#include "sst/elements/memHierarchy/memEventBase.h"
#include "sst/elements/memHierarchy/memEvent.h"
#include "sst/elements/memHierarchy/customcmd/customCmdEvent.h"
#include "sst/elements/memHierarchy/memLinkBase.h"

namespace SST {

class Component;
class Event;

namespace MemHierarchy {

/** Class is used to interface a compute mode (CPU, GPU) to MemHierarchy */
class MHProcInterface : public Interfaces::SimpleMem {

public:
/* Element Library Info */
    SST_ELI_REGISTER_SUBCOMPONENT_DERIVED(MHProcInterface, "memHierarchy", "iface.proc", SST_ELI_ELEMENT_VERSION(1,0,0),
            "Interface allowing processors to directly/coherently interact with a directory (without an L1). Converts SimpleMem requests into MemEventBases.", SST::Interfaces::SimpleMem)

    SST_ELI_DOCUMENT_SUBCOMPONENT_SLOTS( {"memlink", "Required, the memH link manager", "SST::MemHierarchy::MemLinkBase"} )


/* Begin class definition */
    MHProcInterface(SST::ComponentId_t id, Params &params, TimeConverter* time, HandlerBase* handler = NULL);
    
    /** Initialize the link to be used to connect with MemHierarchy */
    virtual bool initialize(const std::string &linkName, HandlerBase *handler = NULL);

    /** Link getter */
    virtual SST::Link* getLink(void) const { return link_; }

    virtual void sendInitData(Request *req);
    virtual void sendRequest(Request *req);
    virtual Request* recvResponse(void);

    void init(unsigned int phase);

protected:
    /** Function to create the custom memEvent that will be used by MemHierarchy */
    virtual MemEventBase* createCustomEvent(Interfaces::SimpleMem::Request* req) const;

    /** Function to update a SimpleMem request with a custom memEvent response */
    virtual void updateCustomRequest(Interfaces::SimpleMem::Request* req, MemEventBase *ev) const;
    
    Output      output;
    Addr        baseAddrMask_;
    std::string rqstr_;
    std::map<MemEventBase::id_type, Interfaces::SimpleMem::Request*> requests_;
    MemLinkBase* mlink_;
    SST::Link* link_;
    
    bool initDone_;
    std::queue<MemEventInit*> initSendQueue_;


private:

    /** Convert any incoming events to updated Requests, and fire handler */
    void handleIncoming(SST::Event *ev);
    
    /** Process MemEvents into updated Requests*/
    Interfaces::SimpleMem::Request* processIncoming(MemEventBase *ev);

    /** Update Request with results of MemEvent. Calls updateCustomRequest for custom events. */
    void updateRequest(Interfaces::SimpleMem::Request* req, MemEvent *me) const;
    
    /** Function used internally to create the memEvent that will be used by MemHierarchy */
    MemEventBase* createMemEvent(Interfaces::SimpleMem::Request* req) const;

    bool clock(Cycle_t time);

    HandlerBase*    recvHandler_;
};

}
}

#endif
