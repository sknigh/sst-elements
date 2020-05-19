// Copyright 2013-2019 NTESS. Under the terms
// of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.
//
// Copyright (c) 2013-2019, NTESS
// All rights reserved.
//
// Portions are copyright of other developers:
// See the file CONTRIBUTORS.TXT in the top level directory
// the distribution for more information.
//
// This file is part of the SST software package. For license
// information, see the LICENSE file in the top level directory of the
// distribution.

#ifndef _MEMHIERARCHY_PCIE_LINK_SUBCOMPONENT_H_
#define _MEMHIERARCHY_PCIE_LINK_SUBCOMPONENT_H_

#include <string>
#include <map>
#include <queue>

#include <sst/core/event.h>
#include <sst/core/output.h>
#include <sst/core/subcomponent.h>

#include "sst/elements/memHierarchy/memEventBase.h"
#include "sst/elements/memHierarchy/memEvent.h"
#include "sst/elements/memHierarchy/util.h"
#include "sst/elements/memHierarchy/memTypes.h"
#include "sst/elements/memHierarchy/memLinkBase.h"

namespace SST {
namespace MemHierarchy {

class PCIE_Link : public MemLinkBase {


	class LinkEvent : public Event {

	  public:

		enum Type { TLP, DLLP } type;
		enum DLLP_type { ACK, CREDIT } dllpType;
		enum CreditType { PH, NPH, CPLH } creditType;

		LinkEvent() : Event() {}
		LinkEvent(MemEventBase* ev, size_t pktLen) : Event(), memEventBase(ev), pktLen(pktLen), type( TLP ) {}
		LinkEvent( size_t pktLen ) : Event(), type(TLP), pktLen(pktLen) {}

		LinkEvent( DLLP_type dllpType, size_t pktLen ) : Event(), type(DLLP), dllpType(ACK), pktLen(pktLen)  {}

		LinkEvent( CreditType type, int hdrCredit, int dataCredit, size_t pktLen ) :
			Event(), type(DLLP), dllpType(CREDIT),
			creditType(type), hdrCredit(hdrCredit), dataCredit(dataCredit), pktLen(pktLen) {}

#if 0
		LinkEvent(const LinkEvent* me) : Event() { copy( *this, *me ); }
    	LinkEvent(const LinkEvent& me) : Event() { copy( *this, me ); }
#endif

		MemEventBase* memEventBase;
		size_t pktLen;
		int hdrCredit;
		int dataCredit;

      private:

#if 0
    	void copy( LinkEvent& to, const LinkEvent& from ) {
			to.memEventBase = from.memEventBase;
			to.type = from.type;
		}
#endif

	  public:
#if 0
   		void serialize_order(SST::Core::Serialization::serializer &ser)  override {
        	Event::serialize_order(ser);
			ser & memEventBase;
			ser & type;
    	}
    	ImplementSerializable(SST::MemHierarchy::PCIE_Link::LinkEvent);
#endif

		NotSerializable( SST::MemHierarchy::PCIE_Link::LinkEvent);
	};

	class SelfEvent : public Event {
	  public:
		enum Type { OutQ, InQ } type;
		SelfEvent( Type type ) : Event(), type( type ) {}
		NotSerializable( SST::MemHierarchy::PCIE_Link::SelfEvent);
	};

public:
    
    SST_ELI_REGISTER_SUBCOMPONENT_DERIVED(PCIE_Link, "memHierarchy", "PCIE_Link", SST_ELI_ELEMENT_VERSION(1,0,0),
            "", SST::MemHierarchy::MemLinkBase)

   	SST_ELI_DOCUMENT_PARAMS()

    SST_ELI_DOCUMENT_STATISTICS()


/* Begin class definition */    

    PCIE_Link(ComponentId_t id, Params &params);
public:
    /* Destructor */
    ~PCIE_Link() { }

    /* Functions called by parent for handling events */
    bool clock();
    virtual void send(MemEventBase * ev); 

    std::set<EndpointInfo>* getSources() { return NULL; }
    std::set<EndpointInfo>* getDests() { return NULL; }
    bool isDest(std::string UNUSED(str)) { return false; }
    bool isSource(std::string UNUSED(str)) { return false; }
    std::string findTargetDestination(Addr addr) { return ""; }

    /* Send and receive functions for MemLink */
    void sendInitData(MemEventInit * ev) {};
    MemEventInit* recvInitData( ) { return NULL; }
    MemEventBase * recv() { return NULL; }
	bool isClocked() { return true; }

