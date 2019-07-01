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

#ifndef L1COHERENCECONTROLLER_H
#define L1COHERENCECONTROLLER_H

#include <iostream>


#include "sst/elements/memHierarchy/coherencemgr/coherenceController.h"


namespace SST { namespace MemHierarchy {

class L1CoherenceController : public CoherenceController {
public:
/* Element Library Info */
    SST_ELI_REGISTER_SUBCOMPONENT_DERIVED(L1CoherenceController, "memHierarchy", "L1CoherenceController", SST_ELI_ELEMENT_VERSION(1,0,0), 
            "Implements MESI or MSI coherence for an L1 cache", SST::MemHierarchy::CoherenceController)

    SST_ELI_DOCUMENT_STATISTICS(
        /* Event sends */
        {"eventSent_GetS",          "Number of GetS requests sent", "events", 2},
        {"eventSent_GetX",          "Number of GetX requests sent", "events", 2},
        {"eventSent_GetSX",         "Number of GetSX requests sent", "events", 2},
        {"eventSent_GetSResp",      "Number of GetSResp responses sent", "events", 2},
        {"eventSent_GetXResp",      "Number of GetXResp responses sent", "events", 2},
        {"eventSent_PutS",          "Number of PutS requests sent", "events", 2},
        {"eventSent_PutE",          "Number of PutE requests sent", "events", 2},
        {"eventSent_PutM",          "Number of PutM requests sent", "events", 2},
        {"eventSent_FetchResp",     "Number of FetchResp requests sent", "events", 2},
        {"eventSent_FetchXResp",    "Number of FetchXResp requests sent", "events", 2},
        {"eventSent_AckInv",        "Number of AckInvs sent", "events", 2},
        {"eventSent_NACK",          "Number of NACKs sent ", "events", 2},
        {"eventSent_FlushLine",     "Number of FlushLine requests sent", "events", 2},
        {"eventSent_FlushLineInv",  "Number of FlushLineInv requests sent", "events", 2},
        {"eventSent_FlushLineResp", "Number of FlushLineResp responses sent", "events", 2},
        /* Event/State combinations - Count how many times an event was seen in particular state */
        {"stateEvent_GetS_I",           "Event/State: Number of times a GetS was seen in state I (Miss)", "count", 3},
        {"stateEvent_GetS_S",           "Event/State: Number of times a GetS was seen in state S (Hit)", "count", 3},
        {"stateEvent_GetS_E",           "Event/State: Number of times a GetS was seen in state E (Hit)", "count", 3},
        {"stateEvent_GetS_M",           "Event/State: Number of times a GetS was seen in state M (Hit)", "count", 3},
        {"stateEvent_GetX_I",           "Event/State: Number of times a GetX was seen in state I (Miss)", "count", 3},
        {"stateEvent_GetX_S",           "Event/State: Number of times a GetX was seen in state S (Miss)", "count", 3},
        {"stateEvent_GetX_E",           "Event/State: Number of times a GetX was seen in state E (Hit)", "count", 3},
        {"stateEvent_GetX_M",           "Event/State: Number of times a GetX was seen in state M (Hit)", "count", 3},
        {"stateEvent_GetSX_I",          "Event/State: Number of times a GetSX was seen in state I (Miss)", "count", 3},
        {"stateEvent_GetSX_S",          "Event/State: Number of times a GetSX was seen in state S (Miss)", "count", 3},
        {"stateEvent_GetSX_E",          "Event/State: Number of times a GetSX was seen in state E (Hit)", "count", 3},
        {"stateEvent_GetSX_M",          "Event/State: Number of times a GetSX was seen in state M (Hit)", "count", 3},
        {"stateEvent_GetSResp_IS",      "Event/State: Number of times a GetSResp was seen in state IS", "count", 3},
        {"stateEvent_GetXResp_IS",      "Event/State: Number of times a GetXResp was seen in state IS", "count", 3},
        {"stateEvent_GetXResp_IM",      "Event/State: Number of times a GetXResp was seen in state IM", "count", 3},
        {"stateEvent_GetXResp_SM",      "Event/State: Number of times a GetXResp was seen in state SM", "count", 3},
        {"stateEvent_Inv_I",            "Event/State: Number of times an Inv was seen in state I", "count", 3},
        {"stateEvent_Inv_IS",           "Event/State: Number of times an Inv was seen in state IS", "count", 3},
        {"stateEvent_Inv_IM",           "Event/State: Number of times an Inv was seen in state IM", "count", 3},
        {"stateEvent_Inv_IB",           "Event/State: Number of times an Inv was seen in state I_B", "count", 3},
        {"stateEvent_Inv_S",            "Event/State: Number of times an Inv was seen in state S", "count", 3},
        {"stateEvent_Inv_SM",           "Event/State: Number of times an Inv was seen in state SM", "count", 3},
        {"stateEvent_Inv_SB",           "Event/State: Number of times an Inv was seen in state S_B", "count", 3},
        {"stateEvent_FetchInv_I",       "Event/State: Number of times a FetchInv was seen in state I", "count", 3},
        {"stateEvent_FetchInv_IS",      "Event/State: Number of times a FetchInv was seen in state IS", "count", 3},
        {"stateEvent_FetchInv_IM",      "Event/State: Number of times a FetchInv was seen in state IM", "count", 3},
        {"stateEvent_FetchInv_SM",      "Event/State: Number of times a FetchInv was seen in state SM", "count", 3},
        {"stateEvent_FetchInv_S",       "Event/State: Number of times a FetchInv was seen in state S", "count", 3},
        {"stateEvent_FetchInv_E",       "Event/State: Number of times a FetchInv was seen in state E", "count", 3},
        {"stateEvent_FetchInv_M",       "Event/State: Number of times a FetchInv was seen in state M", "count", 3},
        {"stateEvent_FetchInv_IB",      "Event/State: Number of times a FetchInv was seen in state I_B", "count", 3},
        {"stateEvent_FetchInv_SB",      "Event/State: Number of times a FetchInv was seen in state S_B", "count", 3},
        {"stateEvent_FetchInvX_I",      "Event/State: Number of times a FetchInvX was seen in state I", "count", 3},
        {"stateEvent_FetchInvX_IS",     "Event/State: Number of times a FetchInvX was seen in state IS", "count", 3},
        {"stateEvent_FetchInvX_IM",     "Event/State: Number of times a FetchInvX was seen in state IM", "count", 3},
        {"stateEvent_FetchInvX_E",      "Event/State: Number of times a FetchInvX was seen in state E", "count", 3},
        {"stateEvent_FetchInvX_M",      "Event/State: Number of times a FetchInvX was seen in state M", "count", 3},
        {"stateEvent_FetchInvX_IB",     "Event/State: Number of times a FetchInvX was seen in state I_B", "count", 3},
        {"stateEvent_FetchInvX_SB",     "Event/State: Number of times a FetchInvX was seen in state S_B", "count", 3},
        {"stateEvent_Fetch_I",          "Event/State: Number of times a Fetch was seen in state I", "count", 3},
        {"stateEvent_Fetch_IS",         "Event/State: Number of times a Fetch was seen in state IS", "count", 3},
        {"stateEvent_Fetch_IM",         "Event/State: Number of times a Fetch was seen in state IM", "count", 3},
        {"stateEvent_Fetch_S",          "Event/State: Number of times a Fetch was seen in state S", "count", 3},
        {"stateEvent_Fetch_SM",         "Event/State: Number of times a Fetch was seen in state SM", "count", 3},
        {"stateEvent_Fetch_IB",         "Event/State: Number of times a Fetch was seen in state I_B", "count", 3},
        {"stateEvent_Fetch_SB",         "Event/State: Number of times a Fetch was seen in state S_B", "count", 3},
        {"stateEvent_AckPut_I",         "Event/State: Number of times an AckPut was seen in state I", "count", 3},
        {"stateEvent_FlushLine_I",      "Event/State: Number of times a FlushLine was seen in state I", "count", 3},
        {"stateEvent_FlushLine_S",      "Event/State: Number of times a FlushLine was seen in state S", "count", 3},
        {"stateEvent_FlushLine_E",      "Event/State: Number of times a FlushLine was seen in state E", "count", 3},
        {"stateEvent_FlushLine_M",      "Event/State: Number of times a FlushLine was seen in state M", "count", 3},
        {"stateEvent_FlushLine_IS",     "Event/State: Number of times a FlushLine was seen in state IS", "count", 3},
        {"stateEvent_FlushLine_IM",     "Event/State: Number of times a FlushLine was seen in state IM", "count", 3},
        {"stateEvent_FlushLine_SM",     "Event/State: Number of times a FlushLine was seen in state SM", "count", 3},
        {"stateEvent_FlushLine_IB",     "Event/State: Number of times a FlushLine was seen in state I_B", "count", 3},
        {"stateEvent_FlushLine_SB",     "Event/State: Number of times a FlushLine was seen in state S_B", "count", 3},
        {"stateEvent_FlushLineInv_I",       "Event/State: Number of times a FlushLineInv was seen in state I", "count", 3},
        {"stateEvent_FlushLineInv_S",       "Event/State: Number of times a FlushLineInv was seen in state S", "count", 3},
        {"stateEvent_FlushLineInv_E",       "Event/State: Number of times a FlushLineInv was seen in state E", "count", 3},
        {"stateEvent_FlushLineInv_M",       "Event/State: Number of times a FlushLineInv was seen in state M", "count", 3},
        {"stateEvent_FlushLineInv_IS",      "Event/State: Number of times a FlushLineInv was seen in state IS", "count", 3},
        {"stateEvent_FlushLineInv_IM",      "Event/State: Number of times a FlushLineInv was seen in state IM", "count", 3},
        {"stateEvent_FlushLineInv_SM",      "Event/State: Number of times a FlushLineInv was seen in state SM", "count", 3},
        {"stateEvent_FlushLineInv_IB",      "Event/State: Number of times a FlushLineInv was seen in state I_B", "count", 3},
        {"stateEvent_FlushLineInv_SB",      "Event/State: Number of times a FlushLineInv was seen in state S_B", "count", 3},
        {"stateEvent_FlushLineResp_I",      "Event/State: Number of times a FlushLineResp was seen in state I", "count", 3},
        {"stateEvent_FlushLineResp_IB",     "Event/State: Number of times a FlushLineResp was seen in state I_B", "count", 3},
        {"stateEvent_FlushLineResp_SB",     "Event/State: Number of times a FlushLineResp was seen in state S_B", "count", 3},
        /* Eviction - count attempts to evict in a particular state */
        {"evict_I",                 "Eviction: Attempted to evict a block in state I", "count", 3},
        {"evict_S",                 "Eviction: Attempted to evict a block in state S", "count", 3},
        {"evict_E",                 "Eviction: Attempted to evict a block in state E", "count", 3},
        {"evict_M",                 "Eviction: Attempted to evict a block in state M", "count", 3},
        {"evict_IS",                "Eviction: Attempted to evict a block in state IS", "count", 3},
        {"evict_IM",                "Eviction: Attempted to evict a block in state IM", "count", 3},
        {"evict_SM",                "Eviction: Attempted to evict a block in state SM", "count", 3},
        {"evict_IB",                "Eviction: Attempted to evict a block in state S_B", "count", 3},
        {"evict_SB",                "Eviction: Attempted to evict a block in state I_B", "count", 3},
        /* Latency for different kinds of misses*/
        {"latency_GetS_IS",         "Latency for read misses in I state", "cycles", 1},
        {"latency_GetS_M",          "Latency for read misses that find the block owned by another cache in M state", "cycles", 1},
        {"latency_GetX_IM",         "Latency for write misses in I state", "cycles", 1},
        {"latency_GetX_SM",         "Latency for write misses in S state", "cycles", 1},
        {"latency_GetX_M",          "Latency for write misses that find the block owned by another cache in M state", "cycles", 1},
        {"latency_GetSX_IM",        "Latency for read-exclusive misses in I state", "cycles", 1},
        {"latency_GetSX_SM",        "Latency for read-exclusive misses in S state", "cycles", 1},
        {"latency_GetSX_M",         "Latency for read-exclusive misses that find the block owned by another cache in M state", "cycles", 1},
        /* Track what happens to prefetched blocks */
        {"prefetch_useful",         "Prefetched block had a subsequent hit (useful prefetch)", "count", 2},
        {"prefetch_evict",          "Prefetched block was evicted/flushed before being accessed", "count", 2},
        {"prefetch_inv",            "Prefetched block was invalidated before being accessed", "count", 2},
        {"prefetch_coherence_miss", "Prefetched block incurred a coherence miss (upgrade) on its first access", "count", 2},
        {"prefetch_redundant",      "Prefetch issued for a block that was already in cache", "count", 2},
        /* Miscellaneous */
        {"EventStalledForLockedCacheline",  "Number of times an event (FetchInv, FetchInvX, eviction, Fetch, etc.) was stalled because a cache line was locked", "instances", 1},
        {"default_stat",            "Default statistic used for unexpected events/states/etc. Should be 0, if not, check for missing statistic registerations.", "none", 7})

/* Class definition */
    /** Constructor for L1CoherenceController */
    L1CoherenceController(Component* comp, Params& params) : CoherenceController(comp, params) { }
    L1CoherenceController(ComponentId_t id, Params& params, Params& ownerParams, bool prefetch) : CoherenceController(id, params, ownerParams, prefetch) {
        params.insert(ownerParams);
        debug->debug(_INFO_,"--------------------------- Initializing [L1Controller] ... \n\n");
        
        snoopL1Invs_ = params.find<bool>("snoop_l1_invalidations", false);
        protocol_ = params.find<bool>("protocol", true);
   
        // Default statistic avoids segfault in case we forgot to implement one
        Statistic<uint64_t> * defStat = registerStatistic<uint64_t>("default_stat");
        for (int i = 0; i < (int)Command::LAST_CMD; i++) {
            stat_eventSent[i] = defStat;
            for (int j = 0; j < LAST_STATE; j++) {
                stat_eventState[i][j] = defStat;
            }
        }

        // Register statistics
        stat_eventState[(int)Command::GetS][I] =      registerStatistic<uint64_t>("stateEvent_GetS_I");
        stat_eventState[(int)Command::GetS][S] =      registerStatistic<uint64_t>("stateEvent_GetS_S");
        stat_eventState[(int)Command::GetS][M] =      registerStatistic<uint64_t>("stateEvent_GetS_M");
        stat_eventState[(int)Command::GetX][I] =      registerStatistic<uint64_t>("stateEvent_GetX_I");
        stat_eventState[(int)Command::GetX][S] =      registerStatistic<uint64_t>("stateEvent_GetX_S");
        stat_eventState[(int)Command::GetX][M] =      registerStatistic<uint64_t>("stateEvent_GetX_M");
        stat_eventState[(int)Command::GetSX][I] =     registerStatistic<uint64_t>("stateEvent_GetSX_I");
        stat_eventState[(int)Command::GetSX][S] =     registerStatistic<uint64_t>("stateEvent_GetSX_S");
        stat_eventState[(int)Command::GetSX][M] =     registerStatistic<uint64_t>("stateEvent_GetSX_M");
        stat_eventState[(int)Command::GetSResp][IS] = registerStatistic<uint64_t>("stateEvent_GetSResp_IS");
        stat_eventState[(int)Command::GetXResp][IS] = registerStatistic<uint64_t>("stateEvent_GetXResp_IS");
        stat_eventState[(int)Command::GetXResp][IM] = registerStatistic<uint64_t>("stateEvent_GetXResp_IM");
        stat_eventState[(int)Command::GetXResp][SM] = registerStatistic<uint64_t>("stateEvent_GetXResp_SM");
        stat_eventState[(int)Command::Inv][I] =       registerStatistic<uint64_t>("stateEvent_Inv_I");
        stat_eventState[(int)Command::Inv][S] =       registerStatistic<uint64_t>("stateEvent_Inv_S");
        stat_eventState[(int)Command::Inv][IS] =      registerStatistic<uint64_t>("stateEvent_Inv_IS");
        stat_eventState[(int)Command::Inv][IM] =      registerStatistic<uint64_t>("stateEvent_Inv_IM");
        stat_eventState[(int)Command::Inv][SM] =      registerStatistic<uint64_t>("stateEvent_Inv_SM");
        stat_eventState[(int)Command::Inv][S_B] =       registerStatistic<uint64_t>("stateEvent_Inv_SB");
        stat_eventState[(int)Command::Inv][I_B] =       registerStatistic<uint64_t>("stateEvent_Inv_IB");
        stat_eventState[(int)Command::FetchInvX][I] =   registerStatistic<uint64_t>("stateEvent_FetchInvX_I");
        stat_eventState[(int)Command::FetchInvX][M] =   registerStatistic<uint64_t>("stateEvent_FetchInvX_M");
        stat_eventState[(int)Command::FetchInvX][IS] =  registerStatistic<uint64_t>("stateEvent_FetchInvX_IS");
        stat_eventState[(int)Command::FetchInvX][IM] =  registerStatistic<uint64_t>("stateEvent_FetchInvX_IM");
        stat_eventState[(int)Command::FetchInvX][S_B] = registerStatistic<uint64_t>("stateEvent_FetchInvX_SB");
        stat_eventState[(int)Command::FetchInvX][I_B] = registerStatistic<uint64_t>("stateEvent_FetchInvX_IB");
        stat_eventState[(int)Command::Fetch][I] =       registerStatistic<uint64_t>("stateEvent_Fetch_I");
        stat_eventState[(int)Command::Fetch][S] =       registerStatistic<uint64_t>("stateEvent_Fetch_S");
        stat_eventState[(int)Command::Fetch][IS] =      registerStatistic<uint64_t>("stateEvent_Fetch_IS");
        stat_eventState[(int)Command::Fetch][IM] =      registerStatistic<uint64_t>("stateEvent_Fetch_IM");
        stat_eventState[(int)Command::Fetch][SM] =      registerStatistic<uint64_t>("stateEvent_Fetch_SM");
        stat_eventState[(int)Command::Fetch][I_B] =     registerStatistic<uint64_t>("stateEvent_Fetch_IB");
        stat_eventState[(int)Command::Fetch][S_B] =     registerStatistic<uint64_t>("stateEvent_Fetch_SB");
        stat_eventState[(int)Command::FetchInv][I] =    registerStatistic<uint64_t>("stateEvent_FetchInv_I");
        stat_eventState[(int)Command::FetchInv][S] =    registerStatistic<uint64_t>("stateEvent_FetchInv_S");
        stat_eventState[(int)Command::FetchInv][M] =    registerStatistic<uint64_t>("stateEvent_FetchInv_M");
        stat_eventState[(int)Command::FetchInv][IS] =   registerStatistic<uint64_t>("stateEvent_FetchInv_IS");
        stat_eventState[(int)Command::FetchInv][IM] =   registerStatistic<uint64_t>("stateEvent_FetchInv_IM");
        stat_eventState[(int)Command::FetchInv][SM] =   registerStatistic<uint64_t>("stateEvent_FetchInv_SM");
        stat_eventState[(int)Command::FetchInv][S_B] =  registerStatistic<uint64_t>("stateEvent_FetchInv_SB");
        stat_eventState[(int)Command::FetchInv][I_B] =  registerStatistic<uint64_t>("stateEvent_FetchInv_IB");
        stat_eventState[(int)Command::FlushLine][I] =   registerStatistic<uint64_t>("stateEvent_FlushLine_I");
        stat_eventState[(int)Command::FlushLine][S] =   registerStatistic<uint64_t>("stateEvent_FlushLine_S");
        stat_eventState[(int)Command::FlushLine][M] =   registerStatistic<uint64_t>("stateEvent_FlushLine_M");
        stat_eventState[(int)Command::FlushLine][IS] =  registerStatistic<uint64_t>("stateEvent_FlushLine_IS");
        stat_eventState[(int)Command::FlushLine][IM] =  registerStatistic<uint64_t>("stateEvent_FlushLine_IM");
        stat_eventState[(int)Command::FlushLine][SM] =  registerStatistic<uint64_t>("stateEvent_FlushLine_SM");
        stat_eventState[(int)Command::FlushLine][I_B] = registerStatistic<uint64_t>("stateEvent_FlushLine_IB");
        stat_eventState[(int)Command::FlushLine][S_B] = registerStatistic<uint64_t>("stateEvent_FlushLine_SB");
        stat_eventState[(int)Command::FlushLineInv][I] =    registerStatistic<uint64_t>("stateEvent_FlushLineInv_I");
        stat_eventState[(int)Command::FlushLineInv][S] =    registerStatistic<uint64_t>("stateEvent_FlushLineInv_S");
        stat_eventState[(int)Command::FlushLineInv][M] =    registerStatistic<uint64_t>("stateEvent_FlushLineInv_M");
        stat_eventState[(int)Command::FlushLineInv][IS] =   registerStatistic<uint64_t>("stateEvent_FlushLineInv_IS");
        stat_eventState[(int)Command::FlushLineInv][IM] =   registerStatistic<uint64_t>("stateEvent_FlushLineInv_IM");
        stat_eventState[(int)Command::FlushLineInv][SM] =   registerStatistic<uint64_t>("stateEvent_FlushLineInv_SM");
        stat_eventState[(int)Command::FlushLineInv][I_B] =  registerStatistic<uint64_t>("stateEvent_FlushLineInv_IB");
        stat_eventState[(int)Command::FlushLineInv][S_B] =  registerStatistic<uint64_t>("stateEvent_FlushLineInv_SB");
        stat_eventState[(int)Command::FlushLineResp][I] =   registerStatistic<uint64_t>("stateEvent_FlushLineResp_I");
        stat_eventState[(int)Command::FlushLineResp][I_B] = registerStatistic<uint64_t>("stateEvent_FlushLineResp_IB");
        stat_eventState[(int)Command::FlushLineResp][S_B] = registerStatistic<uint64_t>("stateEvent_FlushLineResp_SB");
        stat_eventSent[(int)Command::GetS] =            registerStatistic<uint64_t>("eventSent_GetS");
        stat_eventSent[(int)Command::GetX] =            registerStatistic<uint64_t>("eventSent_GetX");
        stat_eventSent[(int)Command::GetSX] =           registerStatistic<uint64_t>("eventSent_GetSX");
        stat_eventSent[(int)Command::PutM] =            registerStatistic<uint64_t>("eventSent_PutM");
        stat_eventSent[(int)Command::NACK] =            registerStatistic<uint64_t>("eventSent_NACK");
        stat_eventSent[(int)Command::FlushLine] =       registerStatistic<uint64_t>("eventSent_FlushLine");
        stat_eventSent[(int)Command::FlushLineInv] =    registerStatistic<uint64_t>("eventSent_FlushLineInv");
        stat_eventSent[(int)Command::FetchResp] =       registerStatistic<uint64_t>("eventSent_FetchResp");
        stat_eventSent[(int)Command::FetchXResp] =      registerStatistic<uint64_t>("eventSent_FetchXResp");
        stat_eventSent[(int)Command::AckInv] =          registerStatistic<uint64_t>("eventSent_AckInv");
        stat_eventSent[(int)Command::GetSResp] =        registerStatistic<uint64_t>("eventSent_GetSResp");
        stat_eventSent[(int)Command::GetXResp] =        registerStatistic<uint64_t>("eventSent_GetXResp");
        stat_eventSent[(int)Command::FlushLineResp] =   registerStatistic<uint64_t>("eventSent_FlushLineResp");
        stat_eventStalledForLock =      registerStatistic<uint64_t>("EventStalledForLockedCacheline");
        stat_evict_S =                  registerStatistic<uint64_t>("evict_S");
        stat_evict_SM =                 registerStatistic<uint64_t>("evict_SM");
   
        /* Only for caches that expect writeback acks but don't know yet and can't register statistics later. Always enabled for now. */
        stat_eventState[(int)Command::AckPut][I] = registerStatistic<uint64_t>("stateEvent_AckPut_I");

        /* Only for caches that don't silently drop clean blocks but don't know yet and can't register statistics later. Always enabled for now. */
        stat_eventSent[(int)Command::PutS] = registerStatistic<uint64_t>("eventSent_PutS");
        stat_eventSent[(int)Command::PutE] = registerStatistic<uint64_t>("eventSent_PutE");

        // Only for caches that forward invs to the processor
        if (snoopL1Invs_) {
            stat_eventSent[(int)Command::Inv] = registerStatistic<uint64_t>("eventSent_Inv");
        }

        /* Prefetch statistics */
        if (prefetch) {
            statPrefetchEvict = registerStatistic<uint64_t>("prefetch_evict");
            statPrefetchInv = registerStatistic<uint64_t>("prefetch_inv");
            statPrefetchHit = registerStatistic<uint64_t>("prefetch_useful");
            statPrefetchUpgradeMiss = registerStatistic<uint64_t>("prefetch_coherence_miss");
            statPrefetchRedundant = registerStatistic<uint64_t>("prefetch_redundant");
        }

        /* MESI-specific statistics (as opposed to MSI) */
        if (protocol_) {
            stat_eventState[(int)Command::GetS][E] =        registerStatistic<uint64_t>("stateEvent_GetS_E");
            stat_eventState[(int)Command::GetX][E] =        registerStatistic<uint64_t>("stateEvent_GetX_E");
            stat_eventState[(int)Command::GetSX][E] =      registerStatistic<uint64_t>("stateEvent_GetSX_E");
            stat_eventState[(int)Command::FlushLine][E] =   registerStatistic<uint64_t>("stateEvent_FlushLine_E");
            stat_eventState[(int)Command::FlushLineInv][E] =    registerStatistic<uint64_t>("stateEvent_FlushLineInv_E");
            stat_eventState[(int)Command::FetchInv][E] =    registerStatistic<uint64_t>("stateEvent_FetchInv_E");
            stat_eventState[(int)Command::FetchInvX][E] =   registerStatistic<uint64_t>("stateEvent_FetchInvX_E");
        }
    }

