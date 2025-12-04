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


std::queue<DSMgr_Power_Event_State_t> pwrEventQueue;

/* Power Controller Event Handling Thread */
static pthread_t edsPwrEventHandlerThreadID; 
static pthread_mutex_t tdsPwrEventMutexLock=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  tdsPwrEventMutexCond=PTHREAD_COND_INITIALIZER;
static pthread_mutex_t tdsPwrEventQueueMutexLock=PTHREAD_MUTEX_INITIALIZER;
static std::atomic<bool> m_dsMgrPwrStopThread(false);


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
static void _PwrEventHandler(const PowerController_PowerState_t currentState,
                             const PowerController_PowerState_t newState,
                             void *userdata);
static int _SetLEDStatus(PowerController_PowerState_t powerState);
int _SetAVPortsPowerState(PowerController_PowerState_t powerState);
static IARM_Result_t _SetStandbyVideoState(void *arg);
static IARM_Result_t _GetStandbyVideoState(void *arg);
static IARM_Result_t _SetAvPortState(void *arg);
static IARM_Result_t _SetLEDState(void *arg);
static IARM_Result_t _SetRebootConfig(void *arg);

static PowerController_PowerState_t curState = POWER_STATE_OFF;

#define RDK_PROFILE "RDK_PROFILE"
#define PROFILE_STR_TV "TV"
#define PROFILE_STR_STB "STB"

static profile_t profileType = PROFILE_INVALID;

void initPwrEventListner()
{
    INT_INFO("Entering [%s]\r\n", __FUNCTION__);
    
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
    IARM_Result_t rc;

    rc = IARM_Bus_RegisterCall(IARM_BUS_DSMGR_API_SetStandbyVideoState, _SetStandbyVideoState);
    if (IARM_RESULT_SUCCESS != rc)
    {
        INT_ERROR("IARM_Bus_RegisterCall Failed for IARM_BUS_DSMGR_API_SetStandbyVideoState, Error:[%d]\r\n", rc);
    }
    rc = IARM_Bus_RegisterCall(IARM_BUS_DSMGR_API_GetStandbyVideoState, _GetStandbyVideoState);
    if (IARM_RESULT_SUCCESS != rc)
    {
        INT_ERROR("IARM_Bus_RegisterCall Failed for IARM_BUS_DSMGR_API_GetStandbyVideoState, Error:[%d]\r\n", rc);
    }
    rc = IARM_Bus_RegisterCall(IARM_BUS_DSMGR_API_SetAvPortState, _SetAvPortState);
    if (IARM_RESULT_SUCCESS != rc)
    {
        INT_ERROR("IARM_Bus_RegisterCall Failed for IARM_BUS_DSMGR_API_SetAvPortState, Error:[%d]\r\n", rc);
    }
    rc = IARM_Bus_RegisterCall(IARM_BUS_DSMGR_API_SetLEDStatus, _SetLEDState);
    if (IARM_RESULT_SUCCESS != rc)
    {
        INT_ERROR("IARM_Bus_RegisterCall Failed for IARM_BUS_DSMGR_API_SetLEDStatus, Error:[%d]\r\n", rc);
    }
    rc = IARM_Bus_RegisterCall(IARM_BUS_DSMGR_API_SetRebootConfig, _SetRebootConfig);
    if (IARM_RESULT_SUCCESS != rc)
    {
        INT_ERROR("IARM_Bus_RegisterCall Failed for IARM_BUS_DSMGR_API_SetRebootConfig, Error:[%d]\r\n", rc);
    }
    /*  Start a new thread and wait until connection established 
     * if connection is established, proceed with fetching and Getting values from Power control and initialize*/
     INT_INFO("DSMgr PowerController_Connect setup in a Thread\r\n");
     dsMgrPwrCtrlEstablishConnection();
}

static void dsMgrPwrCtrlEstablishConnection(void)
{
    INT_INFO("Entering [%s]\r\n", __FUNCTION__);
    
    pthread_t edsPwrConnectThreadID; 

    if(pthread_create (&edsPwrConnectThreadID, NULL, dsMgrPwrRetryEstablishConnThread, NULL)  == 0)
    {
        if(pthread_detach(edsPwrConnectThreadID) != 0) 
        {
            INT_ERROR("DSMgr dsMgrPwrCtrlEstablishConnection Thread detach Failed\r\n");
        }
    }
    else
    {
        INT_ERROR("DSMgr dsMgrPwrCtrlEstablishConnection Thread Creation Failed\r\n");
    }
}

