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
CFLAGS      := -g -fPIC -D_REENTRANT -Wall
LIBNAME     := mfrsrv
LIBNAMEFULL := lib$(LIBNAME).so
INSTALL     := $(IARM_MGRS)/install
OBJS        := $(patsubst %.cpp,%.o,$(wildcard *.cpp))
OBJS        += $(patsubst %.c,%.o,$(wildcard *.c))
CFLAGS 		+=-DRDK_MFRLIB_NAME='"librdkmfr.so"'


INCLUDE     += -I$(IARM_MGRS)/generic/mfr/common      \
				-I$(IARM_MGRS)/generic/mfr/common/include \
				-I$(IARM_PATH)/core  \
				-I$(IARM_PATH)/core/include \
				-I$(DFB_ROOT)/usr/local/include/directfb

CFLAGS      += $(INCLUDE)


all: clean install
	@echo "Build Finished...."

library: $(OBJS)
	@echo "Building $(LIBNAMEFULL) ...."
	$(CC) $(OBJS) $(CFLAGS) -shared -o $(LIBNAMEFULL) 

%.o: %.c
	@echo "Building $@ ...."
	$(CC) -c $<  $(CFLAGS) $(LDFLAGS) -o $@

%.o: %.cpp
	@echo "Building $@ ...."
	$(CC) -std=c++0x -c $<  $(CFLAGS) -o $@

install: library
	@echo "installling...!"
	@cp -f $(LIBNAMEFULL) $(INSTALL)/lib

uninstall:
	@$(RM) $(INSTALL)/lib/$(LIBNAMEFULL)

clean:
	@echo "Cleaning the directory..."
	@$(RM) $(OBJS) $(LIBNAMEFULL)


