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
* @defgroup disk
* @{
**/


#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "libIBus.h"
#include "diskMgr.h"
#include "diskMgrInternal.h"


IARM_Result_t DISKMgr_Start()
{

    LOG("Entering [%s] - [%s] - disabling io redirect buf\r\n", __FUNCTION__, IARM_BUS_DISKMGR_NAME);
	setvbuf(stdout, NULL, _IOLBF, 0);

	IARM_Bus_Init(IARM_BUS_DISKMGR_NAME);
    IARM_Bus_Connect();
    IARM_Bus_RegisterEvent(IARM_BUS_DISKMGR_EVENT_MAX);
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t DISKMgr_Loop()
{
    time_t curr = 0;
    while(1)
    {
        time(&curr);
        LOG("I-ARM Disk Mgr: HeartBeat at %s\r\n", ctime(&curr));
        sleep(2000);
    }
    return IARM_RESULT_SUCCESS;
}


IARM_Result_t DISKMgr_Stop(void)
{
    IARM_Bus_Disconnect();
    IARM_Bus_Term();
    return IARM_RESULT_SUCCESS;
}




/** @} */
/** @} */
