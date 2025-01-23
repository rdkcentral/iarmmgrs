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
* @defgroup dsmgr
* @{
**/


#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "dsRpc.h"
#include "dsMgrPwrEventListener.h"
#include "frontPanelIndicator.hpp"
#include "videoOutputPort.hpp"
#include "host.hpp"
#include "exception.hpp"
#include "manager.hpp"
#include "dsMgrProductTraitsHandler.h"
#include "dsAudioSettings.h"
#include "plat_power.h"

#define PWRMGR_REBOOT_REASON_MAINTENANCE "MAINTENANCE_REBOOT"
#define MAX_NUM_VIDEO_PORTS 5

typedef struct{
    char port[DSMGR_MAX_VIDEO_PORT_NAME_LENGTH];
    bool isEnabled;
}DSMgr_Standby_Video_State_t;

extern IARM_Result_t _dsGetAudioPort(void *arg);
extern IARM_Result_t _dsEnableAudioPort(void *arg);
extern IARM_Result_t _dsSetFPState(void *arg);
extern IARM_Result_t _dsGetEnablePersist(void *arg);
extern void _setEASAudioMode();

static DSMgr_Standby_Video_State_t g_standby_video_port_setting[MAX_NUM_VIDEO_PORTS];
static dsMgrProductTraits::ux_controller * ux = nullptr;
static pthread_mutex_t dsLock = PTHREAD_MUTEX_INITIALIZER;

static bool get_video_port_standby_setting(const char * port);
static void _PwrEventHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len);
static int _SetLEDStatus(PWRMgr_PowerState_t powerState);
int _SetAVPortsPowerState(PWRMgr_PowerState_t powerState);
static IARM_Result_t _SetStandbyVideoState(void *arg);
static IARM_Result_t _GetStandbyVideoState(void *arg);
static IARM_Result_t _SetAvPortState(void *arg);
static IARM_Result_t _SetLEDState(void *arg);
static IARM_Result_t _SetRebootConfig(void *arg);

static PWRMgr_PowerState_t curState = PWRMGR_POWERSTATE_OFF;

#define IARM_BUS_Lock(lock) pthread_mutex_lock(&dsLock)
#define IARM_BUS_Unlock(lock) pthread_mutex_unlock(&dsLock)

#define RDK_PROFILE "RDK_PROFILE"
#define PROFILE_STR_TV "TV"
#define PROFILE_STR_STB "STB"

typedef enum profile {
    PROFILE_INVALID = -1,
    PROFILE_STB = 0,
    PROFILE_TV,
    PROFILE_MAX
}profile_t;

profile_t profileType = PROFILE_INVALID;

profile_t searchRdkProfile(void) {
    INT_DEBUG("Entering [%s]\r\n", __FUNCTION__);
    const char* devPropPath = "/etc/device.properties";
    char line[256], *rdkProfile = NULL;
    profile_t ret = PROFILE_INVALID;
    FILE* file;

    file = fopen(devPropPath, "r");
    if (file == NULL) {
        INT_ERROR("[%s]: File not found.\n", __FUNCTION__);
        return PROFILE_INVALID;
    }

    while (fgets(line, sizeof(line), file)) {
        rdkProfile = strstr(line, RDK_PROFILE);
        if (rdkProfile != NULL) {
            INT_DEBUG("[%s]: Found RDK_PROFILE\r\n", __FUNCTION__);
            break;
        }
    }
    
    if(rdkProfile != NULL)
    {
        rdkProfile += strlen(RDK_PROFILE);
        rdkProfile++; // Move past the '=' character
        if(0 == strncmp(rdkProfile, PROFILE_STR_TV, strlen(PROFILE_STR_TV)))
        {
            ret = PROFILE_TV;
            INT_DEBUG("[%s]: Found RDK_PROFILE is TV", __FUNCTION__);
        }
        else if (0 == strncmp(rdkProfile, PROFILE_STR_STB, strlen(PROFILE_STR_STB)))
        {
            ret = PROFILE_STB;
            INT_DEBUG("[%s]: Found RDK_PROFILE is STB", __FUNCTION__);
        }
    }
    else
    {
        INT_ERROR("[%s]: NOT FOUND RDK_PROFILE in device properties file\r\n", __FUNCTION__);
        ret = PROFILE_INVALID;
    }

    fclose(file);
    INT_INFO("Exit [%s]: RDK_PROFILE = %d\r\n", __FUNCTION__, ret);
    return ret;
}

