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
CFLAGS      := -std=c11 -g -fPIC -D_REENTRANT -Wall -D_TIME_BITS=64 -D_FILE_OFFSET_BITS=64
OBJS        := $(patsubst %.c,%.o,$(wildcard *.c))
CPPLAGS     := -g -fPIC -D_REENTRANT -Wall -D_TIME_BITS=64 -D_FILE_OFFSET_BITS=64
EXECUTABLE  := sysMgrMain

INCLUDE     =	-I. \
				-I./include\
		-I../power/include \
		-I../mfr/include \
		-I$(IARM_PATH)/core/   \
		-I$(IARM_PATH)/core/include \
		-I$(DFB_ROOT)/usr/local/include/directfb \

ifeq ($(RDK_LOGGER_ENABLED),y)
INCLUDE     += -I$(COMBINED_ROOT)/rdklogger/include
LDFLAGS     += -L$(COMBINED_ROOT)/rdklogger/build/lib -lrdkloggers
CFLAGS      += -DRDK_LOGGER_ENABLED
CPPLAGS     += -DRDK_LOGGER_ENABLED
endif

ifeq ($(USE_MFR_FOR_SERIAL),y)
CFLAGS      += -DUSE_MFR_FOR_SERIAL
endif

CFLAGS      += $(INCLUDE)
LDFLAGS     +=-Wl,-rpath,$(SDK_FSROOT)/usr/local/lib
LDFLAGS     += -L$(SDK_FSROOT)/usr/local/lib -lIARMBus

ifeq ($(PLATFORM_SOC),L2HalMock)
LDFLAGS+=-L$(IARM_PATH)/install -lIARMBus -lglib-2.0 -lsafec-3.5
endif

all:clean executable
	@echo "Build Finished...."

executable: $(OBJS)
	@echo "Creating Final Executable ..$(EXECUTABLE) "
	$(CXX) $(CFLAGS) $(OBJS) -o $(EXECUTABLE) $(LDFLAGS)

%.o: %.c
	@echo "Building $@ ...."
	$(CXX) -c $<  $(CFLAGS) -o $@
clean:
	@echo "Cleaning the directory..."
	@$(RM) $(OBJS) $(LIBNAMEFULL) $(EXECUTABLE)
