/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2016 RDK Management
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
* @defgroup iarmmgrs
* @{
* @defgroup deepsleepmgr
* @{
**/


#ifndef _IARM_DEEPSLEEPMGR_LOGGER_
#define _IARM_DEEPSLEEPMGR_LOGGER_


#include <string.h>


#ifdef RDK_LOGGER_ENABLED
#include "rdk_debug.h"
#include "iarmUtil.h"

extern int b_rdk_logger_enabled;


#define LOG(...)              INT_LOG(__VA_ARGS__, "")
#define INT_LOG(FORMAT, ...)  do { \
    if(b_rdk_logger_enabled) { \
        RDK_LOG(RDK_LOG_DEBUG, "LOG.RDK.DEEPSLEEPMGR", FORMAT , __VA_ARGS__); \
    } \
    else \
    { \
        printf(FORMAT, __VA_ARGS__); \
    } \
} while(0)


#else

#define LOG(...)              printf(__VA_ARGS__)

#endif


#endif


/** @} */
/** @} */