void initPwrEventListner()
{
    INT_INFO("Entering [%s]\r\n", __FUNCTION__);
    IARM_Bus_PWRMgr_GetPowerState_Param_t param;
    PWRMgr_PowerState_t beforeRebootPowerState = PWRMGR_POWERSTATE_STANDBY;
    
    profileType = searchRdkProfile();

    if (PROFILE_TV == profileType )
    {
        if(true == dsMgrProductTraits::ux_controller::initialize_ux_controller(dsMgrProductTraits::DEFAULT_TV_PROFILE))
        {
            ux = dsMgrProductTraits::ux_controller::get_instance();
        }
    }
    else
    {
        if(true == dsMgrProductTraits::ux_controller::initialize_ux_controller(dsMgrProductTraits::DEFAULT_STB_PROFILE_EUROPE))
        {
            ux = dsMgrProductTraits::ux_controller::get_instance();
        }
    }
    if(nullptr == ux)
    {
        INT_DEBUG("dsmgr product traits not supported.\n");
    }

    try {
        device::Manager::load();
        INT_DEBUG("device::Manager::load success.\n");
    }
    catch (...){
        INT_DEBUG("Exception Caught during [device::Manager::load]\r\n");
    }   
    IARM_Bus_RegisterEventHandler(IARM_BUS_PWRMGR_NAME,IARM_BUS_PWRMGR_EVENT_MODECHANGED,_PwrEventHandler);
    IARM_Bus_RegisterCall(IARM_BUS_DSMGR_API_SetStandbyVideoState, _SetStandbyVideoState);
    IARM_Bus_RegisterCall(IARM_BUS_DSMGR_API_GetStandbyVideoState, _GetStandbyVideoState);
    IARM_Bus_RegisterCall(IARM_BUS_DSMGR_API_SetAvPortState, _SetAvPortState);
    IARM_Bus_RegisterCall(IARM_BUS_DSMGR_API_SetLEDStatus, _SetLEDState);
    IARM_Bus_RegisterCall(IARM_BUS_DSMGR_API_SetRebootConfig, _SetRebootConfig);
   /*  Read the Device Power State on startup... */
    IARM_Result_t ret = IARM_Bus_Call(IARM_BUS_PWRMGR_NAME, IARM_BUS_PWRMGR_API_GetPowerState, (void *)&param, sizeof(param));
    if(ret == IARM_RESULT_SUCCESS)
    {
        INT_DEBUG("Deep Sleep Manager Init with Power State %d\r\n",param.curState);
        curState = (PWRMgr_PowerState_t)param.curState;
    }
    IARM_Bus_PWRMgr_GetPowerStateBeforeReboot_Param_t preParam;
    IARM_Result_t res = IARM_Bus_Call(IARM_BUS_PWRMGR_NAME,
                              IARM_BUS_PWRMGR_API_GetPowerStateBeforeReboot, (void *)&preParam,
                              sizeof(preParam));

    if(ret == IARM_RESULT_SUCCESS)
    {
        INT_DEBUG("Deep Sleep Manager Init with Previous  Power State before reboot %s\r\n",preParam.powerStateBeforeReboot);
        if (0==strncmp("ON", preParam.powerStateBeforeReboot, 2)){
            beforeRebootPowerState = PWRMGR_POWERSTATE_ON;
        }
    }

    if(nullptr != ux)
                ux->applyPostRebootConfig(curState, beforeRebootPowerState); // This will set up ports, lights and bootloader pattern internally.
    else
    {
        //Sync Port with Power state TODO:
    }
    if(nullptr == ux) // Since ux_controller is not supported, ports need to be set up explicitly.
    {
        _SetLEDStatus(curState);
        _SetAVPortsPowerState(curState);
    }    
}


