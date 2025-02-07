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
LIBNAME     := MAF
LIBNAMEFULL := lib$(LIBNAME).so
CPPLAGS     := -g -fPIC -D_REENTRANT -Wall
EXECUTABLE  := mfrMgrMain

#Plase make setting LINUXKDIR configuratble


INCLUDE     =	-I.\
                -I./include \
		-I$(MFR_PATH)/include	\
		-I$(IARM_PATH)/core/   \
		-I$(IARM_PATH)/core/include \
		-I$(DFB_ROOT)/usr/local/include/directfb \
		 -I$(UTILS_PATH)/ 



ifeq ($(RDK_LOGGER_ENABLED),y)
INCLUDE     += -I$(COMBINED_ROOT)/rdklogger/include
LDFLAGS     += -L$(COMBINED_ROOT)/rdklogger/build/lib -lrdkloggers
CFLAGS      += -DRDK_LOGGER_ENABLED
CPPLAGS     += -DRDK_LOGGER_ENABLED
endif

CFLAGS      += $(INCLUDE) 


CROSS_COMPILE=mipsel-linux-
CC=$(CROSS_COMPILE)gcc
CXX=$(CROSS_COMPILE)g++

LDFLAGS +=-Wl,-rpath,$(SDK_FSROOT)/usr/local/lib
LDFLAGS += -L$(SDK_FSROOT)/usr/local/lib -lIARMBus -lnexus

LDFLAGS += -L${WORK_DIR}/${MFR_LIB_PATH} $(MFR_LIB_NAME) -L$(UTILS_PATH)/ -liarmUtils 
LDFLAGS += -L$(OPENSSL_PATH)/lib -lcrypto -lrt 


all:clean executable
	@echo "Build Finished...."

library: mfrMgr.o 
	@echo "Building $(LIBNAMEFULL) ...."

executable: mfrMgrMain.o mfrMgr.o
	@echo "Building $(EXECUTABLE) ...."
	$(CC) $(CFLAGS) $(LDFLAGS) $(MFR_LDFLAGS) mfrMgr.o mfrMgrMain.o -o $(EXECUTABLE)

%.o: %.c
	@echo "Building $@ ...."
	$(CC) -c $<  $(CFLAGS) -o $@

clean:
	@echo "Cleaning the directory..."
	@$(RM) *.o $(LIBNAMEFULL) 

