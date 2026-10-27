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
 * @file iarmutilslogger.h
 *
 * @defgroup IARMUTILS_LOGGER IARM Utils Logger
 * @ingroup IARM_UTILS
 *
 * IARM Utils Logger is responsible for providing logging functionality
 * across IARM manager components with RDK logger integration support.
 * 
 * Features:
 * - Conditional logging based on RDK logger availability
 * - Multiple log levels (DEBUG, INFO, WARN, ERROR, FATAL)
 * - Fallback to printf when RDK logger is unavailable
 * - Thread-safe logging operations
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
#include <telemetry_busmessage_sender.h>

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
#ifndef INT_DEBUG
#define INT_DEBUG(FORMAT, ...)     do { printf(PREFIX(FORMAT), fileName(__FILE__), __LINE__, __FUNCTION__, ##__VA_ARGS__); } while(0)
#endif

#ifndef INT_ERROR
#define INT_ERROR(FORMAT, ...)     do { printf(PREFIX(FORMAT), fileName(__FILE__), __LINE__, __FUNCTION__, ##__VA_ARGS__); } while(0)
#endif

#ifndef INT_INFO
#define INT_INFO(FORMAT, ...)      do { printf(PREFIX(FORMAT), fileName(__FILE__), __LINE__, __FUNCTION__, ##__VA_ARGS__); } while(0)
#endif

#ifndef INT_WARNING
#define INT_WARNING(FORMAT, ...)   do { printf(PREFIX(FORMAT), fileName(__FILE__), __LINE__, __FUNCTION__, ##__VA_ARGS__); } while(0)
#endif

#endif



#define TELEMETRY_INIT(component) \
    do { \
        t2_init((char*)component); \
    } while(0)

#define TELEMETRY_UNINIT() \
    do { \
        t2_uninit(); \
    } while(0)

#define TELEMETRY_EVENT_STRING(marker, value) \
    do { \
        t2_event_s((char*)marker, (char*)value); \
    } while(0)

#define TELEMETRY_EVENT_FLOAT(marker, value) \
    do { \
        t2_event_f((char*)marker, (double)value); \
    } while(0)

#define TELEMETRY_EVENT_INT(marker, value) \
    do { \
        t2_event_d((char*)marker, (int)value); \
    } while(0)

#endif


/** @} */
/** @} */
