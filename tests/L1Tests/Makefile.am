##########################################################################
# If not stated otherwise in this file or this component's LICENSE
# file the following copyright and licenses apply:
#
# Copyright 2025 RDK Management
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

# Include paths:
#   - tests/L1Tests/stubs          : empty stub headers created at CI time (touch).
#                                    libIBus.h, libIARM.h, libIBusDaemon.h,
#                                    iarmUtil.h and libIARMCore.h are generated
#                                    empty so #include directives resolve without
#                                    error; real declarations come from Iarm.h
#                                    (testframework) or from mfr/include/ below.
#   - mfr/include                  : full HAL mfrTypes.h (sets _MFR_TYPES_H guard
#                                    before testframework's lighter copy is seen);
#                                    also provides mfrMgr.h, mfr_wifi_types.h, etc.
#   - mfr                          : mfrMgrInternal.h
#   - TESTFRAMEWORK mocks          : Iarm.h, Wraps.h, WrapsMock.h, IarmBusMock.h, …
#   - utils                        : rdkProfile.h / rdkProfile.c
#   - sysmgr                       : sysMgr.c (included directly) + sysMgrInternal.h
#   - sysmgr/include               : sysMgr.h (public API types)
#   - stubs                        : top-level stubs (safec_lib.h, secure_wrapper.h, …)
#
# TESTFRAMEWORK_DIR is set by ./configure (AC_ARG_VAR) and points to the
# entservices-testframework checkout.

AM_CPPFLAGS = -I$(top_srcdir)/tests/L1Tests/stubs \
              -I$(top_srcdir)/mfr/include \
              -I$(top_srcdir)/mfr \
              -I$(TESTFRAMEWORK_DIR)/Tests/mocks \
              -I$(top_srcdir)/utils \
              -I$(top_srcdir)/sysmgr \
              -I$(top_srcdir)/sysmgr/include \
              -I$(top_srcdir)/stubs

AM_CXXFLAGS = -Wall -std=c++14 -g

# Linker flags:
#   --wrap intercepts libc calls so the mock can control them.
#   fopen / fclose / fgets      : testframework's Wraps.cpp provides the shims
#   setmntent                   : needed because WrapsMock.h ctor refs __real_setmntent
#   mfr_init                    : mfr test controls MFRLib_Start behaviour
#   mfrGetSerializedData        : mfr test controls getSerializedData_ behaviour
AM_LDFLAGS = -Wl,--wrap,fopen \
             -Wl,--wrap,fclose \
             -Wl,--wrap,fgets \
             -Wl,--wrap,setmntent \
             -Wl,--wrap,mfr_init \
             -Wl,--wrap,mfrGetSerializedData \
             -pthread

# ---------- Test binary ----------
noinst_PROGRAMS = run_L1Tests

run_L1Tests_SOURCES = \
    test_main.cpp \
    utils/test_rdkProfile.cpp \
    sysmgr/test_sysMgr.cpp \
    mfr/test_mfrMgr.cpp \
    $(TESTFRAMEWORK_DIR)/Tests/mocks/Iarm.cpp \
    $(TESTFRAMEWORK_DIR)/Tests/mocks/Wraps.cpp

run_L1Tests_LDADD = -lgtest -lgmock

# Run tests on 'make check'
TESTS = run_L1Tests

# Clean up test artifacts
CLEANFILES = *.log *.trs