    void init(unsigned int phase) {}
    void setup() {}
	void finish() {
#if 0
		printf("%s tlpOutQmax=%d dllpOutQmax=%d\n",getName().c_str(),m_tlpOutQmax,m_dllpOutQmax);
#endif
	}

	bool spaceToSend(MemEventBase* ev) {
		bool retval;
		auto* me = static_cast<MemEvent*>(ev);
		switch ( me->getCmd() ) {
			case Command::PutM:
			case Command::PrWrite:
				retval = m_numHdrCredits[TX][P] > 0 && m_numDataCredits[TX][P] >= me->getPayload().size()/4;
				break;

			case Command::GetX:
				if ( ev->queryFlag(MemEvent::F_NONCACHEABLE) ) {
					retval = m_numHdrCredits[TX][P] > 0 && m_numDataCredits[TX][P] >= me->getPayload().size()/4;
				} else {
					retval = m_numHdrCredits[TX][NP] > 0;
				}
				break;

			case Command::GetS:
			case Command::PrRead:
				retval = m_numHdrCredits[TX][NP] > 0;
				break;

			case Command::GetXResp:
				if ( ev->queryFlag(MemEvent::F_NONCACHEABLE) ) {
					retval = true;
				} else {
					retval = m_numHdrCredits[TX][CPL] > 0 && m_numDataCredits[TX][CPL] >= me->getPayload().size()/4;
				}
				break;

			case Command::GetSResp:
				retval = m_numHdrCredits[TX][CPL] > 0 && m_numDataCredits[TX][CPL] >= me->getPayload().size()/4;
				break;

			default:
				 m_dbg.output( "(%s) Received: '%s'\n", getName().c_str(), ev->getBriefString().c_str());
				assert(0);
		}

		m_dbg.debug(CALL_INFO,1,0,"%s %s %s\n", getName().c_str(), ev->getBriefString().c_str(), retval ? "have space":" no space" );
		return retval;
	}
	size_t getMtuLength() { return m_mtuLength; }

private:
	void handleEvent( SST::Event* );
	void selfLinkHandler( Event* );
	void handleSendSideLat( Event* );
	void handleRecvSideLat( Event* );
	void recvPkt( LinkEvent* );
	void pushLinkEvent( LinkEvent* );

	void updateCredits( MemEventBase* meb ) {
		int hdrCredit = calcHdrCredit(meb);
		int dataCredit = calcDataCredit(meb);
		switch ( getCreditType( meb ) ) {
		  case LinkEvent::CreditType::PH:
				break;
		  case LinkEvent::CreditType::NPH:
				break;
		  case LinkEvent::CreditType::CPLH:
				break;
		}
	}

	int calcHdrCredit(MemEventBase* meb) {
		auto* me = static_cast<MemEvent*>(meb);
		switch ( me->getCmd() ) {
			case Command::PutM:
			case Command::GetX:
			case Command::PrWrite:
			case Command::GetS:
			case Command::PrRead:
			case Command::GetSResp:
			case Command::GetXResp:
				return 1;

			default:
				assert(0);
		}
	}

	int calcDataCredit(MemEventBase* meb) {
		auto* me = static_cast<MemEvent*>(meb);
		switch ( me->getCmd() ) {
			case Command::PutM:
			case Command::PrWrite:
				return me->getPayload().size()/4;

			case Command::GetX:
				if ( me->queryFlag(MemEvent::F_NONCACHEABLE) ) {
					return me->getPayload().size()/4;
				} else {
					return 0;
				}

			case Command::GetS:
			case Command::PrRead:
				return 0;

			case Command::GetSResp:
			case Command::GetXResp:
				return me->getPayload().size()/4;

			default:
				assert(0);
		}
	}

	LinkEvent::CreditType getCreditType(MemEventBase* meb) {
		auto* me = static_cast<MemEvent*>(meb);
		switch ( me->getCmd() ) {
			case Command::PutM:
			case Command::PrWrite:
				return LinkEvent::CreditType::PH;

			case Command::GetX:
				if ( me->queryFlag(MemEvent::F_NONCACHEABLE) ) {
					return LinkEvent::CreditType::PH;
				} else {
					return LinkEvent::CreditType::NPH;
				}

			case Command::GetS:
			case Command::PrRead:
				return LinkEvent::CreditType::NPH;

			case Command::GetSResp:
			case Command::GetXResp:
				return LinkEvent::CreditType::CPLH;

			default:
				assert(0);
		}
	}

