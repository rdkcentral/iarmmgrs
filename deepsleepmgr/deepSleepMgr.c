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
* @defgroup iarmmgrs
* @{
* @defgroup deepsleepmgr
* @{
**/


#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C"
{
#endif
#ifdef __cplusplus
}
#endif
#include <sys/stat.h>
#include <pthread.h>
#include "iarmUtil.h"
#include "pwrMgr.h"
#include "libIBus.h"
#include "deepSleepMgr.h"
#include "deepSleepInternal.h"
#include "libIBusDaemon.h"
#include "deepsleeplogger.h"
#include "irMgr.h"
#include "comcastIrKeyCodes.h"
#include "frontPanelConfig.hpp"
#include "manager.hpp"
#include "plat_power.h"

/* For glib APIs*/
#include <glib.h>

/* Deep Sleep static Functions */
static void _eventHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len);
static IARM_Result_t _DeepSleepWakeup(void *arg);
static IARM_Result_t _SetDeepSleepTimer(void *arg);
static IARM_Result_t _GetDeepSleepStatus(void *arg);
static IARM_Result_t _GetLastWakeupReason(void *arg);
static IARM_Result_t _GetLastWakeupKeyCode(void *arg);

static gboolean heartbeatMsg(gpointer data);
static gboolean deep_sleep_delay_timer_fn(gpointer data);

/* Variables for Deep Sleep */
static uint32_t deep_sleep_delay_timeout = 0; 
static uint32_t deep_sleep_wakeup_timer = 0;
static bool nwStandbyMode_gs = false;
GMainLoop *deepSleepMgr_Loop = NULL;
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

IARM_Result_t DeepSleepMgr_Start(int argc, char *argv[])
{
    char *settingsFile = NULL;
    if (argc == 2) settingsFile = argv[1];

    setvbuf(stdout, NULL, _IOLBF, 0);
    LOG("Entering [%s] - [%s] - disabling io redirect buf\r\n", __FUNCTION__,IARM_BUS_DEEPSLEEPMGR_NAME);
    
    /* Initialize SoC */
    PLAT_DS_INIT();

    /* Initialize with IARM Bus*/
    IARM_Bus_Init(IARM_BUS_DEEPSLEEPMGR_NAME);
    IARM_Bus_Connect();

   /*  Register for IARM events */
    IARM_Bus_RegisterEvent(IARM_BUS_PWRMGR_EVENT_MAX);
    IARM_Bus_RegisterEventHandler(IARM_BUS_PWRMGR_NAME,IARM_BUS_PWRMGR_EVENT_MODECHANGED,_eventHandler);
    IARM_Bus_RegisterEventHandler(IARM_BUS_PWRMGR_NAME,IARM_BUS_PWRMGR_EVENT_DEEPSLEEP_TIMEOUT,_eventHandler);
    IARM_Bus_RegisterCall(IARM_BUS_COMMON_API_DeepSleepWakeup,_DeepSleepWakeup);
    IARM_Bus_RegisterCall(IARM_BUS_DEEPSLEEPMGR_API_SetDeepSleepTimer, _SetDeepSleepTimer);
#ifdef ENABLE_DEEPSLEEP_FPLED_HANDLING
     try {
        device::Manager::Initialize();
    }
    catch (...){
        LOG("Exception Caught during [device::Manager::Initialize]\r\n");
    }
#endif    
    IARM_Bus_RegisterCall("GetDeepSleepStatus", _GetDeepSleepStatus);

    IARM_Bus_RegisterCall(IARM_BUS_DEEPSLEEPMGR_API_GetLastWakeupReason, _GetLastWakeupReason);
    IARM_Bus_RegisterCall(IARM_BUS_DEEPSLEEPMGR_API_GetLastWakeupKeyCode, _GetLastWakeupKeyCode);
    /* Main loop for Deep  Sleep Manager */
    deepSleepMgr_Loop = g_main_loop_new ( NULL , FALSE );
    if(deepSleepMgr_Loop != NULL){
        g_timeout_add_seconds (300 , heartbeatMsg , deepSleepMgr_Loop); 
    }
    else {
        LOG("Fails to Create a main Loop for [%s] \r\n",IARM_BUS_DEEPSLEEPMGR_NAME);
    }


   return IARM_RESULT_SUCCESS;
}

IARM_Result_t DeepSleepMgr_Loop()
{
    IARM_Bus_PWRMgr_GetPowerState_Param_t param;
   
   /*  Read the Device Power State on startup... */
    IARM_Bus_Call(IARM_BUS_PWRMGR_NAME, IARM_BUS_PWRMGR_API_GetPowerState, (void *)&param, sizeof(param));
    LOG("Deep Sleep Manager Init with Power State %d\r\n",param.curState);
    
    /* Deep Sleep Mgr loop */
    if(deepSleepMgr_Loop)
    { 
        g_main_loop_run (deepSleepMgr_Loop);
        g_main_loop_unref(deepSleepMgr_Loop);
    }

    return IARM_RESULT_SUCCESS;
}

IARM_Result_t DeepSleepMgr_Stop(void)
{
    if(deepSleepMgr_Loop)
    { 
        g_main_loop_quit(deepSleepMgr_Loop);
    }

    IARM_Bus_UnRegisterEventHandler(IARM_BUS_PWRMGR_NAME,IARM_BUS_PWRMGR_EVENT_MODECHANGED);
    IARM_Bus_UnRegisterEventHandler(IARM_BUS_PWRMGR_NAME,IARM_BUS_PWRMGR_EVENT_DEEPSLEEP_TIMEOUT);
    IARM_Bus_Disconnect();
    IARM_Bus_Term();
    PLAT_DS_TERM();

    return IARM_RESULT_SUCCESS;
}


static gboolean heartbeatMsg(gpointer data)
{
    time_t curr = 0;
    time(&curr);
    LOG("I-ARM Deep Sleep Mgr: HeartBeat at %s\r\n", ctime(&curr));
    return TRUE;
}


static void _eventHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len)
{
 
   //LOG("Event handler invoked :: \n");
    if (strcmp(owner,IARM_BUS_PWRMGR_NAME) == 0)  
    {
        switch (eventId)  
        {
            case IARM_BUS_PWRMGR_EVENT_DEEPSLEEP_TIMEOUT:
            {
#if !defined (_DISABLE_SCHD_REBOOT_AT_DEEPSLEEP)
                /*Scheduled maintanace reboot is disabled*/
                system("echo 0 > /opt/.rebootFlag");
                system(" echo `/bin/timestamp` ------------- Reboot timer expired while in Deep Sleep --------------- >> /opt/logs/receiver.log");
                system("sleep 5; /rebootNow.sh -s DeepSleepMgr -o 'Rebooting the box due to reboot timer expired while in Deep Sleep...'");
#endif /*End of _DISABLE_SCHD_REBOOT_AT_DEEPSLEEP*/
            }
            break;  
            case IARM_BUS_PWRMGR_EVENT_MODECHANGED:
            {
                IARM_Bus_PWRMgr_EventData_t *param = (IARM_Bus_PWRMgr_EventData_t *)data;
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
                    if (NULL != fpST)
                    {
                        if(0 > fscanf(fpST,"%d",&SleepTimeInSec))
                        {
                            __TIMESTAMP();LOG("Error: fscanf on SleepTimeInSec failed");
                        }
                        else
                        {
                            deep_sleep_delay_timeout = SleepTimeInSec ;
                            __TIMESTAMP();LOG(" /tmp/ override Deep Sleep Time is %d \r\n",deep_sleep_delay_timeout);
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
                    }
                    else{
                        LOG("Enter to Deep sleep Mode..stop Receiver with sleep 2 before DS \r\n");
                        system("sleep 2");
                        if ((stat("/lib/systemd/system/lxc.service", &buf) == 0) && (stat("/opt/lxc_service_disabled",&buf) !=0))
                        {
                            LOG("stopping lxc service\r\n");
                            system("systemctl stop lxc.service");
                            isLxcRestart = 1;
                        }
                        else
                        {
			   LOG("Update the Deepsleep marker to splunk.\n");
			   system("sh /lib/rdk/alertSystem.sh deepSleepMgrMain SYST_INFO_devicetoDS");

                        }
#ifdef ENABLE_DEEPSLEEP_FPLED_HANDLING
                        __TIMESTAMP();LOG("FrontPanelConfig::fPTerm\n");
                        device::FrontPanelConfig::getInstance().fPTerm();
#endif
                        int status = -1;
                        int retryCount = 0;
                        bool userWakeup = 0;
                        while(retryCount< 5)
                        {
                            LOG("Device entering Deep sleep Mode.. \r\n");
                            userWakeup = 0;
                            nwStandbyMode_gs = param->data.state.nwStandbyMode;
                            LOG("\nCalling PLAT_DS_SetDeepSleep with nwStandbyMode: %s\n",
                               nwStandbyMode_gs?("Enabled"):("Disabled"));
                            LOG("Device entered to Deep sleep Mode.. \r\n");
                            status = PLAT_DS_SetDeepSleep(deep_sleep_wakeup_timer,&userWakeup, nwStandbyMode_gs);

                            LOG("Device resumed from Deep sleep Mode.status :%d  \r\n",status);

                            if(status != 0)
                            {
                                sleep(5);
                                retryCount++;
                                if(retryCount >= 5)
                                {
                                    LOG("ERROR: Device failed to enter into Deep sleep Mode generate key event to transition to light sleep.. \r\n");
                                    IsDeviceInDeepSleep = DeepSleepStatus_Failed;
                                    IARM_Bus_IRMgr_EventData_t eventData;
                                    eventData.data.irkey.keyType = KET_KEYDOWN;
                                    eventData.data.irkey.keyCode = KED_DEEPSLEEP_WAKEUP;
                                    eventData.data.irkey.isFP = 0;
                                    eventData.data.irkey.keySrc = IARM_BUS_IRMGR_KEYSRC_IR;

                                    IARM_Bus_BroadcastEvent(IARM_BUS_IRMGR_NAME, (IARM_EventId_t) IARM_BUS_IRMGR_EVENT_IRKEY, (void *)&eventData, sizeof(eventData));
                                    eventData.data.irkey.keyType = KET_KEYUP;
                                    IARM_Bus_BroadcastEvent(IARM_BUS_IRMGR_NAME, (IARM_EventId_t) IARM_BUS_IRMGR_EVENT_IRKEY, (void *)&eventData, sizeof(eventData));
                                    return;
                                }
                            }
                            else
                            {
                                IsDeviceInDeepSleep = DeepSleepStatus_Completed;
                                break;
                            }
                        }
                        if (userWakeup)
                        {
                        /* Always send KED_DEEPSLEEP_WAKEUP when user action wakes the device from deep sleep. Previously this was sent
                           if we woke from a GPIO event, however there are cases where IR events aren't always passed when exiting
                           deep sleep resulting in the device not fully resuming. To resolve this we will ensure the WAKE event
                           is always sent here */
                            LOG("Resumed due to user action. Sending KED_DEEPSLEEP_WAKEUP. \r\n");
                            IARM_Bus_IRMgr_EventData_t eventData;
                            eventData.data.irkey.keyType = KET_KEYDOWN;
                            eventData.data.irkey.keyCode = KED_DEEPSLEEP_WAKEUP;
                            eventData.data.irkey.isFP = 0;
                            eventData.data.irkey.keySrc = IARM_BUS_IRMGR_KEYSRC_IR;

                            IARM_Bus_BroadcastEvent(IARM_BUS_IRMGR_NAME, (IARM_EventId_t) IARM_BUS_IRMGR_EVENT_IRKEY, (void *)&eventData, sizeof(eventData));
                            eventData.data.irkey.keyType = KET_KEYUP;
                            IARM_Bus_BroadcastEvent(IARM_BUS_IRMGR_NAME, (IARM_EventId_t) IARM_BUS_IRMGR_EVENT_IRKEY, (void *)&eventData, sizeof(eventData));
                        }
                        else {
                            LOG("Resumed without user action. Not sending KED_DEEPSLEEP_WAKEUP. \r\n");
                        }

#ifdef USE_WAKEUP_TIMER_EVT
                        DeepSleep_WakeupReason_t wakeupReason = DEEPSLEEP_WAKEUPREASON_UNKNOWN;
                        int reasonStatus = PLAT_DS_GetLastWakeupReason(&wakeupReason);
                        if (DEEPSLEEP_WAKEUPREASON_TIMER == wakeupReason){
                            LOG("Calling IARM_BUS_PWRMGR_API_handleDeepsleepTimeoutWakeup on wakeupReason:%d \n", wakeupReason);
                            IARM_Bus_Call(IARM_BUS_PWRMGR_NAME, IARM_BUS_PWRMGR_API_handleDeepsleepTimeoutWakeup, NULL, 0);
                        }
#endif //End of USE_WAKEUP_TIMER_EVT
                    }
                }
            }
            break;
        default:
            break;
        }
    }
}



