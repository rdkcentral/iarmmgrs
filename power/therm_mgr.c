
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
* @defgroup power
* @{
**/
#ifdef ENABLE_THERMAL_PROTECTION
#include <glib.h>
#endif

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef ENABLE_THERMAL_PROTECTION
#include <sys/stat.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "pwrMgr.h"
#include "plat_power.h"
#include "therm_mon.h"
#include "pwrlogger.h"
#include "libIBus.h"
#include "rfcapi.h"

#define STANDBY_REASON_FILE       "/opt/standbyReason.txt"
#define THERMAL_PROTECTION_GROUP  "Thermal_Config"
#define THERMAL_SHUTDOWN_REASON   "THERMAL_SHUTDOWN"

#define RFC_ENABLE_ThermalProtection
#define RFC_DATA_ThermalProtection_POLL_INTERVAL

#define RFC_DATA_ThermalProtection_REBOOT_CRITICAL_THRESHOLD
#define RFC_DATA_ThermalProtection_REBOOT_CONCERN_THRESHOLD
#define RFC_DATA_ThermalProtection_REBOOT_GRACE_INTERVAL
#define RFC_DATA_ThermalProtection_REBOOT_SAFE_THRESHOLD

#define RFC_DATA_ThermalProtection_DEEPSLEEP_CRITICAL_THRESHOLD
#define RFC_DATA_ThermalProtection_DEEPSLEEP_CONCERN_THRESHOLD
#define RFC_DATA_ThermalProtection_DEEPSLEEP_GRACE_INTERVAL
#define RFC_DATA_ThermalProtection_DEEPSLEEP_SAFE_THRESHOLD

#define RFC_DATA_ThermalProtection_DECLOCK_CRITICAL_THRESHOLD
#define RFC_DATA_ThermalProtection_DECLOCK_CONCERN_THRESHOLD
#define RFC_DATA_ThermalProtection_DECLOCK_GRACE_INTERVAL
#define RFC_DATA_ThermalProtection_DECLOCK_SAFE_THRESHOLD

#ifndef MFR_TEMP_CLOCK_READ
/* Temperature (in celcus) at which box will ALWAYS be rebooted */
static int thermal_reboot_critical_threshold        = 120;
/* Temperature (in celcus) at which box will be rebooted after grace_interval has passed
   Timer is started 2 minutes late to give deepsleep logic a chance to work */
static int thermal_reboot_concern_threshold         = 112;
/* Temperature (in celcus) at which box is considered safe and will stop reboot consideration */
static int thermal_reboot_safe_threshold            = 110;
/* The amount of time (in seconds) that must pass after the temperature has reached 'thermal_reboot_concern_threshold'
    and has not fallen below 'thermal_reboot_safe_threshold' before the box will reboot

    ***NOTE: All temperature based reboot logic will be disabled if 'thermal_reboot_grace_interval' is set to 0 ***  */
static int thermal_reboot_grace_interval            = 600;


/* Temperature (in celcus) at which box will ALWAYS go to deep sleep */
static int thermal_deepsleep_critical_threshold     = 115;
/* Temperature (in celcus) at which box will go to deep sleep after grace_interval has passed */
static int thermal_deepsleep_concern_threshold      = 110;
/* Temperature (in celcus) at which box is considered safe and will stop deep sleep consideration */
static int thermal_deepsleep_safe_threshold         = 100;
/* The amount of time (in seconds) that must pass after the temperature has reached 'thermal_deepsleep_concern_threshold'
    and has not fallen below 'thermal_deepsleep_safe_threshold' before the box will go to deepsleep

    ***NOTE: All temperature based deep sleep logic will be disabled if 'thermal_deepsleep_grace_interval' is set to 0 ***   */
static int thermal_deepsleep_grace_interval         = 600;


/* Temperature (in celcus) at which box will ALWAYS be switched to the LOWEST clock mode */
static int thermal_declock_critical_threshold       = 110;
/* Temperature (in celcus) at which box will ALWAYS be switched to the middle clock mode */
static int thermal_declock_concern_threshold        = 100;
/* Temperature (in celcus) at which box will be switched back to highest clock mode after 'thermal_declock_grace_interval' has passed */
static int thermal_declock_safe_threshold           = 90;
/* The amount of time (in seconds) that must pass after to switch from a lower clock mode to a higher clock mode

    ***NOTE: All temperature based declock logic will be disabled if 'thermal_declock_grace_interval' is set to 0 ***   */
static int thermal_declock_grace_interval           = 60;


// the interval at which temperature will be polled from lower layers
static int thermal_poll_interval        = 30; //in seconds
// the interval after which reboot will happen if the temperature goes above reboot threshold
#else //MFR_TEMP_CLOCK_READ
#define REBOOT_CRITICAL    110
#define REBOOT_CONCERN     102
#define REBOOT_SAFE        100

#define DEEPSLEEP_CRITICAL     105
#define DEEPSLEEP_CONCERN      100
#define DEEPSLEEP_SAFE         90

#define DECLOCK_CRITICAL   100
#define DECLOCK_CONCERN    90
#define DECLOCK_SAFE       80

#define GRACE_INTERVAL     600
#define POLL_INTERVAL      30
#define DECLOCK_GRACE_INTERVAL 60

/* Temperature (in celcus) at which box will ALWAYS be rebooted */
static int thermal_reboot_critical_threshold        = REBOOT_CRITICAL;
/* Temperature (in celcus) at which box will be rebooted after grace_interval has passed
   Timer is started 2 minutes late to give deepsleep logic a chance to work */
static int thermal_reboot_concern_threshold         = REBOOT_CONCERN;
/* Temperature (in celcus) at which box is considered safe and will stop reboot consideration */
static int thermal_reboot_safe_threshold            = REBOOT_SAFE;
/* The amount of time (in seconds) that must pass after the temperature has reached 'thermal_reboot_concern_threshold'
    and has not fallen below 'thermal_reboot_safe_threshold' before the box will reboot

    ***NOTE: All temperature based reboot logic will be disabled if 'thermal_reboot_grace_interval' is set to 0 ***  */
static int thermal_reboot_grace_interval            = GRACE_INTERVAL;


/* Temperature (in celcus) at which box will ALWAYS go to deep sleep */
static int thermal_deepsleep_critical_threshold     = DEEPSLEEP_CRITICAL;
/* Temperature (in celcus) at which box will go to deep sleep after grace_interval has passed */
static int thermal_deepsleep_concern_threshold      = DEEPSLEEP_CONCERN;
/* Temperature (in celcus) at which box is considered safe and will stop deep sleep consideration */
static int thermal_deepsleep_safe_threshold         = DEEPSLEEP_SAFE;
/* The amount of time (in seconds) that must pass after the temperature has reached 'thermal_deepsleep_concern_threshold'
    and has not fallen below 'thermal_deepsleep_safe_threshold' before the box will go to deepsleep

    ***NOTE: All temperature based deep sleep logic will be disabled if 'thermal_deepsleep_grace_interval' is set to 0 ***   */
static int thermal_deepsleep_grace_interval         = GRACE_INTERVAL;


/* Temperature (in celcus) at which box will ALWAYS be switched to the LOWEST clock mode */
static int thermal_declock_critical_threshold       = DECLOCK_CRITICAL;
/* Temperature (in celcus) at which box will ALWAYS be switched to the middle clock mode */
static int thermal_declock_concern_threshold        = DECLOCK_CONCERN;
/* Temperature (in celcus) at which box will be switched back to highest clock mode after 'thermal_declock_grace_interval' has passed */
static int thermal_declock_safe_threshold           = DECLOCK_SAFE;
/* The amount of time (in seconds) that must pass after to switch from a lower clock mode to a higher clock mode

    ***NOTE: All temperature based declock logic will be disabled if 'thermal_declock_grace_interval' is set to 0 ***   */
static int thermal_declock_grace_interval           = DECLOCK_GRACE_INTERVAL;


// the interval at which temperature will be polled from lower layers
static int thermal_poll_interval        = POLL_INTERVAL; //in seconds
// the interval after which reboot will happen if the temperature goes above reboot threshold
 
#endif //MFR_TEMP_CLOCK_READ


//Did we already read config params once ?
static bool read_config_param           = FALSE;

// Is feature enabled ?
static bool isFeatureEnabled            = TRUE;
//Current temperature level
static volatile PWRMgr_ThermalState_t cur_Thermal_Level = PWRMGR_TEMPERATURE_NORMAL;
///Current temperature reading in celcius
static volatile int cur_Thermal_Value =0;
//Current CPU clocking mode.
static uint32_t cur_Cpu_Speed = 0;

// These are the clock rates that will actually be used when declocking. 0 is uninitialized and we'll attempt to auto discover
static uint32_t PLAT_CPU_SPEED_NORMAL = 0;
static uint32_t PLAT_CPU_SPEED_SCALED = 0;
static uint32_t PLAT_CPU_SPEED_MINIMAL = 0;

// Thread id of polling thread
static pthread_t thermalThreadId = NULL;

// RFC parameter storage
#define MAX_THERMAL_RFC 16
static char valueBuf[MAX_THERMAL_RFC];

//Functions used
/**
* Entry function for thread that monitor temperature change
*/
static void *_PollThermalLevels(void *);
/**
* Read configuration values, strat the thread
*/
void initializeThermalProtection();
/**
*  IARM call to return current thermal state
*/
static IARM_Result_t _GetThermalState(void *arg);
/**
* IARM call to set temperature thresholds
*/
static IARM_Result_t _SetTemperatureThresholds(void *arg);
/**
* IARM call to get temperature thresholds
*/
static IARM_Result_t _GetTemperatureThresholds(void *arg);
/**
* IARM call to set the grace interval
*/
static IARM_Result_t _SetOvertempGraceInterval(void *arg);
/**
* IARM call to get the grace interval
*/
static IARM_Result_t _GetOvertempGraceInterval(void *arg);
/**
* This function reads configuration details from relevant configuration files.
*/
static bool read_ConfigProps();
/**
* Check whether RFC based control is present, if yes create a glib style config file
*/
static bool updateRFCStatus();
/**
* @brief Is Thermal protection feature enabled
*
* @return TRUE f enabled, FALSE otherise
*/

static bool isThermalProtectionEnabled()
{
    if (!read_config_param)
    {
        if (updateRFCStatus())
        {
            read_ConfigProps();
        }

        read_config_param= TRUE;
    }

    return isFeatureEnabled;
}

void initializeThermalProtection()
{
    if (isThermalProtectionEnabled())
    {
        IARM_Bus_RegisterCall(IARM_BUS_PWRMGR_API_GetThermalState, _GetThermalState);
        IARM_Bus_RegisterCall(IARM_BUS_PWRMGR_API_SetTemperatureThresholds, _SetTemperatureThresholds);
        IARM_Bus_RegisterCall(IARM_BUS_PWRMGR_API_GetTemperatureThresholds, _GetTemperatureThresholds);
        IARM_Bus_RegisterCall(IARM_BUS_PWRMGR_API_SetOvertempGraceInterval, _SetOvertempGraceInterval);
        IARM_Bus_RegisterCall(IARM_BUS_PWRMGR_API_GetOvertempGraceInterval, _GetOvertempGraceInterval);

        LOG("[%s] Thermal Monitor [REBOOT] Enabled: %s\n", __FUNCTION__, (thermal_reboot_grace_interval > 0) ? "TRUE" : "FALSE");
        if (thermal_reboot_grace_interval > 0) {
            LOG("[%s] Thermal Monitor [REBOOT] Thresholds -- Critical:%d Concern:%d Safe:%d Grace Interval:%d\n", __FUNCTION__,
                thermal_reboot_critical_threshold, thermal_reboot_concern_threshold, thermal_reboot_safe_threshold, thermal_reboot_grace_interval);
        }

        LOG("[%s] Thermal Monitor [DEEP SLEEP] Enabled: %s\n", __FUNCTION__, (thermal_deepsleep_grace_interval > 0) ? "TRUE" : "FALSE");
        if (thermal_deepsleep_grace_interval > 0) {
            LOG("[%s] Thermal Monitor [DEEP SLEEP] Thresholds -- Critical:%d Concern:%d Safe:%d Grace Interval:%d\n", __FUNCTION__,
                thermal_deepsleep_critical_threshold, thermal_deepsleep_concern_threshold, thermal_deepsleep_safe_threshold, thermal_deepsleep_grace_interval);
        }

#ifndef DISABLE_DECLOCKING_LOGIC
        LOG("[%s] Thermal Monitor [DECLOCK] Enabled: %s\n", __FUNCTION__, (thermal_declock_grace_interval > 0) ? "TRUE" : "FALSE");
        if (thermal_declock_grace_interval > 0) {
            LOG("[%s] Thermal Monitor [DECLOCK] Thresholds -- Critical:%d Concern:%d Safe:%d Grace Interval:%d\n", __FUNCTION__,
                thermal_declock_critical_threshold, thermal_declock_concern_threshold, thermal_declock_safe_threshold, thermal_declock_grace_interval);
            /* Discover clock rate for this system. Only discover if the rate is 0 */
            PLAT_API_DetemineClockSpeeds(
                (PLAT_CPU_SPEED_NORMAL == 0 ) ? &PLAT_CPU_SPEED_NORMAL : NULL,
                (PLAT_CPU_SPEED_SCALED == 0 ) ? &PLAT_CPU_SPEED_SCALED : NULL,
                (PLAT_CPU_SPEED_MINIMAL == 0 ) ? &PLAT_CPU_SPEED_MINIMAL : NULL);
            LOG("[%s] Thermal Monitor [DECLOCK] Frequencies -- Normal:%d Scaled:%d Minimal:%d\n", __FUNCTION__, PLAT_CPU_SPEED_NORMAL, PLAT_CPU_SPEED_SCALED, PLAT_CPU_SPEED_MINIMAL);
            if (PLAT_CPU_SPEED_NORMAL == 0 || PLAT_CPU_SPEED_SCALED == 0 || PLAT_CPU_SPEED_MINIMAL == 0) {
                LOG("[%s] Thermal Monitor [DECLOCK] **ERROR** At least one clock speed is 0. Disabling declocking!\n", __FUNCTION__);
                thermal_declock_grace_interval = 0;
            }
            cur_Cpu_Speed = PLAT_CPU_SPEED_NORMAL;
            LOG("[%s] Thermal Monitor [DECLOCK] Default Frequency during bootup :  %d\n", __FUNCTION__, cur_Cpu_Speed);
        }
#endif

        if(!PLAT_API_SetTempThresholds(thermal_declock_concern_threshold, thermal_declock_critical_threshold))
        {
            LOG("*****Critical*** Fails to set temperature thresholds.. \n");
        }

        if (pthread_create(&thermalThreadId, NULL, _PollThermalLevels, NULL))
        {
            LOG("*****Critical*** Fails to Create temperature monitor thread \n");
        }
    }
    else
    {
        LOG("[%s] Thermal protection is disabled from RFC \n",__FUNCTION__);
    }
}

static void logThermalShutdownReason()
{
    //command is echo THERMAL_SHUTDOWN_REASON > STANDBY_REASON_FILE
    int cmdSize =  strlen(THERMAL_SHUTDOWN_REASON) + strlen(STANDBY_REASON_FILE) + 10;
    char logCommand[cmdSize]= {'0'};
    snprintf(logCommand,cmdSize,"echo %s > %s",THERMAL_SHUTDOWN_REASON, STANDBY_REASON_FILE);
    system(logCommand);
}

static void rebootIfNeeded()
{
    struct timeval tv;
    long difftime = 0;
    static struct timeval monitorTime;
    static bool rebootZone = false;

    if (thermal_reboot_grace_interval == 0) {
        /* This check is disable */
        return;
    }

    if (cur_Thermal_Value >= thermal_reboot_critical_threshold)
    {
        LOG("[%s] Rebooting is being forced!\n", __FUNCTION__);
        system("/rebootNow.sh -s Power_Thermmgr -o 'Rebooting the box due to stb temperature greater than thermal_reboot_critical_threshold...'");
    }
    else if (!rebootZone && cur_Thermal_Value >= thermal_reboot_concern_threshold)
    {
        LOG("[%s] Temperature threshold crossed (%d) ENTERING reboot zone\n", __FUNCTION__, cur_Thermal_Value );
        gettimeofday(&monitorTime, NULL);
        rebootZone = true;
    }
    else if (rebootZone && cur_Thermal_Value < thermal_reboot_safe_threshold) {
        LOG("[%s] Temperature threshold crossed (%d) EXITING reboot zone\n", __FUNCTION__, cur_Thermal_Value );
        rebootZone = false;
    }

    if (rebootZone) {
        /* We are in the deep sleep zone. After 'thermal_reboot_grace_interval' passes we will go to deep sleep */
        gettimeofday(&tv, NULL);
        difftime = tv.tv_sec - monitorTime.tv_sec;

        if (difftime >= thermal_reboot_grace_interval)
        {
            LOG("[%s]  - Rebooting since the temperature is still above critical level after %d seconds !! :  \n",
                __FUNCTION__,thermal_reboot_grace_interval);
            system("/rebootNow.sh -s Power_Thermmgr -o 'Rebooting the box as the stb temperature is still above critical level after 20 seconds...'");
        }
        else {
            LOG("[%s] Still in the reboot zone! Will go for reboot in %u seconds unless the temperature falls below %u!\n", __FUNCTION__, thermal_reboot_grace_interval-difftime, thermal_reboot_safe_threshold );
        }
    }
}


static void deepSleepIfNeeded()
{
    struct timeval tv;
    long difftime = 0;
    static struct timeval monitorTime;
    static bool deepSleepZone = false;

    if (thermal_deepsleep_grace_interval == 0) {
        /* This check is disable */
        return;
    }

    if (cur_Thermal_Value >= thermal_deepsleep_critical_threshold)
    {
        logThermalShutdownReason();
        LOG("[%s] Going to deepsleep since the temperature is above %d\n", __FUNCTION__, thermal_deepsleep_critical_threshold);
        system("/lib/rdk/alertSystem.sh pwrMgrMain \"Going to deepsleep due to temperature runaway\"");
        system("/SetPowerState DEEPSLEEP");
    }
    else if (!deepSleepZone && cur_Thermal_Value >= thermal_deepsleep_concern_threshold)
    {
        LOG("[%s] Temperature threshold crossed (%d) ENTERING deepsleep zone\n", __FUNCTION__, cur_Thermal_Value );
        gettimeofday(&monitorTime, NULL);
        deepSleepZone = true;
    }
    else if (deepSleepZone && cur_Thermal_Value < thermal_deepsleep_safe_threshold) {
        LOG("[%s] Temperature threshold crossed (%d) EXITING deepsleep zone\n", __FUNCTION__, cur_Thermal_Value );
        deepSleepZone = false;
    }

    if (deepSleepZone) {
        /* We are in the deep sleep zone. After 'thermal_deepsleep_grace_interval' passes we will go to deep sleep */
        gettimeofday(&tv, NULL);
        difftime = tv.tv_sec - monitorTime.tv_sec;

        if (difftime >= thermal_deepsleep_grace_interval)
        {
            logThermalShutdownReason();
            LOG("[%s] Going to deepsleep since the temperature reached %d and stayed above %d for %d seconds\n",
                __FUNCTION__, thermal_deepsleep_concern_threshold, thermal_deepsleep_safe_threshold, thermal_deepsleep_grace_interval);
            system("/lib/rdk/alertSystem.sh pwrMgrMain \"Going to deepsleep due to over temperature\"");
            system("/SetPowerState DEEPSLEEP");
        }
        else {
            LOG("[%s] Still in the deep sleep zone! Entering deep sleep in %u seconds unless the temperature falls below %u!\n", __FUNCTION__, thermal_deepsleep_grace_interval-difftime, thermal_deepsleep_safe_threshold );
        }
    }
}


static void declockIfNeeded()
{
#ifndef DISABLE_DECLOCKING_LOGIC
    struct timeval tv;
    long difftime = 0;
    static struct timeval monitorTime;

    if (thermal_declock_grace_interval == 0) {
        /* This check is disable */
        return;
    }

    if (cur_Thermal_Value >= thermal_declock_critical_threshold)
    {
        if (cur_Cpu_Speed != PLAT_CPU_SPEED_MINIMAL) {
            LOG("[%s] - Temperature threshold crossed (%d) !!!! Switching to minimal mode !!\n",
                __FUNCTION__, cur_Thermal_Value);
            PLAT_API_SetClockSpeed(PLAT_CPU_SPEED_MINIMAL);
            cur_Cpu_Speed = PLAT_CPU_SPEED_MINIMAL;
        }
        /* Always reset the monitor time */
        gettimeofday(&monitorTime, NULL);
    }
    else if (cur_Thermal_Value >= thermal_declock_concern_threshold)
    {
        if (cur_Cpu_Speed == PLAT_CPU_SPEED_NORMAL) {
            /* Switching from normal to scaled */
            LOG("[%s] - CPU Scaling threshold crossed (%d) !!!! Switching to scaled mode (%d) from normal mode(%d) !!\n",
                __FUNCTION__,cur_Thermal_Value,PLAT_CPU_SPEED_SCALED,PLAT_CPU_SPEED_NORMAL );
            PLAT_API_SetClockSpeed(PLAT_CPU_SPEED_SCALED);
            cur_Cpu_Speed = PLAT_CPU_SPEED_SCALED;
        }
        gettimeofday(&monitorTime, NULL);
    }
    else if (cur_Thermal_Value > thermal_declock_safe_threshold)
        //Between thermal_declock_concern_threshold and thermal_declock_safe_threshold
    {
        if (cur_Cpu_Speed == PLAT_CPU_SPEED_MINIMAL) {
            /* We are already declocked. If we stay in this state for 'thermal_declock_grace_interval' we will change to */
            gettimeofday(&tv, NULL);
            difftime = tv.tv_sec - monitorTime.tv_sec;

            if (difftime >= thermal_declock_grace_interval)
            {
                LOG("[%s] - CPU Scaling threshold crossed (%d) !!!! Switching to scaled mode (%d) from minimal mode(%d) !!\n",
                    __FUNCTION__,cur_Thermal_Value,PLAT_CPU_SPEED_SCALED,PLAT_CPU_SPEED_MINIMAL );
                PLAT_API_SetClockSpeed(PLAT_CPU_SPEED_SCALED);
                cur_Cpu_Speed = PLAT_CPU_SPEED_SCALED;
                gettimeofday(&monitorTime, NULL);
            }
        }
        else {
            /*Still in the correct mode. Always reset the monitor time */
            gettimeofday(&monitorTime, NULL);
        }
    }
    else //cur_Thermal_Value <= thermal_declock_safe_threshold
    {
        if (cur_Cpu_Speed != PLAT_CPU_SPEED_NORMAL) {
            /* We are in the declock zone. After 'thermal_declock_grace_interval' passes we will go back to normal mode */
            gettimeofday(&tv, NULL);
            difftime = tv.tv_sec - monitorTime.tv_sec;

            if (difftime >= thermal_declock_grace_interval)
            {
                LOG("[%s] - CPU rescaling threshold crossed (%d) !!!! Switching to normal mode !!\n",
                    __FUNCTION__,cur_Thermal_Value );
                PLAT_API_SetClockSpeed(PLAT_CPU_SPEED_NORMAL);
                cur_Cpu_Speed = PLAT_CPU_SPEED_NORMAL;
            }
        }

    }
#endif

}

//Thread entry function to monitor thermal levels of the device.
static void *_PollThermalLevels(void *)
{
    mfrTemperatureState_t state;
    float current_Temp = 0;
    float current_WifiTemp = 0;

    unsigned int pollCount = 0;
    int thermalLogInterval = 300/thermal_poll_interval;

    //PACEXI5-2127 //print current temperature levels every 15 mins
    int fifteenMinInterval = 900/thermal_poll_interval; //15 *60 seconds/interval


    LOG("Entering [%s] - [%s] - Start monitoring temeperature every %d seconds log interval: %d\n",
        __FUNCTION__, IARM_BUS_PWRMGR_NAME, thermal_poll_interval, thermalLogInterval);

    while(TRUE)
    {
        int result = PLAT_API_GetTemperature(&state, &current_Temp, &current_WifiTemp);//cur_Thermal_Level
        if(result)
        {
            if(cur_Thermal_Level != state)//State changed, need to broadcast
            {
                LOG("[%s]  - Temeperature level changed %d -> %d :  \n", __FUNCTION__,cur_Thermal_Level,state );
                //Broadcast this event
                IARM_Bus_PWRMgr_EventData_t _eventData;
                _eventData.data.therm.curLevel = (IARM_Bus_PWRMgr_ThermalState_t)cur_Thermal_Level;
                _eventData.data.therm.newLevel = (IARM_Bus_PWRMgr_ThermalState_t)state;
                _eventData.data.therm.curTemperature = current_Temp;

                IARM_Bus_BroadcastEvent( IARM_BUS_PWRMGR_NAME,
                                         IARM_BUS_PWRMGR_EVENT_THERMAL_MODECHANGED,(void *)&_eventData, sizeof(_eventData));

                cur_Thermal_Level = (PWRMgr_ThermalState_t)state;
            }
            //PACEXI5-2127 - BEGIN
            if(0 == (pollCount % fifteenMinInterval))
            {
                LOG("[%s]  - CURRENT_CPU_SCALE_MODE:%s \n", __FUNCTION__,
                    (cur_Cpu_Speed == PLAT_CPU_SPEED_NORMAL)?"Normal":
                    ((cur_Cpu_Speed == PLAT_CPU_SPEED_SCALED)?"Scaled":"Minimal"));
            }
            //PACEXI5-2127 - END
            if (0 == pollCount % thermalLogInterval)
            {
                LOG("[%s]  - Current Temperature %d\n", __FUNCTION__, (int)current_Temp );
            }
            cur_Thermal_Value = (int)current_Temp;

            /* Check if we should enter deepsleep based on the current temperature */
            deepSleepIfNeeded();

            /* Check if we should reboot based on the current temperature */
            rebootIfNeeded();

            /* Check if we should declock based on the current temperature */
            declockIfNeeded();
        }
        else
        {
            LOG("Warning [%s]  - Failed to retrieve temperature from OEM\n", __FUNCTION__);
        }
        sleep(thermal_poll_interval);
        pollCount++;
    }
}

static IARM_Result_t _GetThermalState(void *arg)
{
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;

    LOG("[PwrMgr] thermal state is queried: \n");
    if(NULL != arg)
    {
        IARM_Bus_PWRMgr_GetThermalState_Param_t *param = (IARM_Bus_PWRMgr_GetThermalState_Param_t *)arg;
        param->curLevel = (IARM_Bus_PWRMgr_ThermalState_t)cur_Thermal_Level;
        param->curTemperature = cur_Thermal_Value;
        LOG("[PwrMgr] thermal state is queried: returning %d \n", cur_Thermal_Value);
    }
    else
    {
        retCode = IARM_RESULT_INVALID_PARAM;
    }
    return retCode;
}

static IARM_Result_t _GetTemperatureThresholds(void *arg)
{
    IARM_Result_t retCode = IARM_RESULT_IPCCORE_FAIL;
    int result = 0;

    if(NULL != arg)
    {
        IARM_Bus_PWRMgr_GetTempThresholds_Param_t * param = (IARM_Bus_PWRMgr_GetTempThresholds_Param_t *) arg;
        float high, critical;
        result = PLAT_API_GetTempThresholds(&high,&critical);
        if ( result )
        {
            retCode = IARM_RESULT_SUCCESS;
            param->tempHigh = high;
            param->tempCritical = critical;
            LOG("[PwrMgr] Current thermal threshold : %f , %f \n", param->tempHigh,param->tempCritical);
        }
    }
    else
    {
        retCode = IARM_RESULT_INVALID_PARAM;
    }
    return retCode;
}

static IARM_Result_t _SetTemperatureThresholds(void *arg)
{
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    int result = 0;
    if(NULL != arg)
    {
        IARM_Bus_PWRMgr_SetTempThresholds_Param_t * param = (IARM_Bus_PWRMgr_SetTempThresholds_Param_t *) arg;
        LOG("[PwrMgr] Setting thermal threshold : %f , %f \n", param->tempHigh,param->tempCritical);  //CID:127982 ,127475,103705 - Print_args
        result = PLAT_API_SetTempThresholds(param->tempHigh,param->tempCritical);
        retCode = result?IARM_RESULT_SUCCESS:IARM_RESULT_IPCCORE_FAIL;
    }
    else
    {
        retCode = IARM_RESULT_INVALID_PARAM;
    }
    return retCode;

}

static IARM_Result_t _GetOvertempGraceInterval(void *arg)
{
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;

    if(NULL != arg)
    {
        IARM_Bus_PWRMgr_GetOvertempGraceInterval_Param_t * param = (IARM_Bus_PWRMgr_GetOvertempGraceInterval_Param_t *) arg;
        {
            param->graceInterval = thermal_reboot_grace_interval;
            retCode = IARM_RESULT_SUCCESS;
            LOG("[PwrMgr] Current over temparature grace interval : %d\n", param->graceInterval);
        }
    }
    else
    {
        retCode = IARM_RESULT_INVALID_PARAM;
    }
    return retCode;
}

static IARM_Result_t _SetOvertempGraceInterval(void *arg)
{
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    if(NULL != arg)
    {
        IARM_Bus_PWRMgr_SetOvertempGraceInterval_Param_t * param = (IARM_Bus_PWRMgr_SetOvertempGraceInterval_Param_t *) arg;
        if(param->graceInterval >= 0 )
        {
            LOG("[PwrMgr] Setting over temparature grace interval : %d\n", param->graceInterval);
            thermal_reboot_grace_interval = param->graceInterval;
            thermal_deepsleep_grace_interval = param->graceInterval;
            retCode = IARM_RESULT_SUCCESS;
        }
        else
        {
            retCode = IARM_RESULT_INVALID_PARAM;
        }
    }
    else
    {
        retCode = IARM_RESULT_INVALID_PARAM;
    }
    return retCode;
}

static bool updateRFCStatus()
{
    bool result = false;
    RFC_ParamData_t param;

    isFeatureEnabled = TRUE;

    WDMP_STATUS status = getRFCParameter(THERMAL_PROTECTION_GROUP, "RFC_ENABLE_ThermalProtection", &param);

    if (status == WDMP_SUCCESS)
    {
        LOG("[%s:%d] Key: RFC_ENABLE_ThermalProtection,Value %s  \n", __FUNCTION__, __LINE__, param.value);
        if (0 == strncasecmp(param.value, "false",5))
        {
            isFeatureEnabled = FALSE;
        }
        else
        {
            result = true;
        }
    }
    else
    {
        LOG("[%s:%d] Key: RFC_ENABLE_ThermalProtection is not configured, Status %d  \n", __FUNCTION__, __LINE__, status);
    }

    return result;
}

static char * read_ConfigProperty(const char* key)
{
    char *value = NULL;
    int dataLen;
    RFC_ParamData_t param;


    WDMP_STATUS status = getRFCParameter(THERMAL_PROTECTION_GROUP, key, &param);

    valueBuf[0] = '\0';

    if (status == WDMP_SUCCESS)
    {
        dataLen = strlen(param.value);
        if (dataLen > MAX_THERMAL_RFC-1)
        {
            dataLen = MAX_THERMAL_RFC-1;
        }

        if ( (param.value[0] == '"') && (param.value[dataLen-1] == '"'))
        {
            // remove quotes arround data
            strncpy (valueBuf, &param.value[1], dataLen-2);
            valueBuf[dataLen-2] = '\0';
        }
        else
        {
            strncpy (valueBuf, param.value, MAX_THERMAL_RFC-1);
            valueBuf[MAX_THERMAL_RFC-1] = '\0';
        }

        LOG("name = %s, type = %d, value = %s, status = %d\n", param.name, param.type, param.value, status);
    }
    else
    {
        LOG("[%s:%d] Key: property %s is not configured, Status %d  \n", __FUNCTION__, __LINE__, key, status);
    }


    if (valueBuf[0])
    {
        value = valueBuf;
    }
    else
    {
        LOG("[%s:%d] Unable to find key %s in group %s  \n", __FUNCTION__, __LINE__, key, THERMAL_PROTECTION_GROUP);
    }

    return value;
}

static bool read_ConfigProps()
{
    char *value = NULL;

    //Now override with RFC values if any
    value = read_ConfigProperty("RFC_DATA_ThermalProtection_REBOOT_CRITICAL_THRESHOLD");
    if (NULL != value)
    {
        thermal_reboot_critical_threshold = atoi(value);
    }

    value = read_ConfigProperty("RFC_DATA_ThermalProtection_REBOOT_CONCERN_THRESHOLD");
    if (NULL != value)
    {
        thermal_reboot_concern_threshold = atoi(value);
    }

    value = read_ConfigProperty("RFC_DATA_ThermalProtection_REBOOT_SAFE_THRESHOLD");
    if (NULL != value)
    {
        thermal_reboot_safe_threshold = atoi(value);
    }

    value = read_ConfigProperty("RFC_DATA_ThermalProtection_REBOOT_GRACE_INTERVAL");
    if (NULL != value)
    {
        thermal_reboot_grace_interval = atoi(value);
    }


    value = read_ConfigProperty("RFC_DATA_ThermalProtection_DECLOCK_CRITICAL_THRESHOLD");
    if (NULL != value)
    {
        thermal_declock_critical_threshold = atoi(value);
    }

    value = read_ConfigProperty("RFC_DATA_ThermalProtection_DECLOCK_CONCERN_THRESHOLD");
    if (NULL != value)
    {
        thermal_declock_concern_threshold = atoi(value);
    }

    value = read_ConfigProperty("RFC_DATA_ThermalProtection_DECLOCK_SAFE_THRESHOLD");
    if (NULL != value)
    {
        thermal_declock_safe_threshold = atoi(value);
    }

    value = read_ConfigProperty("RFC_DATA_ThermalProtection_DECLOCK_GRACE_INTERVAL");
    if (NULL != value)
    {
        thermal_declock_grace_interval = atoi(value);
    }

    value = read_ConfigProperty("RFC_DATA_ThermalProtection_DEEPSLEEP_CRITICAL_THRESHOLD");
    if (NULL != value)
    {
        thermal_deepsleep_critical_threshold =atoi(value);
    }

    value = read_ConfigProperty("RFC_DATA_ThermalProtection_DEEPSLEEP_CONCERN_THRESHOLD");
    if (NULL != value)
    {
        thermal_deepsleep_concern_threshold = atoi(value);
    }

    value = read_ConfigProperty("RFC_DATA_ThermalProtection_DEEPSLEEP_SAFE_THRESHOLD");
    if (NULL != value)
    {
        thermal_deepsleep_safe_threshold = atoi(value);
    }

    value = read_ConfigProperty("RFC_DATA_ThermalProtection_DEEPSLEEP_GRACE_INTERVAL");
    if (NULL != value)
    {
        thermal_deepsleep_grace_interval = atoi(value);
    }


    value = read_ConfigProperty("RFC_DATA_ThermalProtection_POLL_INTERVAL");
    if (NULL != value)
    {
        thermal_poll_interval = atoi(value);
    }

    value = read_ConfigProperty("RFC_DATA_ThermalProtection_PLAT_CPU_SPEED_NORMAL");
    if (NULL != value)
    {
        PLAT_CPU_SPEED_NORMAL =atoi(value);
    }

    value = read_ConfigProperty("RFC_DATA_ThermalProtection_PLAT_CPU_SPEED_SCALED");
    if (NULL != value)
    {
        PLAT_CPU_SPEED_SCALED =atoi(value);
    }

    value = read_ConfigProperty("RFC_DATA_ThermalProtection_PLAT_CPU_SPEED_MINIMAL");
    if (NULL != value)
    {
        PLAT_CPU_SPEED_MINIMAL =atoi(value);
    }

    return true;
}

#endif//ENABLE_THERMAL_PROTECTION

#ifdef __cplusplus
}
#endif

/** @} */
/** @} */
