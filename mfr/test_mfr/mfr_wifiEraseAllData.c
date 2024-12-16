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
#include <malloc.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "libIARMCore.h"
#include "libIBus.h"
#include "mfrMgr.h"

int main()
{
#ifdef ENABLE_MFR_WIFI
        IARM_Result_t ret;
        IARM_Bus_Init("Tool-mfrWifiEraseAllData");
        IARM_Bus_Connect();
        printf("Tool-mfrWifiEraseAllData Entering\r\n");

    ret = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME, IARM_BUS_MFRLIB_API_WIFI_EraseAllData, 0, 0);

    if(ret != IARM_RESULT_SUCCESS)
    {
        printf("Call failed for %s: error code:%d\n","IARM_BUS_MFRLIB_API_WIFI_EraseAllData",ret);
    }
    else
    {
        printf("Call succeed for %s: error code:%d\n","IARM_BUS_MFRLIB_API_WIFI_EraseAllData",ret);
    }


        IARM_Bus_Disconnect();
        IARM_Bus_Term();
        printf("Tool-mfrWifiEraseAllData Exiting\r\n");
#else
    printf("WIFI NOT SUPPORTED IN THIS DEVICE Exiting\r\n");
#endif
}


/** @} */
/** @} */
  
