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


static void initBuf(void* ptr, size_t length ) {
	unsigned char* buf = (unsigned char*) ptr;
	for ( int i = 0; i < length; i++ ) {
		buf[i]=i;
	}
}

static void checkBuf( void* ptr, size_t length ) {
	unsigned char* buf = (unsigned char*) ptr;
	for ( int i = 0; i < length; i++ ) {
		if ( buf[i] != i % 256 ) {
			printf("buf[%d] = %d\n",i , buf[i]);
		}
	}
}
