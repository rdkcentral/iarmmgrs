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
OBJS        := $(patsubst %.c,%.o,rdkProfile.c)
CPPLAGS     := -g -fPIC -D_REENTRANT -Wall -D_TIME_BITS=64 -D_FILE_OFFSET_BITS=64
LDFLAGS     += -shared
LIBRARY_NAME = libiarmUtils.so

INCLUDE     =	-I.\
		-I$(IARM_PATH)/core/include


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


LDFLAGS+=  -L$(CROSS_TOOLCHAIN)/lib

all:clean objs library
	@echo "Build Finished...."

objs: $(OBJS)

%.o: %.c 
	@echo "Building $@ ...."
	$(CXX) -c $<  $(CFLAGS) -o $@

%.o: %.cpp
	@echo "Building $@ ...."
	$(CXX) -c $<  $(CFLAGS) -o $@

library: $(OBJS)
	$(CC) -fPIC $(CFLAGS) -shared -Wl,-soname,$(LIBRARY_NAME).0.0.0 $(OBJS) -o $(LIBRARY_NAME) $(LDFLAGS)

clean:
	@echo "Cleaning the directory..."
	@$(RM) $(OBJS) $(LIBNAMEFULL) $(LIBRARY_NAME)
