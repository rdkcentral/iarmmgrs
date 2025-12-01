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

#include <string.h>
#if (defined(RDK_LOGGER_ENABLED))
#include "rdk_debug.h"
#include "iarmUtil.h"

#define INT_ERROR(FORMAT, ...)       LOG_ERROR(PREFIX(FORMAT), __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define INT_WARNING(FORMAT,  ...)       LOG_WARNING(PREFIX(FORMAT),  __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define INT_INFO(FORMAT,  ...)       LOG_INFO(PREFIX(FORMAT),  __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define INT_DEBUG(FORMAT, ...)       LOG_DEBUG(PREFIX(FORMAT), __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define INT_TRACE(FORMAT, ...)       LOG_TRACE(PREFIX(FORMAT), __LINE__, __FUNCTION__, ##__VA_ARGS__)

#define PREFIX(FORMAT)  "%d\t: %s - " FORMAT

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
        RDK_LOG(RDK_LOG_DEBUG, "LOG.RDK.IARMUTILS", FORMAT , ##__VA_ARGS__); \
    } \
    else { \
        printf(FORMAT, ##__VA_ARGS__); \
    } \
} while(0)

#define LOG_ERROR(FORMAT, ...)  do { \
    if(b_rdk_logger_enabled) { \
        RDK_LOG(RDK_LOG_ERROR, "LOG.RDK.IARMUTILS", FORMAT , ##__VA_ARGS__); \
    } \
    else { \
        printf(FORMAT, ##__VA_ARGS__); \
    } \
} while(0)

#define LOG_INFO(FORMAT, ...)   do { \
    if(b_rdk_logger_enabled) { \
        RDK_LOG(RDK_LOG_INFO, "LOG.RDK.IARMUTILS", FORMAT , ##__VA_ARGS__); \
    } \
    else { \
        printf(FORMAT, ##__VA_ARGS__); \
    } \
} while(0)

#define LOG_WARNING(FORMAT, ...)  do { \
    if(b_rdk_logger_enabled) { \
        RDK_LOG(RDK_LOG_WARN, "LOG.RDK.IARMUTILS", FORMAT , ##__VA_ARGS__); \
    } \
    else { \
        printf(FORMAT, ##__VA_ARGS__); \
    } \
} while(0)

#else

/* Fallback macros for non-RDK logger builds - consistent formatting */
#define INT_DEBUG(FORMAT, ...)     do { printf(FORMAT, ##__VA_ARGS__); } while(0)
#define INT_ERROR(FORMAT, ...)     do { printf(FORMAT, ##__VA_ARGS__); } while(0)
#define INT_INFO(FORMAT, ...)      do { printf(FORMAT, ##__VA_ARGS__); } while(0)
#define INT_WARNING(FORMAT, ...)   do { printf(FORMAT, ##__VA_ARGS__); } while(0)

#endif



#endif


/** @} */
/** @} */
