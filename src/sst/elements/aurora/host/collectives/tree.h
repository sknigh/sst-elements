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


#ifndef COMPONENTS_AURORA_HOST_COLLECTIVES_TREE_H
#define COMPONENTS_AURORA_HOST_COLLECTIVES_TREE_H

#include <vector>

namespace SST {
namespace Aurora {
namespace Collectives { 

class Tree {
  public:
    Tree( int my_pe, int num_pes, int requested_crossover, int requested_radix );

    void build_kary_tree(int radix, int PE_start, int stride, int PE_size, int PE_root, int *parent,
                                                                      int *num_children, int *children);
    int numPes() { return m_num_pes; }
    int myPe() { return m_my_pe; }
    int parent() { return m_parent; }
    int numChildren() { return m_num_children; }
    int radix() { return m_radix; }
    int children(int index) { return m_children[index]; }
    int circular_iter_next(int curr, int PE_start, int logPE_stride, int PE_size);

  private:
    bool m_debug;
    int m_my_pe;
    int m_num_pes;
    int m_parent;
    int m_radix;
    int m_crossover;
    int m_num_children;
    std::vector<int> m_children;
};

}
}
}

#endif

