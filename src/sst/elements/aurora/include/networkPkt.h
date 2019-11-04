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


#ifndef COMPONENTS_AURORA_NETWORK_PKT_H
#define COMPONENTS_AURORA_NETWORK_PKT_H

#include <sst/core/interfaces/simpleNetwork.h>

namespace SST {
namespace Aurora {

typedef uint64_t StreamId;
class NetworkPkt : public Event {
  public:

	NetworkPkt( int size ) : Event(), m_payloadSize(0), m_popOffset(0), m_head(false) {
   		m_payload.resize(size); 
	}					

	// add 16 bytes: 
	// 2 byte srcPid
	// 2 byte destPid
	// 2 byte srcNid 
	// 1 bit head
	// 2 bits protocol
	// 4 bytes stream ID
	// 4 bytes stream offset 
	int payloadSize() { return m_payloadSize + 16; } 

	uint64_t pktNum;
	void setProto( uint8_t proto ) { m_proto = proto; }
	void setHead() { m_head = true; } 

	void setStreamId( StreamId id  ) { m_streamId = id; } 
	void setStreamOffset( uint32_t offset ) { m_streamOffset = offset; }
	void setSrcPid( int pid ) { m_srcPid = pid; }
	void setSrcNid( int nid ) { m_srcNid = nid; }
	void setDestPid( int pid ) { m_destPid = pid; }

	bool isHead() { return m_head; }
	uint8_t getProto() { return m_proto; }	
	StreamId getStreamId() { return m_streamId; }
	uint32_t getStreamOffset() { return m_streamOffset; }
	int getDestPid() { return m_destPid; }
	int getSrcPid() { return m_srcPid; }
	int getSrcNid() { return m_srcNid; }
	

	size_t payloadPush( void* ptr, size_t length ) {
		size_t bytesLeft = pushBytesLeft();
		//printf("%s() ptr=%p length=%zu\n", __func__, ptr, length );
		length = length <= bytesLeft ? length : bytesLeft;	
		if ( ptr ) {
            memcpy( &m_payload[m_payloadSize], (const char*) ptr, length );
		}
		m_payloadSize += length;	
		return length;
	}

	void printPayload() {
		for ( int i = 0; i < m_payloadSize; i ++ ) {
			if ( 0 == ( i % 16 ) ) {
				printf("0x%02x: ",i);
			}
			printf( "0x%02x ", m_payload[i] );
			if ( 0 == (i + 1) % 16 ) {
				printf("\n");
			}
		}	
	}
	size_t payloadPop( void* ptr, size_t length ) {
		//printf("%s() ptr=%p length=%zu\n", __func__, ptr, length );
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
		ser & m_head;
		ser & m_proto;
		ser & m_streamId;
		ser & m_streamOffset;
		ser & pktNum;
    }

  private:
	NetworkPkt() : m_payloadSize(0), m_popOffset(0) { m_payload.resize(2048); }
    ImplementSerializable(SST::Aurora::NetworkPkt);

	std::vector<unsigned char> m_payload;
	int m_srcPid;
	int m_srcNid;
	int m_destPid;
	bool m_head;
	unsigned char m_proto;
	StreamId m_streamId;
	uint32_t m_streamOffset;


	int m_payloadSize;
	size_t m_popOffset;

};

}
}

#endif

