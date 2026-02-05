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
#include "audioOutputPortType.hpp"
#include "videoOutputPortType.hpp"
#include "exception.hpp"
#include "manager.hpp"
#include "dsMgrProductTraitsHandler.h"
#include "dsAudioSettings.h"
#include "plat_power.h"
#include "rdkProfile.h"

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
extern IARM_Result_t _dsGetVideoPort(void *arg);
extern IARM_Result_t _dsEnableVideoPort(void *arg);

static DSMgr_Standby_Video_State_t g_standby_video_port_setting[MAX_NUM_VIDEO_PORTS];
static dsMgrProductTraits::ux_controller * ux = nullptr;

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

#define RDK_PROFILE "RDK_PROFILE"
#define PROFILE_STR_TV "TV"
#define PROFILE_STR_STB "STB"

static profile_t profileType = PROFILE_INVALID;

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

/**
 * @brief Enables or disables a given video output port using libdssrv APIs.
 *
 * This function configures the specified video output port by enabling or disabling it,
 * depending on the value of the requestEnable parameter. It interacts with the underlying
 * device abstraction layer to retrieve the port handle and set its enabled state.
 *
 * @param[in] vPort Reference to the VideoOutputPort object to be configured.
 * @param[in] requestEnable Boolean flag indicating whether to enable (true) or disable (false) the port.
 *
 * @return IARM_Result_t Returns IARM_RESULT_SUCCESS on success, or an appropriate error code
 *         (e.g., IARM_RESULT_INVALID_STATE) on failure.
 */
static IARM_Result_t configureVideoPort(device::VideoOutputPort& vPort, bool requestEnable)
{
    dsVideoPortGetHandleParam_t vHandleParam;
    dsVideoPortSetEnabledParam_t vPortEnableParam;
    IARM_Result_t vPortRetCode = IARM_RESULT_SUCCESS;
    try
    {
        dsVideoPortType_t videoPortType = static_cast<dsVideoPortType_t>(vPort.getType().getId());
        int index = vPort.getIndex();
        INT_INFO("[%s] VideoPort[%s] Type[%d] Index[%d] Enabled[%d]\r\n", __FUNCTION__, vPort.getName().c_str(), videoPortType, index, requestEnable);

        vHandleParam.type = videoPortType;
        vHandleParam.index = index;
        vHandleParam.handle = (intptr_t)0;

        vPortRetCode = _dsGetVideoPort(&vHandleParam);
        if (IARM_RESULT_SUCCESS != vPortRetCode)
        {
            INT_ERROR("[%s] Failed to _dsGetVideoPort for port [%s], Error:[%d]\r\n", __FUNCTION__, vPort.getName().c_str(), vPortRetCode);
        }
        else
        {
            vPortEnableParam.enabled = requestEnable;
            vPortEnableParam.handle = vHandleParam.handle;
            snprintf(vPortEnableParam.portName, sizeof(vPortEnableParam.portName), "%s", vPort.getName().c_str());

            vPortRetCode = _dsEnableVideoPort(&vPortEnableParam);
            if (IARM_RESULT_SUCCESS != vPortRetCode)
            {
                INT_ERROR("[%s] Failed to _dsEnableVideoPort for port [%s], Error:[%d]\r\n", __FUNCTION__, vPort.getName().c_str(), vPortRetCode);
            }
            else
            {
                INT_INFO("VideoPort[%s] successfully %s\r\n", vPort.getName().c_str(), (requestEnable ? "enabled" : "disabled"));
            }
        }
    }
    catch (...)
    {
        INT_DEBUG("Exception Caught during [DSMGR - configureVideoPort]\r\n");
        vPortRetCode = IARM_RESULT_INVALID_STATE;
    }
    return vPortRetCode;
}

/**
 * @brief Configures the enable/disable state of the specified audio output port.
 *
 * This function sets the enabled state of the given audio output port (`aPort`)
 * to the value specified by `requestEnable`. If the port is already in the desired
 * state, the function may skip the operation to avoid unnecessary changes.
 * Special handling is included to persist the enable state as needed.
 *
 * @param[in]  aPort         Reference to the AudioOutputPort object to configure.
 * @param[in]  requestEnable Boolean value indicating whether to enable (true) or disable (false) the port.
 *
 * @return IARM_RESULT_SUCCESS on success,
 *         IARM_RESULT_INVALID_STATE if an error or exception occurs,
 *         or another appropriate IARM_Result_t error code.
 */
