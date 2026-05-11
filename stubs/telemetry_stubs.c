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
 * Stub implementations of the Telemetry 2.0 API for native (non-test) builds.
 * All functions are no-ops that let the component code compile and link without
 * requiring the real libtelemetry_msgsender library.
 */

#include "telemetry_busmessage_sender.h"

void t2_init(char *component)
{
    (void)component;
}

void t2_uninit(void)
{
}

T2ERROR t2_event_s(char *marker, char *value)
{
    (void)marker;
    (void)value;
    return T2ERROR_SUCCESS;
}

T2ERROR t2_event_d(char *marker, int value)
{
    (void)marker;
    (void)value;
    return T2ERROR_SUCCESS;
}

T2ERROR t2_event_f(char *marker, double value)
{
    (void)marker;
    (void)value;
    return T2ERROR_SUCCESS;
}
