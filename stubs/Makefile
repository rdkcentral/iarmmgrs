##########################################################################
# If not stated otherwise in this file or this component's LICENSE
# file the following copyright and licenses apply:
#
# Copyright 2016 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
##########################################################################

RM          := rm -rf
CFLAGS      := -g -fPIC -D_REENTRANT -Wall -std=c++11
CPPFLAGS    := -g -fPIC -D_REENTRANT -Wall -std=c++11
LIBNAME     := ds
LIBNAMEHAL  := dshal  
LIBNAMEHALSRV := dshalsrv
LIBNAMEFULL := lib$(LIBNAME).so
LIBNAMEFULHAL := lib$(LIBNAMEHAL).so
LIBNAMEFLHALSRV := lib$(LIBNAMEHALSRV).so

# Source files
CPP_OBJS    := $(patsubst %.cpp,%.o,$(wildcard *.cpp))
C_OBJS      := $(patsubst %.c,%.o,$(wildcard *.c))
ALL_OBJS    := $(CPP_OBJS) $(C_OBJS)

INCLUDE     = -I. \
              -I$(IARM_PATH)/core/include \
              -I$(IARM_MGRS)/utils

# Build all stub libraries
all: clean $(LIBNAMEFULL) $(LIBNAMEFULHAL) $(LIBNAMEFLHALSRV)
	@echo "Build Finished...."

# Create libds.so from ds_stubs.cpp and iarm_stubs.cpp  
$(LIBNAMEFULL): ds_stubs.o iarm_stubs.o
	@echo "Creating shared library $@"
	$(CXX) -shared -o $@ $^ $(LDFLAGS)

# Create libdshal.so from dshal_stubs.c
$(LIBNAMEFULHAL): dshal_stubs.o
	@echo "Creating shared library $@"
	$(CXX) -shared -o $@ $^ $(LDFLAGS)

# Create libdshalsrv.so from dshalsrv_stubs.c  
$(LIBNAMEFLHALSRV): dshalsrv_stubs.o powerctrl_stubs.o
	@echo "Creating shared library $@"
	$(CXX) -shared -o $@ $^ $(LDFLAGS)

# Compile C++ files
%.o: %.cpp
	@echo "Building $@ ...."
	$(CXX) -c $(CPPFLAGS) $(INCLUDE) $< -o $@

# Compile C files  
%.o: %.c
	@echo "Building $@ ...."
	$(CXX) -c $(CFLAGS) $(INCLUDE) $< -o $@

clean:
	@echo "Cleaning stubs directory..."
	@$(RM) $(ALL_OBJS) $(LIBNAMEFULL) $(LIBNAMEFULHAL) $(LIBNAMEFLHALSRV)
