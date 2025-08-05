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


#include <stdio.h>
#include <memory.h>
#include <dlfcn.h>
#include <unistd.h>
#include "mfrMgrInternal.h"
#include "mfrMgr.h"
#include "libIARMCore.h"
#include "mfrTypes.h"
#include "nxclient.h"
#include "safec_lib.h"

IARM_Result_t MFRLib_Start(void)
{
    
    NxClient_JoinSettings joinSettings;
    NxClient_AllocSettings allocSettings;
    int rc;
    NxClient_GetDefaultJoinSettings(&joinSettings);
    errno_t safec_rc = -1;
    safec_rc = strcpy_s(joinSettings.name, sizeof(joinSettings.name), "MFRMgr");
    if(safec_rc != EOK)
    {
        ERR_CHK(safec_rc);
        return IARM_RESULT_INVALID_PARAM;
    }
    printf("register nxclient MFRMgr\r\n");
    rc = NxClient_Join(&joinSettings);
    printf("register nxclient MFR  Mgr - %d\r\n",rc);

    return mfrMgr_start();
}

IARM_Result_t MFRLib_Stop(void)
{
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t MFRLib_Loop()
{
    struct timeval tv;
    struct tm tm_info;
    char buf[64] = {'\0'};

    while(1)
    {
        gettimeofday(&tv, NULL);
	localtime_r(&tv.tv_sec, &tm_info);
	strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_info);
	printf("I-ARM MFR Lib: HeartBeat at %s.%06ld\r\n", buf, (long)tv.tv_usec);
        sleep(60);
    }
    return IARM_RESULT_SUCCESS;
}


/** @} */
/** @} */