    ~L1CoherenceController() {}
    
    /** Used to determine in advance if an event will be a miss (and which kind of miss)
     * Used for statistics only
     */
    int isCoherenceMiss(MemEvent* event, CacheLine* cacheLine);
    
    /* Event handlers called by cache controller */
    /** Send cache line data to the lower level caches */
    CacheAction handleEviction(CacheLine* wbCacheLine, string origRqstr, bool ignoredParam=false);

    /** Process new cache request:  GetX, GetS, GetSX */
    CacheAction handleRequest(MemEvent* event, CacheLine* cacheLine, bool replay);
   
    /** Process replacement - implemented for compatibility with CoherenceController but L1s do not receive replacements */
    CacheAction handleReplacement(MemEvent* event, CacheLine* cacheLine, MemEvent * origRequest, bool replay);
    
    /** Process Inv */
    CacheAction handleInvalidationRequest(MemEvent *event, CacheLine* cacheLine, MemEvent * collisionEvent, bool replay);

    /** Process responses */
    CacheAction handleCacheResponse(MemEvent* responseEvent, CacheLine* cacheLine, MemEvent* origRequest);
    CacheAction handleFetchResponse(MemEvent* responseEvent, CacheLine* cacheLine, MemEvent* origRequest);


    /* Methods for sending events, called by cache controller */
    /** Send response up (to processor) */
    uint64_t sendResponseUp(MemEvent * event, vector<uint8_t>* data, bool replay, uint64_t baseTime, bool atomic = false);
    
