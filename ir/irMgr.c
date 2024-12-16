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
* @defgroup ir
* @{
**/


#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "libIARM.h"
#include "iarmUtil.h"
#include "plat_ir.h"
#include "comcastIrKeyCodes.h"
#include "irMgr.h"
#include "irMgrInternal.h"
#include "libIBus.h"
#include "sysMgr.h"
#include <pthread.h>
#include <errno.h>
#include "manager.hpp"
#include "rfcapi.h"

#define MAX_KEY_REPEATS 6
#define LAST_KEY_NUM_SECONDS 2
#define MICRO_SECS_PER_SEC   1000000L
#define MICROSECS_PER_MILLISEC  1000L

// Log translate event helper macro
#define LOG_TRANSLATE_EVENT(OLD_KEY,NEW_KEY) LOG("IR Key " #OLD_KEY "(0x%x) event is translated to " #NEW_KEY "(0x%x)\n", \
 (unsigned int)OLD_KEY, (unsigned int)NEW_KEY)

static pthread_t eRepeatThreadID;
static pthread_mutex_t tLastKeyMutex;
static pthread_mutex_t tKeySeqMutex;
static pthread_mutex_t tMutexLock;
static pthread_cond_t  tMutexCond;
static int gCurrentKeyCode = KED_UNDEFINEDKEY;
static int gCurrentKeySrcId = 0;
static const unsigned int gInitialWaitTimeout = 500;
static unsigned int gRepeatKeyInterval = 50;
static int numKeyRepeats = 0;
static int keyLogStatus = 1;

//For handling LAST key as an exit key when holding it down
static pthread_t eLASTKeyTimerThreadID;
static void* _LASTKeyTimerThreadFunc(void *arg);
bool bLastKeyUpReceived = false;
bool bLastTimerRunning = false;
bool bIgnoreLastKeyup = false;
int LASTkeySrc = IARM_BUS_IRMGR_KEYSRC_IR;
int lastKeyNumMSecs = 1500;

/*Default is IR*/
static bool bNeedRFKeyUp = false;

static void* _KeyRepeatThreadFunc (void *arg);
#ifdef XMP_TAG_OWNER_SUPPORT
   static void _IrKeyCallback(PLAT_irKey_metadata_t *irKey);
   static void _IrKeyPairing(PLAT_irKey_metadata *irKey);
   static void _IrKeyCallbackFrom(PLAT_irKey_metadata *irKey, int keySrc, unsigned int keySrcId);
   static void _IrInputKeyEventHandler(PLAT_irKey_metadata_t *irKey, int keySrc, unsigned int keySrcId);
#else
   static void _IrKeyCallback(int keyType, int keyCode);
   static void _IrKeyCallbackFrom(int keyType, int keyCode, int keySrc, unsigned int keySrcId);
   static void _IrInputKeyEventHandler(int keyType, int keyCode, int keySrc, unsigned int keySrcId);
#endif
static void _logEventHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len);

#ifndef NO_RF4CE
#ifdef USE_UNIFIED_CONTROL_MGR_API_1
#include "ctrlm_ipc.h"
#include "ctrlm_ipc_rcu.h"
    static void _ctrlmEventHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len);
#else
#warning "No RF4CE API defined"
#endif  // USE_UNIFIED_CONTROL_MGR_API_1
#endif  // NO_RF4CE

static IARM_Result_t _SetRepeatInterval(void *arg);
static IARM_Result_t _GetRepeatInterval(void *arg);

static uinput_dispatcher_t udispatcher = NULL;

