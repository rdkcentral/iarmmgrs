##########################################################################
# If not stated otherwise in this file or this component's LICENSE
# file the following copyright and licenses apply:
#
# Copyright 2018 RDK Management
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


SUBDIRS =
DIST_SUBDIRS =

IARM        = $(includedir)/rdk/iarmbus
IARM_MGR    = $(includedir)/rdk/iarmmgrs-hal

INCLUDES = -I=$(IARM) -I=$(IARM_MGR) \
           -I ./include \
           -I=$(includedir)/directfb \
           -I ../power/include \
           -I ../sysmgr/include \
           -I=$(includedir)/rdk/ds \
           -I=$(includedir)/rdk/ds-hal

CC = $(CXX)

AM_CFLAGS = $(INCLUDES) -fPIC -D_REENTRANT -Wall -DGP_VERSION_11 -D_TIME_BITS=64 -D_FILE_OFFSET_BITS=64

bin_PROGRAMS = pwrstate_notifier

pwrstate_notifier_SOURCES = pwrstate_notifier.c
pwrstate_notifier_LDADD = -liarmmgrs-power-hal