    /** Call through to coherenceController with statistic recording */
    void addToOutgoingQueue(Response& resp);
    void addToOutgoingQueueUp(Response& resp);

/* Miscellaneous */
    /** Determine whether a retry of a NACKed event is needed */
    bool isRetryNeeded(MemEvent * event, CacheLine * cacheLine);

    void printData(vector<uint8_t> * data, bool set);

/* Temporary */
    void setCacheArray(CacheArray* arrayptr) { cacheArray_ = arrayptr; }

private:
    bool        protocol_;  // True for MESI, false for MSI
    bool        snoopL1Invs_;
    CacheArray* cacheArray_;

    /* Statistics */
    Statistic<uint64_t>* stat_eventStalledForLock;
    Statistic<uint64_t>* stat_evict_S;
    Statistic<uint64_t>* stat_evict_SM;
    Statistic<uint64_t>* stat_eventSent[(int)Command::LAST_CMD];
    std::array<std::array<Statistic<uint64_t>*, LAST_STATE>, (int)Command::LAST_CMD> stat_eventState;

    /* Private event handlers */
    /** Handle GetX request. Request upgrade if needed */
    CacheAction handleGetXRequest(MemEvent* event, CacheLine* cacheLine, bool replay);
    
    /** Handle GetS request. Request block if needed */
    CacheAction handleGetSRequest(MemEvent* event, CacheLine* cacheLine, bool replay);
    
