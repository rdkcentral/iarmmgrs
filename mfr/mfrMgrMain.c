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
* @defgroup mfr
* @{
**/


#include "mfrMgrInternal.h"

#ifdef __cplusplus 
extern "C" {
#endif
#include <stdio.h>
#ifdef ENABLE_SD_NOTIFY
#include <systemd/sd-daemon.h>
#endif
#ifdef __cplusplus 
}
#endif
#include "safec_lib.h"

#ifdef RDK_LOGGER_ENABLED

int b_rdk_logger_enabled = 0;

void logCallback(const char *buff)
{
    LOG("%s",buff);
}
#endif


int main(int argc, char *argv[])
{
    const char* debugConfigFile = NULL;
    int itr=0;
    errno_t safec_rc = -1;
    int ind = -1;
    int debug_len = strlen("--debugconfig");
        while (itr < argc)
        {
            safec_rc = strcmp_s( "--debugconfig", debug_len, argv[itr], &ind);
            ERR_CHK(safec_rc);
            if ((!ind) && (safec_rc == EOK))
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


    MFRLib_Start();

#ifdef ENABLE_SD_NOTIFY
    sd_notifyf(0, "READY=1\n"
              "STATUS=mfrMgrMain is Successfully Initialized\n"
              "MAINPID=%lu",
              (unsigned long) getpid());
#endif

    MFRLib_Loop();
    MFRLib_Stop();
}



/** @} */
/** @} */
