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

INCLUDE     :=	-I.\
		-I./include	\
		-I$(IARM_PATH)/core/                            \
		-I$(IARM_PATH)/core/include                      \
		-I$(IARM_PATH)/template/include                 \
		-I$(IARM_MGRS)/generic/ir/include                      \
		-I$(IARM_MGRS)/generic/hal/include                      \
		-I$(IARM_MGRS)/generic/power/include                      \
		-I$(IARM_MGRS)/generic/mfr/include                      \
		-I$(IARM_MGRS)/generic/sysmgr/include                      \
		-I$(IARM_MGRS)/generic/tr69Bus/include                      \
		-I$(DFB_ROOT)/usr/local/include/directfb

CFLAGS      += $(INCLUDE)

LDFLAGS+=-Wl,-rpath,$(SDK_FSROOT)/usr/local/lib
LDFLAGS+=-L$(SDK_FSROOT)/usr/local/lib -lIARMBus

LDFLAGS+=-L$(DFB_LIB) -L$(OPENSOURCE_BASE)/lib
LDFLAGS+=-L$(GLIB_LIBRARY_PATH)/
LDFLAGS+=$(GLIBS)


.PHONY: $(BINARIES)

BINARIES    := $(patsubst %.c,%,$(wildcard *.c))

all: clean $(BINARIES)
	@echo "Build Finished...."

$(BINARIES):
	@echo "Building $@ ...."
	$(CXX) $(CFLAGS) $@.c -o $@ $(LDFLAGS)

clean:
	@echo "Cleaning the directory..."
	@$(RM) $(BINARIES)
