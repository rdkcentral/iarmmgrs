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

SUBDIRS =
DIST_SUBDIRS =

IARM        = $(includedir)/rdk/iarmbus
IARM_MGR    = $(includedir)/rdk/iarmmgrs-hal

INCLUDES = -I=$(IARM) -I=$(IARM_MGR) \
           -I ./include \
           -I=$(includedir)/directfb \
           -I ../sysmgr/include \
           -I=${includedir}/ccec/drivers \
           -I=$(includedir)/rdk/ds \
           -I=$(includedir)/rdk/ds-hal \
           -I=/usr/include/wdmp-c \
           -I=/usr/include \
           -I=$(includedir)/rdk/halif/power-manager

CC = $(CXX)

AM_CFLAGS = $(INCLUDES) -fPIC -D_REENTRANT -Wall -DGP_VERSION_11

bin_PROGRAMS = pwrMgrMain2

pwrMgrMain2_SOURCES = coldFactoryReset.c \
                      customerReset.c \
                      factoryReset.c \
                      personalityReset.c \
                      userFactoryReset.c \
                      pwrMgr.c \
                      pwrMgrMain.c \
                      sleepTimer.c \
                      wareHouseReset.c \
                      wareHouseClear.c \
                      pwrlogger.h \
                      pwrMgrInternal.h \
                      pwrMgrHelper.cpp
pwrMgrMain2_LDADD = -lds -ldshalcli -liarmmgrs-power-hal -lrfcapi