static void _PwrEventHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len)
{
    INT_INFO("Entering [%s]\r\n", __FUNCTION__);
    PWRMgr_PowerState_t newState;
    PWRMgr_PowerState_t curState;
    
    /*Handle only Sys Manager Events */
    if (strcmp(owner, IARM_BUS_PWRMGR_NAME)  == 0) 
    {
        /* Only handle state events */
        switch (eventId) {
            case IARM_BUS_PWRMGR_EVENT_MODECHANGED:
                {

                    IARM_Bus_PWRMgr_EventData_t *eventData = (IARM_Bus_PWRMgr_EventData_t *)data;
                       
                    INT_DEBUG("[%s] Got MODCHANGED Event from %d to %d  \r\n",__FUNCTION__, eventData->data.state.curState, eventData->data.state.newState);
                    newState = (PWRMgr_PowerState_t)eventData->data.state.newState;     
                    curState = (PWRMgr_PowerState_t)eventData->data.state.curState;     
                    if (nullptr != ux) //If ux_controller is supported, it will set up AV ports and LEDs in the below call.
                        ux->applyPowerStateChangeConfig(newState, curState);
                    else
                    {
                        _SetLEDStatus(newState);
                        _SetAVPortsPowerState(newState);
                    }
                }
                break;
            default:
                break;
            }
    }
}

static int _SetLEDStatus(PWRMgr_PowerState_t powerState)
{
    INT_INFO("Entering [%s]\r\n", __FUNCTION__);
    try {
        
        dsFPDStateParam_t param;

        param.eIndicator = dsFPD_INDICATOR_POWER;

        if( powerState != PWRMGR_POWERSTATE_ON )
        {
            if(PROFILE_TV == profileType)
            {
                param.state = dsFPD_STATE_ON;
                INT_DEBUG("[PWRMgr-_SetLEDStatus] Settings the Power LED State to ON\r\n");
            }
            else
            {
                param.state = dsFPD_STATE_OFF;
                INT_DEBUG("[PWRMgr-_SetLEDStatus] Settings the Power LED State to OFF \r\n");
            }
        }
        else
        {
            param.state = dsFPD_STATE_ON;
            INT_DEBUG("[PWRMgr-_SetLEDStatus] Settings the Power LED State to ON \r\n");
        }

        _dsSetFPState(&param);
    }
    catch (...) {
        INT_DEBUG("Exception Caught during [PWRMgr - _SetLEDStatus]\r\n");
        return 0;
    }
    return 0;
}

int _SetAVPortsPowerState(PWRMgr_PowerState_t powerState)
{
    INT_INFO("Entering [%s] powerState:%d \r\n", __FUNCTION__,powerState);

    try
    {
        if (powerState != PWRMGR_POWERSTATE_ON)
        {
            if (PWRMGR_POWERSTATE_OFF != powerState)
            {
                INT_INFO("[%s] POWERSTATE %d \r\n", __FUNCTION__, powerState);
                // We're in one of the standby modes. Certain ports may have to be left on.
                try
                {
                    device::List<device::VideoOutputPort> videoPorts = device::Host::getInstance().getVideoOutputPorts();
                    for (size_t i = 0; i < videoPorts.size(); i++)
                    {
                        bool doEnable = get_video_port_standby_setting(videoPorts.at(i).getName().c_str());
                        INT_DEBUG("[%s]Video port %s will be %s in standby mode.\n", __FUNCTION__, videoPorts.at(i).getName().c_str(), (doEnable ? "enabled" : "disabled"));
                        if (false == doEnable)
                            videoPorts.at(i).disable();
                    }
                }
                catch (...)
                {
                    INT_DEBUG("[%s] Video port exception %d \r\n", __FUNCTION__, powerState);
                }
                INT_INFO("[%s] Video port standby done \r\n",__FUNCTION__);
            }
            else
            {
                try
                {
                    // Disable all ports when going into POWERSTATE_OFF
                    device::List<device::VideoOutputPort> videoPorts = device::Host::getInstance().getVideoOutputPorts();
                    for (size_t i = 0; i < videoPorts.size(); i++)
                    {
                        videoPorts.at(i).disable();
                    }
                }
                catch (...)
                {
                    INT_DEBUG("[%s] Video port exception %d \r\n", __FUNCTION__, powerState);
                }
                INT_INFO("[%s] Video port disable done \r\n", __FUNCTION__ );
            }
            try
            {
                dsAudioGetHandleParam_t getHandle;
                dsAudioPortEnabledParam_t setMode;
                int numPorts, i = 0;

                numPorts = dsUTL_DIM(kSupportedPortTypes);
                for (i = 0; i < numPorts; i++)
                {
                    const dsAudioPortType_t *audioPort = &kSupportedPortTypes[i];
                    memset(&getHandle, 0, sizeof(getHandle));
                    getHandle.type = *audioPort;
                    getHandle.index = 0;
                    _dsGetAudioPort(&getHandle);

                    memset(&setMode, 0, sizeof(setMode));
                    setMode.handle = getHandle.handle;
                    setMode.enabled = false;
                    _dsEnableAudioPort(&setMode);
                }
            }
            catch (...)
            {
                INT_DEBUG("[%s] audio port exception \r\n", __FUNCTION__);
            }
        }
        else
        {

            try
            {
                device::List<device::VideoOutputPort> videoPorts = device::Host::getInstance().getVideoOutputPorts();
                for (size_t i = 0; i < videoPorts.size(); i++)
                {
                    videoPorts.at(i).enable();
                }
        
                dsAudioGetHandleParam_t getHandle;
                dsAudioPortEnabledParam_t setMode;

                int numPorts, i = 0;

                numPorts = dsUTL_DIM(kSupportedPortTypes);
                for (i = 0; i < numPorts; i++)
                {
                    const dsAudioPortType_t *audioPort = &kSupportedPortTypes[i];
                    memset(&getHandle, 0, sizeof(getHandle));
                    getHandle.type = *audioPort;
                    getHandle.index = 0;
                    _dsGetAudioPort(&getHandle);

                    memset(&setMode, 0, sizeof(setMode));
                    setMode.handle = getHandle.handle;
                    setMode.enabled = false;
                    _dsGetEnablePersist(&setMode);

                    if (setMode.enabled == true)
                    {
                        /*Instead of enabling all the audio ports on power transition */
                        /*Get the values from persistent storage & update */
                        INT_INFO("[%s] Enabling audio ports %d \r\n", __FUNCTION__, powerState);
                        _dsEnableAudioPort(&setMode);
                    }
                }
                if (isEAS == IARM_BUS_SYS_MODE_EAS)
                {
                    /* Force Stereo in EAS mode. */
                    INT_INFO("[%s] Force Stereo in EAS mode \r\n", __FUNCTION__);
                    _setEASAudioMode();
                }
            }
            catch (...)
            {
                INT_DEBUG("[%s] Audio port exception \r\n",__FUNCTION__);
            }

        }
    }
    catch (...)
    {
        INT_DEBUG("Exception Caught during [%s]\r\n", __FUNCTION__);
        return 0;
    }
    return 0;
}

