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
OBJS        := $(patsubst %.c,%.o,$(wildcard *.c)) dsMgrProductTraitsHandler.o dsMgrHelper.o
CPPLAGS      := -g -fPIC -D_REENTRANT -Wall
EXECUTABLE  =   dsMgrMain
INCLUDE     =   -I. \
                -I$(IARM_PATH)/core/   \
                -I$(IARM_PATH)/core/include \
				-I$(IARM_MGRS)/sysmgr/include \
				-I$(IARM_MGRS)/utils \
                -I$(DS_PATH)/rpc/include \
				-I$(DS_PATH)/hal/include \
		        -I$(DS_PATH)/ds/include \
				-I$(UTILS_PATH)/ \
				-I$(DS_PATH)/hal/src \
				-I$(GLIB_HEADER_PATH)/ \
				-I$(GLIB_HEADER_PATH)/glib/ \
				-I$(GLIB_CONFIG_PATH)/../lib/glib-2.0/include/ \
                -I$(DFB_ROOT)/usr/local/include/directfb

INSTALL     := $(PWD)/install


ifeq ($(RDK_LOGGER_ENABLED),y)
INCLUDE     += -I$(COMBINED_ROOT)/rdklogger/include
LDFLAGS     += -L$(COMBINED_ROOT)/rdklogger/build/lib -lrdkloggers
CFLAGS      += -DRDK_LOGGER_ENABLED
CPPLAGS     += -DRDK_LOGGER_ENABLED
endif

ifeq ($(IP_RDK_DFW),y)
CFLAGS      += -DIP_RDK_DFW
CPPLAGS     += -DIP_RDK_DFW
endif

CFLAGS      += $(INCLUDE)

CFLAGS += $(_INIT_RESN_SETTINGS)

LDFLAGS +=-Wl,-rpath,$(SDK_FSROOT)/usr/local/lib
LDFLAGS += -L$(SDK_FSROOT)/usr/local/lib -lIARMBus
LDFLAGS += -L$(INSTALL)/lib -ldshalcli -lds -L$(UTILS_PATH)/ -liarmUtils 

ifeq ($(PLATFORM_SOC),L2HalMock)
export LDFLAGS += -L$(DS_PATH)/install/lib -L$(IARM_PATH)/install -L$(UTILS_PATH)/ -liarmUtils -ldshalsrv -lds-hal -lds -ldshalcli -lIARMBus -lglib-2.0 -lsafec-3.5
endif

INTEL_SHLIBS=   \
                                -lismd_core \
                                -losal \
	                         -lismd_viddec \
	                         -lismd_vidsink_direct \
                                -lgdl \
                                -lismd_vidpproc \
                                -lismd_vidrend \
                                -lismd_tsout \
                                -lismd_audio
		

all:clean executable
	@echo "Build Finished...."

executable: $(OBJS)
	$(CXX) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $(EXECUTABLE)

%.o: %.c
	@echo "Building $@ ...."
	$(CXX) -c $<  $(CFLAGS) -o $@

%.o: %.cpp
	@echo "Building $@ ...."
	$(CXX) -c $<  $(CFLAGS) -o $@

clean:
	@echo "Cleaning the directory..."
	@$(RM) $(OBJS) $(EXECUTABLE)

