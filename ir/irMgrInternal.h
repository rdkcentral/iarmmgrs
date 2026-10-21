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
* @file irMgrInternal.h
*
* @brief IARM-Bus IR Manager Internal API.
*
* This API defines the operations for key management.
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



/**
* @defgroup iarmmgrs
* @{
* @defgroup ir
* @{
**/


#ifndef _IARM_IRMGR_INTERNAL_
#define _IARM_IRMGR_INTERNAL_
#include "libIARM.h"
#include <string.h>
#include <stdbool.h>


#ifdef RDK_LOGGER_ENABLED
#include "rdk_debug.h"
#include "iarmUtil.h"

extern int b_rdk_logger_enabled;

#define LOG(...)              INT_LOG(__VA_ARGS__, "")
#define INT_LOG(FORMAT, ...)     if(b_rdk_logger_enabled) {\
RDK_LOG(RDK_LOG_DEBUG, "LOG.RDK.IRMGR", FORMAT , __VA_ARGS__);\
}\
else\
{\
printf(FORMAT, __VA_ARGS__);\
}

#else

#define LOG(...)              printf(__VA_ARGS__)

#endif


typedef void (* uinput_dispatcher_t) (int keyCode, int keyType, int source);

/** @addtogroup IARM_BUS_IRMGR_INTERNAL_API IARM-Bus IR Manager internal API.
*  @ingroup IARM_BUS
*
* Described here in are functions to manage the lifecycle of IR manager.
*
*  @{
*/

/** 
 * @brief Starts the IR manager.
 *
 * This function registers and connects IR Manager to the IARM bus.
 * 
 * @return Error code if start fails.
 */
IARM_Result_t IRMgr_Start(int argc, char *argv[]);
/**
 * @brief Loop to keep the IR manager alive.
 *
 * This function will loop around to keep IR manager alive.
 *
 */
IARM_Result_t IRMgr_Loop();

/**
 * @brief Terminates the IR manager.
 *
 *This function disconnects and terminates IR manager from IARM bus.
 *
 * @return Error code if stop fails.
 */

IARM_Result_t IRMgr_Stop(void);

/**
 * @brief register a uinput dispatcher.
 *
 * This function is invoked whenever a IR key event is sent out on IARM 
 * This should be called before IRMgr_Start()
 *
 * @return Error code if stop fails.
 */

IARM_Result_t IRMgr_Register_uinput(uinput_dispatcher_t f);

#ifdef __cplusplus
extern "C" { 
#endif


/**
 * @brief uinput module init.
 *
 * initialize uinput module. If /dev/uinput does not exist, fail;
 *
 * @return Error code if fails.
 */
int UINPUT_init(bool useSimulatorVendorId);

/**
 * @brief get the dispather that will listen for IARM  IR 
 *
 * @return NULL if uinput is not available.
 */
uinput_dispatcher_t UINPUT_GetDispatcher(void);

/**
 * @brief uinput module term.
 *
 * release uinput module. 
 *
 * @return Error code if fails.
 */
int UINPUT_term(void);

#ifdef __cplusplus
}
#endif
IARM_Result_t IRMgr_Register_uinput(uinput_dispatcher_t f);
#endif


/* End of IARM_BUS_IRMGR_INTERNAL_API doxygen group */
/**
 * @}
 */


/** @} */
/** @} */
