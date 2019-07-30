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


class FifoBuf {
  public:
    FifoBuf() : m_offset(0) {}
    void push( void* ptr, size_t length ) {
        void* buf = m_addr.getBacking( m_offset );
#if 0
        printf("%s() addr=0x%" PRIx64 " backing=%p offset=%zu length=%zu\n",
                __func__, m_addr.getSimVAddr(), buf,m_offset, length );
#endif
        assert( buf );
        if ( ptr ) {
            memcpy( buf, ptr, length );
        }
        m_offset += length;
    }

    void pop( void* ptr, size_t length ) {
        void* buf = m_addr.getBacking( m_offset );
#if 0
        printf("%s() addr=0x%" PRIx64 " backing=%p offset=%zu length=%zu\n",
                __func__, m_addr.getSimVAddr(), buf,m_offset, length );
#endif
        assert( buf );
        if ( ptr ) {
            memcpy( ptr, buf, length );
        }
        m_offset += length;
    }
    size_t length() { return m_offset; }
    Hermes::MemAddr& addr() { return m_addr; }

  private:
    size_t m_offset;
    Hermes::MemAddr m_addr;
};