static IARM_Result_t _DeepSleepWakeup(void *arg)
{
    IARM_Bus_CommonAPI_PowerPreChange_Param_t *param = (IARM_Bus_CommonAPI_PowerPreChange_Param_t *) arg;


    LOG("RPC IARM_BUS_COMMON_API_DeepSleepWakeup : State Changed %d -- > %d\r" , param->curState , param->newState);

    /* Support Deep sleep to Power ON, Light Sleep and Standby Transition. */
    if( (PWRMGR_POWERSTATE_STANDBY_DEEP_SLEEP == param->curState) &&
         (PWRMGR_POWERSTATE_STANDBY_DEEP_SLEEP != param->newState))
    {
        LOG("GOT EVENT TO EXIT FROM DEEP SLEEP \r\n");
#ifdef ENABLE_DEEPSLEEP_FPLED_HANDLING
        __TIMESTAMP();LOG("FrontPanelConfig::fPInit\n");
        device::FrontPanelConfig::getInstance().fPInit();
#endif
        /*Call Wake up API */
        PLAT_DS_DeepSleepWakeup();

        /* Remove the Event source  */
        if(dsleep_delay_event_src)
        {
            g_source_remove(dsleep_delay_event_src);
            dsleep_delay_event_src = 0;
        }

        if(IsDeviceInDeepSleep)
        {
            /*Restart Moca service when exit from Deep Sleep*/
            if (isLxcRestart)
            {
                LOG("Restarting Lxc Service After Waking up from Deep Sleep\r\n");
                system("systemctl restart lxc.service");
                isLxcRestart = 0;
            }
        }
        IsDeviceInDeepSleep = DeepSleepStatus_NotStarted;
    
        LOG("Device woke up from Deep sleep Mode.. \r\n");
    }
	return IARM_RESULT_SUCCESS;
}

