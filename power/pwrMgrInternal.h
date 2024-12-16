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
 * @file pwrMgrInternal.h
 *
 * @defgroup IARMBUS_POWER_MGR Power Manager
 * @ingroup IARM_MGR_RPC
 *
 * Power Manager monitors Power key events and reacts to power state changes. It dispatches Power Mode Change events to IARM-Bus.
 *
 * @par Feature Summary
 *
 * - Persist current power state (on/off) across power cycles.
 * - If box was shutdown via a scheduled reboot, then restore the last known power state
 * - If box was shutdown due to crash, then restore the last known power state.
 * - In all other cases, default to power on at startup.
 *
 * @par Possible power states
 * Power State                                    | Description
 * -----------------------------------------------|------------
 * PWRMGR_POWERSTATE_OFF                 | Power state OFF
 * PWRMGR_POWERSTATE_STANDBY             | Power state STANDBY
 * PWRMGR_POWERSTATE_ON                  | Power state ON
 * PWRMGR_POWERSTATE_STANDBY_LIGHT_SLEEP | Power state Light Sleep
 * PWRMGR_POWERSTATE_STANDBY_DEEP_SLEEP  | Power state Deep Sleep
 *
 * @par Power manager sends these events to other applications
 *
 * - IARM_BUS_PWRMGR_EVENT_MODECHANGED
 * - IARM_BUS_PWRMGR_EVENT_DEEPSLEEP_TIMEOUT
 *
 * @par The Event Data contains
 * @n
 * @code
 *     PWRMgr_PowerState_t curState; // current power state
 *     PWRMgr_PowerState_t newState; // A new power state
 *     unsigned int timeout; //Timeout
 * @endcode
 *
 * @par Power Manager: Events
 *
 * - @b IARM_BUS_PWRMGR_EVENT_MODECHANGED
 * @n Event to notify power mode change.
 * This event is broadcasted in case of power state change.
 * @n Along with the current power state and neew state, this event is boradcasted which will be handled by the application.
 * @n Example:
 * @code
 * IARM_Bus_PWRMgr_EventData_t param;
 * param.data.state.curState = curState;
 * param.data.state.newState = newState;
 * IARM_Bus_BroadcastEvent( IARM_BUS_PWRMGR_NAME, IARM_BUS_PWRMGR_EVENT_MODECHANGED,
 *                          (void *)&param, sizeof(param));
 * @endcode
 *
 * - @b IARM_BUS_PWRMGR_EVENT_DEEPSLEEP_TIMEOUT
 * @n Event to notify deep sleep timeout.
 * Deep sleep timeout is set for the time set by the IARM_BUS_PWRMGR_API_SetDeepSleepTimeOut
 * API after which this event will be broadcasted.
 * @n Example:
 * @code
 *  IARM_BUS_PWRMgr_DeepSleepTimeout_EventData_t param;
 *  param.timeout = deep_sleep_wakeup_timeout_sec;
 *  IARM_Bus_BroadcastEvent( IARM_BUS_PWRMGR_NAME, IARM_BUS_PWRMGR_EVENT_DEEPSLEEP_TIMEOUT,
 *             (void*)&param, sizeof(param));
 * @endcode
 *
 * @par Power Manager: RPC Methods
 * @n
 * Power manager publishes four RPC Methods
 * @n
 * - @b IARM_BUS_PWRMGR_API_SetPowerState
 * @n This RPC method is used to set a new power state and the power state can be ON, OFF, STANDBY,
 * STANDY-LIGHT-SLEEP or STANDBY-DEEP-SLEEP
 * @n Example:
 * @code
 *       IARM_Bus_PWRMgr_SetPowerState_Param_t param;
 *       param.newState = PWRMGR_POWERSTATE_ON
 *       IARM_Bus_Call(IARM_BUS_PWRMGR_NAME,
 *          IARM_BUS_PWRMGR_API_SetPowerState, (void *)&param, sizeof(param));
 * @endcode
 *
 *
 * - @b IARM_BUS_PWRMGR_API_GetPowerState
 * @n This API is used to retrieve the current  power state of the box
 * @n Example:
 * @code
 *       IARM_Bus_PWRMgr_GetPowerState_Param_t param;
 *       IARM_Bus_Call(IARM_BUS_PWRMGR_NAME,
 *                       IARM_BUS_PWRMGR_API_GetPowerState, (void *)&param, sizeof(param));
 * @endcode
 *
 *
 * - @b IARM_BUS_PWRMGR_API_WareHouseReset
 * @n This API is used to perform a Ware House Reset to user data so that STB can be recycled.
 * @n Example:
 * @code
 * IARM_Bus_Call(IARM_BUS_PWRMGR_NAME, IARM_BUS_PWRMGR_API_WareHouseReset, NULL, 0);
 * @endcode
 *
 *
 * - @b IARM_BUS_PWRMGR_API_SetDeepSleepTimeOut
 * This API is used to sets the timeout for deep sleep
 * @n Example:
 * @code
 * IARM_Bus_PWRMgr_SetDeepSleepTimeOut_Param_t param;
 * param.timeout = timeOut;
 * IARM_Bus_Call(IARM_BUS_PWRMGR_NAME,
 *               IARM_BUS_PWRMGR_API_SetDeepSleepTimeOut, (void *)&param, sizeof(param));
 * @endcode
 */

/**
* @defgroup iarmmgrs
* @{
* @defgroup power
* @{
**/

#ifndef _IARM_PWRMGR_INTERNAL_
#define _IARM_PWRMGR_INTERNAL_
#include "libIARM.h"
#include "pwrMgr.h"
#include "plat_power.h"
IARM_Result_t PWRMgr_Start(int argc, char *argv[]);
IARM_Result_t PWRMgr_Loop();
IARM_Result_t PWRMgr_Stop(void);

IARM_Result_t initReset();
int checkResetSequence(int keyType, int keyCode);
void setResetPowerState(PWRMgr_PowerState_t newPwrState);
void PwrMgr_Reset(PWRMgr_PowerState_t newState, bool isFPKeyPress);
void performReboot(const char * requestor, const char * reboot_reason_custom, const char * reboot_reason_other);

#endif


/** @} */
/** @} */


