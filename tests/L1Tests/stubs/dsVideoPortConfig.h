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
 * @file tests/L1Tests/stubs/dsVideoPortConfig.h
 *
 * Test-build stub that shadows the platform dsVideoPortConfig.h.
 *
 * Purpose
 * -------
 * The real dsVideoPortConfig.h lives under the HAL interface tree which is
 * not available during L1 unit-test builds.  This stub satisfies all uses
 * of dsVideoPortConfig.h within dsMgr.c and dshalsrv_stubs.c by providing:
 *
 *   - _dsGetVideoPortResolutions() declaration: returns the platform
 *     resolution table.  dsMgr.c calls this directly from
 *     isResolutionSupported() and _SetResolution().
 *
 *   - _dsGetDefaultResolutionIndex() declaration: returns the index of the
 *     default resolution entry.  dsMgr.c calls this directly from
 *     _SetResolution().
 *
 * Both functions are bridged to DsHal mock methods in dsHalMock.h
 * (dsGetVideoPortResolutions / dsGetDefaultResolutionIndex).  The extern "C"
 * bridge definitions are emitted by dsHalMock.h when
 * DSHAL_MOCK_DEFINE_BRIDGES is defined, so they exist in exactly one
 * translation unit.  Individual tests control return values and output
 * arrays via ON_CALL / EXPECT_CALL on the DsHalMock fixture member.
 *
 * What is intentionally omitted
 * ------------------------------
 *   - dsVideoPortResolution_t : already provided by dsTypes.h (via the
 *     testframework's devicesettings.h force-include).
 *   - Any platform-specific resolution arrays or initialiser macros: not
 *     referenced by the code under test.
 */

#ifndef _DS_VIDEO_PORT_CONFIG_H_
#define _DS_VIDEO_PORT_CONFIG_H_

#include "dsError.h"   /* dsError_t, dsERR_NONE              */
#include "dsTypes.h"   /* dsVideoPortResolution_t             */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Return the platform video-port resolution table.
 *
 * The real implementation reads a static platform config array.  In the
 * test build, this function is bridged to
 * DsHalMock::dsGetVideoPortResolutions() by the extern "C" bridge defined
 * in dsHalMock.h (DSHAL_MOCK_DEFINE_BRIDGES section), allowing individual
 * tests to control the returned table and error code via ON_CALL /
 * EXPECT_CALL.
 *
 * @param[out] outResolutionSize  Number of entries written to *outResolutions.
 * @param[out] outResolutions     Pointer updated to point at the resolution array.
 * @return dsERR_NONE on success; dsERR_GENERAL (or another dsError_t) on
 *         failure.
 */
dsError_t _dsGetVideoPortResolutions(int                      *outResolutionSize,
                                     dsVideoPortResolution_t **outResolutions);

/**
 * @brief Return the index of the default resolution in the resolution table.
 *
 * The real implementation reads a platform constant.  In the test build,
 * this function is bridged to DsHalMock::dsGetDefaultResolutionIndex() by
 * the extern "C" bridge defined in dsHalMock.h (DSHAL_MOCK_DEFINE_BRIDGES
 * section), allowing individual tests to control the returned index and
 * error code via ON_CALL / EXPECT_CALL.
 *
 * @param[out] outDefaultIndex  Set to the zero-based index of the default
 *                              resolution entry.
 * @return dsERR_NONE on success; dsERR_GENERAL (or another dsError_t) on
 *         failure.
 */
dsError_t _dsGetDefaultResolutionIndex(int *outDefaultIndex);

#ifdef __cplusplus
}
#endif

#endif /* _DS_VIDEO_PORT_CONFIG_H_ */
