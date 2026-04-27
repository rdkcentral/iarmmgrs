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
* @defgroup dsmgr
* @{
**/


#include "dsMgrInternal.h"
#include "dsserverregisterlog.h"
#ifdef __cplusplus 
extern "C" {
#endif
#include <stdio.h>

#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include "libIBus.h"
#include <stdlib.h>
#ifdef __cplusplus 
}
#endif
#ifdef ENABLE_SD_NOTIFY
#include <systemd/sd-daemon.h>
#endif

#ifdef DSMGR_LOGGER_ENABLED

int b_rdk_logger_enabled = 0;

#endif

#ifdef RDK_LOGGER_ENABLED

void logCallback(const char *buff)
{
    INT_DEBUG("%s",buff);
}
void dslogCallback(int priority,const char *buff)
{
    if(priority == 0)
    {
        INT_INFO("%s",buff);
    }
    else if(priority == 1)
    {
        INT_WARN("%s",buff);
    }
    else if(priority == 2)
    {
        INT_ERROR("%s",buff);
    }
    else if(priority == 3)
    {
        INT_DEBUG("%s",buff);
    }
} 
  
#endif

int main(int argc, char *argv[])
{
    const char* debugConfigFile = NULL;
    int itr=0;

        while (itr < argc)
        {
                if(strcmp(argv[itr],"--debugconfig")==0)
                {
                        itr++;
                        if (itr < argc)
                        {
                                debugConfigFile = argv[itr];
                        }
                        else
                        {
                                break;
                        }
                }
                itr++;
        }

#if (defined(RDK_LOGGER_ENABLED) || defined(DSMGR_LOGGER_ENABLED))

    const char* PdebugConfigFile = NULL;
    const char* DSMGR_DEBUG_ACTUAL_PATH    = "/etc/debug.ini";
    const char* DSMGR_DEBUG_OVERRIDE_PATH  = "/opt/debug.ini";

        /* Init the logger */
    if (access(DSMGR_DEBUG_OVERRIDE_PATH, F_OK) != -1 ) {
        PdebugConfigFile = DSMGR_DEBUG_OVERRIDE_PATH;
    }
    else {
        PdebugConfigFile = DSMGR_DEBUG_ACTUAL_PATH;
    }

    if (rdk_logger_init(PdebugConfigFile) == 0) {
       b_rdk_logger_enabled = 1;
    }

//    IARM_Bus_RegisterForLog(logCallback);
//    DSServer_RegisterForLog(dslogCallback);

//    TODO: This could be enabled only after ensuring that all platforms have DSHal_RegisterForLog implemented 
//    in devicesettings/hal 
//
//    DSHal_RegisterForLog(dslogCallback);

#endif
    if (IARM_RESULT_SUCCESS != DSMgr_Start()) {
        INT_ERROR("DSMgr_Start() failed\n");
        return -1;
    }

    usleep(10000); // Sleep for 10 milliseconds to allow the d-bus to initialize

#ifdef DSMGR_TEST_NOTIFY_TIMEOUT
    /*
     * DSMGR_TEST_NOTIFY_TIMEOUT — instrumentation build flag.
     *
     * When defined, sd_notify(READY=1) is intentionally withheld so that
     * systemd's TimeoutStartSec fires and ExecStopPost/ds-reboot.sh can be
     * validated against the 'timeout' SERVICE_RESULT path.
     *
     * DSMgr_Loop() is still entered so the process stays alive long enough
     * for the timeout to trigger naturally (it will be killed by systemd).
     *
     * DO NOT enable in production builds.
     */
    INT_WARN("[TEST] DSMGR_TEST_NOTIFY_TIMEOUT enabled — "
             "skipping sd_notify(READY=1) to trigger systemd start-timeout.\n");
#else
    /* Runtime test hook: if trigger file exists, skip sd_notify(READY=1)
     * to simulate a start-timeout without needing a special build.
     * Usage on device:  touch /tmp/dsmgr_test_notify_timeout
     *                   systemctl restart dsmgr
     * The file is automatically removed after use (one-shot).
     */
    if (access("/tmp/dsmgr_notimeout", F_OK) == 0) {
        INT_WARN("[TEST] /tmp/dsmgr_notimeout present — "
                 "skipping sd_notify(READY=1) to trigger systemd start-timeout.\n");
        remove("/tmp/dsmgr_notimeout"); /* one-shot: remove after use */
    } else {
    #ifdef ENABLE_SD_NOTIFY
           sd_notifyf(0, "READY=1\n"
           "STATUS=DsMgr is Successfully Initialized\n"
              "MAINPID=%lu", (unsigned long) getpid());
    #endif
    }
#endif /* DSMGR_TEST_NOTIFY_TIMEOUT */

#ifdef PID_FILE_PATH
#define xstr(s) str(s)
#define str(s) #s
    // write pidfile because sd_notify() does not work inside container
    IARM_Bus_WritePIDFile(xstr(PID_FILE_PATH) "/dsmgr.pid");
#endif
    DSMgr_Loop();
    DSMgr_Stop();
    return 0;
}




/** @} */
/** @} */
