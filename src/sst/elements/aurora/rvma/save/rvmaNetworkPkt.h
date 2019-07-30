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


#ifndef COMPONENTS_SNOTRA_RVMA_NETWORK_PKT_H
#define COMPONENTS_SNOTRA_RVMA_NETWORK_PKT_H

#include <sst/core/interfaces/simpleNetwork.h>

namespace SST {
namespace Snotra {
namespace RVMA {

class NetworkPkt : public Event {
  public:

	NetworkPkt( int size ) : m_payloadSize(0), m_popOffset(0) {
   		m_payload.resize(size); 
	}					

	// add 8 bytes for a 2 byte srcPid, 2 byte destPid, 2 byte srcNid and 2 byte for extra
	int payloadSize() { return m_payloadSize + 8; } 
	
	void setSrcPid( int pid ) { m_srcPid = pid; }
	void setSrcNid( int nid ) { m_srcNid = nid; }

	void setDestPid( int pid ) { m_destPid = pid; }
	void setOp( uint8_t op ) { m_op = op; }

	int getDestPid() { return m_destPid; }
	uint8_t getOp( ) { return m_op; }

	size_t payloadPush( void* ptr, size_t length ) {
		size_t bytesLeft = pushBytesLeft();
		length = length <= bytesLeft ? length : bytesLeft;	
		if ( ptr ) {
            memcpy( &m_payload[m_payloadSize], (const char*) ptr, length );
		}
		m_payloadSize += length;	
		return length;
	}

	size_t payloadPop( void* ptr, size_t length ) {
		size_t bytesLeft = popBytesLeft();
		length = length <= bytesLeft ? length : bytesLeft ; 
		memcpy( ptr, &m_payload.at(m_popOffset), length );
		m_popOffset += length;
		return length;
	}

	unsigned char* payload() { return &m_payload.at(m_popOffset); }

	size_t popBytesLeft() { return m_payloadSize - m_popOffset; }
	size_t pushBytesLeft() { return m_payload.size() - m_payloadSize; }

  public:
    void serialize_order(SST::Core::Serialization::serializer &ser)  override {
        Event::serialize_order(ser);
        ser & m_payload;
		ser & m_payloadSize;
		ser & m_popOffset;
		ser & m_srcNid;
		ser & m_srcPid;
		ser & m_destPid;
		ser & m_op;
    }

  private:
	NetworkPkt() : m_payloadSize(0), m_popOffset(0) { m_payload.resize(2048); }
    ImplementSerializable(SST::Snotra::RVMA::NetworkPkt);

	std::vector<unsigned char> m_payload;
	int m_srcPid;
	int m_srcNid;
	int m_destPid;
	int m_op;

	int m_payloadSize;
	size_t m_popOffset;

};

}
}
}

#endif

