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


#ifndef COMPONENTS_AURORA_HOST_COLLECTIVES_BASE_H
#define COMPONENTS_AURORA_HOST_COLLECTIVES_BASE_H

#include "./base.h"
#include "sst/elements/hermes/mpiPt2Pt.h"

namespace SST {
namespace Aurora {
namespace Collectives { 

class Base {
  public:
    Base( Mpi::Interface& api ) : m_api(api) {}

  protected:
	Mpi::Interface& api() { return m_api; }

  private:
    Mpi::Interface& m_api;
};

}
}
}

#endif