static IARM_Result_t configureAudioPort(device::AudioOutputPort& aPort, bool requestEnable)
{
    dsAudioGetHandleParam_t aHandleParam;
    dsAudioPortEnabledParam_t aPortEnableParam;
    IARM_Result_t aPortRetCode = IARM_RESULT_SUCCESS;

    try
    {
        dsAudioPortType_t portType = static_cast<dsAudioPortType_t>(aPort.getType().getId());
        int index = aPort.getIndex();
        INT_INFO("[%s] AudioPort[%s] Type[%d] Index[%d] Enabled[%d]\r\n", __FUNCTION__, aPort.getName().c_str(), portType, index, requestEnable);

        aHandleParam.type = portType;
        aHandleParam.index = index;
        aHandleParam.handle = (intptr_t)0;

        aPortRetCode = _dsGetAudioPort(&aHandleParam);
        if (IARM_RESULT_SUCCESS != aPortRetCode)
        {
            INT_ERROR("[%s] Failed to _dsGetAudioPort for port [%s], Error:[%d]\r\n", __FUNCTION__, aPort.getName().c_str(), aPortRetCode);
        }
        else
        {
            bool skipOperation = false;
            aPortEnableParam.handle = aHandleParam.handle;
            snprintf(aPortEnableParam.portName, sizeof(aPortEnableParam.portName), "%s", aPort.getName().c_str());

            // Retrieve the persistent enable state only if we are enabling the port
            if (true == requestEnable)
            {
                aPortEnableParam.enabled = false; // Value will be retrieved from persistent storage

                aPortRetCode = _dsGetEnablePersist(&aPortEnableParam);
                if (IARM_RESULT_SUCCESS != aPortRetCode)
                {
                    INT_ERROR("[%s] Failed to _dsGetEnablePersist for port [%s], Error:[%d]\r\n", __FUNCTION__, aPort.getName().c_str(), aPortRetCode);
                }
                else
                {
                    INT_INFO("[%s] Audio PortName[%s] isEnablePersist[%d]\r\n", __FUNCTION__, aPort.getName().c_str(), aPortEnableParam.enabled);
                    skipOperation = !aPortEnableParam.enabled;
                }
            }
            else
            {
                aPortEnableParam.enabled = false; // Disabling the port
            }

            // Proceed to enable/disable the port only if previous operation was successful
            if (IARM_RESULT_SUCCESS == aPortRetCode)
            {
                // skip enabling the port if persistent state is disabled
                if (skipOperation)
                {
                    INT_INFO("[%s] Enable AudioPort[%s] skipped!!!\r\n", __FUNCTION__, aPort.getName().c_str());
                }
                else
                {
                    INT_INFO("[%s] Setting AudioPort[%s] to [%s]\r\n", __FUNCTION__, aPort.getName().c_str(), aPortEnableParam.enabled ? "Enabled" : "Disabled");
                    aPortRetCode = _dsEnableAudioPort(&aPortEnableParam);
                    if (IARM_RESULT_SUCCESS != aPortRetCode)
                    {
                        INT_ERROR("[%s] _dsEnableAudioPort failed for port [%s], Error:[%d]\r\n", __FUNCTION__, aPort.getName().c_str(), aPortRetCode);
                    }
                    else
                    {
                        INT_INFO("AudioPort[%s] successfully %s\r\n", aPort.getName().c_str(), (aPortEnableParam.enabled ? "enabled" : "disabled"));
                    }
                }
            }
        }
    }
    catch (...)
    {
        INT_DEBUG("Exception Caught during [DSMGR - configureAudioPort]\r\n");
        aPortRetCode = IARM_RESULT_INVALID_STATE;
    }
    return aPortRetCode;
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
    INT_INFO("Entering [%s] powerState:%d \r\n", __FUNCTION__, powerState);
    try
    {
        if (PWRMGR_POWERSTATE_ON != powerState)
        {
            INT_INFO("[%s] POWERSTATE %d \r\n", __FUNCTION__, powerState);
            // We're in a non-ON power state (standby or off). Certain ports may have to be left on in standby modes.
            try
            {
                device::List<device::VideoOutputPort> videoPorts = device::Host::getInstance().getVideoOutputPorts();
                INT_INFO("[%s] Number of Video Ports: [%d] \r\n", __FUNCTION__, videoPorts.size());
                for (size_t i = 0; i < videoPorts.size(); i++)
                {
                    try
                    {
                        device::VideoOutputPort vPort = videoPorts.at(i);
                        bool doEnable = get_video_port_standby_setting(vPort.getName().c_str());
                        INT_DEBUG("[%s] Video port %s will be %s for PowerState[%d]\n", __FUNCTION__, vPort.getName().c_str(), (doEnable ? "enabled" : "disabled"), powerState);
                        if ((false == doEnable) || (PWRMGR_POWERSTATE_OFF == powerState))
                        {
                            // Disable the port
                            IARM_Result_t retCode = configureVideoPort(vPort, false);
                            if (IARM_RESULT_SUCCESS == retCode)
                            {
                                INT_INFO("[%s] VideoPort[%d] disabled for powerState [%d] \r\n", __FUNCTION__, i, powerState);
                            }
                            else
                            {
                                INT_ERROR("[%s] Failed to disable VideoPort[%d] for powerState [%d] \r\n", __FUNCTION__, i, powerState);
                            }
                        }
                        else
                        {
                            INT_INFO("[%s] Disable VideoPort[%d] skipped!!!\r\n", __FUNCTION__, i);
                        }
                    }
                    catch (...)
                    {
                        INT_DEBUG("[%s] video port exception at %d\r\n", __FUNCTION__, i);
                    }
                }
            }
            catch (...)
            {
                INT_DEBUG("[%s] Video port exception %d \r\n", __FUNCTION__, powerState);
            }
            INT_INFO("[%s] VideoPort configuration done \r\n", __FUNCTION__);

            try
            {
                device::List<device::AudioOutputPort> aPorts = device::Host::getInstance().getAudioOutputPorts();
                INT_INFO("[%s] Number of Audio Ports: [%d] \r\n", __FUNCTION__, aPorts.size());
                for (size_t i = 0; i < aPorts.size(); i++)
                {
                    try
                    {
                        // Disable the Audio Port
                        device::AudioOutputPort aPort = aPorts.at(i);
                        IARM_Result_t retCode = configureAudioPort(aPort, false);
                        if (IARM_RESULT_SUCCESS == retCode)
                        {
                            INT_INFO("[%s] AudioPort[%d] disabled for powerState [%d] \r\n", __FUNCTION__, i, powerState);
                        }
                        else
                        {
                            INT_ERROR("[%s] Failed to disable AudioPort[%d] for powerState [%d] \r\n", __FUNCTION__, i, powerState);
                        }
                    }
                    catch (...)
                    {
                        INT_DEBUG("[%s] audio port exception at %d\r\n", __FUNCTION__, i);
                    }
                }
            }
            catch (...)
            {
                INT_DEBUG("[%s] audio port exception \r\n", __FUNCTION__);
            }
            INT_INFO("[%s] AudioPort configuration done \r\n", __FUNCTION__);
        }
        else
        {
            try
            {
                device::List<device::VideoOutputPort> videoPorts = device::Host::getInstance().getVideoOutputPorts();
                INT_INFO("[%s] Number of Video Ports: [%d] \r\n", __FUNCTION__, videoPorts.size());
                for (size_t i = 0; i < videoPorts.size(); i++)
                {
                    try
                    {
                        device::VideoOutputPort vPort = videoPorts.at(i);
                        // Enable the Video Port
                        IARM_Result_t retCode = configureVideoPort(vPort, true);
                        if (IARM_RESULT_SUCCESS == retCode)
                        {
                            INT_INFO("[%s] VideoPort[%s] enabled for powerState[%d] \r\n", __FUNCTION__, vPort.getName().c_str(), powerState);
                        }
                        else
                        {
                            INT_ERROR("[%s] Failed to enable VideoPort[%s] for powerState[%d] \r\n", __FUNCTION__, vPort.getName().c_str(), powerState);
                        }
                    }
                    catch (...)
                    {
                        INT_DEBUG("[%s] video port exception at %d\r\n", __FUNCTION__, i);
                    }
                }
                INT_INFO("[%s] VideoPort configuration done \r\n", __FUNCTION__);

                device::List<device::AudioOutputPort> aPorts = device::Host::getInstance().getAudioOutputPorts();
                INT_INFO("[%s] Number of Audio Ports: [%d] \r\n", __FUNCTION__, aPorts.size());
                for (size_t i = 0; i < aPorts.size(); i++)
                {
                    try
                    {
                        device::AudioOutputPort aPort = aPorts.at(i);
                        // Enable the Audio Port
                        IARM_Result_t retCode = configureAudioPort(aPort, true);
                        if (IARM_RESULT_SUCCESS == retCode)
                        {
                            INT_INFO("[%s] AudioPort[%s] enabled for powerState[%d] \r\n", __FUNCTION__, aPort.getName().c_str(), powerState);
                        }
                        else
                        {
                            INT_ERROR("[%s] Failed to enable AudioPort[%s] for powerState[%d] \r\n", __FUNCTION__, aPort.getName().c_str(), powerState);
                        }
                    }
                    catch (...)
                    {
                        INT_DEBUG("[%s] Audio port exception at %d \r\n", __FUNCTION__, i);
                    }
                }
                INT_INFO("[%s] AudioPort configuration done \r\n", __FUNCTION__);
                if (isEAS == IARM_BUS_SYS_MODE_EAS)
                {
                    /* Force Stereo in EAS mode. */
                    INT_INFO("[%s] Force Stereo in EAS mode \r\n", __FUNCTION__);
                    _setEASAudioMode();
                }
            }
            catch (...)
            {
                INT_DEBUG("[%s] Audio port exception \r\n", __FUNCTION__);
            }
        }
    }
    catch (...)
    {
        INT_DEBUG("Exception Caught during [%s]\r\n", __FUNCTION__);
        return 0;
    }
    INT_DEBUG("Exiting [%s]\r\n", __FUNCTION__);
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
