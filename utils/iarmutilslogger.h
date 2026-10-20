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
 * @file dsMgrInternal.h
 *
 * @defgroup IARMBUS_DS_MGR DS Manager
 * @ingroup IARM_MGR_RPC
 *
 * DS (Device Settings) Manager is responsible for managing the following operations,
 * - Audio Output Ports (Volume, Mute, etc.)
 * - Video Ouptut Ports (Resolutions, Aspect Ratio, etc.)
 * - Front Panel Indicators
 * - Zoom Settings
 * - Display (Aspect Ratio, EDID data etc.)
 * - General Host configuration (Power managements, event management etc.)
 */

/**
* @defgroup iarmmgrs
* @{
* @defgroup dsmgr
* @{
**/


#ifndef _IARM_IARMUTILSLOG_INTERNAL_
#define _IARM_IARMUTILSLOG_INTERNAL_
#include "libIARM.h"
#include "libIBusDaemon.h"

#include <string.h>
#include <stdio.h>

/* Helper to extract the file name from the full path (Unix style paths only) */
static inline const char* fileName(const char* path) {
    const char* slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

#define PREFIX(FORMAT)  "[%s:%d] %s: " FORMAT

/* RDK logger module used by the logging macros, a component can define its own before including this header */
#ifndef IARMUTILS_LOG_MODULE
#define IARMUTILS_LOG_MODULE "LOG.RDK.IARMUTILS"
#endif

/* IARMUTILS_LIB_BUILD: code built into the shared libiarmUtils.so is linked by several managers (dsmgr, mfr, ...)
 * and must not depend on a b_rdk_logger_enabled symbol unless every one of them defines it. DSMGR_LOGGER_ENABLED only
 * guarantees that for dsMgrMain, so the library ignores it and keeps using the RDK_LOGGER_ENABLED gate. */
#if (defined(RDK_LOGGER_ENABLED) || (defined(DSMGR_LOGGER_ENABLED) && !defined(IARMUTILS_LIB_BUILD)))
#include "rdk_debug.h"
#include "iarmUtil.h"

#define INT_ERROR(FORMAT, ...)       LOG_ERROR(PREFIX(FORMAT), fileName(__FILE__), __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define INT_WARNING(FORMAT,  ...)    LOG_WARNING(PREFIX(FORMAT), fileName(__FILE__), __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define INT_INFO(FORMAT,  ...)       LOG_INFO(PREFIX(FORMAT), fileName(__FILE__), __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define INT_DEBUG(FORMAT, ...)       LOG_DEBUG(PREFIX(FORMAT), fileName(__FILE__), __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define INT_TRACE(FORMAT, ...)       LOG_TRACE(PREFIX(FORMAT), fileName(__FILE__), __LINE__, __FUNCTION__, ##__VA_ARGS__)

extern int b_rdk_logger_enabled;


/*
 * Safe logging macros with do-while(0) wrappers
 * 
 * These macros provide:
 * - Thread-safe logging operations
 * - Conditional RDK logger support with printf fallback
 * - Safe macro expansion preventing statement ambiguity
 * - Consistent formatting across all log levels
 */

#define LOG_DEBUG(FORMAT, ...)  do { \
    if(b_rdk_logger_enabled) { \
        RDK_LOG(RDK_LOG_DEBUG, IARMUTILS_LOG_MODULE, FORMAT , ##__VA_ARGS__); \
    } \
    else { \
        printf(FORMAT, ##__VA_ARGS__); \
    } \
} while(0)

#define LOG_ERROR(FORMAT, ...)  do { \
    if(b_rdk_logger_enabled) { \
        RDK_LOG(RDK_LOG_ERROR, IARMUTILS_LOG_MODULE, FORMAT , ##__VA_ARGS__); \
    } \
    else { \
        printf(FORMAT, ##__VA_ARGS__); \
    } \
} while(0)

#define LOG_INFO(FORMAT, ...)   do { \
    if(b_rdk_logger_enabled) { \
        RDK_LOG(RDK_LOG_INFO, IARMUTILS_LOG_MODULE, FORMAT , ##__VA_ARGS__); \
    } \
    else { \
        printf(FORMAT, ##__VA_ARGS__); \
    } \
} while(0)

#define LOG_WARNING(FORMAT, ...)  do { \
    if(b_rdk_logger_enabled) { \
        RDK_LOG(RDK_LOG_WARN, IARMUTILS_LOG_MODULE, FORMAT , ##__VA_ARGS__); \
    } \
    else { \
        printf(FORMAT, ##__VA_ARGS__); \
    } \
} while(0)

#else

/* Fallback macros for non-RDK logger builds - same format as the RDK logger build */
#define INT_DEBUG(FORMAT, ...)     do { printf(PREFIX(FORMAT), fileName(__FILE__), __LINE__, __FUNCTION__, ##__VA_ARGS__); } while(0)
#define INT_ERROR(FORMAT, ...)     do { printf(PREFIX(FORMAT), fileName(__FILE__), __LINE__, __FUNCTION__, ##__VA_ARGS__); } while(0)
#define INT_INFO(FORMAT, ...)      do { printf(PREFIX(FORMAT), fileName(__FILE__), __LINE__, __FUNCTION__, ##__VA_ARGS__); } while(0)
#define INT_WARNING(FORMAT, ...)   do { printf(PREFIX(FORMAT), fileName(__FILE__), __LINE__, __FUNCTION__, ##__VA_ARGS__); } while(0)
#define INT_TRACE(FORMAT, ...)     do { printf(PREFIX(FORMAT), fileName(__FILE__), __LINE__, __FUNCTION__, ##__VA_ARGS__); } while(0)


#endif



#endif


/** @} */
/** @} */
