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
#include <sys/wait.h>
#include <time.h>

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

/* Secure system command wrapper to prevent injection attacks */
static int secure_system_command(char * const argv[]) {
    if (!argv || !argv[0]) {
        LOG("Error: Invalid command arguments\n");
        return -1;
    }
    
    pid_t pid = fork();
    if (pid == -1) {
        LOG("Error: fork() failed\n");
        return -1;
    } else if (pid == 0) {
        /* Child process */
        execv(argv[0], argv);
        _exit(127); /* exec failed */
    } else {
        /* Parent process */
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            LOG("Error: waitpid() failed\n");
            return -1;
        }
        return WEXITSTATUS(status);
    }
}

static int secure_file_write(const char* filename, const char* content) {
    if (!filename || !content) {
        LOG("Error: Invalid file write parameters\n");
        return -1;
    }
    
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        LOG("Error: Cannot open file %s for writing\n", filename);
        return -1;
    }
    
    ssize_t bytes_written = write(fd, content, strlen(content));
    close(fd);
    
    if (bytes_written == -1) {
        LOG("Error: Failed to write to file %s\n", filename);
        return -1;
    }
    
    return 0;
}

static int secure_service_control(const char* action, const char* service) {
    if (!action || !service) {
        LOG("Error: Invalid service control parameters\n");
        return -1;
    }
    
    /* Validate action to prevent injection */
    if (strcmp(action, "start") != 0 && strcmp(action, "stop") != 0 && strcmp(action, "restart") != 0) {
        LOG("Error: Invalid service action: %s\n", action);
        return -1;
    }
    
    /* Validate service name to prevent injection */
    if (strstr(service, "..") || strchr(service, '/') || strchr(service, ';') || strchr(service, '|')) {
        LOG("Error: Invalid service name: %s\n", service);
        return -1;
    }
    
    char *argv[] = {"/bin/systemctl", (char*)action, (char*)service, NULL};
    return secure_system_command(argv);
}

static void secure_sleep(unsigned int seconds) {
    if (seconds > 60) {  /* Sanity check */
        LOG("Warning: Sleep duration capped to 60 seconds\n");
        seconds = 60;
    }
    sleep(seconds);
}

static void secure_log_message(const char* message) {
    if (!message) {
        LOG("Error: Invalid log message\n");
        return;
    }
    
    /* Get current timestamp securely */
    time_t now;
    struct tm *timeinfo;
    char timestamp[64];
    
    time(&now);
    timeinfo = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
    
    /* Create the log entry */
    char log_entry[512];
    snprintf(log_entry, sizeof(log_entry), "%s %s\n", timestamp, message);
    
    /* Write to log file securely */
    int fd = open("/opt/logs/receiver.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd != -1) {
        write(fd, log_entry, strlen(log_entry));
        close(fd);
    } else {
        LOG("Error: Cannot write to receiver.log\n");
    }
}

/* Thread-safe accessors for shared state variables */
static void set_deep_sleep_status(DeepSleepStatus_t status) {
    pthread_mutex_lock(&deep_sleep_mutex);
    IsDeviceInDeepSleep = status;
    pthread_mutex_unlock(&deep_sleep_mutex);
}

static DeepSleepStatus_t get_deep_sleep_status(void) {
    pthread_mutex_lock(&deep_sleep_mutex);
    DeepSleepStatus_t status = IsDeviceInDeepSleep;
    pthread_mutex_unlock(&deep_sleep_mutex);
    return status;
}

static void set_deep_sleep_delay_timeout(uint32_t timeout) {
    pthread_mutex_lock(&deep_sleep_mutex);
    deep_sleep_delay_timeout = timeout;
    pthread_mutex_unlock(&deep_sleep_mutex);
}

static uint32_t get_deep_sleep_delay_timeout(void) {
    pthread_mutex_lock(&deep_sleep_mutex);
    uint32_t timeout = deep_sleep_delay_timeout;
    pthread_mutex_unlock(&deep_sleep_mutex);
    return timeout;
}

static void set_deep_sleep_wakeup_timer(uint32_t timer) {
    pthread_mutex_lock(&deep_sleep_mutex);
    deep_sleep_wakeup_timer = timer;
    pthread_mutex_unlock(&deep_sleep_mutex);
}

static uint32_t get_deep_sleep_wakeup_timer(void) {
    pthread_mutex_lock(&deep_sleep_mutex);
    uint32_t timer = deep_sleep_wakeup_timer;
    pthread_mutex_unlock(&deep_sleep_mutex);
    return timer;
}

