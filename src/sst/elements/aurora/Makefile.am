# -*- Makefile -*-
#
#

AM_CPPFLAGS = $(MPI_CPPFLAGS) \
	-I$(top_srcdir)/src

compdir = $(pkglibdir)
comp_LTLIBRARIES = libaurora.la

libaurora_la_SOURCES = \
	nic/nic.cc\
	nic/nic.h\
	nic/nicSubComponent.cc\
	nic/nicSubComponent.h\
	rdma/rdmaMpiPt2PtLib.cc\
	rdma/rdmaMpiPt2PtLib.h\
	rdma/rdmaNic.cc\
	rdma/rdmaNic.h\
	rdma/rdmaLib.cc\
	rdma/rdmaLib.h\
	rvma/rvmaMpiPt2PtLib.cc\
	rvma/rvmaMpiPt2PtLib.h\
	rvma/rvmaNic.cc\
	rvma/rvmaNic.h\
	rvma/rvmaLib.cc\
	rvma/rvmaLib.h\
	host/collectives/tree.h\
	host/collectives/tree.cc\
	host/mpiLib.h\
	host/mpiLib.cc\
	host/misc.h\
	host/host.cc\
	host/host.h

libaurora_la_LDFLAGS = -module -avoid-version
