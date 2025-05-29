/*
 * Copyright 2025 Comcast Cable Communications Management, LLC
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
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include <stdlib.h>
#include <setjmp.h>
#include "plat_power.h"
#include "power_controller.h"

void PowerController_Init()
{

}

void PowerController_Term()
{

}

bool PowerController_IsOperational()
{
    return true;
}

uint32_t PowerController_Connect()
{
    return POWER_CONTROLLER_ERROR_NONE;
}

uint32_t PowerController_GetPowerState(PowerController_PowerState_t* currentState, PowerController_PowerState_t* previousState)
{
    return POWER_CONTROLLER_ERROR_NONE;
}

uint32_t PowerController_SetPowerState(const int keyCode, const PowerController_PowerState_t powerstate, const char* reason)
{
    return POWER_CONTROLLER_ERROR_NONE;
}

uint32_t PowerController_PowerModePreChangeComplete(const uint32_t clientId, const int transactionId)
{
    return POWER_CONTROLLER_ERROR_NONE;
}

uint32_t PowerController_RegisterPowerModePreChangeCallback(PowerController_PowerModePreChangeCb callback, void* userdata)
{
    return POWER_CONTROLLER_ERROR_NONE;
}

uint32_t PowerController_UnRegisterPowerModePreChangeCallback(PowerController_PowerModePreChangeCb callback)
{
    return POWER_CONTROLLER_ERROR_NONE;
}

uint32_t PowerController_AddPowerModePreChangeClient(const char* clientName, uint32_t* clientId)
{
    return POWER_CONTROLLER_ERROR_NONE;
}

uint32_t PowerController_RemovePowerModePreChangeClient(const uint32_t clientId)
{
    return POWER_CONTROLLER_ERROR_NONE;
}

uint32_t PowerController_DelayPowerModeChangeBy(const uint32_t clientId, const int transactionId, const int delayPeriod)
{
    return POWER_CONTROLLER_ERROR_NONE;
}

uint32_t PowerController_RegisterPowerModeChangedCallback(PowerController_PowerModeChangedCb callback, void* userdata)
{
    return POWER_CONTROLLER_ERROR_NONE;
}

uint32_t PowerController_GetPowerStateBeforeReboot(PowerController_PowerState_t* powerStateBeforeReboot)
{
    return POWER_CONTROLLER_ERROR_NONE;
}

pmStatus_t PLAT_INIT(void)
{
  return (pmStatus_t)0;
}

pmStatus_t PLAT_API_SetPowerState(PWRMgr_PowerState_t newState)
{
  return (pmStatus_t)0;
}

pmStatus_t PLAT_API_GetPowerState(PWRMgr_PowerState_t* curState)
{
  return (pmStatus_t)0;
}

pmStatus_t PLAT_API_SetWakeupSrc(PWRMGR_WakeupSrcType_t srcType, bool enable)
{
  return (pmStatus_t)0;
}

pmStatus_t PLAT_API_GetWakeupSrc(PWRMGR_WakeupSrcType_t srcType, bool* enable)
{
  return (pmStatus_t)0;
}

pmStatus_t PLAT_Reset(PWRMgr_PowerState_t newState)
{
  return (pmStatus_t)0;
}

pmStatus_t PLAT_TERM(void)
{
  return (pmStatus_t)0;
}

int v_secure_system(const char *format, ...)
{
  return 0;
}
