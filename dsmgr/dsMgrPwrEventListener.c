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

static DSMgr_Standby_Video_State_t g_standby_video_port_setting[MAX_NUM_VIDEO_PORTS];
static dsMgrProductTraits::ux_controller * ux = nullptr;
static pthread_mutex_t dsLock = PTHREAD_MUTEX_INITIALIZER;

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

#define IARM_BUS_Lock(lock) pthread_mutex_lock(&dsLock)
#define IARM_BUS_Unlock(lock) pthread_mutex_unlock(&dsLock)

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

    IARM_Bus_RegisterCall(IARM_BUS_DSMGR_API_SetStandbyVideoState, _SetStandbyVideoState);
    IARM_Bus_RegisterCall(IARM_BUS_DSMGR_API_GetStandbyVideoState, _GetStandbyVideoState);
    IARM_Bus_RegisterCall(IARM_BUS_DSMGR_API_SetAvPortState, _SetAvPortState);
    IARM_Bus_RegisterCall(IARM_BUS_DSMGR_API_SetLEDStatus, _SetLEDState);
    IARM_Bus_RegisterCall(IARM_BUS_DSMGR_API_SetRebootConfig, _SetRebootConfig);

    /* Check for Power Controller Connection and if failure, start a new thread and wait until connection established 
     * if connection is eastablished, proceed with fetching and Getting values from Power control and initialize*/
    if(POWER_CONTROLLER_ERROR_NONE == PowerController_Connect())
    {
        INT_DEBUG("PowerController_Connect is Success \r\n");
        dsMgrPwrControllerFetchNinitStateValues();
    }
    else
    {
        INT_ERROR("DSMgr PowerController_Connect Failed Retrying in Thread\r\n");
        dsMgrPwrCtrlEstablishConnection();
    }
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
    INT_INFO("%s: Entry  \r\n",__FUNCTION__);

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
    INT_INFO("Entering [%s] powerState:%d \r\n", __FUNCTION__,powerState);

    try
    {
        if (POWER_STATE_ON != powerState)
        {
            if (POWER_STATE_OFF != powerState)
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
		    device::AudioOutputPort aPort = device::Host::getInstance().getAudioOutputPort(*audioPort);
                    memset(&getHandle, 0, sizeof(getHandle));
                    getHandle.type = *audioPort;
                    getHandle.index = 0;
                    _dsGetAudioPort(&getHandle);

                    memset(&setMode, 0, sizeof(setMode));
                    setMode.handle = getHandle.handle;
		    strncpy(setMode.portName, aPort.getName().c_str(), 31);
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
		    device::AudioOutputPort aPort = device::Host::getInstance().getAudioOutputPort(*audioPort);
                    memset(&getHandle, 0, sizeof(getHandle));
                    getHandle.type = *audioPort;
                    getHandle.index = 0;
                    _dsGetAudioPort(&getHandle);

                    memset(&setMode, 0, sizeof(setMode));
                    setMode.handle = getHandle.handle;
		    strncpy(setMode.portName, aPort.getName().c_str(), 31);
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
    if((NULL != param) && (NULL == param->port))
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
    dsMgrStandbyVideoStateParam_t *param = (dsMgrStandbyVideoStateParam_t *)arg;
    if((NULL != param) && (NULL == param->port))
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
    INT_DEBUG("[%s] _state=[%d]  powerState[%d]\r\n", __FUNCTION__,_state,powerState);
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
        pthread_cond_wait(&tdsPwrEventMutexCond, &tdsPwrEventMutexLock);
        if(m_dsMgrPwrStopThread)
        {
             /* This case can enter if the de init is trigerred which wants to exit the thread function 
                  and unlock happens after the end of while loop, it will not process any events for this case*/
            INT_DEBUG("[%s:%d]:dsMgrPwrEventHandlingThreadFunc Exiting due to m_dsMgrPwrStopThread true \n",__FUNCTION__,__LINE__);
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
    INT_DEBUG("[%s:%d]:Before Joining thread id\n",__FUNCTION__,__LINE__);
    pthread_join(edsPwrEventHandlerThreadID, NULL);
    INT_DEBUG("[%s:%d]:Completed  Joined thread id\n",__FUNCTION__,__LINE__);

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