static bool get_video_port_standby_setting(const char * port)
{
    INT_INFO("Entering [%s]\r\n", __FUNCTION__);
    if(NULL == port)
    {
        INT_DEBUG("[%s]Error! Port name is NULL!\n", __FUNCTION__);
        return false;
    }
    for(int i = 0; i < MAX_NUM_VIDEO_PORTS; i++)
    {
        if(0 == strncasecmp(port, g_standby_video_port_setting[i].port, DSMGR_MAX_VIDEO_PORT_NAME_LENGTH))
        {
            return g_standby_video_port_setting[i].isEnabled;
        }
    }
    return false; //Default setting: video port is disabled in standby mode
}

static IARM_Result_t _SetStandbyVideoState(void *arg)
{
    INT_INFO("Entering [%s]\r\n", __FUNCTION__);
    dsMgrStandbyVideoStateParam_t *param = (dsMgrStandbyVideoStateParam_t *)arg;
    if(NULL == param->port)
    {
        param->result = -1;
        INT_DEBUG("[%s] empty port name. Cannot proceed.\n", __FUNCTION__);
        return IARM_RESULT_SUCCESS;
    }
    else
        param->result = 0;

    int i = 0;
    for(i = 0; i < MAX_NUM_VIDEO_PORTS; i++)
    {
        if(0 == strncasecmp(param->port, g_standby_video_port_setting[i].port, DSMGR_MAX_VIDEO_PORT_NAME_LENGTH))
        {
            /*Found a match. Update it*/
            g_standby_video_port_setting[i].isEnabled = ((0 == param->isEnabled) ? false : true);
            break;
        }
    }
    if(MAX_NUM_VIDEO_PORTS == i)
    {
        /*No matching entries are present. Add one.*/
        for(i = 0; i < MAX_NUM_VIDEO_PORTS; i++)
        {
            if('\0' == g_standby_video_port_setting[i].port[0])
            {
                strncpy(g_standby_video_port_setting[i].port, param->port, (DSMGR_MAX_VIDEO_PORT_NAME_LENGTH - 1));
                g_standby_video_port_setting[i].isEnabled = ((0 == param->isEnabled) ? false : true);
                break;
            }
        }
    }
    if(MAX_NUM_VIDEO_PORTS == i)
    {
        INT_DEBUG("[%s] Error! Out of room to write new video port setting for standby mode.\n", __FUNCTION__);
    }

    try
    {
        device::VideoOutputPort &vPort = device::Host::getInstance().getVideoOutputPort(param->port);
        if((PWRMGR_POWERSTATE_ON != curState) && (PWRMGR_POWERSTATE_OFF != curState))
        {
            /*We're currently in one of the standby states. This new setting needs to be applied right away.*/
            INT_DEBUG("[%s] Setting standby %s port status to %s.\n", __FUNCTION__, param->port, ((1 == param->isEnabled)? "enabled" : "disabled"));
            if(1 == param->isEnabled)
                vPort.enable();
            else
                vPort.disable();
        }
        else
        {
            INT_DEBUG("[%s] video port %s will be %s when going into standby mode.\n", __FUNCTION__, param->port, ((1 == param->isEnabled)? "enabled" : "disabled"));
        }
    }
    catch (...)
    {
        INT_DEBUG("Exception Caught during [%s]. Possible bad video port.\n", __FUNCTION__);
        param->result = -1;
    }
    return IARM_RESULT_SUCCESS;
}

