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
OBJS        := $(patsubst %.c,%.o,$(wildcard *.c)) pwrMgrHelper.o
CPPLAGS      := -g -fPIC -D_REENTRANT -Wall
EXECUTABLE  := pwrMgrMain

INCLUDE     =	-I.\
		-I../power\
		-I../power/include \
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
		-I$(RF4CE_PATH)/include \
		-I$(UTILS_PATH)/


ifeq ($(RDK_LOGGER_ENABLED),y)
INCLUDE     += -I$(COMBINED_ROOT)/rdklogger/include
LDFLAGS     += -L$(COMBINED_ROOT)/rdklogger/build/lib -lrdkloggers
CFLAGS      += -DRDK_LOGGER_ENABLED
CPPLAGS     += -DRDK_LOGGER_ENABLED
endif

CFLAGS      += $(INCLUDE) 

CFLAGS += $(_ENABLE_WAKEUP_KEY)
CFLAGS += $(RF4CE_API)


LDFLAGS+=-Wl,-rpath,$(SDK_FSROOT)/usr/local/lib


LDFLAGS+=-L$(SDK_FSROOT)/lib -L$(SDK_FSROOT)/usr/lib -L$(SDK_FSROOT)/usr/local/lib/  -L$(CROSS_TOOLCHAIN)/lib
ISMD_LIBS=-L$(TOOLCHAIN_DIR)/lib -lismd_audio
GDL_LIBS=-L$(TOOLCHAIN_DIR)/lib -lgdl
ifneq ($(STANDALONE_BUILD_ENABLED),y)
ifneq ($(PLATFORM_SOC),L2HalMock)
LDFLAGS += -lIARMBus -lrt -L$(DS_PATH)/install/lib -lnspr4 -losal -lrt -lMAF_StackAdapter -lMAF_InputHandler -lMAF_ConfigMgmt -lMAF_DbusAdapter -lpace_spi_drv -lparker_fpd_library -ldshalcli -lds
LDFLAGS += $(ISMD_LIBS)
LDFLAGS += $(GDL_LIBS)
endif
endif

LDFLAGS += -L$(DS_PATH)/install/lib -lds -ldshalcli -L$(UTILS_PATH)/ -liarmUtils
LDFLAGS += -L$(IARM_MGRS)/install/lib -lPwrMgr

ifeq ($(PLATFORM_SOC),L2HalMock)
LDFLAGS+=-L$(DS_PATH)/install/lib -L$(IARM_PATH)/install -lIARMBus -lglib-2.0 -lsafec-3.5
endif

all:clean objs executable
	@echo "Build Finished...."

objs: $(OBJS)

%.o: %.c 
	@echo "Building $@ ...."
	$(CXX) -c $<  $(CFLAGS) -o $@

%.o: %.cpp
	@echo "Building $@ ...."
	$(CXX) -c $<  $(CFLAGS) -o $@

executable: $(OBJS)
	$(CXX) $(CFLAGS) $(OBJS) -o $(EXECUTABLE) $(LDFLAGS)

clean:
	@echo "Cleaning the directory..."
	@$(RM) $(OBJS) $(LIBNAMEFULL) $(EXECUTABLE)
