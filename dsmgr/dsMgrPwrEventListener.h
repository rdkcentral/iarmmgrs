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



/**
 * @file dsMgrPwrEventListener.h
 *
 * @defgroup IARMBUS_DS_MGR DS Manager
 * @ingroup IARM_MGR_RPC
 *
 * DS (Device Settings) Manager is responsible for listening Power manager event and Set,
 * - LED/Front Panel Indicators
 * - Display (Aspect Ratio, EDID data etc.)
 * - General Host configuration (Power managements, event management etc.)
 */

/**
* @defgroup iarmmgrs
* @{
* @defgroup dsmgr
* @{
**/
#ifndef DSMGR_DSMGRPWREVENTLISTENER_H_
#define DSMGR_DSMGRPWREVENTLISTENER_H_

#include <queue>
#include <atomic>
#include <unistd.h>
#include "libIARM.h"
#include "libIBusDaemon.h"
#include "dsMgrInternal.h"
#include "sysMgr.h"
#include "power_controller.h"
#include "dsMgr.h"
#include "libIBus.h"
#include "iarmUtil.h"
#include "plat_power.h"

/* Retry every 300 msec */
#define DSMGR_PWR_CNTRL_CONNECT_WAIT_TIME_MS   (300*1000)


/* Power Controller State Data Structure to Pass to the Thread */
struct DSMgr_Power_Event_State_t{
    PowerController_PowerState_t currentState;
    PowerController_PowerState_t newState;
    DSMgr_Power_Event_State_t(PowerController_PowerState_t currSt, PowerController_PowerState_t newSt)
        : currentState(currSt), newState(newSt) {}
};

void initPwrEventListner(void);
void dsMgrInitPwrControllerEvt(void);
void dsMgrDeinitPwrControllerEvt(void);
static void dsMgrHandlePwrEventData(const PowerController_PowerState_t currentState,
                const PowerController_PowerState_t newState);
static PowerController_PowerState_t dsMgrPwrMgrToPowerControllerPowerState(PWRMgr_PowerState_t _state);
static void* dsMgrPwrEventHandlingThreadFunc(void *arg);
static void* dsMgrPwrRetryEstablishConnThread(void *arg);
static void dsMgrPwrCtrlEstablishConnection(void);
static void dsMgrPwrControllerFetchNinitStateValues(void);

#endif /* DSMGR_DSMGRPWREVENTLISTENER_H_ */
