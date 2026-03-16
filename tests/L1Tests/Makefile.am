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
#   - tests/L1Tests/stubs          : stub headers for the test build.
#                                    libIBus.h, libIARM.h, libIBusDaemon.h,
#                                    iarmUtil.h and libIARMCore.h are generated
#                                    empty at CI time (touch).  mfrMgr.h is a
#                                    hand-written stub that avoids type
#                                    redefinition conflicts with Iarm.h from the
#                                    testframework: it provides all macros +
#                                    non-conflicting types and lets the
#                                    conflicting types come from Iarm.h.
#   - mfr/include                  : HAL mfrTypes.h, mfr_wifi_types.h,
#                                    mfr_wifi_api.h.  (mfrMgr.h here is
#                                    shadowed by the stub above.)
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
              -I$(top_srcdir)/tests/L1Tests/mocks \
              -I$(top_srcdir)/mfr/include \
              -I$(top_srcdir)/mfr \
              -I$(TESTFRAMEWORK_DIR)/Tests/mocks \
              -I$(top_srcdir)/utils \
              -I$(top_srcdir)/sysmgr \
              -I$(top_srcdir)/sysmgr/include \
              -I$(top_srcdir)/stubs \
              -I$(top_srcdir)/dsmgr \
              -include $(TESTFRAMEWORK_DIR)/Tests/mocks/Telemetry.h \
              -UMAX_LANGUAGE_LEN -UdsFPDColor_Make \
              -include $(TESTFRAMEWORK_DIR)/Tests/mocks/devicesettings.h \
              -I/usr/include/libdrm \
              -I$(DS_IF_DIR)/include \
              -I$(top_srcdir)/tests/L1Tests/stubs \
              $(GLIB_CFLAGS) \
              -DRDK_MFRLIB_NAME='"libmfr_test_stub.so"'

AM_CXXFLAGS = -Wall -std=c++14 -g

# Linker flags:
#   --wrap intercepts libc calls so the mock can control them.
#   fopen / fclose / fgets      : testframework's Wraps.cpp provides the shims
#   setmntent                   : needed because WrapsMock.h ctor refs __real_setmntent
#   mfr_init                    : mfr test controls MFRLib_Start behaviour
#   mfrGetSerializedData        : mfr test controls getSerializedData_ behaviour
#   pthread_cond_init /
#   pthread_create              : DSMgr_Start thread-creation failure paths
#   usleep                      : 100 ms wait inside _SetVideoPortResolution
#   g_main_loop_new /
#   g_timeout_add_seconds /
#   g_main_loop_unref           : DSMgr_Start GLib loop branches
#   dlopen / dlsym / dlclose    : mfr tests control which library-load branch
#                                  is taken inside each RPC handler
AM_LDFLAGS = -Wl,--wrap,fopen \
             -Wl,--wrap,fclose \
             -Wl,--wrap,fgets \
             -Wl,--wrap,setmntent \
             -Wl,--wrap,mfr_init \
             -Wl,--wrap,mfrGetSerializedData \
             -Wl,--wrap,dlopen \
             -Wl,--wrap,dlsym \
             -Wl,--wrap,dlclose \
             -Wl,--wrap,pthread_cond_init \
             -Wl,--wrap,pthread_create \
             -Wl,--wrap,usleep \
             -Wl,--wrap,g_main_loop_new \
             -Wl,--wrap,g_timeout_add_seconds \
             -Wl,--wrap,g_main_loop_unref \
             -pthread

# ---------- Test binary ----------
noinst_PROGRAMS = run_L1Tests

run_L1Tests_SOURCES = \
    test_main.cpp \
    utils/test_rdkProfile.cpp \
    sysmgr/test_sysMgr.cpp \
    mfr/test_mfrMgr.cpp \
    dsmgr/test_dsMgr.cpp \
    $(TESTFRAMEWORK_DIR)/Tests/mocks/Iarm.cpp \
    $(TESTFRAMEWORK_DIR)/Tests/mocks/Wraps.cpp \
    $(TESTFRAMEWORK_DIR)/Tests/mocks/Telemetry.cpp \
    $(TESTFRAMEWORK_DIR)/Tests/mocks/devicesettings.cpp

run_L1Tests_LDADD = -lgtest -lgmock $(GLIB_LIBS)

# Run tests on 'make check'
TESTS = run_L1Tests

# Clean up test artifacts
CLEANFILES = *.log *.trs
