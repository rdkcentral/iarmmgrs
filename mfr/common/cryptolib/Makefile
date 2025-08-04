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
CFLAGS      := -g -fPIC -D_REENTRANT -Wall -D_TIME_BITS=64 -D_FILE_OFFSET_BITS=64
INSTALL     := $(IARM_MGRS)/install
LIBNAME     := mfrcrypto
LIBNAMEFULL := lib$(LIBNAME).so
CPPLAGS     := -g -fPIC -D_REENTRANT -Wall -D_TIME_BITS=64 -D_FILE_OFFSET_BITS=64

#Plase make setting NEXUS_APP_INCLUDE_PATHS configuratble

INCLUDE     =	-I.\
                -I../include \
		-I$(MFR_PATH)/include	\
		-I$(IARM_PATH)/core/   \
		-I$(IARM_PATH)/core/include \

ifeq ($(RDK_LOGGER_ENABLED),y)
INCLUDE     += -I$(COMBINED_ROOT)/rdklogger/include
LDFLAGS     += -L$(COMBINED_ROOT)/rdklogger/build/lib -lrdkloggers
CFLAGS      += -DRDK_LOGGER_ENABLED
CPPLAGS     += -DRDK_LOGGER_ENABLED
endif

CFLAGS	    += $(INCLUDE) 

LDFLAGS +=-L$(IARM_PATH)/core/
LDFLAGS+=-L$(DFB_LIB) -L$(OPENSOURCE_BASE)/lib -llog4c 


CFLAGS += $(OPENSSL_CFLAGS)
#LDFLAGS += $(OPENSSL_LDFLAGS)


all:clean library install 
	@echo "Build Finished...."

library: mfrCrypto.o
	@echo "Building $(CRYPTLIB) ..."
	$(CC) $(CFLAGS) $(LDFLAGS) -shared mfrCrypto.o -o $(LIBNAMEFULL) 
%.o: %.c
	@echo "Building $@ ...."
	$(CC) -c $<  $(CFLAGS) -o $@

install: library
	@echo "installling...!"
	@cp -f $(LIBNAMEFULL) $(INSTALL)/lib

uninstall:
	@$(RM) $(INSTALL)/lib/$(LIBNAMEFULL)

clean:
	@echo "Cleaning the directory..."
	@$(RM) *.o $(LIBNAMEFULL) 

