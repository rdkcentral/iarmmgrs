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
LIBNAME     := MAF
LIBNAMEFULL := lib$(LIBNAME).so
CPPLAGS     := -g -fPIC -D_REENTRANT -Wall
executable  := mfrMgrMain
CFLAGS +=-DRDK_MFRLIB_NAME='"librdkmfr.so"'

LDFLAGS += -L$(COMBINED_ROOT)/iarmmgrs/install/lib -lmfrsrv 


# FOr logger include
ifeq ($(RDK_LOGGER_ENABLED),y)
INCLUDE     += -I$(COMBINED_ROOT)/rdklogger/include
LDFLAGS     += -L$(COMBINED_ROOT)/rdklogger/build/lib -lrdkloggers
CFLAGS      += -DRDK_LOGGER_ENABLED
CPPLAGS     += -DRDK_LOGGER_ENABLED
endif


#Common Include
INCLUDE     =	-I.\
                -I./include \
		-I$(MFR_PATH)/common/include	\
		-I$(IARM_PATH)/core/   \
		-I$(IARM_PATH)/core/include \
		-I$(DFB_ROOT)/usr/local/include/directfb \

IARM_NEXUS_INCLUDE     =-I$(LINUXKDIR)/include \
			$(addprefix -I,$(NEXUS_APP_INCLUDE_PATHS)) $(addprefix -D,$(NEXUS_APP_DEFINES))

NEXUS_BUILD_CFLAGS+=-DBSTD_CPU_ENDIAN=${NEXUS_ENDIAN}


CFLAGS      += $(INCLUDE) 
CFLAGS      += ${NEXUS_BUILD_CFLAGS}
CFLAGS      += ${IARM_NEXUS_INCLUDE}
CFLAGS       += -I$(BUILDS_DIR)/work${COMCAST_PLATFORM}/Refsw/nexus/nxclient/include/ 
CFLAGS       += -I$(NEXUS_BIN_DIR)/include


#Common LD Flags
LDFLAGS += -lssl
LDFLAGS +=-Wl,-rpath,$(SDK_FSROOT)/usr/local/lib
LDFLAGS += -L$(SDK_FSROOT)/usr/local/lib -lIARMBus
LDFLAGS += -L$(OPENSSL_PATH)/lib -lcrypto -lrt 
CFLAGS	+= $(OPENSSL_CFLAGS)

all:clean cryptlib client srv executable test
	@echo "Build Finished...."

cryptlib: 
	@echo "Building $(CRYPTLIB) ..."
	make -C cryptolib	

client:
	@echo "Building client ..."
	make -C rpc/cli
srv:
	@echo "Building server ..."
	make -C rpc/srv

executable: mfrMgrMain.o mfrMgr.o
	@echo "Building $(executable) ...."
	$(CC) $(CFLAGS) $(LDFLAGS)  mfrMgr.o mfrMgrMain.o -o $(executable) 

test:
	@echo "Building test_mfr_client ..."
	make -C test_mfr_client/

%.o: %.c
	@echo "Building $@ ...."
	$(CC) -c $<  $(CFLAGS) -o $@

clean:
	@echo "Cleaning the directory..."
	@$(RM) *.o $(executable) 
	make -C rpc/cli clean
	make -C rpc/srv clean
	make -C cryptolib clean

