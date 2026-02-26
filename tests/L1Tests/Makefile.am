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
#   - tests/L1Tests/stubs          : minimal stub headers (libIARM.h, libIBusDaemon.h)
#   - TESTFRAMEWORK mocks          : Wraps.h, WrapsMock.h, Iarm.h, etc.
#   - tests/mocks/iarmmgrs         : local WrapsExt (fclose/fgets not in testframework)
#   - utils                        : source under test (rdkProfile.h, iarmutilslogger.h)
#
# The stubs path comes first so that iarmutilslogger.h's
#   #include "libIARM.h"  /  #include "libIBusDaemon.h"
# resolve to the lightweight stubs rather than failing.
#
# TESTFRAMEWORK_DIR is set by ./configure (AC_ARG_VAR) and points to the
# entservices-testframework checkout.

AM_CPPFLAGS = -I$(top_srcdir)/tests/L1Tests/stubs \
              -I$(TESTFRAMEWORK_DIR)/Tests/mocks \
              -I$(top_srcdir)/tests/mocks/iarmmgrs \
              -I$(top_srcdir)/utils

AM_CXXFLAGS = -Wall -std=c++14 -g

# Linker flags:
#   --wrap intercepts libc calls so the mock can control them.
#   fopen                  : testframework's Wraps.cpp provides __wrap_fopen
#   fclose / fgets         : local WrapsExt.cpp provides __wrap_fclose / __wrap_fgets
#   setmntent              : needed because testframework's WrapsMock.h constructor
#                            references __real_setmntent (links cleanly, never called)
AM_LDFLAGS = -Wl,--wrap,fopen \
             -Wl,--wrap,fclose \
             -Wl,--wrap,fgets \
             -Wl,--wrap,setmntent \
             -pthread

# ---------- Test binary ----------
noinst_PROGRAMS = run_L1Tests

run_L1Tests_SOURCES = \
    test_main.cpp \
    utils/test_rdkProfile.cpp \
    $(TESTFRAMEWORK_DIR)/Tests/mocks/Wraps.cpp \
    $(top_srcdir)/tests/mocks/iarmmgrs/WrapsExt.cpp

run_L1Tests_LDADD = -lgtest -lgmock

# Run tests on 'make check'
TESTS = run_L1Tests

# Clean up test artifacts
CLEANFILES = *.log *.trs
