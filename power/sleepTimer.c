/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2018 RDK Management
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

#include <glib.h>
#include <stdio.h>
#include <pthread.h>

#define _TIME_BITS 64
#include <time.h>

#include "libIBus.h"
#include "pwrlogger.h"
#include "pwrMgr.h"
#include "pwrMgrInternal.h"
#ifdef SLEEPTIMER_USE_CECSTANDBY
#include "CecIARMBusMgr.h"
#endif
#include "safec_lib.h"

static GMainLoop *mainloop = NULL;
static guint timerSource = 0;
static time_t timerEnd = 0;
static pthread_mutex_t  _lock = PTHREAD_MUTEX_INITIALIZER;
#define LOCK() pthread_mutex_lock(&_lock)
#define UNLOCK() pthread_mutex_unlock(&_lock)

#ifdef SLEEPTIMER_USE_CECSTANDBY
static void SendCECStandby(void)
{
    errno_t rc = -1;
    IARM_Bus_CECMgr_Send_Param_t dataToSend;
    unsigned char buf[] = {0x30, 0x36}; //standby msg, from TUNER to TV
    memset(&dataToSend, 0, sizeof(dataToSend));
    dataToSend.length = sizeof(buf);
    rc = memcpy_s(dataToSend.data,sizeof(dataToSend.data), buf, dataToSend.length);
    if(rc!=EOK)
    {
        ERR_CHK(rc);
    }
    LOG("SleepTimer send CEC Standby\r\n");
    IARM_Bus_Call(IARM_BUS_CECMGR_NAME,IARM_BUS_CECMGR_API_Send,(void *)&dataToSend, sizeof(dataToSend));
}
#endif

static gboolean _onSleepTimerFire(void *)
{
    LOCK();
    LOG("SleepTimer %d start at %ld, fired at %ld, set standby\r\n", timerSource, timerEnd, time(NULL));
    timerEnd = 0;
    timerSource = 0;
    UNLOCK();
#ifdef SLEEPTIMER_USE_CECSTANDBY
    SendCECStandby();
#else
    LOG("No STANDBY action implemented\r\n");
#endif
    return FALSE;
}

static IARM_Result_t _SetSleepTimer(void *arg)
{
    IARM_Bus_PWRMgr_SleepTimer_Param_t *param = (IARM_Bus_PWRMgr_SleepTimer_Param_t *)arg;

    if(param != NULL)
    {
        LOCK();
        if (param->start == 1) {
            time(&timerEnd);
            timerEnd += (time_t)param->time;
            LOG("SleepTimer (re)set :%.3f, curr source=%d, to fire at %ld\r\n", param->time, timerSource, timerEnd);
            if (timerSource) {
                g_source_remove(timerSource);
                timerSource = 0;
            }
            timerSource = g_timeout_add_seconds((guint)(param->time), _onSleepTimerFire, NULL);
        }
        else if (param->start == 0) {
            LOG("SleepTimer %d cancelled\r\n", timerSource);
            if (timerSource) {
                g_source_remove(timerSource);
            }
            timerSource = 0;
            timerEnd = 0;
        }
        UNLOCK();
        return IARM_RESULT_SUCCESS;
    }
    return IARM_RESULT_IPCCORE_FAIL;
}

static IARM_Result_t _GetSleepTimer(void *arg)
{
    IARM_Bus_PWRMgr_SleepTimer_Param_t *param = (IARM_Bus_PWRMgr_SleepTimer_Param_t *)arg;
    param->time = 0.;
    param->start = 0;
    LOCK();
    if (timerSource) {
        time_t now = time(NULL);
        param->time = (double)((now < timerEnd) ? (timerEnd - now) :0);
        param->start = (param->time ? 1 : 0);
    }
    UNLOCK();
    LOG("SleepTimer %d has %.3lf remaining\r\n", param->start, param->time);  //CID:128053,127752,97127,95718 - Print_Args
    return IARM_RESULT_SUCCESS;
}

void IARM_Bus_PWRMGR_RegisterSleepTimerAPIs(void *context)
{
    LOG("SleepTimer PwrMgr register APIs\r\n");
    mainloop = (GMainLoop*)context;
    IARM_Bus_RegisterCall(IARM_BUS_PWRMGR_API_SetSleepTimer, _SetSleepTimer);
    IARM_Bus_RegisterCall(IARM_BUS_PWRMGR_API_GetSleepTimer, _GetSleepTimer);
}

