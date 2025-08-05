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
CFLAGS      := -std=c11 -g -fPIC -D_REENTRANT -Wall -D_TIME_BITS=64 -D_FILE_OFFSET_BITS=64
OBJS        := $(patsubst %.c,%.o,$(wildcard *.c))
CPPLAGS     := -g -fPIC -D_REENTRANT -Wall -D_TIME_BITS=64 -D_FILE_OFFSET_BITS=64
EXECUTABLE  := pwrstate_notifier

INCLUDE     =	-I.\
		-I../pwrstate\
		-I../hal/include \
		-I../sysmgr/include \
		-I$(IARM_PATH)/core/   \
		-I$(IARM_PATH)/core/include \
		-I$(DFB_ROOT)/usr/local/include/directfb \
		-I$(IARM_PATH)/soc/${PLATFORM_SOC}/deepsleep \
		-I$(DS_PATH)/hal/include \
		-I$(DS_PATH)/rpc/include \
		-I$(DS_PATH)/ds/include \
		-I$(LOGGER_PATH)/include \
		-I$(GLIB_HEADER_PATH)/ \
		-I$(GLIB_HEADER_PATH)/glib/ \
		-I$(GLIB_CONFIG_PATH)/../lib/glib-2.0/include/ \


ifeq ($(RDK_LOGGER_ENABLED),y)
INCLUDE     += -I$(COMBINED_ROOT)/rdklogger/include
LDFLAGS     += -L$(COMBINED_ROOT)/rdklogger/build/lib -lrdkloggers
CFLAGS      += -DRDK_LOGGER_ENABLED
CPPLAGS     += -DRDK_LOGGER_ENABLED
endif

CFLAGS      += $(INCLUDE)
LDFLAGS +=-Wl,-rpath,$(SDK_FSROOT)/usr/local/lib
LDFLAGS += -L$(SDK_FSROOT)/usr/local/lib -lIARMBus -lnexus

LDFLAGS += -L$(DS_PATH)/install/lib -lds -ldshalcli
LDFLAGS += -lIARMBus -L$(IARM_MGRS)/install/lib -lpwrstate


all:clean objs executable
	@echo "Build Finished...."

objs: $(OBJS)

%.o: %.c
	@echo "Building $@ ...."
	$(CXX) -c $<  $(CFLAGS) -o $@

executable: $(OBJS)
	$(CXX) $(CFLAGS) $(OBJS) -o $(EXECUTABLE) $(LDFLAGS)

clean:
	@echo "Cleaning the directory..."
	@$(RM) $(OBJS) $(LIBNAMEFULL) $(EXECUTABLE)
