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

/*
 * Stub header for the Telemetry 2.0 API used in the native (non-test) build.
 * For L1 test builds, Telemetry.h from entservices-testframework is
 * force-included instead and provides the mock-based declarations.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    T2ERROR_SUCCESS = 0,
    T2ERROR_FAILURE,
    T2ERROR_INVALID_PROFILE,
    T2ERROR_PROFILE_NOT_FOUND,
    T2ERROR_PROFILE_NOT_SET,
    T2ERROR_MAX_PROFILES_REACHED,
    T2ERROR_MEMALLOC_FAILED,
    T2ERROR_INVALID_ARGS,
    T2ERROR_INTERNAL_ERROR
} T2ERROR;

void    t2_init(char *component);
void    t2_uninit(void);
T2ERROR t2_event_s(char *marker, char *value);
T2ERROR t2_event_d(char *marker, int value);
T2ERROR t2_event_f(char *marker, double value);

#ifdef __cplusplus
}
#endif
