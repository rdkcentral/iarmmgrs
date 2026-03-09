/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file tests/L1Tests/mocks/dsHalMock.h
 * @brief GMock interface for DS HAL C functions called directly by dsMgr.c.
 *
 * Background
 * ----------
 * Most DS HAL functions in dsMgr.c are reachable via the _ds*()-prefixed
 * extern wrapper pattern and are already stubbed in test_dsMgr.cpp as plain
 * no-op functions.  A small number of HAL functions are called directly
 * (without a wrapper), making them unreachable via --wrap and requiring a
 * definition in the same translation unit.
 *
 * This header follows the same interface+mock+impl-pointer idiom used by
 * the testframework's IarmBusMock.h so that each test case can install its
 * own NiceMock / StrictMock and program positive or negative return values.
 *
 * Direct DS HAL calls currently wrapped here
 * ------------------------------------------
 *   dsGetDisplay()                — called from _SetResolution() and
 *                                   dumpEdidOnChecksumDiff()
 *   _dsGetVideoPortResolutions()  — called from isResolutionSupported() and
 *                                   _SetResolution()
 *   _dsGetDefaultResolutionIndex()— called from _SetResolution()
 *   _dsGetAudioTypeConfigs()      — called from _setAudioMode() and
 *                                   _setEASAudioMode()
 *
 * Usage
 * -----
 * 1. Include this header in the test file AFTER dsMgr.c is included (so the
 *    HAL types from dsDisplay.h are already in scope).
 * 2. Add a NiceMock<DsHalMock> member to the test fixture.
 * 3. Call DsHal::setImpl(&mock) in SetUp() and DsHal::setImpl(nullptr) in
 *    TearDown().
 * 4. Program expectations with EXPECT_CALL as usual.
 *
 * The extern "C" bridge functions are defined inline in this header
 * (guarded by DSHAL_MOCK_DEFINE_BRIDGES) so they appear in exactly one
 * translation unit — the test file that defines DSHAL_MOCK_DEFINE_BRIDGES
 * before including this header.
 */

#pragma once

#include <gmock/gmock.h>
#include <cstdint>

/* dsError_t, dsVideoPortType_t, dsERR_NONE, IARM_Result_t, and the _ds*
 * RPC parameter struct types (dsVideoPortGetHandleParam_t, etc.) all come
 * from headers that dsMgr.c (or the test stub dsMgr.h) pulls in before
 * this header is included. */

/* =========================================================================
 * Abstract interface
 * ======================================================================= */

class DsHal
{
public:
    virtual ~DsHal() = default;

    /**
     * Obtain the opaque display handle for the given video port.
     * Maps 1-to-1 onto the DS HAL dsGetDisplay() C function.
     */
    virtual dsError_t dsGetDisplay(dsVideoPortType_t type,
                                   int               index,
                                   intptr_t         *handle) = 0;

    /* ---- _ds* IARM RPC wrapper functions --------------------------------
     * These mirror the _ds*()-prefixed extern functions in dsMgr.c.  The
     * bridge functions in the test TU cast void*arg to the typed struct
     * pointer and call through here, allowing tests to control return
     * values and output fields via ON_CALL / EXPECT_CALL.
     * ------------------------------------------------------------------- */
    virtual IARM_Result_t dsGetVideoPort(dsVideoPortGetHandleParam_t *p)               = 0;
    virtual IARM_Result_t dsIsDisplayConnected(dsVideoPortIsDisplayConnectedParam_t *p) = 0;
    virtual IARM_Result_t dsGetResolution(dsVideoPortGetResolutionParam_t *p)          = 0;
    virtual IARM_Result_t dsGetEDID(dsDisplayGetEDIDParam_t *p)                        = 0;
    virtual IARM_Result_t dsGetEDIDBytes(dsDisplayGetEDIDBytesParam_t *p)              = 0;
    virtual IARM_Result_t dsGetStereoMode(dsAudioSetStereoModeParam_t *p)              = 0;

    /* ---- Platform configuration functions called directly by dsMgr.c -- */
    virtual dsError_t dsGetVideoPortResolutions(int *outSize,
                                               dsVideoPortResolution_t **outRes)       = 0;
    virtual dsError_t dsGetDefaultResolutionIndex(int *outIndex)                       = 0;
    virtual dsError_t dsGetAudioTypeConfigs(int *outSize,
                                            const dsAudioTypeConfig_t **outConfigs)    = 0;