IARM_Result_t IRMgr_Register_uinput(uinput_dispatcher_t f)
{
    udispatcher = f;
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t IRMgr_Start(int argc, char *argv[])
{
	char *settingsFile = NULL;

	if (argc == 2) settingsFile = argv[1];

	LOG("Entering [%s] - [%s] - disabling io redirect buf\r\n", __FUNCTION__, IARM_BUS_IRMGR_NAME);
	setvbuf(stdout, NULL, _IOLBF, 0);

    PLAT_API_INIT();

    IARM_Bus_Init(IARM_BUS_IRMGR_NAME);
    IARM_Bus_Connect();
    IARM_Bus_RegisterCall(IARM_BUS_IRMGR_API_SetRepeatInterval, _SetRepeatInterval);
    IARM_Bus_RegisterCall(IARM_BUS_IRMGR_API_GetRepeatInterval, _GetRepeatInterval);
    IARM_Bus_RegisterEventHandler(IARM_BUS_SYSMGR_NAME,IARM_BUS_SYSMGR_EVENT_KEYCODE_LOGGING_CHANGED,_logEventHandler);
	
#ifndef NO_RF4CE
  #ifdef USE_UNIFIED_CONTROL_MGR_API_1
    IARM_Bus_RegisterEventHandler(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_RCU_IARM_EVENT_KEY_PRESS, _ctrlmEventHandler);
    IARM_Bus_RegisterEventHandler(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_RCU_IARM_EVENT_KEY_GHOST, _ctrlmEventHandler);
    IARM_Bus_RegisterEventHandler(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_RCU_IARM_EVENT_FUNCTION, _ctrlmEventHandler);
  #else
  #warning "No RF4CE API defined"
  #endif  // USE_UNIFIED_CONTROL_MGR_API_1
#endif  // NO_RF4CE

    IARM_BUS_SYSMGR_KEYCodeLoggingInfo_Param_t param;
    IARM_Bus_Call(IARM_BUS_SYSMGR_NAME, IARM_BUS_SYSMGR_API_GetKeyCodeLoggingPref, (void *)&param, sizeof(param));
    keyLogStatus = param.logStatus;

    pthread_mutex_init (&tKeySeqMutex, NULL);
    pthread_mutex_init (&tMutexLock, NULL);
    pthread_mutex_init (&tLastKeyMutex, NULL);
    pthread_cond_init (&tMutexCond, NULL);
    pthread_create (&eRepeatThreadID, NULL, _KeyRepeatThreadFunc, NULL);

#ifdef XMP_TAG_OWNER_SUPPORT
    PLAT_API_RegisterIRKeyCallbackExtended(_IrKeyCallback);
#else
    PLAT_API_RegisterIRKeyCallback(_IrKeyCallback);
#endif
    IARM_Bus_RegisterEvent(IARM_BUS_IRMGR_EVENT_MAX);

     try {
        device::Manager::Initialize();
    }
    catch (...){
        LOG("Exception Caught during [device::Manager::Initialize]\r\n");
    }
  
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t IRMgr_Loop()
{
    time_t curr = 0;
    while(1)
    {
        time(&curr);
        PLAT_API_LOOP();
        LOG("I-ARM IR Mgr: HeartBeat at %s\r\n", ctime(&curr));
        sleep(300);
    }
	return IARM_RESULT_SUCCESS;
}


IARM_Result_t IRMgr_Stop(void)
{
#ifndef NO_RF4CE
  #ifdef USE_UNIFIED_CONTROL_MGR_API_1
    IARM_Bus_RemoveEventHandler(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_RCU_IARM_EVENT_KEY_PRESS, _ctrlmEventHandler);
    IARM_Bus_RemoveEventHandler(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_RCU_IARM_EVENT_KEY_GHOST, _ctrlmEventHandler);
    IARM_Bus_RemoveEventHandler(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_RCU_IARM_EVENT_FUNCTION, _ctrlmEventHandler);
  #endif  // USE_UNIFIED_CONTROL_MGR_API_1
#endif  // NO_RF4CE

    IARM_Bus_UnRegisterEventHandler(IARM_BUS_SYSMGR_NAME,IARM_BUS_SYSMGR_EVENT_KEYCODE_LOGGING_CHANGED);
     try {
        device::Manager::DeInitialize();
    }
    catch (...){
        LOG("Exception Caught during [device::Manager::DeInitialize]\r\n");
    }  

    IARM_Bus_Disconnect();
    IARM_Bus_Term();
    PLAT_API_TERM();
    pthread_mutex_destroy (&tKeySeqMutex);
    pthread_mutex_destroy (&tMutexLock);
    pthread_mutex_destroy (&tLastKeyMutex);
    pthread_cond_destroy  (&tMutexCond);

    return IARM_RESULT_SUCCESS;
}

/**
 * @brief Sets key repeat interval
 *
 * This functions sets/updates the interval between two KET_KEYREPEAT events
 *
 * @param callCtx: Context to the caller function
 *
 * @param methodID: Method to be invoke
 *
 * @param arg: Specifies the timeout to be set
 *
 * @param serial: Handshake code to share with
 *
 * @return None
 */
static IARM_Result_t _SetRepeatInterval(void *arg)
{
	IARM_Bus_IRMgr_SetRepeatInterval_Param_t *param = (IARM_Bus_IRMgr_SetRepeatInterval_Param_t *)arg;
    __TIMESTAMP(); LOG ("The timeout interval is set to %dms\n", param->timeout);
    gRepeatKeyInterval = param->timeout;
    return IARM_RESULT_SUCCESS;
}

/**
 * @brief Gets key repeat interval
 *
 * This functions sets/updates the interval between two KET_KEYREPEAT events
 *
 * @param callCtx: Context to the caller function
 *
 * @param methodID: Method to be invoke
 *
 * @param arg: Receives the timeout that is set
 *
 * @param serial: Handshake code to share with
 *
 * @return None
 */
static IARM_Result_t _GetRepeatInterval(void *arg)
{
	IARM_Bus_IRMgr_GetRepeatInterval_Param_t *param = (IARM_Bus_IRMgr_GetRepeatInterval_Param_t *)arg;
    __TIMESTAMP(); LOG ("The timeout interval is set to %dms\n", gRepeatKeyInterval);
    param->timeout = gRepeatKeyInterval;
    return IARM_RESULT_SUCCESS;
}

#ifndef XMP_TAG_OWNER_SUPPORT
static void _IrKeyCallback(int keyType, int keyCode)
{

	_IrKeyCallbackFrom(keyType, keyCode, IARM_BUS_IRMGR_KEYSRC_IR, 0x0);

}
#else
static void _IrKeyCallback(PLAT_irKey_metadata_t *irKey)
{
   __TIMESTAMP(); LOG("IR Key received (%x, %x, %x, %x)\n", irKey->tag,
                                                            irKey->owner,
                                                            irKey->type,
                                                            irKey->code);
#if defined PLATCO || defined PLATCO_REMOTE_SUPPORT
   if (irKey->tag != XMP_TAG_PLATCO && irKey->tag < XMP_TAG_PLATCOV2)
   {
      __TIMESTAMP(); LOG("IR Key tag is invalid (0x%x != 0x%x or 0x%x)\n", irKey->tag, XMP_TAG_PLATCO, XMP_TAG_PLATCOV2);
      return;
   }
   else
   {
      switch (irKey->owner)
      {
         case XMP_OWNER_NORMAL:
            _IrKeyCallbackFrom(irKey, IARM_BUS_IRMGR_KEYSRC_IR, 0x0);
            break;
         case XMP_OWNER_PAIRING:
            _IrKeyPairing(irKey);
            break;
         case XMP_OWNER_UNDEFINED:
         default:
            __TIMESTAMP(); LOG("Unknown owner field %x\n", irKey->owner);
            break;
      }
   }
#else
   bool tag_is_valid   = false;
   bool owner_is_valid = false;

   switch (irKey->owner)
   {
      case XMP_OWNER_NORMAL:
      case XMP_OWNER_PAIRING:
         owner_is_valid = true;
         break;
      case XMP_OWNER_UNDEFINED:
      default:
         __TIMESTAMP(); LOG("Unknown owner field %x\n", irKey->owner);
         break;
   }

   switch (irKey->tag)
   {
      case XMP_TAG_COMCAST:
      case XMP_TAG_XR11V2:
      case XMP_TAG_XR15V1:
      case XMP_TAG_XR15V2:
      case XMP_TAG_XR16V1:
      case XMP_TAG_XRAV1:
      case XMP_TAG_XR20V1:
         tag_is_valid = true;
         break;
      case XMP_TAG_PLATCO:
      case XMP_TAG_PLATCOV2:
         break;
      default:
         __TIMESTAMP(); LOG("Unknown tag field %x\n", irKey->tag);
         break;
   }

   if (tag_is_valid && owner_is_valid) {
      _IrKeyCallbackFrom(irKey, IARM_BUS_IRMGR_KEYSRC_IR, 0x0);
   }
#endif
}

static void _IrKeyPairing(PLAT_irKey_metadata_t *irKey)
{
   IARM_Result_t                              result;
   ctrlm_main_iarm_call_status_t              status;
   ctrlm_iarm_call_StartPairWithCode_params_t pair_data;

   if ( (irKey->tag != XMP_TAG_PLATCO && irKey->tag < XMP_TAG_PLATCOV2) || irKey->owner != XMP_OWNER_PAIRING)
   {
      return;
   }

   memset(&status,    0, sizeof(ctrlm_main_iarm_call_status_t));
   memset(&pair_data, 0, sizeof(ctrlm_iarm_call_StartPairWithCode_params_t));
   status.api_revision    = CTRLM_MAIN_IARM_BUS_API_REVISION;
   pair_data.api_revision = CTRLM_MAIN_IARM_BUS_API_REVISION;
   pair_data.pair_code    = irKey->code;

   result = IARM_Bus_Call(CTRLM_MAIN_IARM_BUS_NAME,
                          CTRLM_MAIN_IARM_CALL_STATUS_GET,
                          (void *) &status,
                          sizeof(status));

   if (result != IARM_RESULT_SUCCESS)
   {
      __TIMESTAMP(); LOG("%s: Failed = %d\n", CTRLM_MAIN_IARM_CALL_STATUS_GET,
                                              result);
      return;
   }

   for (int i = 0; i < status.network_qty; i++)
   {
      if (status.networks[i].type == CTRLM_NETWORK_TYPE_BLUETOOTH_LE)
      {
         pair_data.network_id = status.networks[i].id;

         result = IARM_Bus_Call(CTRLM_MAIN_IARM_BUS_NAME,
                                CTRLM_MAIN_IARM_CALL_START_PAIR_WITH_CODE,
                                (void *) &pair_data,
                                sizeof(ctrlm_iarm_call_StartPairWithCode_params_t));

         if (result != IARM_RESULT_SUCCESS) 
         {
            __TIMESTAMP(); LOG("%s: Failed = %d\n", CTRLM_MAIN_IARM_CALL_START_PAIR_WITH_CODE,
                                                    result);
         }
      }
   }
}
#endif

#ifndef XMP_TAG_OWNER_SUPPORT
static void _IrKeyCallbackFrom(int keyType, int keyCode, int keySrc, unsigned int keySrcId)
#else
static void _IrKeyCallbackFrom(PLAT_irKey_metadata_t *irKey,  int keySrc, unsigned int keySrcId)
#endif
{
#ifdef XMP_TAG_OWNER_SUPPORT
    int keyType = irKey->type;
    int keyCode = irKey->code;
#endif
    /**
     * Ignore all the repeat keys that is trigger by MAF and instead handle only PRESS & RELEASE.
     * The _KeyRepeatThreadFunc will manage posting KET_KEYREPEAT keys.
     */
    if (keyType == KET_KEYDOWN)
    {
        pthread_mutex_lock(&tMutexLock);
        gCurrentKeyCode = keyCode;
        gCurrentKeySrcId = keySrcId;

#ifndef XMP_TAG_OWNER_SUPPORT
        _IrInputKeyEventHandler (keyType, keyCode, keySrc, keySrcId);
#else
        _IrInputKeyEventHandler (irKey, keySrc, keySrcId);
#endif
        pthread_cond_signal(&tMutexCond);
        pthread_mutex_unlock(&tMutexLock);
    }
    else if (keyType == KET_KEYUP)
    {
        pthread_mutex_lock(&tMutexLock);
        gCurrentKeyCode = KED_UNDEFINEDKEY;
        gCurrentKeySrcId = keySrcId;

#ifndef XMP_TAG_OWNER_SUPPORT
        _IrInputKeyEventHandler (keyType, keyCode, keySrc, keySrcId);
#else
        _IrInputKeyEventHandler (irKey, keySrc, keySrcId);
#endif
        pthread_cond_signal(&tMutexCond);
        pthread_mutex_unlock(&tMutexLock);
    }
}

/**
 * @brief Calback funtion to that is registered with MAF to get notified for the key press
 *
 * This functions receives KET_KEYDOWN, KET_KEYREPEAT and KET_KEYUP and signals the thread.
 *
 * @param keyType: Type of event (KEYDOWN or KEYREPEAT or KEYUP)
 *
 * @param keyCode: Contains the key code that is trigger by the user
 *
 * @param keySrc: Contains the key source that is trigger by the user
 * 
 * @return None
 */
#ifndef XMP_TAG_OWNER_SUPPORT
static void _IrInputKeyEventHandler(int keyType, int keyCode , int keySrc, unsigned int keySrcId)
#else
static void _IrInputKeyEventHandler(PLAT_irKey_metadata_t *irKey, int keySrc, unsigned int keySrcId)
#endif
{
     static IARM_Bus_IRMgr_EventData_t prevEventData = { { { KET_KEYUP , KED_UNDEFINEDKEY, XMP_TAG_UNDEFINED, XMP_OWNER_UNDEFINED, 0, IARM_BUS_IRMGR_KEYSRC_IR } } } ;
#ifdef XMP_TAG_OWNER_SUPPORT
     int keyType = irKey->type;
     int keyCode = irKey->code;
#endif
     static bool xr15_or_newer_notify_call = false;

     if(0 != keyLogStatus)
	 LOG("COMCAST IR Key (%x, %x, %x) From Remote Device received\r\n", keyType, keyCode, keySrcId);
	
   
    switch(keyCode)
    {

        case KED_RF_PAIR_GHOST:
#ifndef USE_UNIFIED_CONTROL_MGR_API_1
        {
            LOG("Auto-binding key. Don't pass on.\n");
            return;
        }
#endif
        case KED_VOLUME_OPTIMIZE:
        case KED_XR11_NOTIFY:
        case KED_XR15V1_NOTIFY:
        case KED_XR16V1_NOTIFY:
        case KED_SCREEN_BIND_NOTIFY:
        case KED_VOLUMEUP:
        case KED_VOLUMEDOWN:
        case KED_MUTE:
        case KED_TVPOWER:
        case KED_INPUTKEY:
        case KED_PUSH_TO_TALK:
        case KED_XR2V3:
        case KED_XR5V2:
        case KED_XR11V2:
        case KED_XR13:
        {
            if (KET_KEYDOWN == keyType)
            {
                if (keyCode == KED_PUSH_TO_TALK && xr15_or_newer_notify_call == false)
                {
#ifndef XMP_TAG_OWNER_SUPPORT
                    _IrInputKeyEventHandler(KET_KEYDOWN, KED_XR11_NOTIFY, keySrc, keySrcId);
#else
                    irKey->type = KET_KEYDOWN;
                    irKey->code = KED_XR11_NOTIFY;
                    _IrInputKeyEventHandler(irKey, keySrc, keySrcId);
#endif
                }

                LOG("IR Control Event: keyCode: 0x%x, keySrc: 0x%x.\n", keyCode, keySrc);
                pthread_mutex_lock(&tKeySeqMutex);
                IARM_Bus_IRMgr_EventData_t eventData;
                eventData.data.irkey.keyType = KET_KEYDOWN;
                eventData.data.irkey.keyCode = keyCode;
#ifdef XMP_TAG_OWNER_SUPPORT
                eventData.data.irkey.keyTag = irKey->tag;
                eventData.data.irkey.keyOwner = irKey->owner;
#else
                eventData.data.irkey.keyTag = XMP_TAG_UNDEFINED;
                eventData.data.irkey.keyOwner = XMP_OWNER_UNDEFINED;
#endif
                eventData.data.irkey.keySrc = (IARM_Bus_IRMgr_KeySrc_t)keySrc;
                eventData.data.irkey.keySourceId = keySrcId;
                IARM_Bus_BroadcastEvent(IARM_BUS_IRMGR_NAME, (IARM_EventId_t) IARM_BUS_IRMGR_EVENT_CONTROL, (void *)&eventData, sizeof(eventData));
                pthread_mutex_unlock(&tKeySeqMutex);
            }
            return;             
        }
        // Handle LAST
        case KED_LAST:
        {
            //On LAST keydown
            if (KET_KEYDOWN == keyType)
            {
                pthread_mutex_lock(&tKeySeqMutex);

                //Clear flag - LAST keyup not received
                bLastKeyUpReceived = false;

                //Clear flag - look for LAST keyup
                bIgnoreLastKeyup = false;

                //Keep the keySrc needed for the timer
                LASTkeySrc = keySrc;

                //If the timer is not already running...
                if(bLastTimerRunning == false)
                {
                    //Start LAST key timer thread
#ifndef XMP_TAG_OWNER_SUPPORT
                    pthread_create (&eLASTKeyTimerThreadID, NULL, _LASTKeyTimerThreadFunc, NULL);
#else
                    pthread_create (&eLASTKeyTimerThreadID, NULL, _LASTKeyTimerThreadFunc, (void *)irKey);
#endif
                }

                pthread_mutex_unlock(&tKeySeqMutex);
            }
            //ON LAST keyup
            else if (KET_KEYUP == keyType)
            {
                pthread_mutex_lock(&tKeySeqMutex);

                //Set flag that LAST keyup was received
                bLastKeyUpReceived = true;

                //If we're not ignoring the LAST keyup, send the LAST keydown and keyup
                if(bIgnoreLastKeyup == false)
                {
                    IARM_Bus_IRMgr_EventData_t eventData;
                    LOG("Sending LAST key, not the EXIT key\n");

                    // Send Key Down
                    eventData.data.irkey.keyType = KET_KEYDOWN;
                    eventData.data.irkey.keyCode = KED_LAST;
#ifdef XMP_TAG_OWNER_SUPPORT
                    eventData.data.irkey.keyTag = irKey->tag;
                    eventData.data.irkey.keyOwner = irKey->owner;
#else
                    eventData.data.irkey.keyTag = XMP_TAG_UNDEFINED;
                    eventData.data.irkey.keyOwner = XMP_OWNER_UNDEFINED;
#endif
                    eventData.data.irkey.isFP = 0; 
                    eventData.data.irkey.keySrc = (_IRMgr_KeySrc_t)keySrc;
                    eventData.data.irkey.keySourceId = keySrcId;
                    if (udispatcher){
                        udispatcher(eventData.data.irkey.keyCode, eventData.data.irkey.keyType, eventData.data.irkey.keySrc);
                    }
                    IARM_Bus_BroadcastEvent(IARM_BUS_IRMGR_NAME, (IARM_EventId_t) IARM_BUS_IRMGR_EVENT_IRKEY, (void *)&eventData, sizeof(eventData));
        
                    // Send Key UP
                    eventData.data.irkey.keyType = KET_KEYUP;
                    if (udispatcher){
                        udispatcher(eventData.data.irkey.keyCode, eventData.data.irkey.keyType, eventData.data.irkey.keySrc);
                    }
                    IARM_Bus_BroadcastEvent(IARM_BUS_IRMGR_NAME, (IARM_EventId_t) IARM_BUS_IRMGR_EVENT_IRKEY, (void *)&eventData, sizeof(eventData));
                }

                pthread_mutex_unlock(&tKeySeqMutex);
            }

            return;
        }
        case  KED_SELECT:
            if ((KET_KEYDOWN == keyType) && (xr15_or_newer_notify_call == false) && (keySrc == IARM_BUS_IRMGR_KEYSRC_IR))
            {
#ifndef XMP_TAG_OWNER_SUPPORT
                _IrInputKeyEventHandler(KET_KEYDOWN, KED_XR11_NOTIFY, keySrc, keySrcId);
#else
                irKey->type = KET_KEYDOWN;
                irKey->code = KED_XR11_NOTIFY;
                _IrInputKeyEventHandler(irKey, keySrc, keySrcId);
#endif
            }
            break;
        case KED_XR15V1_PUSH_TO_TALK:
            LOG_TRANSLATE_EVENT(KED_XR15V1_PUSH_TO_TALK, KED_PUSH_TO_TALK);
            if (KET_KEYDOWN == keyType)
            {
                xr15_or_newer_notify_call = true;
#ifndef XMP_TAG_OWNER_SUPPORT
                _IrInputKeyEventHandler(KET_KEYDOWN, KED_XR15V1_NOTIFY, keySrc, keySrcId);
#else
                irKey->type = KET_KEYDOWN;
                irKey->code = KED_XR15V1_NOTIFY;
                _IrInputKeyEventHandler(irKey, keySrc, keySrcId);
#endif
                gCurrentKeyCode = KED_PUSH_TO_TALK;
            }
            else
            {
                xr15_or_newer_notify_call = false;
                gCurrentKeyCode = KED_UNDEFINEDKEY;
            }
#ifndef XMP_TAG_OWNER_SUPPORT
            _IrInputKeyEventHandler(keyType, KED_PUSH_TO_TALK, keySrc, keySrcId);
#else
            irKey->type = keyType;
            irKey->code = KED_PUSH_TO_TALK;
            _IrInputKeyEventHandler(irKey, keySrc, keySrcId);
#endif
            return;
        case KED_XR15V1_SELECT:
            LOG_TRANSLATE_EVENT(KED_XR15V1_SELECT, KED_SELECT);
            if (KET_KEYDOWN == keyType)
            {
                xr15_or_newer_notify_call = true;
#ifndef XMP_TAG_OWNER_SUPPORT
                _IrInputKeyEventHandler(KET_KEYDOWN, KED_XR15V1_NOTIFY, keySrc, keySrcId);
#else
                irKey->type = KET_KEYDOWN;
                irKey->code = KED_XR15V1_NOTIFY;
                _IrInputKeyEventHandler(irKey, keySrc, keySrcId);
#endif
                gCurrentKeyCode = KED_SELECT;
            }
            else
            {
                xr15_or_newer_notify_call = false;
                gCurrentKeyCode = KED_UNDEFINEDKEY;
            }
#ifndef XMP_TAG_OWNER_SUPPORT
            _IrInputKeyEventHandler(keyType, KED_SELECT, keySrc, keySrcId);
#else
            irKey->type = keyType;
            irKey->code = KED_SELECT;
            _IrInputKeyEventHandler(irKey, keySrc, keySrcId);
#endif
            return;
        case KED_XR16V1_PUSH_TO_TALK:
            LOG_TRANSLATE_EVENT(KED_XR16V1_PUSH_TO_TALK, KED_PUSH_TO_TALK);
            if (KET_KEYDOWN == keyType)
            {
                xr15_or_newer_notify_call = true;
#ifndef XMP_TAG_OWNER_SUPPORT
                _IrInputKeyEventHandler(KET_KEYDOWN, KED_XR16V1_NOTIFY, keySrc, keySrcId);
#else
                irKey->type = KET_KEYDOWN;
                irKey->code = KED_XR16V1_NOTIFY;
                _IrInputKeyEventHandler(irKey, keySrc, keySrcId);
#endif
                gCurrentKeyCode = KED_PUSH_TO_TALK;
            }
            else
            {
                xr15_or_newer_notify_call = false;
                gCurrentKeyCode = KED_UNDEFINEDKEY;
            }
#ifndef XMP_TAG_OWNER_SUPPORT
            _IrInputKeyEventHandler(keyType, KED_PUSH_TO_TALK, keySrc, keySrcId);
#else
            irKey->type = keyType;
            irKey->code = KED_PUSH_TO_TALK;
            _IrInputKeyEventHandler(irKey, keySrc, keySrcId);
#endif
            return;
        case KED_XR16V1_SELECT:
            LOG_TRANSLATE_EVENT(KED_XR16V1_SELECT, KED_SELECT);
            if (KET_KEYDOWN == keyType)
            {
                xr15_or_newer_notify_call = true;
#ifndef XMP_TAG_OWNER_SUPPORT
                _IrInputKeyEventHandler(KET_KEYDOWN, KED_XR16V1_NOTIFY, keySrc, keySrcId);
#else
                irKey->type = KET_KEYDOWN;
                irKey->code = KED_XR16V1_NOTIFY;
                _IrInputKeyEventHandler(irKey, keySrc, keySrcId);
#endif
                gCurrentKeyCode = KED_SELECT;
            }
            else
            {
                xr15_or_newer_notify_call = false;
                gCurrentKeyCode = KED_UNDEFINEDKEY;
            }
#ifndef XMP_TAG_OWNER_SUPPORT
            _IrInputKeyEventHandler(keyType, KED_SELECT, keySrc, keySrcId);
#else
            irKey->type = keyType;
            irKey->code = KED_SELECT;
            _IrInputKeyEventHandler(irKey, keySrc, keySrcId);
#endif
            return;

        default: 
            break;
    }

    if (!(keyCode == KED_UNDEFINEDKEY))
    {
        pthread_mutex_lock(&tKeySeqMutex);
        IARM_Bus_IRMgr_EventData_t eventData;
        if ((keyCode != prevEventData.data.irkey.keyCode) && 
            (keyType == KET_KEYDOWN) && (prevEventData.data.irkey.keyType != KET_KEYUP))
        {
            eventData.data.irkey.keyType = KET_KEYUP;
            eventData.data.irkey.keyCode = prevEventData.data.irkey.keyCode;
#ifdef XMP_TAG_OWNER_SUPPORT
            eventData.data.irkey.keyTag = irKey->tag;
            eventData.data.irkey.keyOwner = irKey->owner;
#else
            eventData.data.irkey.keyTag = XMP_TAG_UNDEFINED;
            eventData.data.irkey.keyOwner = XMP_OWNER_UNDEFINED;
#endif
            eventData.data.irkey.isFP = prevEventData.data.irkey.isFP; 
            eventData.data.irkey.keySrc = prevEventData.data.irkey.keySrc;
            eventData.data.irkey.keySourceId = keySrcId;
            if (udispatcher){
                udispatcher(keyCode, keyType, keySrc);
            }
            IARM_Bus_BroadcastEvent(IARM_BUS_IRMGR_NAME, (IARM_EventId_t) IARM_BUS_IRMGR_EVENT_IRKEY, (void *)&eventData, sizeof(eventData));
        }
        eventData.data.irkey.keyType = keyType;
        eventData.data.irkey.keyCode = keyCode;
                eventData.data.irkey.keySourceId = keySrcId;
		eventData.data.irkey.isFP = (keyCode == KED_POWER) ? 1:0; /*1 -> Front Panel , 0 -> IR or RF*/
		eventData.data.irkey.keySrc = (keyCode == KED_POWER) ? IARM_BUS_IRMGR_KEYSRC_FP:(_IRMgr_KeySrc_t)keySrc; 
#ifdef XMP_TAG_OWNER_SUPPORT
        eventData.data.irkey.keyTag = irKey->tag;
        eventData.data.irkey.keyOwner = irKey->owner;
#else
        eventData.data.irkey.keyTag = XMP_TAG_UNDEFINED;
        eventData.data.irkey.keyOwner = XMP_OWNER_UNDEFINED;
#endif
       
		/*Default to IR*/
		prevEventData.data.irkey.isFP = (keyCode == KED_POWER) ? 1:0; /*1 -> Front Panel , 0 -> IR or RF*/
		prevEventData.data.irkey.keySrc = (keyCode == KED_POWER) ? IARM_BUS_IRMGR_KEYSRC_FP:(_IRMgr_KeySrc_t)keySrc; 
		prevEventData.data.irkey.keyType = keyType;
                prevEventData.data.irkey.keySourceId = keySrcId;
        prevEventData.data.irkey.keyCode = keyCode;
#ifdef XMP_TAG_OWNER_SUPPORT
        prevEventData.data.irkey.keyTag = irKey->tag;
        prevEventData.data.irkey.keyOwner = irKey->owner;
#else
        eventData.data.irkey.keyTag = XMP_TAG_UNDEFINED;
        eventData.data.irkey.keyOwner = XMP_OWNER_UNDEFINED;
#endif
#ifdef _DISABLE_KEY_POWEROFF    //XITHREE-7832
        bool broadcastEvent = true;
        static int prevKeyType = keyType;

        if(keyCode == KED_FP_POWER) {
            switch(keyType) {
                case KET_KEYREPEAT:
                    if(prevKeyType == KET_KEYDOWN) {
                        keyType = KET_KEYUP;
                    }
                    else {
                        broadcastEvent = false;
                    }
                    break;

                case KET_KEYUP:
                    if(prevKeyType != KET_KEYDOWN) {
                        broadcastEvent = false;
                    }
                    break;

                default:
                    break;

            }
        }
        prevKeyType = keyType;
        eventData.data.irkey.keyType = keyType;
        if(!broadcastEvent) {
            LOG("Discarding key event!!! keyCode: %x keyType: %x\n",keyCode,keyType);
            pthread_mutex_unlock(&tKeySeqMutex);
            return;
        }
        LOG("Broadcasting key event!!! keyCode: %x keyType: %x\n",eventData.data.irkey.keyCode,eventData.data.irkey.keyType);
#endif
        if (udispatcher){
            udispatcher(keyCode, keyType, keySrc);
        }
        IARM_Bus_BroadcastEvent(IARM_BUS_IRMGR_NAME, (IARM_EventId_t) IARM_BUS_IRMGR_EVENT_IRKEY, (void *)&eventData, sizeof(eventData));
        pthread_mutex_unlock(&tKeySeqMutex);
    }
}

/**
 * @brief Calback funtion to that is registered with MAF to get notified for the key press
 *
 * This function Waits for given timeout for the key to be released. Upon timeout, returns true
 *
 * @param uTimeoutValue: Time in milliseconds to wait for.
 *
 * @return true or false
 */
static bool isKeyReleased (unsigned int uTimeoutValue)
{
    int ret = 0;
    struct timespec   ts;
    struct timeval    tv;

    gettimeofday(&tv, NULL);

    /** Adapt absolute time to timeval first */
    tv.tv_sec += (tv.tv_usec + uTimeoutValue * 1000L ) / 1000000;
    tv.tv_usec = (tv.tv_usec + uTimeoutValue * 1000L ) % 1000000;

    /** Convert from timeval to timespec */
    ts.tv_sec  = tv.tv_sec;
    ts.tv_nsec = tv.tv_usec * 1000;

    __TIMESTAMP(); LOG ("Block for %dms & look for KEY RELEASE\n", uTimeoutValue);
    ret = pthread_cond_timedwait(&tMutexCond, &tMutexLock, &ts);

    /** Verify the return code to decide on the key release */
    if (ret == ETIMEDOUT)
    {
        __TIMESTAMP(); LOG ("Key NOT released yet. Ready to post REPEAT keys\n");
        return false;
    }
    else
    {
        __TIMESTAMP(); LOG ("Key released..\n");
        return true;
    }
}



static void* _LASTKeyTimerThreadFunc(void *arg)
{
#ifdef XMP_TAG_OWNER_SUPPORT
    PLAT_irKey_metadata_t *irKey = (PLAT_irKey_metadata_t *)arg;
#endif
    int i;
    useconds_t usecondsToWait = 50000;
    bool bSendExitKey = true;

    //Set flag that the timer is running
    pthread_mutex_lock(&tKeySeqMutex);
    bLastTimerRunning = true;
    pthread_mutex_unlock(&tKeySeqMutex);

    //Wait for timer to expire or for the LAST keyup 
    for (i=0; i<((lastKeyNumMSecs*MICROSECS_PER_MILLISEC) / usecondsToWait); i++)
    {
        usleep(usecondsToWait);

        //If a LAST keyup was received...
        if(bLastKeyUpReceived == true)
        {
            bSendExitKey = false;
            break;
        }
    }

    pthread_mutex_lock(&tKeySeqMutex);

    //Set flag to ignore LAST key up
    bIgnoreLastKeyup = true;

    //If we timed out, then send the EXIT keydown and keyup
    if(bSendExitKey == true)
    {
        IARM_Bus_IRMgr_EventData_t eventData;

        // Send Key Down
        LOG("Sending EXIT key instead of LAST\n");
        eventData.data.irkey.keyType = KET_KEYDOWN;
        eventData.data.irkey.keyCode = KED_EXIT;
#ifdef XMP_TAG_OWNER_SUPPORT
        eventData.data.irkey.keyTag = irKey->tag;
        eventData.data.irkey.keyOwner = irKey->owner;
#else
        eventData.data.irkey.keyTag = XMP_TAG_UNDEFINED;
        eventData.data.irkey.keyOwner = XMP_OWNER_UNDEFINED;
#endif
        eventData.data.irkey.isFP = 0; 
        eventData.data.irkey.keySrc = (_IRMgr_KeySrc_t)LASTkeySrc;
        eventData.data.irkey.keySourceId = gCurrentKeySrcId;
        if (udispatcher){
            udispatcher(eventData.data.irkey.keyCode, eventData.data.irkey.keyType, eventData.data.irkey.keySrc);
        }
        IARM_Bus_BroadcastEvent(IARM_BUS_IRMGR_NAME, (IARM_EventId_t) IARM_BUS_IRMGR_EVENT_IRKEY, (void *)&eventData, sizeof(eventData));

        // Send Key UP
        eventData.data.irkey.keyType = KET_KEYUP;
        if (udispatcher){
            udispatcher(eventData.data.irkey.keyCode, eventData.data.irkey.keyType, eventData.data.irkey.keySrc);
        }
        IARM_Bus_BroadcastEvent(IARM_BUS_IRMGR_NAME, (IARM_EventId_t) IARM_BUS_IRMGR_EVENT_IRKEY, (void *)&eventData, sizeof(eventData));
    }

    //Reset flag that the timer is no longer running
    bLastTimerRunning = false;
    pthread_mutex_unlock(&tKeySeqMutex);
    return NULL;
}

/**
 * @brief Thread entry fuction to post Repeat Keys
 *
 * This functions handles posting a KET_KEYREPEAT with specified timeout
 *
 * @param void pointer (NULL)
 *
 * @return void pointer (NULL)
 */
static void* _KeyRepeatThreadFunc (void *arg)
{
    bool isTimedout = false;

    /** Loop in..! */
    while (1)
    {
        pthread_mutex_lock(&tMutexLock);
        /** When the thread starts, it should wait for the signal..
         *  The signal will be posted by the very first down key
         */
        if (!isTimedout)
        {
            __TIMESTAMP(); LOG ("Block unconditionally & look for KEY PRESS\n");
            pthread_cond_wait(&tMutexCond, &tMutexLock);

            /**
             * At this point of time, the KET_DOWN is received. So wait for 500ms to see
             * the is released or not
             */
            if (isKeyReleased(gInitialWaitTimeout))
            {
                /**
                 * When key released within the timeframe of 500ms, no need to post repeat key.
                 * Just go back and wait for next key press event
                 */
                pthread_mutex_unlock(&tMutexLock);
                continue;
            }
            else
            {
                /**
                 * Post atleast one REPEAT key when the timeout is set to zero by the
                 * UIDev_SetRepeatInterval() by the clients
                 */
#ifndef XMP_TAG_OWNER_SUPPORT
                _IrInputKeyEventHandler (KET_KEYREPEAT, gCurrentKeyCode, IARM_BUS_IRMGR_KEYSRC_IR, gCurrentKeySrcId);
#else
                PLAT_irKey_metadata_t irKey = {KET_KEYREPEAT, gCurrentKeyCode, XMP_TAG_COMCAST, XMP_OWNER_NORMAL};
                _IrInputKeyEventHandler (&irKey, IARM_BUS_IRMGR_KEYSRC_IR, gCurrentKeySrcId);
#endif
            }
        }

        /**
         * If the Interval is not set do not start the timer for repeat.
         * Only one repeat should be posted thats done from the signaling function itself.
         */
        if (gRepeatKeyInterval != 0)
        {
            if (isKeyReleased(gRepeatKeyInterval))
            {
                isTimedout = false;
                __TIMESTAMP(); LOG ("Key pressed.. Do not post Repeat key...\n");
            }
            else
            {
                if (KED_UNDEFINEDKEY != gCurrentKeyCode)
                {
                    isTimedout = true;

                    //If the last key was RF...
                    if( bNeedRFKeyUp )
                    {
                        numKeyRepeats++;

                        //More than 6 repeats means that we lost connection to rf4ceMgr
                        // numKeyRepeats also gets reset to 0 in the RF event handler
                        // so the key repeat code will get executed
                        if(numKeyRepeats > MAX_KEY_REPEATS)
                        {
                            __TIMESTAMP(); LOG ("******** Lost connection to rf4ceMgr. Sending Up key...\n");
#ifndef XMP_TAG_OWNER_SUPPORT
                            _IrInputKeyEventHandler(KET_KEYUP, gCurrentKeyCode, IARM_BUS_IRMGR_KEYSRC_IR, gCurrentKeySrcId);
#else
                            PLAT_irKey_metadata_t irKey = {KET_KEYUP, gCurrentKeyCode, XMP_TAG_COMCAST, XMP_OWNER_NORMAL};
                            _IrInputKeyEventHandler(&irKey, IARM_BUS_IRMGR_KEYSRC_IR, gCurrentKeySrcId);
#endif
                            gCurrentKeyCode = KED_UNDEFINEDKEY;
                            numKeyRepeats = 0;
                            bNeedRFKeyUp = false;
                        }
                        //Else send a repeat
                        else
                        {
                            __TIMESTAMP(); LOG ("Post Repeat RF key...\n");
#ifndef XMP_TAG_OWNER_SUPPORT
                            _IrInputKeyEventHandler(KET_KEYREPEAT, gCurrentKeyCode,IARM_BUS_IRMGR_KEYSRC_RF, gCurrentKeySrcId);
#else
                            PLAT_irKey_metadata_t irKey = {KET_KEYREPEAT, gCurrentKeyCode, XMP_TAG_COMCAST, XMP_OWNER_NORMAL};
                            _IrInputKeyEventHandler(&irKey, IARM_BUS_IRMGR_KEYSRC_RF, gCurrentKeySrcId);
#endif
                        } 
                    }
                    //Else this is IR
                    else
                    {
                        __TIMESTAMP(); LOG ("Post Repeat key...\n");
#ifndef XMP_TAG_OWNER_SUPPORT
                        _IrInputKeyEventHandler(KET_KEYREPEAT, gCurrentKeyCode,IARM_BUS_IRMGR_KEYSRC_IR, gCurrentKeySrcId);
#else
                        PLAT_irKey_metadata_t irKey = {KET_KEYREPEAT, gCurrentKeyCode, XMP_TAG_COMCAST, XMP_OWNER_NORMAL};
                        _IrInputKeyEventHandler(&irKey, IARM_BUS_IRMGR_KEYSRC_IR, gCurrentKeySrcId);
#endif
                    }
                }
                else
                {
                    isTimedout = false;
                }
            }
        }
        pthread_mutex_unlock(&tMutexLock);
    }

    return arg;
}

/**
 * @brief Event Handler for RF Key Events
 *
 * This function is a Event handler for RF key events. Post the Key to IARM IR key handler.
 *
 * @param void pointer (Event ID , Data and Length)
 *
 * @return void pointer (NULL)
 */
#ifndef NO_RF4CE
#ifdef USE_UNIFIED_CONTROL_MGR_API_1
static void _ctrlmEventHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len)
{
    int keyCode = 0;
    int keyType = 0;
    unsigned int remoteId = 0;

    if (eventId == CTRLM_RCU_IARM_EVENT_KEY_PRESS) {
        ctrlm_rcu_iarm_event_key_press_t* keyEvent = (ctrlm_rcu_iarm_event_key_press_t*)data;
        if (keyEvent != NULL) {
            if (keyEvent->api_revision != CTRLM_RCU_IARM_BUS_API_REVISION) {
                LOG("CTRLM key event: ERROR - Wrong CTRLM API revision - expected %d, event is %d!!",
                     CTRLM_RCU_IARM_BUS_API_REVISION, keyEvent->api_revision);
                return;
            }
     
            if(0 != keyLogStatus)
            {
                LOG("CTRLM key event: network_id: %d, network_type: %d, controller_id: %d, key_status: %d, key_code: 0x%02X.\n",
                   keyEvent->network_id, keyEvent->network_type, keyEvent->controller_id, keyEvent->key_status, keyEvent->key_code);
            }

            if (keyEvent->key_status != CTRLM_KEY_STATUS_INVALID) {
                remoteId = keyEvent->controller_id;
                // Translate CTRLM key status into Comcast RDK key types
                switch (keyEvent->key_status) {
                    case CTRLM_KEY_STATUS_DOWN:     keyType = KET_KEYDOWN; break;
                    case CTRLM_KEY_STATUS_UP:       keyType = KET_KEYUP; break;
                    case CTRLM_KEY_STATUS_REPEAT:   keyType = KET_KEYREPEAT; break;
                }
                // Translate CTRLM key codes into Comcast RDK key codes
                switch (keyEvent->key_code) {
                    case CTRLM_KEY_CODE_OK:             keyCode = KED_SELECT; break;

                    case CTRLM_KEY_CODE_UP_ARROW:       keyCode = KED_ARROWUP; break;
                    case CTRLM_KEY_CODE_DOWN_ARROW:     keyCode = KED_ARROWDOWN; break;
                    case CTRLM_KEY_CODE_LEFT_ARROW:     keyCode = KED_ARROWLEFT; break;
                    case CTRLM_KEY_CODE_RIGHT_ARROW:    keyCode = KED_ARROWRIGHT; break;

                    case CTRLM_KEY_CODE_MENU:           keyCode = KED_MENU; break;
                    case CTRLM_KEY_CODE_DVR:            keyCode = KED_MYDVR; break;
                    case CTRLM_KEY_CODE_FAV:            keyCode = KED_FAVORITE; break;
                    case CTRLM_KEY_CODE_EXIT:           keyCode = KED_EXIT; break;
//                    case CTRLM_KEY_CODE_HOME:           keyCode = KED_????; break;

                    case CTRLM_KEY_CODE_DIGIT_0:        keyCode = KED_DIGIT0; break;
                    case CTRLM_KEY_CODE_DIGIT_1:        keyCode = KED_DIGIT1; break;
                    case CTRLM_KEY_CODE_DIGIT_2:        keyCode = KED_DIGIT2; break;
                    case CTRLM_KEY_CODE_DIGIT_3:        keyCode = KED_DIGIT3; break;
                    case CTRLM_KEY_CODE_DIGIT_4:        keyCode = KED_DIGIT4; break;
                    case CTRLM_KEY_CODE_DIGIT_5:        keyCode = KED_DIGIT5; break;
                    case CTRLM_KEY_CODE_DIGIT_6:        keyCode = KED_DIGIT6; break;
                    case CTRLM_KEY_CODE_DIGIT_7:        keyCode = KED_DIGIT7; break;
                    case CTRLM_KEY_CODE_DIGIT_8:        keyCode = KED_DIGIT8; break;
                    case CTRLM_KEY_CODE_DIGIT_9:        keyCode = KED_DIGIT9; break;
                    case CTRLM_KEY_CODE_PERIOD:         keyCode = KED_PERIOD; break;

                    case CTRLM_KEY_CODE_RETURN:         keyCode = KED_ENTER; break;
                    case CTRLM_KEY_CODE_CH_UP:          keyCode = KED_CHANNELUP; break;
                    case CTRLM_KEY_CODE_CH_DOWN:        keyCode = KED_CHANNELDOWN; break;
                    case CTRLM_KEY_CODE_LAST:           keyCode = KED_LAST; break;
                    case CTRLM_KEY_CODE_LANG:           keyCode = KED_LANGUAGE; break;
                    case CTRLM_KEY_CODE_INPUT_SELECT:   keyCode = KED_INPUTKEY; break;
                    case CTRLM_KEY_CODE_INFO:           keyCode = KED_INFO; break;
                    case CTRLM_KEY_CODE_HELP:           keyCode = KED_HELP; break;
                    case CTRLM_KEY_CODE_PAGE_UP:        keyCode = KED_PAGEUP; break;
                    case CTRLM_KEY_CODE_PAGE_DOWN:      keyCode = KED_PAGEDOWN; break;
//                    case CTRLM_KEY_CODE_MOTION:         keyCode = KED_????; break;
                    case CTRLM_KEY_CODE_SEARCH:         keyCode = KED_SEARCH; break;
                    case CTRLM_KEY_CODE_LIVE:           keyCode = KED_LIVE; break;
//                    case CTRLM_KEY_CODE_HD_ZOOM:        keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_SHARE:          keyCode = KED_????; break;

                    case CTRLM_KEY_CODE_TV_POWER:       keyCode = KED_TVPOWER; break;
                    case CTRLM_KEY_CODE_VOL_UP:         keyCode = KED_VOLUMEUP; break;
                    case CTRLM_KEY_CODE_VOL_DOWN:       keyCode = KED_VOLUMEDOWN; break;
                    case CTRLM_KEY_CODE_MUTE:           keyCode = KED_MUTE; break;
                    case CTRLM_KEY_CODE_PLAY:           keyCode = KED_PLAY; break;
                    case CTRLM_KEY_CODE_STOP:           keyCode = KED_STOP; break;
                    case CTRLM_KEY_CODE_PAUSE:          keyCode = KED_PAUSE; break;
                    case CTRLM_KEY_CODE_RECORD:         keyCode = KED_RECORD; break;
                    case CTRLM_KEY_CODE_REWIND:         keyCode = KED_REWIND; break;
                    case CTRLM_KEY_CODE_FAST_FORWARD:   keyCode = KED_FASTFORWARD; break;
//                    case CTRLM_KEY_CODE_30_SEC_SKIP:    keyCode = KED_????; break;
                    case CTRLM_KEY_CODE_REPLAY:         keyCode = KED_REPLAY; break;

                    case CTRLM_KEY_CODE_SWAP:           keyCode = KED_DISPLAY_SWAP; break;  // Swap display, or channel?
                    case CTRLM_KEY_CODE_ON_DEMAND:      keyCode = KED_ONDEMAND; break;
                    case CTRLM_KEY_CODE_GUIDE:          keyCode = KED_GUIDE; break;
                    case CTRLM_KEY_CODE_PUSH_TO_TALK:   keyCode = KED_PUSH_TO_TALK; break;

                    case CTRLM_KEY_CODE_PIP_ON_OFF:     keyCode = KED_PINP_TOGGLE; break;
                    case CTRLM_KEY_CODE_PIP_MOVE:       keyCode = KED_PINP_MOVE; break;
                    case CTRLM_KEY_CODE_PIP_CH_UP:      keyCode = KED_PINP_CHUP; break;
                    case CTRLM_KEY_CODE_PIP_CH_DOWN:    keyCode = KED_PINP_CHDOWN; break;

//                    case CTRLM_KEY_CODE_LOCK:           keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_DAY_PLUS:       keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_DAY_MINUS:      keyCode = KED_????; break;
                    case CTRLM_KEY_CODE_PLAY_PAUSE:     keyCode = KED_PLAY; break;
//                    case CTRLM_KEY_CODE_STOP_VIDEO:     keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_MUTE_MIC:       keyCode = KED_????; break;

                    case CTRLM_KEY_CODE_POWER_TOGGLE:   keyCode = KED_RF_POWER; break;
                    case CTRLM_KEY_CODE_POWER_OFF:      keyCode = KED_DISCRETE_POWER_STANDBY; break;
                    case CTRLM_KEY_CODE_POWER_ON:       keyCode = KED_DISCRETE_POWER_ON; break;

                    case CTRLM_KEY_CODE_OCAP_B:         keyCode = KED_KEYB; break;
                    case CTRLM_KEY_CODE_OCAP_C:         keyCode = KED_KEYC; break;
                    case CTRLM_KEY_CODE_OCAP_D:         keyCode = KED_KEYD; break;
                    case CTRLM_KEY_CODE_OCAP_A:         keyCode = KED_KEYA; break;

                    case CTRLM_KEY_CODE_CC:             keyCode = KED_CLOSED_CAPTIONING; break;
//                    case CTRLM_KEY_CODE_PROFILE:        keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_CALL:           keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_HOLD:           keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_END:            keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_VIEWS:          keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_SELF_VIEW:      keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_ZOOM_IN:        keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_ZOOM_OUT:       keyCode = KED_????; break;
                    case CTRLM_KEY_CODE_BACKSPACE:      keyCode = KED_DELETE; break;
//                    case CTRLM_KEY_CODE_LOCK_UNLOCK:    keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_CAPS:           keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_ALT:            keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_SPACE:          keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_WWW_DOT:        keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_DOT_COM:        keyCode = KED_????; break;

//                    case CTRLM_KEY_CODE_UPPER_A:        keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_UPPER_B:        keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_UPPER_C:        keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_UPPER_D:        keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_UPPER_E:        keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_UPPER_F:        keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_UPPER_G:        keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_UPPER_H:        keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_UPPER_I:        keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_UPPER_J:        keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_UPPER_K:        keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_UPPER_L:        keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_UPPER_M:        keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_UPPER_N:        keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_UPPER_O:        keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_UPPER_P:        keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_UPPER_Q:        keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_UPPER_R:        keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_UPPER_S:        keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_UPPER_T:        keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_UPPER_U:        keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_UPPER_V:        keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_UPPER_W:        keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_UPPER_X:        keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_UPPER_Y:        keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_UPPER_Z:        keyCode = KED_????; break;

//                    case CTRLM_KEY_CODE_LOWER_A:        keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_LOWER_B:        keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_LOWER_C:        keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_LOWER_D:        keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_LOWER_E:        keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_LOWER_F:        keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_LOWER_G:        keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_LOWER_H:        keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_LOWER_I:        keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_LOWER_J:        keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_LOWER_K:        keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_LOWER_L:        keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_LOWER_M:        keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_LOWER_N:        keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_LOWER_O:        keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_LOWER_P:        keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_LOWER_Q:        keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_LOWER_R:        keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_LOWER_S:        keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_LOWER_T:        keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_LOWER_U:        keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_LOWER_V:        keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_LOWER_W:        keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_LOWER_X:        keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_LOWER_Y:        keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_LOWER_Z:        keyCode = KED_????; break;

//                    case CTRLM_KEY_CODE_QUESTION:       keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_EXCLAMATION:    keyCode = KED_????; break;
                    case CTRLM_KEY_CODE_POUND:          keyCode = KED_POUND; break;
//                    case CTRLM_KEY_CODE_DOLLAR:         keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_PERCENT:        keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_AMPERSAND:      keyCode = KED_????; break;
                    case CTRLM_KEY_CODE_ASTERISK:       keyCode = KED_PROGRAM; break;
//                    case CTRLM_KEY_CODE_LEFT_PAREN:     keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_RIGHT_PAREN:    keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_PLUS:           keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_MINUS:          keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_EQUAL:          keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_FORWARD_SLASH:  keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_UNDERSCORE:     keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_DOUBLE_QUOTE:   keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_COLON:          keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_SEMICOLON:      keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_COMMERCIAL_AT:  keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_APOSTROPHE:     keyCode = KED_????; break;
//                    case CTRLM_KEY_CODE_COMMA:          keyCode = KED_????; break;
                    case CTRLM_KEY_CODE_INVALID:        keyCode = KED_UNDEFINEDKEY; break;

                    default:                            keyCode = KED_UNDEFINEDKEY; break;
                }
            } else {
                LOG("CTRLM key event: ERROR: INVALID key status!!\n");
                return;
            }
        } else {
            LOG("CTRLM key event: ERROR: NULL event data!!\n");
            return;
        }
    } else if (eventId == CTRLM_RCU_IARM_EVENT_KEY_GHOST) {
        ctrlm_rcu_iarm_event_key_ghost_t* ghostEvent = (ctrlm_rcu_iarm_event_key_ghost_t*)data;
        if (ghostEvent != NULL) {
            if (ghostEvent->api_revision != CTRLM_RCU_IARM_BUS_API_REVISION) {
                LOG("CTRLM ghost key event: ERROR - Wrong CTRLM API revision - expected %d, event is %d!!",
                     CTRLM_RCU_IARM_BUS_API_REVISION, ghostEvent->api_revision);
                return;
            }
            LOG("CTRLM ghost code event: network_id: %d, network_type: %d, controller_id: %d, ghost_code: %d.\n",
                ghostEvent->network_id, ghostEvent->network_type, ghostEvent->controller_id, ghostEvent->ghost_code);
            // Translate the ghost code into a fake keystroke
            remoteId = ghostEvent->controller_id;
            switch (ghostEvent->ghost_code) {
                case CTRLM_RCU_GHOST_CODE_VOLUME_UNITY_GAIN:
                    keyType = KET_KEYDOWN;
                    keyCode = KED_VOLUME_OPTIMIZE;
                    break;
                // We don't translate the power ghost codes to fake keystrokes.
                default:
                    // Do nothing
                    return;
            }
        } else {
            LOG("CTRLM ghost code event: ERROR: NULL event data!!\n");
            return;
        }
    } else if (eventId == CTRLM_RCU_IARM_EVENT_FUNCTION) {
        ctrlm_rcu_iarm_event_function_t* functionEvent = (ctrlm_rcu_iarm_event_function_t*)data;
        if (functionEvent != NULL) {
            if (functionEvent->api_revision != CTRLM_RCU_IARM_BUS_API_REVISION) {
                LOG("CTRLM function event: ERROR - Wrong CTRLM API revision - expected %d, event is %d!!",
                     CTRLM_RCU_IARM_BUS_API_REVISION, functionEvent->api_revision);
                return;
            }
            LOG("CTRLM rcu function event: network_id: %d, network_type: %d, controller_id: %d, function: %d, value: 0x%08X.\n",
                functionEvent->network_id, functionEvent->network_type, functionEvent->controller_id, functionEvent->function, functionEvent->value);
            // We're not generating a fake keystroke for the SETUP function event, or any other, right now.
            return;
        } else {
            LOG("CTRLM rcu function event: ERROR: NULL event data!!\n");
            return;
        }
    } else {
        LOG("CTRLM key event: ERROR: bad event type %d!!\n", eventId);
        return;
    }
    /*
            * The key code  should not driver the Power standby
            * It shall behave like normal Remote All Power Key
            * Fix to  XONE-10023
    */
    if (keyCode == KED_POWER)
    {
        keyCode = KED_RF_POWER;
    }

    //Reset number of key repeats
    numKeyRepeats = 0;

    //If key up then we don't need artificially send a key up if rf4ceMgr crashes
    if( keyType == KET_KEYUP )
        bNeedRFKeyUp = false;
    else
        bNeedRFKeyUp = true;

#ifndef XMP_TAG_OWNER_SUPPORT
    _IrKeyCallbackFrom(keyType, keyCode, IARM_BUS_IRMGR_KEYSRC_RF, remoteId);
#else
    PLAT_irKey_metadata_t irKey = {keyType, keyCode, XMP_TAG_COMCAST, XMP_OWNER_NORMAL};
    _IrKeyCallbackFrom(&irKey, IARM_BUS_IRMGR_KEYSRC_RF, remoteId);
#endif
}

#endif  // USE_UNIFIED_CONTROL_MGR_API_1
#endif  // NO_RF4CE





static void _logEventHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len)
{
    unsigned long messageLength=0;
    if ((strcmp(owner, IARM_BUS_SYSMGR_NAME) == 0) && (eventId == IARM_BUS_SYSMGR_EVENT_KEYCODE_LOGGING_CHANGED)) {
        IARM_Bus_SYSMgr_EventData_t *eventData = (IARM_Bus_SYSMgr_EventData_t*)data;
        LOG("Received Keycodelogging event with log status: %d \n", eventData->data.keyCodeLogData.logStatus);
        keyLogStatus = eventData->data.keyCodeLogData.logStatus;
    }
}
/** @} */
/** @} */
