####################################################################
#
#  Name:         Makefile
#  Created by:   Stefan Ritt
#
#  Contents:     Makefile for the v1720 frontend
#
#  $Id: Makefile 3655 2007-03-21 20:51:28Z amaudruz $
#
#####################################################################
#

# Path to gcc 4.8.1 binaries (needed to use new C++ stuff)
# PATH := /home/ucn/packages/newgcc/bin:$(PATH)

#--------------------------------------------------------------------
# The MIDASSYS should be defined prior the use of this Makefile
ifndef MIDASSYS
missmidas::
	@echo "...";
	@echo "Missing definition of environment variable 'MIDASSYS' !";
	@echo "...";
endif

#--------------------------------------------------------------------
# The following lines contain specific switches for different UNIX
# systems. Find the one which matches your OS and outcomment the 
# lines below.
#
# get OS type from shell
OSTYPE = $(shell uname)

#-----------------------------------------
# This is for Linux
ifeq ($(OSTYPE),Linux)
OSTYPE = linux
endif

ifeq ($(OSTYPE),linux)
#OS_DIR = linux-m64
OS_DIR = linux
OSFLAGS = -DOS_LINUX -DLINUX
CFLAGS = -g -Wall #-fno-omit-frame-pointer 
#CFLAGS = -Wall -O2 -g -Wall -DOS_LINUX -Dextname
#CFLAGS += -std=c++11 -Wall -O2 -g -I. -I$(INC_DIR) -I$(MIDASSYS)/../mxml/ -I$(DRV_DIR)
#CFLAGS += $(PGFLAGS)
LDFLAGS = -g -lm -lz -lutil -lnsl -lpthread -lrt -lc 
endif

#-----------------------------------------
# optimize?

# CFLAGS += -O2

#-----------------------------------------
# ROOT flags and libs
#
ifdef ROOTSYS
ROOTCFLAGS := $(shell  $(ROOTSYS)/bin/root-config --cflags)
ROOTCFLAGS += -DHAVE_ROOT -DUSE_ROOT
ROOTLIBS   := $(shell  $(ROOTSYS)/bin/root-config --libs) -Wl,-rpath,$(ROOTSYS)/lib
ROOTLIBS   += -lThread
else
missroot:
	@echo "...";
	@echo "Missing definition of environment variable 'ROOTSYS' !";
	@echo "...";
endif

# Google perf tools
PERFLIBS=
ifdef GPROFILE
CXXFLAGS += -I/usr/local/include/google/ # -I/home/lindner/tools/gperftools-2.1/src
PERFLIBS += -lprofiler # /home/lindner/tools/gperftools-2.1/libprofiler.la
endif


#-------------------------------------------------------------------
# The following lines define directories. Adjust if necessary
#
CONET2_DIR   = $(HOME)/packages/Caen
CAENCOMM_DIR = $(CONET2_DIR)/CAENComm-1.02
CAENCOMM_LIB = $(CAENCOMM_DIR)/lib
CAENDGTZ_DIR = $(CONET2_DIR)/CAENDigitizer_2.3.2
CAENDGTZ_LIB = $(CAENCOMM_DIR)/lib
CAENVME_DIR  = $(CONET2_DIR)/CAENVMELib-2.30.2
CAENVME_LIB  = $(CAENVME_DIR)/lib
MIDAS_INC    = $(MIDASSYS)/include
#MIDAS_LIB    = $(MIDASSYS)/$(OS_DIR)/lib
MIDAS_LIB    = $(MIDASSYS)/lib
MIDAS_SRC    = $(MIDASSYS)/src
MIDAS_DRV    = $(MIDASSYS)/drivers/vme
ROOTANA      = $(HOME)/packages/rootana
LIB = $(MIDAS_LIB)/libmidas.a -lrt

####################################################################
# Lines below here should not be edited
####################################################################
#
# compiler
CC   = gcc #-std=c99
#CXX  = g++ -g -std=c++11
CXX  = g++ -g -std=c++0x
#
# MIDAS library
LIBMIDAS=-L$(MIDAS_LIB) -lmidas
#
# CAENComm
LIBCAENCOMM=-L$(CAENCOMM_LIB) -lCAENComm
#
# CAENVME
LIBCAENVME=-L$(CAENVME_LIB) -lCAENVME
#
# CAENDigitizer
LIBCAENDGTZ =-L$(CAENDGTZ_LIB) -lCAENDigitizer
#
# All includes
MIDASINCS = -I. -I$(MIDAS_INC)
#MIDASINCS = -I. -I./include -I$(MIDAS_INC) -I$(MIDAS_DRV)
#CAENINCS = -I$(CAENVME_DIR)/include -I$(CAENCOMM_DIR)/include 
CAENINCS = -I$(CAENCOMM_DIR)/include 

