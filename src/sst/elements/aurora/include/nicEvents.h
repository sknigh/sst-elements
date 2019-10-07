// Copyright 2013-2018 NTESS. Under the terms
// of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.
//
// Copyright (c) 2013-2018, NTESS
// All rights reserved.
//
// Portions are copyright of other developers:
// See the file CONTRIBUTORS.TXT in the top level directory
// the distribution for more information.
//
// This file is part of the SST software package. For license
// information, see the LICENSE file in the top level directory of the
// distribution.

#ifndef COMPONENTS_AURORA_NICEVENTS_H
#define COMPONENTS_AURORA_NICEVENTS_H

namespace SST {
namespace Aurora {

class NicInitEvent : public SST::Event {

  public:
    int nodeNum;
    int numCores;
    int coreId;

    NicInitEvent() : Event() {}

    NicInitEvent( int nodeNum, int coreId, int numCores ) :
        Event(), nodeNum( nodeNum ), coreId( coreId ), numCores( numCores ) {}

    void serialize_order(SST::Core::Serialization::serializer &ser)  override {
        Event::serialize_order(ser);
        ser & nodeNum;
        ser & coreId;
        ser & numCores;
    }

    ImplementSerializable(SST::Aurora::NicInitEvent);
};

class NicEvent : public SST::Event {
  public:
	Event* payload;
	enum Type { Credit, Payload } type;

    NicEvent() : Event() {}
	NicEvent( Event* payload, Type type ) : Event(), type(type), payload(payload) {}

    void serialize_order(SST::Core::Serialization::serializer &ser)  override {
        Event::serialize_order(ser);
		ser & payload;
		ser & type;
	}

    ImplementSerializable(SST::Aurora::NicEvent);
};

}
}

#endif