    /* ---- impl-pointer management -------------------------------------- */
    static DsHal *getInstance() { return s_impl; }
    static void   setImpl(DsHal *impl) { s_impl = impl; }

private:
    static DsHal *s_impl;
};

/* =========================================================================
 * GMock concrete implementation
 * ======================================================================= */

class DsHalMock : public DsHal
{
public:
    MOCK_METHOD(dsError_t, dsGetDisplay,
                (dsVideoPortType_t type, int index, intptr_t *handle),
                (override));
    MOCK_METHOD(IARM_Result_t, dsGetVideoPort,
                (dsVideoPortGetHandleParam_t *p),
                (override));
    MOCK_METHOD(IARM_Result_t, dsIsDisplayConnected,
                (dsVideoPortIsDisplayConnectedParam_t *p),
                (override));
    MOCK_METHOD(IARM_Result_t, dsGetResolution,
                (dsVideoPortGetResolutionParam_t *p),
                (override));
    MOCK_METHOD(IARM_Result_t, dsGetEDID,
                (dsDisplayGetEDIDParam_t *p),
                (override));
    MOCK_METHOD(IARM_Result_t, dsGetEDIDBytes,
                (dsDisplayGetEDIDBytesParam_t *p),
                (override));
    MOCK_METHOD(IARM_Result_t, dsGetStereoMode,
                (dsAudioSetStereoModeParam_t *p),
                (override));
    MOCK_METHOD(dsError_t, dsGetVideoPortResolutions,
                (int *outSize, dsVideoPortResolution_t **outRes),
                (override));
    MOCK_METHOD(dsError_t, dsGetDefaultResolutionIndex,
                (int *outIndex),
                (override));
    MOCK_METHOD(dsError_t, dsGetAudioTypeConfigs,
                (int *outSize, const dsAudioTypeConfig_t **outConfigs),
                (override));
};

/* =========================================================================
 * Static member definition — emitted in exactly one TU.
 *
 * Define DSHAL_MOCK_DEFINE_BRIDGES before including this header in the
 * single test .cpp file that owns the mock lifetime.
 * ======================================================================= */

#ifdef DSHAL_MOCK_DEFINE_BRIDGES

DsHal *DsHal::s_impl = nullptr;

/**
 * extern "C" bridge — satisfies the linker when dsMgr.c calls dsGetDisplay().
 * Forwards the call to the currently-installed DsHal mock; falls back to a
 * safe no-op (handle = 0, return dsERR_NONE) when no mock is active.
 */
extern "C" dsError_t dsGetDisplay(dsVideoPortType_t type,
                                  int               index,
                                  intptr_t         *handle)
{
    DsHal *impl = DsHal::getInstance();
    if (impl)
        return impl->dsGetDisplay(type, index, handle);

    /* Safe fallback: no display available */
    if (handle) *handle = 0;
    return dsERR_NONE;
}

dsError_t _dsGetVideoPortResolutions(int *outSize, dsVideoPortResolution_t **outRes)
{
    DsHal *impl = DsHal::getInstance();
    if (impl)
        return impl->dsGetVideoPortResolutions(outSize, outRes);
    if (outSize) *outSize = 0;
    if (outRes)  *outRes  = nullptr;
    return dsERR_NONE;
}

dsError_t _dsGetDefaultResolutionIndex(int *outIndex)
{
    DsHal *impl = DsHal::getInstance();
    if (impl)
        return impl->dsGetDefaultResolutionIndex(outIndex);
    if (outIndex) *outIndex = 0;
    return dsERR_NONE;
}

dsError_t _dsGetAudioTypeConfigs(int *outSize, const dsAudioTypeConfig_t **outConfigs)
{
    DsHal *impl = DsHal::getInstance();
    if (impl)
        return impl->dsGetAudioTypeConfigs(outSize, outConfigs);
    if (outSize)    *outSize    = 0;
    if (outConfigs) *outConfigs = nullptr;
    return dsERR_NONE;
}

#endif /* DSHAL_MOCK_DEFINE_BRIDGES */
