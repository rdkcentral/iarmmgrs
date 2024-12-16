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


#include "irMgrInternal.h"
#ifdef __cplusplus 
extern "C" {
#endif
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "plat_ir.h"
#include "libIBus.h"
#ifdef __cplusplus 
}
#endif
#ifdef RDK_LOGGER_ENABLED

int b_rdk_logger_enabled = 0;

void logCallback(const char *buff)
{
    LOG("%s",buff);
}

#endif
#include "irMgr.h"
#include "cap.h"
#ifdef ENABLE_SD_NOTIFY
#include <systemd/sd-daemon.h>
#endif


static bool drop_root()
{
    bool ret = false,retval = false;
    cap_user appcaps = {{0, 0, 0, '\0', 0, 0, 0, '\0'}};
    ret = isBlocklisted();
    if(ret)
    {
         LOG("NonRoot feature is disabled\n");
    }
    else
    {
        LOG("NonRoot feature is enabled\n");
         appcaps.caps = NULL;
         appcaps.user_name = NULL;
         if(init_capability() != NULL) {
            if(drop_root_caps(&appcaps) != -1) {
               if(update_process_caps(&appcaps) != -1) {
                   read_capability(&appcaps);
                   retval = true;
               }
            }
         }
    }
    return retval;
}


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

#ifdef RDK_LOGGER_ENABLED

    if(rdk_logger_init(debugConfigFile) == 0) b_rdk_logger_enabled = 1;
    IARM_Bus_RegisterForLog(logCallback);

#endif
     if(!drop_root())
    {
         LOG("drop_root function failed!\n");
    }

    UINPUT_init(false);
    IRMgr_Register_uinput(UINPUT_GetDispatcher());

    IRMgr_Start(argc, argv);
    #ifdef ENABLE_SD_NOTIFY
       sd_notifyf(0, "READY=1\n"
              "STATUS=IRMgr is Successfully Initialized\n"
              "MAINPID=%lu", (unsigned long) getpid());
    #endif

#ifdef PID_FILE_PATH
#define xstr(s) str(s)
#define str(s) #s
    // write pidfile because sd_notify() does not work inside container
    IARM_Bus_WritePIDFile(xstr(PID_FILE_PATH) "/irmgr.pid");
#endif

    IRMgr_Loop();
    IRMgr_Stop();

    UINPUT_term();
    return 0;
}




/** @} */
/** @} */
