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
* @file
*
* @brief IARM-Bus MFR Manager Internal API.
*
* This API defines the operations used to starting and stopping IARM-Bus 
* MFR Manager.
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

/** @defgroup IARM_BUS IARM-Bus HAL API
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






/**
* @defgroup iarmmgrs
* @{
* @defgroup mfr
* @{
**/


#ifndef _MFRMGRINTERNAL_H_
#define _MFRMGRINTERNAL_H_


#include "libIARM.h"
#include <string.h>
#include "iarmUtil.h"

#ifdef RDK_LOGGER_ENABLED
#include "rdk_debug.h"
extern int b_rdk_logger_enabled;

#define LOG(...)              INT_LOG(__VA_ARGS__, "")
#define INT_LOG(FORMAT, ...)     if(b_rdk_logger_enabled) {\
RDK_LOG(RDK_LOG_DEBUG, "LOG.RDK.MFRMGR", FORMAT , __VA_ARGS__);\
}\
else\
{\
printf(FORMAT, __VA_ARGS__);\
}

#else

#define LOG(...)              printf(__VA_ARGS__)

#endif




/** @defgroup IARM_BUS MFR Manager MFR lib
*   @ingroup IARM_RDK
*
*/

/** @addtogroup IARM_BUS_MFR_INTERNAL_API IARM-MFR Manager internal API
*  @ingroup IARM_BUS
*
*  Described herein are functions that are used to initialize and manage MFR library.
*
*  @{
*/

 /**
 * @brief Starts the MFR lib.
 *
 * This function registers and connects MFR Manager to the iarm bus.
 * Register Events that this module publishes and register APIs that 
 * can be RPCs by other entities on the bus.
 *
 * @return IARM_Result_t Error Code.
 * @retval IARM_RESULT_SUCCESS on success
 */
IARM_Result_t MFRLib_Start(void);

/**
 * @brief Terminates the MFR lib
 *
 * This function disconnects MFR Manager from the iarm bus and terminates 
 * MFR Manager.
 *
 * @return IARM_Result_t Error Code.
 * @retval IARM_RESULT_SUCCESS on success
 */
IARM_Result_t MFRLib_Stop(void);

/**
 * @brief Listens for component specific events from drivers.
 *
 * @return IARM_Result_t Error Code.
 * @retval IARM_RESULT_SUCCESS on success
 */
IARM_Result_t MFRLib_Loop(void);

IARM_Result_t mfrMgr_start(void);

#endif


/* End of IARM_BUS_MFR_INTERNAL_API doxygen group */
/**
 * @}
 */


/** @} */
/** @} */