####################################################################
# General commands
####################################################################

all: feTCP.exe feLabview.exe LabViewDriver.exe
	@echo "***** Finished"
	@echo "***** Use 'make doc' to build documentation"

fe :LabViewDriver.exe

doc ::
	doxygen
	@echo "***** Use firefox --no-remote doc/html/index.html to view if outside gateway"

####################################################################
# Libraries/shared stuff
####################################################################

#v6533.o : $(MIDAS_DRV)/v1720.c
#v6533.o : v6533.c
#	$(CC) -c $(CFLAGS) $(MIDASINCS) $(CAENINCS) $< -o $@ 

#v6533main.o : v6533.c
#	$(CC) -DMAIN_ENABLE -c $(CFLAGS) $(MIDASINCS) $(CAENINCS) $< -o $@ 

feTCP.o: feTCP.cxx
	$(CXX) $(CFLAGS) $(OSFLAGS) $(MIDASINCS) $(CAENINCS) -c $< -o $@

feLabview.o: feLabview.cxx
	$(CXX) $(CFLAGS) $(OSFLAGS) $(MIDASINCS) $(CAENINCS) -c $< -o $@

KOtcp.o: KOtcp.cxx
	$(CXX) $(CFLAGS) $(OSFLAGS) $(MIDASINCS) $(CAENINCS) -c $< -o $@

####################################################################
# Single-thread frontend
####################################################################
feTCP.exe: $(LIB) feTCP.o $(MIDAS_LIB)/mvodb.o $(MIDAS_LIB)/tmfe.o KOtcp.o
	$(CXX) -o $@ $(CFLAGS) $^ $(LIB) $(LDFLAGS) $(LIBS)

feLabview.exe: $(LIB) feLabview.o $(MIDAS_LIB)/mvodb.o $(MIDAS_LIB)/tmfe.o KOtcp.o
	$(CXX) -o $@ $(CFLAGS) $^ $(LIB) $(LDFLAGS) $(LIBS)

LabViewDriver.exe: $(LIB) $(MIDAS_LIB)/mfe.o LabViewDriver.o $(MIDAS_LIB)/mvodb.o $(MIDAS_LIB)/tmfe.o KOtcp.o
	$(CXX) -o $@ $(CFLAGS) $^ $(LIB) $(LDFLAGS) $(LIBS)

#scV6533.exe: $(MIDAS_LIB)/mfe.o  scV6533.o
#	$(CXX) $(OSFLAGS) scV6533.o $(MIDAS_LIB)/mfe.o -o $@ $(LIBMIDAS) \
#	$(LIBCAENVME) $(LIBCAENCOMM)  $(LDFLAGS) $(PERFLIBS)

LabViewDriver.o : LabViewDriver.cxx
	$(CXX) $(CFLAGS) $(OSFLAGS) $(MIDASINCS) $(CAENINCS) -c $< -o $@

#v6533main.exe : v6533main.o 
#	$(CC) $(CFLAGS) $(OSFLAGS) $(HWFLAGS) $(MIDASINCS) $(CAENINCS) $< -o $@ \
#	$(LIBMIDAS) $(LIBCAENCOMM) $(LIBCAENVME) -o $@ $(LDFLAGS) $(PERFLIBS)

#v6533CONET2.o : v6533CONET2.cxx
#	$(CXX) $(CFLAGS) $(OSFLAGS) $(HWFLAGS) $(MIDASINCS) $(CAENINCS) -Ife -c $< -o $@

$(MIDAS_LIB)/mfe.o:
	@cd $(MIDASSYS) && make

# %.o: %.cxx
#	$(CXX) -o $@ $(CFLAGS) -c $<

%.o: %.cxx
	$(CXX) -o $@ $(CFLAGS) -c $<
####################################################################
# Clean
####################################################################

clean:
	rm -f *.o *.exe
	rm -f *~
	rm -rf html
	rm -rf stress

#end file
