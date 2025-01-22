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
* @defgroup iarmmgrs
* @{
* @defgroup deepsleepmgr
* @{
**/


#ifdef ENABLE_DEEP_SLEEP

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif
#include <sys/stat.h>
#include <pthread.h>
#include "iarmUtil.h"
#include "pwrMgr.h"
#include "libIBus.h"
//#include "plat_deepsleep.h"
#ifdef __cplusplus
}
#endif
#include "pwrlogger.h"
#include "plat_power.h"
#include "deepSleepMgr.h"
#include "libIBusDaemon.h"
#include "comcastIrKeyCodes.h"
#include "manager.hpp"

extern IARM_Result_t _SetPowerState(void *arg);

/* For glib APIs*/
#include <glib.h>

/* Deep Sleep static Functions */
static IARM_Result_t _GetLastWakeupReason(void *arg);
static IARM_Result_t _GetLastWakeupKeyCode(void *arg);

IARM_Result_t GetPwrMgrDeepSleepStatus(int *status);
IARM_Result_t PwrMgrDeepSleepWakeup(IARM_Bus_CommonAPI_PowerPreChange_Param_t *arg);

static gboolean heartbeatMsg(gpointer data);
static gboolean deep_sleep_delay_timer_fn(gpointer data);

/* Variables for Deep Sleep */
static uint32_t deep_sleep_delay_timeout = 0;
static uint32_t deep_sleep_wakeup_timer = 0;
static bool nwStandbyMode_gs = false;
GMainLoop *deepSleepMgr_Loop = NULL;
static GMainLoop *mainloop = NULL;
static guint dsleep_delay_event_src = 0;
static DeepSleepStatus_t IsDeviceInDeepSleep = DeepSleepStatus_NotStarted;
static gboolean isLxcRestart = 0;
static int read_tmp_integer_conf (const char* file_name);

int read_tmp_integer_conf(const char* file_name) {
    int i = 0;
    int ret = 0;

    // Perform the access check
    if (access(file_name, F_OK) == 0) {
        FILE* file = fopen(file_name, "r");
        if (file != NULL) {
            if (0 < fscanf(file, "%d", &i)) {
                ret = i;
            } else {
                __TIMESTAMP(); LOG("Error: fscanf on read_tmp_integer_conf");
            }
            fclose(file);
        } else {
            __TIMESTAMP(); LOG("Error: Cannot open file %s", file_name);
        }
    } else {
        __TIMESTAMP(); LOG("Error: File %s does not exist", file_name);
    }

    return ret;
}
extern void  _handleDeepsleepTimeoutWakeup ();


void IARM_Bus_PWRMGR_RegisterDeepSleepAPIs()
{
    /*  Register for IARM events */
    IARM_Bus_RegisterCall(IARM_BUS_PWRMGR_API_GetLastWakeupReason, _GetLastWakeupReason);
    IARM_Bus_RegisterCall(IARM_BUS_PWRMGR_API_GetLastWakeupKeyCode, _GetLastWakeupKeyCode);
}


void PwrMgrDeepSleepTimeout()
{
#if !defined (_DISABLE_SCHD_REBOOT_AT_DEEPSLEEP)
    /*Scheduled maintanace reboot is disabled*/
    system("echo 0 > /opt/.rebootFlag");
    system(" echo `/bin/timestamp` ------------- Reboot timer expired while in Deep Sleep --------------- >> /opt/logs/receiver.log");
    system("sleep 5; /rebootNow.sh -s DeepSleepMgr -o 'Rebooting the box due to reboot timer expired while in Deep Sleep...'");
#endif /*End of _DISABLE_SCHD_REBOOT_AT_DEEPSLEEP*/
}



static void SetPwrMgrDeepSleepMode(void *data)
{
    LOG("[%s:%d] Entering...\n",__FUNCTION__,__LINE__);
    IARM_Bus_PWRMgr_EventData_t *param = (IARM_Bus_PWRMgr_EventData_t *)data;
    IARM_Bus_PWRMgr_SetPowerState_Param_t pwr_param;
    IARM_Result_t retCode;

    if(IsDeviceInDeepSleep != DeepSleepStatus_InProgress)
    {
        LOG("Event IARM_BUS_PWRMGR_EVENT_MODECHANGED: State Changed %d -- > %d\r\n",
            param->data.state.curState, param->data.state.newState);

        if(IARM_BUS_PWRMGR_POWERSTATE_STANDBY_DEEP_SLEEP == param->data.state.newState)
        {

            LOG("GOT EVENT TO GO TO DEEP SLEEP \r\n");

            /*Here we are changing the DeesSleep State assuming that the api would succeed.
            This is because, once the deep sleep api is called, the CPU would be idle.*/
            /*Call Deep Sleep API*/
            deep_sleep_wakeup_timer = param->data.state.deep_sleep_timeout;
            FILE *fpST = NULL;
            uint32_t SleepTimeInSec = 0;
            struct stat buf;
            IsDeviceInDeepSleep = DeepSleepStatus_InProgress;

            /* Read the Delay Sleep Time  */
            fpST = fopen("/tmp/deepSleepTimer","r");

            if (NULL != fpST) {
                if(0 > fscanf(fpST,"%d",&SleepTimeInSec)) {
                    __TIMESTAMP();
                    LOG("Error: fscanf on SleepTimeInSec failed");
                } else {
                    deep_sleep_delay_timeout = SleepTimeInSec ;
                    __TIMESTAMP();
                    LOG(" /tmp/ override Deep Sleep Time is %d \r\n",deep_sleep_delay_timeout);
                }
                fclose (fpST);
            }
            uint32_t tmp_deep_sleep_wakeup_timer = read_tmp_integer_conf("/tmp/deepSleepTimerVal");
            if (tmp_deep_sleep_wakeup_timer){
                deep_sleep_wakeup_timer = tmp_deep_sleep_wakeup_timer;
                __TIMESTAMP();LOG(" /tmp/ override Deep Sleep wakeup time value is %d \r\n", deep_sleep_wakeup_timer);;
            }
            LOG("Deep Sleep wakeup time value is %d Secs.. \r\n",deep_sleep_wakeup_timer);
            if (deep_sleep_delay_timeout) {
                /* start a Deep sleep timer thread */
                LOG("Schedule Deep SLeep After %d Sec.. \r\n",deep_sleep_delay_timeout);
                dsleep_delay_event_src = g_timeout_add_seconds ((guint) deep_sleep_delay_timeout,deep_sleep_delay_timer_fn,deepSleepMgr_Loop);
            } else {
                LOG("Enter to Deep sleep Mode..stop Receiver with sleep 2 before DS \r\n");
                system("sleep 2");
                int status = -1;
                int retryCount = 0;
                bool userWakeup = 0;
                while(retryCount< 5) {
                    LOG("Device entering Deep sleep Mode.. \r\n");
                    userWakeup = 0;
                    nwStandbyMode_gs = param->data.state.nwStandbyMode;
                    LOG("\nCalling PLAT_DS_SetDeepSleep with nwStandbyMode: %s\n",
                        nwStandbyMode_gs?("Enabled"):("Disabled"));
                    LOG("Device entered to Deep sleep Mode.. \r\n");
                    status = PLAT_DS_SetDeepSleep(deep_sleep_wakeup_timer,&userWakeup, nwStandbyMode_gs);
                    LOG("Device resumed from Deep sleep Mode.status :%d  \r\n",status);

                    #ifdef PLATCO_BOOTTO_STANDBY
                    pwr_param.newState = IARM_BUS_PWRMGR_POWERSTATE_STANDBY_LIGHT_SLEEP;
                    LOG("Set newState to IARM_BUS_PWRMGR_POWERSTATE_STANDBY_LIGHT_SLEEP \n");
                    #else
                    pwr_param.newState = IARM_BUS_PWRMGR_POWERSTATE_ON;
                    LOG("Set newState to IARM_BUS_PWRMGR_POWERSTATE_ON \n");
                    #endif

                    if(status != 0) {
                        sleep(5);
                        retryCount++;
                        if(retryCount >= 5) {
                            LOG(" ERROR: Device failed to enter into Deep sleep Mode.  Set Power state newState= %d\n", pwr_param.newState);
                            LOG("calling _SetPowerState() with  newState= %d\n", pwr_param.newState);
                            retCode = _SetPowerState((void *)&pwr_param);
                            if(retCode != IARM_RESULT_SUCCESS )
                            {
                                LOG(" ERROR: _SetPowerState() call failed with retCode=%d\n", retCode);
                            }
                            break;
                        }
                    } else {
                        IsDeviceInDeepSleep = DeepSleepStatus_Completed;
                        break;
                    }
                }

                if (userWakeup) {
                        LOG(" Resumed due to user action. calling _SetPowerState() with newState= %d\n", pwr_param.newState);
                        retCode = _SetPowerState((void *)&pwr_param);
                        if(retCode != IARM_RESULT_SUCCESS )
                        {
                            LOG(" ERROR: _SetPowerState() call failed with retCode=%d\n", retCode);
                        }
                } else {
                    LOG("Resumed without user action. Not calling _SetPowerState().\r\n");
                }

                #ifdef USE_WAKEUP_TIMER_EVT
                DeepSleep_WakeupReason_t wakeupReason = DEEPSLEEP_WAKEUPREASON_UNKNOWN;
                int reasonStatus = PLAT_DS_GetLastWakeupReason(&wakeupReason);
                if (DEEPSLEEP_WAKEUPREASON_TIMER == wakeupReason){
                    LOG("Calling IARM_BUS_PWRMGR_API_handleDeepsleepTimeoutWakeup on wakeupReason:%d \n", wakeupReason);
                    _handleDeepsleepTimeoutWakeup();
                }
                #endif
            }
        }
    } else {
        LOG("[%s]DeepSleepStatus InProgress. Failed to Update DeepSleep ModeChange \r\n", __FUNCTION__);
    }

    if(data) {
        free(data);
        data = NULL;
    }
    LOG("[%s:%d] Exiting...\r\n",__FUNCTION__,__LINE__);
}

static void* DeepsleepStateChangeThread(void* arg)
{
    SetPwrMgrDeepSleepMode(arg);
    pthread_exit(NULL);
}

void HandleDeepSleepStateChange(void *data)
{

    pthread_t pwrMgrDSEventThreadId;

    IARM_Bus_PWRMgr_EventData_t *param = (IARM_Bus_PWRMgr_EventData_t *)data;

    LOG("Event IARM_BUS_PWRMGR_EVENT_MODECHANGED: State Changed %d -- > %d\r\n",
        param->data.state.curState, param->data.state.newState);

    int err = pthread_create(&pwrMgrDSEventThreadId, NULL, DeepsleepStateChangeThread, data);
    if(err != 0) {
        LOG("DeepsleepStateChangeThread thread create failed \r\n");
    } else {
        err = pthread_detach(pwrMgrDSEventThreadId);
        if(err != 0) {
            LOG("DeepsleepStateChangeThread thread detach failed \r\n");
        }
    }
}

IARM_Result_t PwrMgrDeepSleepWakeup(IARM_Bus_CommonAPI_PowerPreChange_Param_t *arg)
{
    IARM_Bus_CommonAPI_PowerPreChange_Param_t *param = (IARM_Bus_CommonAPI_PowerPreChange_Param_t *) arg;

    LOG(" State Changed %d -- > %d\r", param->curState, param->newState);

    /* Support Deep sleep to Power ON, Light Sleep and Standby Transition. */
    if( (PWRMGR_POWERSTATE_STANDBY_DEEP_SLEEP == param->curState) &&
            (PWRMGR_POWERSTATE_STANDBY_DEEP_SLEEP != param->newState)) {
        LOG("GOT EVENT TO EXIT FROM DEEP SLEEP \r\n");

        /*Call Wake up API */
        PLAT_DS_DeepSleepWakeup();

        /* Remove the Event source  */
        if(dsleep_delay_event_src) {
            g_source_remove(dsleep_delay_event_src);
            dsleep_delay_event_src = 0;
        }

        IsDeviceInDeepSleep = DeepSleepStatus_NotStarted;

        LOG("Device woke up from Deep sleep Mode.. \r\n");
    }
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t GetPwrMgrDeepSleepStatus(int *status)
{
    *status = IsDeviceInDeepSleep;
    return IARM_RESULT_SUCCESS;
}

static gboolean deep_sleep_delay_timer_fn(gpointer data)
{
    struct stat buf;
    int status = -1;

    LOG("Deep Sleep Timer Expires :Enter to Deep sleep Mode..stop Receiver with sleep 10 before DS \r\n");
    system("sleep 10");

    if ((stat("/lib/systemd/system/lxc.service", &buf) == 0) && (stat("/opt/lxc_service_disabled",&buf) !=0)) {
        system("systemctl stop lxc.service");
        isLxcRestart = 1;
    }
    bool userWakeup = 0;
    status = PLAT_DS_SetDeepSleep(deep_sleep_wakeup_timer,&userWakeup, false);
    if(status != 0) {
        LOG("deep_sleep_delay_timer_fn: Failed to enter deepsleep state \n");
    }

    #ifdef USE_WAKEUP_TIMER_EVT
    //Call pwrmgr InvokeDeepsleepTimeout here
    DeepSleep_WakeupReason_t wakeupReason = DEEPSLEEP_WAKEUPREASON_UNKNOWN;
    int reasonStatus = PLAT_DS_GetLastWakeupReason(&wakeupReason);
    if (DEEPSLEEP_WAKEUPREASON_TIMER == wakeupReason){
        LOG("Calling IARM_BUS_PWRMGR_API_handleDeepsleepTimeoutWakeup on wakeupReason:%d \n", wakeupReason);
        _handleDeepsleepTimeoutWakeup();
    }
    #endif
    return FALSE; // Send False so the handler should not be called again
}

static IARM_Result_t _GetLastWakeupReason(void *arg)
{
    DeepSleep_WakeupReason_t *wakeupReason = (DeepSleep_WakeupReason_t *)arg;
    int status = PLAT_DS_GetLastWakeupReason(wakeupReason);
    return (IARM_Result_t) status;
}

static IARM_Result_t _GetLastWakeupKeyCode(void *arg)
{
    DeepSleepMgr_WakeupKeyCode_Param_t *wakeupKeyCode = (DeepSleepMgr_WakeupKeyCode_Param_t *)arg;
    int status = PLAT_DS_GetLastWakeupKeyCode(wakeupKeyCode);
    return (IARM_Result_t) status;

}
#endif
/** @} */
/** @} */