	void addTxCredits(LinkEvent* le) {
		addCredits( le->creditType, le->hdrCredit, le->dataCredit, "TX", m_numHdrCredits[TX], m_numDataCredits[TX], m_maxHdrCredits, m_maxDataCredits );
	}

	void addRxCredits(MemEvent* me ) {
		addCredits( getCreditType( me ), calcHdrCredit( me ), calcDataCredit( me ),
		"RX", m_numHdrCredits[RX], m_numDataCredits[RX], m_maxHdrCredits, m_maxDataCredits );
	}

	void addCredits( LinkEvent::CreditType creditType, uint32_t hdrCredit, uint32_t dataCredit, std::string type, 
			std::vector<uint32_t>& hdrV, std::vector<uint32_t>& dataV, uint32_t maxHdr, uint32_t maxData ) 
	{
		uint32_t* hdr;
		uint32_t* data;
		switch ( creditType ) {
			case LinkEvent::CreditType::PH:
				hdr = &hdrV[P];
			    data = &dataV[P];
				type += " P";
				break;

			case LinkEvent::CreditType::NPH:
				hdr = &hdrV[NP];
			    data = &dataV[NP];
				type += " NP";
				break;

			case LinkEvent::CreditType::CPLH:
				hdr = &hdrV[CPL];
			    data = &dataV[CPL];
				type += " CPL";
				break;
		}
		m_dbg.debug(CALL_INFO,1,0,"%s PH %d add %d\n" , type.c_str(), *hdr, hdrCredit );
		*hdr += hdrCredit;
		assert( *hdr <= maxHdr);
		m_dbg.debug(CALL_INFO,1,0,"%s PD %d add %d\n", type.c_str(), *data, dataCredit );
		*data += dataCredit;
		assert( *data <= maxData );
	}

	void resetCredits( int x, int y ) {
		m_numHdrCredits[x][y] = 0;
		m_numDataCredits[x][y] = 0;
	}

	void consumeTxCredits(MemEventBase* ev) {
		consumeCredits( ev, "TX", m_numHdrCredits[TX], m_numDataCredits[TX] );
	}

	void consumeCredits( MemEventBase* ev, std::string type, std::vector<uint32_t>& hdrV, std::vector<uint32_t>& dataV ) {
		uint32_t hdrValue;
		uint32_t dataValue;
		uint32_t* hdr;
		uint32_t* data;
		auto* me = static_cast<MemEvent*>(ev);
		switch ( me->getCmd() ) {
			case Command::PrWrite:
			case Command::PutM:

				hdr = &hdrV[P];
				data = &dataV[P];
				hdrValue = 1;
				dataValue = me->getPayload().size()/4;
				type += " Posted";
				break;

			case Command::GetX:
				if ( ev->queryFlag(MemEvent::F_NONCACHEABLE) ) {
					hdr = &hdrV[P];
					data = &dataV[P];
					hdrValue = 1;
					dataValue = me->getPayload().size()/4;
					type += " Posted";
				} else {
					hdr = &hdrV[NP];
					data = &dataV[NP];
					hdrValue = 1;
					dataValue = 0;
					type += " NonPosted";
				}
				break;

			case Command::GetS:
			case Command::PrRead:
				hdr = &hdrV[NP];
				data = &dataV[NP];
				hdrValue = 1;
				dataValue = 0;
				type += " NonPosted";
				break;

			case Command::GetSResp:
			case Command::GetXResp:

				hdr = &hdrV[CPL];
				data = &dataV[CPL];
				hdrValue = 1;
				dataValue = me->getPayload().size()/4;
				type += " Compl";
				break;

			default:
				assert(0);
		}
		assert( *hdr - 1 >= 0);
		*hdr -= hdrValue;
		assert( *data - dataValue >= 0 );
		*data -= dataValue;

		m_dbg.debug(CALL_INFO,1,0,"%s hdr %d data %d\n" , type.c_str(), *hdr, *data );
	}
	
