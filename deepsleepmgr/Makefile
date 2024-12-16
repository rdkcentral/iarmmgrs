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
CFLAGS      := -g -fPIC -D_REENTRANT -Wall
OBJS        := $(patsubst %.c,%.o,$(wildcard *.c))
CPPLAGS      := -g -fPIC -D_REENTRANT -Wall
EXECUTABLE  := deepSleepMgrMain

INCLUDE     =	-I.\
		-I../deepsleepmgr\
		-I../ir\
		-I../ir/include \
		-I../hal/include \
		-I../sysmgr/include \
		-I$(IARM_PATH)/core/   \
		-I$(IARM_PATH)/core/include \
		-I$(DFB_ROOT)/usr/local/include/directfb \
		-I$(DS_PATH)/hal/include \
		-I$(DS_PATH)/rpc/include \
		-I$(DS_PATH)/ds/include \
		-I$(LOGGER_PATH)/include \
		-I$(GLIB_HEADER_PATH)/ \
		-I$(GLIB_HEADER_PATH)/glib/ \
		-I$(GLIB_CONFIG_PATH)/../lib/glib-2.0/include/ \
		-I$(RF4CE_PATH)/include 


CFLAGS      += $(INCLUDE) 

CFLAGS += $(_ENABLE_WAKEUP_KEY)
CFLAGS += $(USE_GREEN_PEAK_RF4CE_INTERFACE)
CFLAGS += $(USE_RF4CE_INTERFACE)


LDFLAGS +=-Wl,-rpath,$(SDK_FSROOT)/usr/local/lib


LDFLAGS += -L$(DS_PATH)/install/lib -lds -ldshalcli
LDFLAGS += -lIARMBus -L$(IARM_MGRS)/install/lib -lDeepSleepMgr



all:clean objs executable
	@echo "Build Finished...."

objs: $(OBJS)

%.o: %.c
	@echo "Building $@ ...."
	$(CXX) -c $<  $(CFLAGS) -o $@

executable: $(OBJS)
	$(CXX) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $(EXECUTABLE)

clean:
	@echo "Cleaning the directory..."
	@$(RM) $(OBJS) $(LIBNAMEFULL) $(EXECUTABLE)
