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


#include "libIBus.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "mfrMgr.h"

int main(int argc, char** argv)
{
#ifdef ENABLE_MFR_WIFI
    int i = 0;
    IARM_BUS_MFRLIB_API_WIFI_Credentials_Param_t param;


    printf("Client Entering\r\n");
    IARM_Bus_Init("wifiGetCredentials");
    IARM_Bus_Connect();
    param.requestType=WIFI_GET_CREDENTIALS;

    if(IARM_RESULT_SUCCESS == IARM_Bus_Call(IARM_BUS_MFRLIB_NAME,IARM_BUS_MFRLIB_API_WIFI_Credentials,(void *)&param,sizeof(param)))
    {
        printf("IARM_Bus_Call Success...\n");
    }
    else
    {
        printf("Problem with IARM_Bus_Call \n");
    }
    printf("\n The value of SSID is %s  \n The value of Security mode is %d ",param.wifiCredentials.cSSID,param.wifiCredentials.iSecurityMode);

    while(getchar()!='x') {
        sleep(1);
    }

    IARM_Bus_Disconnect();
    IARM_Bus_Term();
    printf("Client Exiting\r\n");
#else
    printf("WIFI NOT SUPPORTED IN THIS DEVICE Exiting\r\n");
#endif
}