    bool isRead( MemEvent* me ) {
        dbg.debug( CALL_INFO,1,0,"%s %s %s\n",getName().c_str(),
            me->getBriefString().c_str(), me->queryFlag(MemEvent::F_NONCACHEABLE)? "NONCACHEABLE":"CACHEABLE");
        switch ( me->getCmd() )  {
            case Command::GetX:
                if ( me->queryFlag(MemEvent::F_NONCACHEABLE) ) {
                    return false;
                } else {
                    return true;
                }

            case Command::GetS:
            case Command::PrRead:
                return true;

            case Command::PrWrite:
                return false;

            default:
                printf("%s %s() %s\n",getName().c_str(),__func__,me->getBriefString().c_str());
                assert(0);
                return false;
        }
    }

	size_t calcPktLength( MemEventBase* ev) {
		auto* me = static_cast<MemEvent*>(ev);
		size_t len;
		switch ( ev->getCmd() ) {
			case Command::PutM:
			case Command::PrWrite:
				len = m_dllpHdrLength + m_tlpSeqLength + m_tlpHdrLength + me->getPayload().size() + m_lcrcLength;
				break;

			case Command::GetX:
                if ( me->queryFlag(MemEvent::F_NONCACHEABLE) ) {
					len = m_dllpHdrLength + m_tlpSeqLength + m_tlpHdrLength + me->getPayload().size() + m_lcrcLength;
				} else {
					len = m_dllpHdrLength + m_tlpSeqLength + m_tlpHdrLength + m_lcrcLength;
				}
				break;

			case Command::GetS: // read non-cacheable
			case Command::PrRead:
				len = m_dllpHdrLength + m_tlpSeqLength + m_tlpHdrLength + m_lcrcLength;
				break;

			case Command::GetXResp: // PrWrite or GetX  Resp
                if ( me->queryFlag(MemEvent::F_NONCACHEABLE) ) {
					len = m_dllpHdrLength + m_tlpSeqLength + m_tlpHdrLength + m_lcrcLength;
				} else {
					len = m_dllpHdrLength + m_tlpSeqLength + m_tlpHdrLength + me->getPayload().size() + m_lcrcLength;
				}
				break;

			case Command::GetSResp: // PrRead or GetS  Resp
				len = m_dllpHdrLength + m_tlpSeqLength + m_tlpHdrLength + me->getPayload().size() + m_lcrcLength;
				break;

			default:
				assert(0);
		}
#if 0
        printf("%s %s() %zu %zu %s %s\n",
			getName().c_str(),
			__func__,
			len,
			me->getPayload().size(),
			me->getBriefString().c_str(),
			me->queryFlag(MemEvent::F_NONCACHEABLE)?"NONCACHE":"CACHE");
#endif
		return len;
	}

	SimTime_t calcNumClocks(size_t length ) {
		SimTime_t clocks = ( length / m_linkWidthBytes ) + ( length % m_linkWidthBytes ? 1 : 0 ); 
		m_dbg.debug(CALL_INFO,1,0,"length=%zu bytesPerClock=%d clocks=%" PRIu64 " lat=%" PRIu64" ps.\n",
				length, m_linkWidthBytes, clocks, clocks * m_latPerByte_ps);
		return clocks; 
	}


	uint32_t m_linkWidthBytes;
	SimTime_t m_latPerByte_ps;

	uint32_t m_maxMtuLength;

	uint32_t m_tlpHdrLength;
	uint32_t m_tlpSeqLength;
	uint32_t m_lcrcLength;
	uint32_t m_dllpHdrLength;
	
	enum { P, NP, CPL };
	enum { TX, RX };

	std::vector< std::vector<uint32_t> > m_numHdrCredits;
	std::vector< std::vector<uint32_t> > m_numDataCredits;
	uint32_t m_maxHdrCredits;
	uint32_t m_maxDataCredits;

	Output m_dbg;
	Link* m_link;
	Link* m_selfLink;
	Link* m_inDelayLink;
	Link* m_outDelayLink;
	std::map<Event::id_type, MemEventBase*> m_req;

	std::queue<LinkEvent*> m_tlpOutQ;
	std::queue<LinkEvent*> m_dllpFcOutQ;
	std::queue<LinkEvent*> m_dllpAckOutQ;

	std::queue<LinkEvent*> m_inQ;

	std::queue<MemEvent*> m_nackEventQ;

	int m_tlpOutQmax;
	int m_dllpOutQmax;

	bool m_linkIdle;
	double m_creditThreshold;

	SimTime_t m_oldestAck;
	SimTime_t m_ackTimeout;
	size_t m_mtuLength;

	int m_outLatencyClocks;
	int m_inLatencyClocks;
};

} //namespace memHierarchy
} //namespace SST

#endif
