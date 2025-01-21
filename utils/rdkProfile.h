/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2021 RDK Management
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

#ifndef _IARM_UTILS_
#define _IARM_UTILS_
#include <string.h>
#include <pthread.h>
#include "libIBus.h"
#include "libIBusDaemon.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum profile {
    PROFILE_INVALID = -1,
    PROFILE_STB = 0,
    PROFILE_TV,
    PROFILE_MAX
}profile_t;

profile_t searchRdkProfile(void);

#ifdef __cplusplus
}
#endif

#endif