static void* dsMgrPwrRetryEstablishConnThread(void *arg)
{
    INT_INFO("%s: Entry  \r\n", __FUNCTION__);

    while(1)
    {
        if(POWER_CONTROLLER_ERROR_NONE == PowerController_Connect())
        {
            INT_DEBUG("dsMgrPwrRetryEstablishConnThread PowerController_Connect is Success \r\n");
            dsMgrPwrControllerFetchNinitStateValues();
            break;
        }
        else
        {
            /* 300 msec wait and recheck connect */
            usleep(DSMGR_PWR_CNTRL_CONNECT_WAIT_TIME_MS);
        }
    }
    INT_DEBUG("dsMgrPwrRetryEstablishConnThread Completed Exit \r\n");
    return arg;
}

static void dsMgrPwrControllerFetchNinitStateValues(void)
{
    INT_INFO("Entering [%s]\r\n", __FUNCTION__);
    
    PowerController_PowerState_t _curState = POWER_STATE_UNKNOWN;
    PowerController_PowerState_t _prevState = POWER_STATE_UNKNOWN;
    PowerController_PowerState_t powerStateBeforeReboot = POWER_STATE_STANDBY;

    PowerController_RegisterPowerModeChangedCallback(_PwrEventHandler, nullptr);
    /*  Read the Device Power State on startup... */
    if (POWER_CONTROLLER_ERROR_NONE == PowerController_GetPowerState(&_curState, &_prevState))
    {
        INT_DEBUG("Deep Sleep Manager Init with Power State %d\r\n", _curState);
        curState = _curState;
    }

    if (POWER_CONTROLLER_ERROR_NONE != PowerController_GetPowerStateBeforeReboot(&powerStateBeforeReboot))
    {
        INT_ERROR("DSMgr GetPowerStateBeforeReboot Failed\r\n");
    }

    if (nullptr != ux) 
    {
        // This will set up ports, lights and bootloader pattern internally
        ux->applyPostRebootConfig(curState, powerStateBeforeReboot);
    } else {
        // Sync Port with Power state TODO:
    }

    if (nullptr == ux) // Since ux_controller is not supported, ports need to be
                       // set up explicitly.
    {
#ifndef DISABLE_LED_SYNC_IN_BOOTUP
        _SetLEDStatus(curState);
#endif
        _SetAVPortsPowerState(curState);
    }
}

