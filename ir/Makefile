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
UINPUT      := uinput 
OBJS        := $(patsubst %.c,%.o,$(filter-out uinput.c,$(wildcard *.c)))
OBJS_uinput := $(patsubst %.c,%.o,uinput.c)
CPPLAGS      := -g -fPIC -D_REENTRANT -Wall
UILIB        = libuinput

INCLUDE     =	-I.\
		-I$(IARM_PATH)/core/include	\
		-I../ir	\
		-I../ir/include \
		-I../sysmgr/include \
		-I../hal/include \
		-I$(DFB_ROOT)/usr/local/include/directfb \
		-I$(RF4CE_PATH)/include 
		
ifeq ($(RDK_LOGGER_ENABLED),y)
INCLUDE     += -I$(COMBINED_ROOT)/rdklogger/include
LDFLAGS     += -L$(COMBINED_ROOT)/rdklogger/build/lib -lrdkloggers
CFLAGS      += -DRDK_LOGGER_ENABLED
CPPLAGS     += -DRDK_LOGGER_ENABLED
endif


CFLAGS      += $(INCLUDE) 
CFLAGS += $(RF4CE_API)

LDFLAGS     +=-Wl,-rpath,$(SDK_FSROOT)/usr/local/lib



LDFLAGS     += -L$(SDK_FSROOT)/usr/local/lib -lIARMBus
CFLAGS      += $(INCLUDE) 
CFLAGS += -I$(SDK_FSROOT)/usr/local/include/nspr
CFLAGS += -I$(SDK_FSROOT)/usr/include/nspr
MAF_LIBS=-lnspr4 -losal -lrt -lMAF_StackAdapter -lMAF_InputHandler -lMAF_ConfigMgmt -lMAF_DbusAdapter -lfusiondale
LDFLAGS += -L$(SDK_FSROOT)/usr/lib $(MAF_LIBS)

LDFLAGS +=-L$(IARM_MGRS)/install/lib -lirInput


all:clean objs executable
	@echo "Build Finished...."

objs: $(OBJS)

%.o: %.c
	@echo "Building $@ ...."
	$(CXX) -c $<  $(CFLAGS) -o $@

executable: $(OBJS) $(UILIB).so
	$(CXX) $(CFLAGS) $(OBJS) -o irMgrMain -L./  -l$(UINPUT) $(LDFLAGS)

$(UILIB).so: $(OBJS_uinput)
	$(CC) -fPIC $(CFLAGS) -shared -Wl,-soname,$@.0.0.0 $(OBJS_uinput) -o $@ $(LDFLAGS)

clean:
	@echo "Cleaning the directory..."
	@$(RM) $(OBJS) 