static void set_nw_standby_mode(bool mode) {
    pthread_mutex_lock(&deep_sleep_mutex);
    nwStandbyMode_gs = mode;
    pthread_mutex_unlock(&deep_sleep_mutex);
}

static bool get_nw_standby_mode(void) {
    pthread_mutex_lock(&deep_sleep_mutex);
    bool mode = nwStandbyMode_gs;
    pthread_mutex_unlock(&deep_sleep_mutex);
    return mode;
}

static void set_lxc_restart_flag(gboolean flag) {
    pthread_mutex_lock(&deep_sleep_mutex);
    isLxcRestart = flag;
    pthread_mutex_unlock(&deep_sleep_mutex);
}

static gboolean get_lxc_restart_flag(void) {
    pthread_mutex_lock(&deep_sleep_mutex);
    gboolean flag = isLxcRestart;
    pthread_mutex_unlock(&deep_sleep_mutex);
    return flag;
}

static void set_dsleep_delay_event_src(guint src) {
    pthread_mutex_lock(&deep_sleep_mutex);
    dsleep_delay_event_src = src;
    pthread_mutex_unlock(&deep_sleep_mutex);
}

static guint get_dsleep_delay_event_src(void) {
    pthread_mutex_lock(&deep_sleep_mutex);
    guint src = dsleep_delay_event_src;
    pthread_mutex_unlock(&deep_sleep_mutex);
    return src;
}

/* Variables for Deep Sleep */
static pthread_mutex_t deep_sleep_mutex = PTHREAD_MUTEX_INITIALIZER;
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
    FILE* file = NULL;

    // Validate input parameter
    if (!file_name) {
        LOG("Error: Invalid file name parameter\n");
        return ret;
    }

    // Perform the access check
    if (access(file_name, F_OK) == 0) {
        file = fopen(file_name, "r");
        if (file != NULL) {
            // Check fscanf return value for exactly 1 successful conversion
            int scan_result = fscanf(file, "%d", &i);
            if (scan_result == 1) {
                ret = i;
            } else {
                __TIMESTAMP(); 
                LOG("Error: fscanf on read_tmp_integer_conf failed to read integer from %s\n", file_name);
            }
            // Ensure fclose is called in all paths
            if (fclose(file) != 0) {
                LOG("Warning: Failed to close file %s\n", file_name);
            }
            file = NULL; // Prevent double-close
        } else {
            __TIMESTAMP(); LOG("Error: Cannot open file %s\n", file_name);
        }
    } else {
        __TIMESTAMP(); LOG("Error: File %s does not exist\n", file_name);
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
        LOG("CRITICAL ERROR: Failed to create main loop for [%s] \r\n",IARM_BUS_DEEPSLEEPMGR_NAME);
        return IARM_RESULT_OOM; /* Return appropriate error */
    }


   return IARM_RESULT_SUCCESS;
}

IARM_Result_t DeepSleepMgr_Loop()
{
    IARM_Bus_PWRMgr_GetPowerState_Param_t param;
   
   /*  Read the Device Power State on startup... */
    IARM_Bus_Call(IARM_BUS_PWRMGR_NAME, IARM_BUS_PWRMGR_API_GetPowerState, (void *)&param, sizeof(param));
    LOG("Deep Sleep Manager Init with Power State %d\r\n",param.curState);
    
    /* Deep Sleep Mgr loop with null check */
    if(deepSleepMgr_Loop)
    { 
        g_main_loop_run (deepSleepMgr_Loop);
        g_main_loop_unref(deepSleepMgr_Loop);
        deepSleepMgr_Loop = NULL; /* Prevent accidental reuse */
    }
    else {
        LOG("ERROR: deepSleepMgr_Loop is NULL, cannot start main loop\n");
        return IARM_RESULT_INVALID_STATE;
    }

    return IARM_RESULT_SUCCESS;
}