static void _PwrEventHandler(const PowerController_PowerState_t currentState,
    const PowerController_PowerState_t newState,
    void* userdata)
{
    INT_INFO("Entering [%s]\r\n", __FUNCTION__);

    /* In this call back function, received event is pushed to queue guarded by queue mutex and signaled for the thread to 
        pop out from queue and process the pushed event. This will allow the CB to be handled separately 
        in a different thread context, userdata parameter is empty and not handled*/

    pthread_mutex_lock(&tdsPwrEventQueueMutexLock);
    pwrEventQueue.emplace(currentState, newState);
    pthread_mutex_unlock(&tdsPwrEventQueueMutexLock);
    
    INT_DEBUG("Sending Signal to Thread for Processing Callback Event \r\n");

    pthread_mutex_lock(&tdsPwrEventMutexLock);
    pthread_cond_signal(&tdsPwrEventMutexCond);
    pthread_mutex_unlock(&tdsPwrEventMutexLock);
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
    IARM_Result_t vPortRetCode = IARM_RESULT_SUCCESS;
    try
    {
        dsVideoPortGetHandleParam_t vHandleParam = { dsVIDEOPORT_TYPE_MAX, -1, 0 };
        dsVideoPortSetEnabledParam_t vPortEnableParam = { 0, false, "" };

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
            memset(&vPortEnableParam.portName, 0, sizeof(vPortEnableParam.portName));
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
 * to the value specified by `requestEnable`. When enabling, the function retrieves
 * the persistent enable state from storage and skips the operation if persistence is disabled.
 *
 * @param[in]  aPort         Reference to the AudioOutputPort object to configure.
 * @param[in]  requestEnable Boolean value indicating whether to enable (true) or disable (false) the port.
 * @param[out]  isConfigurationSkippedPtr Pointer to a boolean that will be set to true if the configuration was skipped.
 *
 * @return IARM_RESULT_SUCCESS on success,
 *         IARM_RESULT_INVALID_PARAM if isConfigurationSkippedPtr is nullptr,
 *         IARM_RESULT_INVALID_STATE if an error or exception occurs,
 *         or another appropriate IARM_Result_t error code.
 */
static IARM_Result_t configureAudioPort(device::AudioOutputPort& aPort, bool requestEnable, bool* isConfigurationSkippedPtr)
{
    IARM_Result_t aPortRetCode = IARM_RESULT_SUCCESS;

    if (nullptr == isConfigurationSkippedPtr)
    {
        return IARM_RESULT_INVALID_PARAM;
    }

    // Initialize output parameter with default value as false, indicating configuration is not skipped
    *isConfigurationSkippedPtr = false;

    try
    {
        dsAudioGetHandleParam_t aHandleParam = { dsAUDIOPORT_TYPE_MAX, -1, 0 };
        dsAudioPortEnabledParam_t aPortEnableParam = { 0, false, "" };

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
            memset(&aPortEnableParam.portName, 0, sizeof(aPortEnableParam.portName));
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
                    *isConfigurationSkippedPtr = !aPortEnableParam.enabled;
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
                if (*isConfigurationSkippedPtr)
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

static int _SetLEDStatus(PowerController_PowerState_t powerState)
{
    INT_INFO("Entering [%s]\r\n", __FUNCTION__);
    try {
        dsFPDStateParam_t param;

        param.eIndicator = dsFPD_INDICATOR_POWER;

        if( POWER_STATE_ON  != powerState )
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

int _SetAVPortsPowerState(PowerController_PowerState_t powerState)
{
    INT_INFO("Entering [%s] powerState:%d \r\n", __FUNCTION__, powerState);
    try
    {
        if (POWER_STATE_ON != powerState)
        {
            INT_INFO("[%s] POWERSTATE %d \r\n", __FUNCTION__, powerState);
            // We're in a non-ON power state (standby or off). Certain ports may have to be left on in standby modes.
            try
            {
                device::List<device::VideoOutputPort> videoPorts = device::Host::getInstance().getVideoOutputPorts();
                INT_INFO("[%s] Number of Video Ports: [%zu] \r\n", __FUNCTION__, videoPorts.size());
                for (size_t i = 0; i < videoPorts.size(); i++)
                {
                    try
                    {
                        device::VideoOutputPort vPort = videoPorts.at(i);
                        bool doEnable = get_video_port_standby_setting(vPort.getName().c_str());
                        INT_DEBUG("[%s] Video port %s will be %s for PowerState[%d]\n", __FUNCTION__, vPort.getName().c_str(), (doEnable ? "enabled" : "disabled"), powerState);
                        if ((false == doEnable) || (POWER_STATE_OFF == powerState))
                        {
                            // Disable the port
                            IARM_Result_t retCode = configureVideoPort(vPort, false);
                            if (IARM_RESULT_SUCCESS == retCode)
                            {
                                INT_INFO("[%s] VideoPort[%s] disabled for powerState [%d] \r\n", __FUNCTION__, vPort.getName().c_str(), powerState);
                            }
                            else
                            {
                                INT_ERROR("[%s] Failed to disable VideoPort[%s] for powerState [%d] \r\n", __FUNCTION__, vPort.getName().c_str(), powerState);
                            }
                        }
                        else
                        {
                            INT_INFO("[%s] Disable VideoPort[%s] skipped!!!\r\n", __FUNCTION__, vPort.getName().c_str());
                        }
                    }
                    catch (...)
                    {
                        INT_DEBUG("[%s] video port exception at %zu\r\n", __FUNCTION__, i);
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
                INT_INFO("[%s] Number of Audio Ports: [%zu] \r\n", __FUNCTION__, aPorts.size());
                for (size_t i = 0; i < aPorts.size(); i++)
                {
                    try
                    {
                        // Disable the Audio Port
                        device::AudioOutputPort aPort = aPorts.at(i);
                        bool isOperationSkipped = false;
                        IARM_Result_t retCode = configureAudioPort(aPort, false, &isOperationSkipped);
                        if (IARM_RESULT_SUCCESS == retCode)
                        {
                            INT_INFO("[%s] AudioPort[%s] disabled for powerState [%d] \r\n", __FUNCTION__, aPort.getName().c_str(), powerState);
                        }
                        else
                        {
                            INT_ERROR("[%s] Failed to disable AudioPort[%s] for powerState [%d] \r\n", __FUNCTION__, aPort.getName().c_str(), powerState);
                        }
                    }
                    catch (...)
                    {
                        INT_DEBUG("[%s] audio port exception at %zu\r\n", __FUNCTION__, i);
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
                INT_INFO("[%s] Number of Video Ports: [%zu] \r\n", __FUNCTION__, videoPorts.size());
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
                        INT_DEBUG("[%s] video port exception at %zu\r\n", __FUNCTION__, i);
                    }
                }
                INT_INFO("[%s] VideoPort configuration done \r\n", __FUNCTION__);

                device::List<device::AudioOutputPort> aPorts = device::Host::getInstance().getAudioOutputPorts();
                INT_INFO("[%s] Number of Audio Ports: [%zu] \r\n", __FUNCTION__, aPorts.size());
                for (size_t i = 0; i < aPorts.size(); i++)
                {
                    try
                    {
                        device::AudioOutputPort aPort = aPorts.at(i);
                        bool isOperationSkipped = false;
                        // Enable the Audio Port
                        IARM_Result_t retCode = configureAudioPort(aPort, true, &isOperationSkipped);
                        if (IARM_RESULT_SUCCESS == retCode)
                        {
                            if (isOperationSkipped)
                            {
                                INT_INFO("[%s] Enabling AudioPort[%s] skipped for powerState[%d] \r\n", __FUNCTION__, aPort.getName().c_str(), powerState);
                            }
                            else
                            {
                                INT_INFO("[%s] AudioPort[%s] enabled for powerState[%d] \r\n", __FUNCTION__, aPort.getName().c_str(), powerState);
                            }
                        }
                        else
                        {
                            INT_ERROR("[%s] Failed to enable AudioPort[%s] for powerState[%d] \r\n", __FUNCTION__, aPort.getName().c_str(), powerState);
                        }
                    }
                    catch (...)
                    {
                        INT_DEBUG("[%s] Audio port exception at %zu \r\n", __FUNCTION__, i);
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
    if (NULL == arg) {
	return IARM_RESULT_INVALID_PARAM;
    }
    dsMgrStandbyVideoStateParam_t *param = (dsMgrStandbyVideoStateParam_t *)arg;
    if(NULL == param->port)
    {
        param->result = -1;
        INT_DEBUG("[%s] empty port name. Cannot proceed.\n", __FUNCTION__);
        return IARM_RESULT_SUCCESS;
    }
    
    // FIX(Coverity): NO_EFFECT 
    // Reason: Removed redundant assignment - result is set based on logic flow
    // Impact: Internal logic corrected. Public API unchanged.
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
        if((POWER_STATE_ON != curState) && (POWER_STATE_OFF != curState))
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
    if (NULL == arg) {
	return IARM_RESULT_INVALID_PARAM;
    }
    dsMgrStandbyVideoStateParam_t *param = (dsMgrStandbyVideoStateParam_t *)arg;
    if(NULL == param->port)
    {
        INT_DEBUG("[%s]Bad port name. Cannot get state.\n", __FUNCTION__);
        return IARM_RESULT_SUCCESS;
    }

    try
    {
        // FIX(Coverity): NO_EFFECT
        // Reason: Removed unused VideoOutputPort variable - only validating port name
        // Impact: Internal logic corrected. Public API unchanged.
        device::Host::getInstance().getVideoOutputPort(param->port);
    }
    catch (...)
    {
        INT_DEBUG("Exception Caught during [%s]. Possible bad video port.\n", __FUNCTION__);
        param->result = -1;
        return IARM_RESULT_SUCCESS;
    }
    param->isEnabled = (get_video_port_standby_setting(param->port) ? 1 : 0);
    param->result = 0;
    return IARM_RESULT_SUCCESS;
}

static PowerController_PowerState_t dsMgrPwrMgrToPowerControllerPowerState(PWRMgr_PowerState_t _state)
{
    PowerController_PowerState_t powerState = POWER_STATE_UNKNOWN;
    switch (_state) {
    case PWRMGR_POWERSTATE_OFF:
        powerState = POWER_STATE_OFF;
        break;
    case PWRMGR_POWERSTATE_STANDBY:
        powerState = POWER_STATE_STANDBY;
        break;
    case PWRMGR_POWERSTATE_ON:
        powerState = POWER_STATE_ON;
        break;
    case PWRMGR_POWERSTATE_STANDBY_LIGHT_SLEEP:
        powerState = POWER_STATE_STANDBY_LIGHT_SLEEP;
        break;
    case PWRMGR_POWERSTATE_STANDBY_DEEP_SLEEP:
        powerState = POWER_STATE_STANDBY_DEEP_SLEEP;
        break;
    default:
        /* powerState is already UNKNOWN in the init of the fn*/
        INT_ERROR("DSMgr dsMgrPwrMgrToPowerControllerPowerState Invalid Power State\r\n");
        break;
    }
    INT_DEBUG("[%s] _state=[%d]  powerState[%d]\r\n", __FUNCTION__, _state, powerState);
    return powerState;
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

    PWRMgr_PowerState_t pwrState = (PWRMgr_PowerState_t)param->avPortPowerState;
    PowerController_PowerState_t powerState = dsMgrPwrMgrToPowerControllerPowerState(pwrState);
    if(POWER_STATE_UNKNOWN != powerState)
    {
        _SetAVPortsPowerState(powerState);
    }
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

    PWRMgr_PowerState_t pwrState = (PWRMgr_PowerState_t)param->ledState;
    PowerController_PowerState_t powerState = dsMgrPwrMgrToPowerControllerPowerState(pwrState);
    if(POWER_STATE_UNKNOWN != powerState)
    {
        _SetLEDStatus(powerState);
    }
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
        PWRMgr_PowerState_t pwrState = (PWRMgr_PowerState_t)param->powerState;
        PowerController_PowerState_t powerState = dsMgrPwrMgrToPowerControllerPowerState(pwrState);

        if(POWER_STATE_UNKNOWN != powerState)
        {
            if(0 == strncmp(PWRMGR_REBOOT_REASON_MAINTENANCE, param->reboot_reason_custom, sizeof(param->reboot_reason_custom)))
                ux->applyPreMaintenanceRebootConfig(powerState);
            else
                ux->applyPreRebootConfig(powerState);
       }
    }
    param->result = 0;
    return IARM_RESULT_SUCCESS;
}

static void* dsMgrPwrEventHandlingThreadFunc(void *arg)
{
    INT_INFO("%s: Entry  \r\n",__FUNCTION__);

    /* In Loop and waiting for an event conditionally */
    while (true)
    {
        pthread_mutex_lock(&tdsPwrEventMutexLock);
        INT_DEBUG("dsMgrPwrEventHandlingThreadFunc.... Wait for Events from Power manager Controller Callback\r\n");
        
        // Check queue status with proper locking to fix MISSING_LOCK issue
        pthread_mutex_lock(&tdsPwrEventQueueMutexLock);
        bool queueEmpty = pwrEventQueue.empty();
        pthread_mutex_unlock(&tdsPwrEventQueueMutexLock);
        
        while (!m_dsMgrPwrStopThread && queueEmpty)
        {
            pthread_cond_wait(&tdsPwrEventMutexCond, &tdsPwrEventMutexLock);
            // Re-check queue status after waking up
            pthread_mutex_lock(&tdsPwrEventQueueMutexLock);
            queueEmpty = pwrEventQueue.empty();
            pthread_mutex_unlock(&tdsPwrEventQueueMutexLock);
        }
        
        if(m_dsMgrPwrStopThread)
        {
             /* This case can enter if the de init is triggered which wants to exit the thread function 
                  and unlock happens after the end of while loop, it will not process any events for this case*/
            INT_DEBUG("[%s:%d]:dsMgrPwrEventHandlingThreadFunc Exiting due to m_dsMgrPwrStopThread true \n", __FUNCTION__, __LINE__);
            pthread_mutex_unlock(&tdsPwrEventMutexLock);
            break;
        }
        pthread_mutex_unlock(&tdsPwrEventMutexLock);

        
	/*  Directly read the contents of the PwrEvtQueue, release the lock when processing the data and relock it after 
            wards until the complete data in the queue is processed. This will ensure the lock retention period is less 
	    and all the updated data from the queue is processed */
	pthread_mutex_lock(&tdsPwrEventQueueMutexLock);
	while (!pwrEventQueue.empty())
	{
            DSMgr_Power_Event_State_t pwrEvent = pwrEventQueue.front();
            pwrEventQueue.pop();
            pthread_mutex_unlock(&tdsPwrEventQueueMutexLock);
            /* Release the Lock when processing the data */
	    dsMgrHandlePwrEventData(pwrEvent.currentState,pwrEvent.newState);
            /* Reacquire the Lock before checking the Queue contents in loop */
            pthread_mutex_lock(&tdsPwrEventQueueMutexLock);
        }
        /* Release the Lock either exit is due to empty directly or processing completed  */
        pthread_mutex_unlock(&tdsPwrEventQueueMutexLock);
    }
    return arg;
}

static void dsMgrHandlePwrEventData(const PowerController_PowerState_t currentState,
    const PowerController_PowerState_t newState)
{
    INT_INFO("Entering [%s]\r\n", __FUNCTION__);

    /* This is the Handler for Power State after Separation of the Context to a new thread context.
        This function is getting executed in a new thread context.with currentState and newState values*/
    INT_DEBUG("In [%s] currentState [%d]   newState[%d]\r\n", __FUNCTION__,currentState,newState);
    
    if (nullptr != ux) {
        // If ux_controller is supported, it will set up AV ports and LEDs in the below call.
        ux->applyPowerStateChangeConfig(newState, currentState);
    } else {
#ifndef DISABLE_LED_SYNC_IN_BOOTUP
        _SetLEDStatus(newState);
#endif
        _SetAVPortsPowerState(newState);
    }
    INT_DEBUG("Completed [%s]\r\n", __FUNCTION__);
}


void dsMgrInitPwrControllerEvt(void)
{
    INT_INFO("Entering [%s]\r\n", __FUNCTION__);

    /* Initialize two Mutexes and Wait Signal Variable. 
     *  1. One Lock mutex variable to Guard the Event Queue 
     *  2. Another mutex variable to Guard the Wait -Signal Condition, thread creation variable,stop thread
     *  Create the thread used to separate the context of the Event Callback 
     */
     
    pthread_mutex_init (&tdsPwrEventQueueMutexLock, NULL);
    pthread_mutex_init (&tdsPwrEventMutexLock, NULL);
    pthread_cond_init (&tdsPwrEventMutexCond, NULL);

    m_dsMgrPwrStopThread = false;
    if(pthread_create (&edsPwrEventHandlerThreadID, NULL, dsMgrPwrEventHandlingThreadFunc, NULL) != 0)
    {
        INT_ERROR("DSMgr dsMgrInitPwrControllerEvt Thread not created\r\n");
    }
}    

void dsMgrDeinitPwrControllerEvt(void) 
{    
    INT_INFO("Entering [%s]\r\n", __FUNCTION__);

    /* Lock the Queue, Clear the elements by looping through the Queue, Unlock  and Destroy the Mutex Lock  */
    pthread_mutex_lock(&tdsPwrEventMutexLock);
    /* Notify the Event thread function to Exit, send signal with stop thread true so that the thread exits   */
    m_dsMgrPwrStopThread=true;
    pthread_cond_signal(&tdsPwrEventMutexCond);
    pthread_mutex_unlock(&tdsPwrEventMutexLock);

    /* wait for the event thread handler to complete its operation */
    INT_DEBUG("[%s:%d]:Before Joining thread id\n", __FUNCTION__, __LINE__);
    pthread_join(edsPwrEventHandlerThreadID, NULL);
    INT_DEBUG("[%s:%d]:Completed  Joined thread id\n", __FUNCTION__, __LINE__);

    /* clean the queue with guarding mutex*/
    pthread_mutex_lock(&tdsPwrEventQueueMutexLock); 
    pwrEventQueue = std::queue<DSMgr_Power_Event_State_t>();
    pthread_mutex_unlock(&tdsPwrEventQueueMutexLock);


    /* Lock the Mutex, Destroy the Wait/Signal Condition Variable, Unlock the Mutex*/
    pthread_cond_destroy(&tdsPwrEventMutexCond);

    /* Destroy mutexes */
    pthread_mutex_destroy(&tdsPwrEventQueueMutexLock);
    pthread_mutex_destroy(&tdsPwrEventMutexLock);
}