    /** Handle FlushLine request. */
    CacheAction handleFlushLineRequest(MemEvent *event, CacheLine* cacheLine, MemEvent* reqEvent, bool replay);
    
    /** Handle FlushLineInv request */
    CacheAction handleFlushLineInvRequest(MemEvent *event, CacheLine* cacheLine, MemEvent* reqEvent, bool replay);

    /** Handle Inv request */
    CacheAction handleInv(MemEvent * event, CacheLine * cacheLine, bool replay);
    
    /** Handle ForceInv request */
    CacheAction handleForceInv(MemEvent * event, CacheLine * cacheLine, bool replay);
    
    /** Handle Fetch */
    CacheAction handleFetch(MemEvent * event, CacheLine * cacheLine, bool replay);
    
    /** Handle FetchInv */
    CacheAction handleFetchInv(MemEvent * event, CacheLine * cacheLine, bool replay);
    
    /** Handle FetchInvX */
    CacheAction handleFetchInvX(MemEvent * event, CacheLine * cacheLine, bool replay);

    /** Handle data response - GetSResp or GetXResp */
    void handleDataResponse(MemEvent* responseEvent, CacheLine * cacheLine, MemEvent * reqEvent);

    
    /* Methods for sending events */
    /** Send response memEvent to lower level caches */
    void sendResponseDown(MemEvent* event, CacheLine* cacheLine, bool replay);

    /** Send writeback request to lower level caches */
    void sendWriteback(Command cmd, CacheLine* cacheLine, bool dirty, string origRqstr);

    /** Send AckInv response to lower level caches */
    void sendAckInv(MemEvent * request, CacheLine * cacheLine);

    /** Forward a flush line request, with or without data */
    void forwardFlushLine(Addr baseAddr, Command cmd, string origRqstr, CacheLine * cacheLine);
    
    /** Send response to a flush request */
    void sendFlushResponse(MemEvent * requestEvent, bool success, uint64_t baseTime, bool replay);

    /* Methods for recording statistics */
    void recordEvictionState(State state);
    void recordStateEventCount(Command cmd, State state);
    void recordEventSent(Command cmd);
    void recordEventSentUp(Command cmd) { recordEventSent(cmd); }
    void recordEventSentDown(Command cmd) { recordEventSent(cmd); }
};


}}


#endif

