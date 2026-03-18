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
 * @file tests/L1Tests/stubs/dsAudioConfig.h
 *
 * Test-build stub that shadows the platform dsAudioConfig.h.
 *
 * Purpose
 * -------
 * The real dsAudioConfig.h lives under the HAL interface tree which is not
 * available during L1 unit-test builds.  This stub satisfies all uses of
 * dsAudioConfig.h within dsMgr.c and dshalsrv_stubs.c by providing:
 *
 *   - dsAudioTypeConfig_t  : the only struct type accessed by dsMgr.c.
 *     Only the 'typeId' field is read by dsMgr.c (_setAudioMode /
 *     _setEASAudioMode), so that is the only member required here.
 *
 *   - _dsGetAudioTypeConfigs() declaration: the C function that dsMgr.c
 *     calls directly.  Return values are controlled per-test via the
 *     DsHalMock::dsGetAudioTypeConfigs() MOCK_METHOD in dsHalMock.h; the
 *     extern "C" bridge function that routes calls to the mock is emitted
 *     by dsHalMock.h when DSHAL_MOCK_DEFINE_BRIDGES is defined.
 *
 * What is intentionally omitted
 * ------------------------------
 *   - dsAudioPortType_t / dsAUDIOPORT_TYPE_* enumerators: already provided
 *     by dsTypes.h (via the testframework's devicesettings.h force-include).
 *   - Any platform-specific config arrays or initialiser macros: not
 *     referenced by the code under test.
 */

#ifndef _DS_AUDIO_CONFIG_H_
#define _DS_AUDIO_CONFIG_H_

#include "dsError.h"   /* dsError_t, dsERR_NONE */
#include "dsTypes.h"   /* dsAudioPortType_t     */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Per-port audio-type configuration descriptor.
 *
 * In the production build this struct may carry additional fields (e.g.
 * numPorts, portId).  Only 'typeId' is accessed by dsMgr.c, so only that
 * field is declared here.  Tests initialise instances with memset and then
 * set .typeId to the desired dsAudioPortType_t value.
 */
typedef struct _dsAudioTypeConfig_t {
    int typeId; /**< Audio-port type; cast to dsAudioPortType_t by dsMgr.c */
} dsAudioTypeConfig_t;

/**
 * @brief Return the platform audio-port type configuration table.
 *
 * The real implementation reads a static platform config array.  In the
 * test build, this function is bridged to DsHalMock::dsGetAudioTypeConfigs()
 * by the extern "C" bridge defined in dsHalMock.h (DSHAL_MOCK_DEFINE_BRIDGES
 * section), allowing individual tests to control the returned table and error
 * code via ON_CALL / EXPECT_CALL.
 *
 * @param[out] outConfigSize  Number of entries written to *outConfigs.
 * @param[out] outConfigs     Pointer updated to point at the config array.
 * @return dsERR_NONE on success; dsERR_GENERAL (or another dsError_t) on
 *         failure.
 */
dsError_t _dsGetAudioTypeConfigs(int *outConfigSize,
                                 const dsAudioTypeConfig_t **outConfigs);

#ifdef __cplusplus
}
#endif

#endif /* _DS_AUDIO_CONFIG_H_ */