IARM_Result_t DeepSleepMgr_Stop(void)
{
    if(deepSleepMgr_Loop)
    { 
        g_main_loop_quit(deepSleepMgr_Loop);
        /* Note: Loop will be unref'd in DeepSleepMgr_Loop() */
    }
    else {
        LOG("Warning: deepSleepMgr_Loop is NULL during stop\n");
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
    LOG("I-ARM Deep Sleep Mgr: HeartBeat ping.\r\n");
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
                if (secure_file_write("/opt/.rebootFlag", "0") != 0) {
                    LOG("Error: Failed to write reboot flag\n");
                }
                secure_log_message("------------- Reboot timer expired while in Deep Sleep ---------------");
                
                char *reboot_argv[] = {"/rebootNow.sh", "-s", "DeepSleepMgr", "-o", 
                    "Rebooting the box due to reboot timer expired while in Deep Sleep...", NULL};
                secure_sleep(5);
                if (secure_system_command(reboot_argv) != 0) {
                    LOG("Error: Failed to execute reboot command\n");
                }
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
                    set_deep_sleep_wakeup_timer(param->data.state.deep_sleep_timeout);
                    FILE *fpST = NULL;
                    uint32_t SleepTimeInSec = 0;
                    struct stat buf;
                    set_deep_sleep_status(DeepSleepStatus_InProgress);
                    /* Read the Delay Sleep Time  */
                    fpST = fopen("/tmp/deepSleepTimer","r");
                    if (NULL != fpST)
                    {
                        int scan_result = fscanf(fpST,"%d",&SleepTimeInSec);
                        if(scan_result != 1)
                        {
                            __TIMESTAMP();LOG("Error: fscanf on SleepTimeInSec failed - expected 1 conversion, got %d\n", scan_result);
                        }
                        else
                        {
                            set_deep_sleep_delay_timeout(SleepTimeInSec);
                            __TIMESTAMP();LOG(" /tmp/ override Deep Sleep Time is %d \r\n",get_deep_sleep_delay_timeout());
                        }
                        if (fclose(fpST) != 0) {
                            LOG("Warning: Failed to close deepSleepTimer file\n");
                        }
                        fpST = NULL; /* Prevent accidental reuse */
                    }

                    uint32_t tmp_deep_sleep_wakeup_timer = read_tmp_integer_conf("/tmp/deepSleepTimerVal");
                    if (tmp_deep_sleep_wakeup_timer){
                        set_deep_sleep_wakeup_timer(tmp_deep_sleep_wakeup_timer);
                        __TIMESTAMP();LOG(" /tmp/ override Deep Sleep wakeup time value is %d \r\n", get_deep_sleep_wakeup_timer());;
                    }

                    LOG("Deep Sleep wakeup time value is %d Secs.. \r\n",get_deep_sleep_wakeup_timer());
                    uint32_t current_delay_timeout = get_deep_sleep_delay_timeout();
                    if (current_delay_timeout) {
                        /* start a Deep sleep timer thread */
                         LOG("Schedule Deep SLeep After %d Sec.. \r\n",current_delay_timeout);
                         if (deepSleepMgr_Loop != NULL) {
                             guint new_src = g_timeout_add_seconds ((guint) current_delay_timeout,deep_sleep_delay_timer_fn,deepSleepMgr_Loop); 
                             set_dsleep_delay_event_src(new_src);
                         } else {
                             LOG("ERROR: deepSleepMgr_Loop is NULL, cannot schedule deep sleep timer\n");
                         }
                    }
                    else{
                        LOG("Enter to Deep sleep Mode..stop Receiver with sleep 2 before DS \r\n");
                        secure_sleep(2);
                        if ((stat("/lib/systemd/system/lxc.service", &buf) == 0) && (stat("/opt/lxc_service_disabled",&buf) !=0))
                        {
                            LOG("stopping lxc service\r\n");
                            if (secure_service_control("stop", "lxc.service") != 0) {
                                LOG("Error: Failed to stop lxc.service\n");
                            }
                            set_lxc_restart_flag(1);
                        }
                        else
                        {
			   LOG("Update the Deepsleep marker to splunk.\n");
			   char *alert_argv[] = {"/bin/sh", "/lib/rdk/alertSystem.sh", "deepSleepMgrMain", "SYST_INFO_devicetoDS", NULL};
			   if (secure_system_command(alert_argv) != 0) {
			       LOG("Error: Failed to execute alert system command\n");
			   }
                        }
#ifdef ENABLE_DEEPSLEEP_FPLED_HANDLING
                        __TIMESTAMP();LOG("FrontPanelConfig::fPTerm\n");
                        device::FrontPanelConfig::getInstance().fPTerm();
#endif
                        int status = -1;
                        int retryCount = 0;
                        bool userWakeup = 0;
                        time_t start_time = time(NULL);
                        const int MAX_RETRIES = 5;
                        const int MAX_TIMEOUT_SECONDS = 30;  /* Maximum total time for retries */
                        
                        while(retryCount < MAX_RETRIES)
                        {
                            /* Check for total timeout to prevent infinite loops */
                            time_t current_time = time(NULL);
                            if (difftime(current_time, start_time) > MAX_TIMEOUT_SECONDS) {
                                LOG("ERROR: Deep sleep retry timeout exceeded (%d seconds)\n", MAX_TIMEOUT_SECONDS);
                                set_deep_sleep_status(DeepSleepStatus_Failed);
                                break;
                            }
                            
                            LOG("Device entering Deep sleep Mode.. (retry %d/%d)\r\n", retryCount + 1, MAX_RETRIES);
                            userWakeup = 0;
                            set_nw_standby_mode(param->data.state.nwStandbyMode);
                            LOG("\nCalling PLAT_DS_SetDeepSleep with nwStandbyMode: %s\n",
                               get_nw_standby_mode()?("Enabled"):("Disabled"));
                            LOG("Device entered to Deep sleep Mode.. \r\n");
                            status = PLAT_DS_SetDeepSleep(get_deep_sleep_wakeup_timer(),&userWakeup, get_nw_standby_mode());

                            LOG("Device resumed from Deep sleep Mode.status :%d  \r\n",status);

                            if(status != 0)
                            {
                                /* Exponential backoff delay before retry */
                                unsigned int delay = 2 + (retryCount * 2);  /* 2, 4, 6, 8, 10 seconds */
                                LOG("Deep sleep failed, waiting %d seconds before retry %d\n", delay, retryCount + 1);
                                secure_sleep(delay);
                                retryCount++;
                                if(retryCount >= MAX_RETRIES)
                                {
                                    LOG("ERROR: Device failed to enter into Deep sleep Mode after %d retries, generate key event to transition to light sleep.. \r\n", MAX_RETRIES);
                                    set_deep_sleep_status(DeepSleepStatus_Failed);
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
                                set_deep_sleep_status(DeepSleepStatus_Completed);
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
        guint current_event_src = get_dsleep_delay_event_src();
        if(current_event_src)
        {
            g_source_remove(current_event_src);
            set_dsleep_delay_event_src(0);
        }

        if(get_deep_sleep_status())
        {
            /*Restart Moca service when exit from Deep Sleep*/
            if (get_lxc_restart_flag())
            {
                LOG("Restarting Lxc Service After Waking up from Deep Sleep\r\n");
                if (secure_service_control("restart", "lxc.service") != 0) {
                    LOG("Error: Failed to restart lxc.service\n");
                }
                set_lxc_restart_flag(0);
            }
        }
        set_deep_sleep_status(DeepSleepStatus_NotStarted);
    
        LOG("Device woke up from Deep sleep Mode.. \r\n");
    }
	return IARM_RESULT_SUCCESS;
}

static IARM_Result_t _GetDeepSleepStatus(void *arg)
{
    int *status = (int *)arg;
    *status = get_deep_sleep_status();
    return IARM_RESULT_SUCCESS;
}


static IARM_Result_t _SetDeepSleepTimer(void *arg)
{
    DeepSleepMgr_SetDeepSleepTimer_Param_t *param = (DeepSleepMgr_SetDeepSleepTimer_Param_t *)arg;

    if(param != NULL)
    {  
        LOG("Deep sleep timer set to : %d Seconds \r\n", param->timeout);
        set_deep_sleep_delay_timeout(param->timeout);
        return IARM_RESULT_SUCCESS; 
    }
    return IARM_RESULT_IPCCORE_FAIL; 
}


static gboolean deep_sleep_delay_timer_fn(gpointer data)
{
    struct stat buf;
    int status = -1;
  
    LOG("Deep Sleep Timer Expires :Enter to Deep sleep Mode..stop Receiver with sleep 10 before DS \r\n");       
    secure_sleep(10);

    if ((stat("/lib/systemd/system/lxc.service", &buf) == 0) && (stat("/opt/lxc_service_disabled",&buf) !=0))
    {
        if (secure_service_control("stop", "lxc.service") != 0) {
            LOG("Error: Failed to stop lxc.service in timer function\n");
        }
        set_lxc_restart_flag(1);
    }
    bool userWakeup = 0;
    status = PLAT_DS_SetDeepSleep(get_deep_sleep_wakeup_timer(),&userWakeup, false);
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