static IARM_Result_t _GetDeepSleepStatus(void *arg)
{
    int *status = (int *)arg;
    *status = IsDeviceInDeepSleep;
    return IARM_RESULT_SUCCESS;
}


static IARM_Result_t _SetDeepSleepTimer(void *arg)
{
    DeepSleepMgr_SetDeepSleepTimer_Param_t *param = (DeepSleepMgr_SetDeepSleepTimer_Param_t *)arg;

    if(param != NULL)
    {  
        LOG("Deep sleep timer set to : %d Seconds \r\n", param->timeout);
        deep_sleep_delay_timeout = param->timeout;
        return IARM_RESULT_SUCCESS; 
    }
    return IARM_RESULT_IPCCORE_FAIL; 
}


static gboolean deep_sleep_delay_timer_fn(gpointer data)
{
    struct stat buf;
    int status = -1;
  
    LOG("Deep Sleep Timer Expires :Enter to Deep sleep Mode..stop Receiver with sleep 10 before DS \r\n");       
    system("sleep 10");

    if ((stat("/lib/systemd/system/lxc.service", &buf) == 0) && (stat("/opt/lxc_service_disabled",&buf) !=0))
    {
        system("systemctl stop lxc.service");
        isLxcRestart = 1;
    }
    bool userWakeup = 0;
    status = PLAT_DS_SetDeepSleep(deep_sleep_wakeup_timer,&userWakeup, false);
    if(status != 0)
    {
       LOG("deep_sleep_delay_timer_fn: Failed to enter deepsleep state \n");
    }
#ifdef USE_WAKEUP_TIMER_EVT
    //Call pwrmgr InvokeDeepsleepTimeout here
    DeepSleep_WakeupReason_t wakeupReason = DEEPSLEEP_WAKEUPREASON_UNKNOWN;
    int reasonStatus = PLAT_DS_GetLastWakeupReason(&wakeupReason);
    if (DEEPSLEEP_WAKEUPREASON_TIMER == wakeupReason){
        LOG("Calling IARM_BUS_PWRMGR_API_handleDeepsleepTimeoutWakeup on wakeupReason:%d \n", wakeupReason);
        IARM_Bus_Call(IARM_BUS_PWRMGR_NAME, IARM_BUS_PWRMGR_API_handleDeepsleepTimeoutWakeup, NULL, 0);
    }
#endif //End of USE_WAKEUP_TIMER_EVT
    return FALSE; // Send False so the handler should not be called again
}

static IARM_Result_t _GetLastWakeupReason(void *arg)
{
	DeepSleep_WakeupReason_t *wakeupReason = (DeepSleep_WakeupReason_t *)arg;
	int status = PLAT_DS_GetLastWakeupReason(wakeupReason);
	return IARM_RESULT_SUCCESS;
}

static IARM_Result_t _GetLastWakeupKeyCode(void *arg)
{
	DeepSleepMgr_WakeupKeyCode_Param_t *wakeupKeyCode = (DeepSleepMgr_WakeupKeyCode_Param_t *)arg;
	int status = PLAT_DS_GetLastWakeupKeyCode(wakeupKeyCode);
	return (IARM_Result_t)status;

}
/** @} */
/** @} */
