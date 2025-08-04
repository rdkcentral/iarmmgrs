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
#

RM          := rm -rf
CFLAGS      := -g -fPIC -D_REENTRANT -Wall -D_TIME_BITS=64 -D_FILE_OFFSET_BITS=64
OBJS        := $(patsubst %.c,%.o,$(wildcard *.c))
OBJS        += $(patsubst %.cpp,%.o,$(wildcard *.cpp))
CPPLAGS     := -g -fPIC -D_REENTRANT -Wall -D_TIME_BITS=64 -D_FILE_OFFSET_BITS=64
EXECUTABLE  := deviceUpdateMgrMain

INCLUDE     =	-I. \
				-I./include\
		-I../power/include \
		-I$(IARM_PATH)/core/   \
		-I$(IARM_PATH)/core/include \
		-I$(DFB_ROOT)/usr/local/include/directfb \
		-I$(OPENSOURCE_BASE)/include \
		-I$(CURL_INCLUDE_PATH) \
		-I$(OPENSOURCE_BASE)/include/libxml2 \


LDFLAGS     +=-Wl,-rpath,$(SDK_FSROOT)/usr/local/lib


ifeq ($(RDK_LOGGER_ENABLED),y)
INCLUDE     += -I$(COMBINED_ROOT)/rdklogger/include
LDFLAGS     += -L$(COMBINED_ROOT)/rdklogger/build/lib -lrdkloggers
CFLAGS      += -DRDK_LOGGER_ENABLED
CPPLAGS     += -DRDK_LOGGER_ENABLED
endif

CFLAGS      += $(INCLUDE)
CPPFLAGS    += $(INCLUDE)
LDFLAGS     += -L$(SDK_FSROOT)/usr/local/lib -lIARMBus

LDFLAGS     +=-L$(OPENSOURCE_BASE)/lib -lyajl
LDFLAGS     += -L$(CURL_LIBRARY_PATH)
LDFLAGS     += -lcurl -lssl -lcrypto


all:clean executable
	@echo "Build Finished...."

executable: $(OBJS)
	@echo "Creating Final Executable ..$(EXECUTABLE) "
	$(CXX) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $(EXECUTABLE)

%.o: %.c
	@echo "Building $@ ...."
	$(CXX) -c $<  $(CFLAGS) -o $@
clean:
	@echo "Cleaning the directory..."
	@$(RM) $(OBJS) $(LIBNAMEFULL) $(EXECUTABLE)