static IARM_Result_t _GetStandbyVideoState(void *arg)
{
    INT_INFO("Entering [%s]\r\n", __FUNCTION__);
    dsMgrStandbyVideoStateParam_t *param = (dsMgrStandbyVideoStateParam_t *)arg;
    if(NULL == param->port)
    {
        INT_DEBUG("[%s]Bad port name. Cannot get state.\n", __FUNCTION__);
        return IARM_RESULT_SUCCESS;
    }

    try
    {
        device::VideoOutputPort &vPort = device::Host::getInstance().getVideoOutputPort(param->port);

    }
    catch (...)
    {
        INT_DEBUG("Exception Caught during [%s]. Possible bad video port.\n", __FUNCTION__);
        param->result = -1;
        return IARM_RESULT_SUCCESS;
    }
    param->isEnabled = ((true == get_video_port_standby_setting(param->port))? 1 : 0);
    param->result = 0;
    return IARM_RESULT_SUCCESS;
}

static IARM_Result_t _SetAvPortState(void *arg)
{
    INT_INFO("Entering [%s]\r\n", __FUNCTION__);
    dsMgrAVPortStateParam_t *param = (dsMgrAVPortStateParam_t *)arg;
    if(NULL == param)
    {
        INT_DEBUG("Bad Parameter.\n");
        return IARM_RESULT_SUCCESS;
    }

    _SetAVPortsPowerState((PWRMgr_PowerState_t)param->avPortPowerState);
    param->result = 0;
    return IARM_RESULT_SUCCESS;
}

static IARM_Result_t _SetLEDState(void *arg)
{
    INT_INFO("Entering [%s]\r\n", __FUNCTION__);
    dsMgrLEDStatusParam_t *param = (dsMgrLEDStatusParam_t *)arg;
    if(NULL == param)
    {
        INT_DEBUG("[%s] Bad Parameter.\n", __FUNCTION__);
        return IARM_RESULT_SUCCESS;
    }

    _SetLEDStatus((PWRMgr_PowerState_t)param->ledState);
    param->result = 0;
    return IARM_RESULT_SUCCESS;
}

static IARM_Result_t _SetRebootConfig(void *arg)
{
    INT_INFO("Entering [%s]\r\n", __FUNCTION__);
    dsMgrRebootConfigParam_t *param = (dsMgrRebootConfigParam_t *)arg;
    if(NULL == param)
    {
        INT_DEBUG("[%s]Bad Parameter.\n", __FUNCTION__);
        return IARM_RESULT_SUCCESS;
    }
    param->reboot_reason_custom[sizeof(param->reboot_reason_custom) - 1] = '\0'; //Just to be on the safe side.
    if(nullptr != ux)
    {
        if(0 == strncmp(PWRMGR_REBOOT_REASON_MAINTENANCE, param->reboot_reason_custom, sizeof(param->reboot_reason_custom)))
            ux->applyPreMaintenanceRebootConfig((PWRMgr_PowerState_t)param->powerState);
        else
            ux->applyPreRebootConfig((PWRMgr_PowerState_t)param->powerState);
    }

    param->result = 0;
    return IARM_RESULT_SUCCESS;
}
