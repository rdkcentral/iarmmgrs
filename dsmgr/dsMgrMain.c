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
    DSMgr_Start();

    usleep(10000); // Sleep for 10 milliseconds to allow the d-bus to initialize
    #ifdef ENABLE_SD_NOTIFY
           sd_notifyf(0, "READY=1\n"
           "STATUS=DsMgr is Successfully Initialized\n"
              "MAINPID=%lu", (unsigned long) getpid());
    #endif

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
