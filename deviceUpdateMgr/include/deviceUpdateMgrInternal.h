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
* @file sysMgrInternal.h
*
* @brief IARM-Bus Sys Manager Internal API.
*
* This API defines the operations for Sys manager
*
* @par Document
* Document reference.
*
* @par Open Issues (in no particular order)
* -# None
*
* @par Assumptions
* -# None
*
* @par Abbreviations
* - BE:       ig-Endian.
* - cb:       allback function (suffix).
* - DS:      Device Settings.
* - FPD:     Front-Panel Display.
* - HAL:     Hardware Abstraction Layer.
* - LE:      Little-Endian.
* - LS:      Least Significant.
* - MBZ:     Must be zero.
* - MS:      Most Significant.
* - RDK:     Reference Design Kit.
* - _t:      Type (suffix).
*
* @par Implementation Notes
* -# None
*
*/

/** @defgroup IARM_BUS IARM-Sys Manager API
*   @ingroup IARM_RDK
*
*  IARM-Bus is a platform agnostic Inter-process communication (IPC) interface. It allows
*  applications to communicate with each other by sending Events or invoking Remote
*  Procedure Calls. The common programming APIs offered by the RDK IARM-Bus interface is
*  independent of the operating system or the underlying IPC mechanism.
*
*  Two applications connected to the same instance of IARM-Bus are able to exchange events
*  or RPC calls. On a typical system, only one instance of IARM-Bus instance is needed. If
*  desired, it is possible to have multiple IARM-Bus instances. However, applications
*  connected to different buses will not be able to communicate with each other.
*/

/** @addtogroup IARM_BUS_VREXMGR_INTERNAL_API IARM-Sys Manager internal API.
*  @ingroup IARM_BUS
*
*  Described herein are the Sys Manager types and functions.
*
*  @{
*/

/**
 * @brief Starts the Sys manager.
 *
 * This function registers and connects Sys Manager to the iarm bus.
 * Register Events that this module publishes and register APIs that 
 * can be RPCs by other entities on the bus.
 *
 * @return Error code if start fails.
 */



/**
* @defgroup iarmmgrs
* @{
* @defgroup deviceUpdateMgr
* @{
**/


#ifndef _IARM_VREXMGR_INTERNAL_
#define _IARM_VREXMGR_INTERNAL_
#include "libIARM.h"

#include <string.h>

#ifdef RDK_LOGGER_ENABLED
#include "rdk_debug.h"
#include "iarmUtil.h"

extern int b_rdk_logger_enabled;

#define INT_LOG(FORMAT, ...)     if(b_rdk_logger_enabled) {\
RDK_LOG(RDK_LOG_DEBUG, "LOG.RDK.VREXMGR", FORMAT , __VA_ARGS__);\
}\
else\
{\
printf(FORMAT, __VA_ARGS__);\
}

#else

#define INT_LOG(...)      printf(__VA_ARGS__)

#endif /* RDK_LOGGER_ENABLED */

#define STATUS_LOG(...)       LOG("IARMSTATUS: " __VA_ARGS__)


#endif


/** @} */
/** @} */
