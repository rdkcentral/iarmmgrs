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
#include "safec_lib.h"

#define SAFEC_ERR_CHECK(safec_rc)      if(safec_rc != EOK) {\
ERR_CHK(safec_rc); \
goto Error; \
}

int main(int argc, char** argv)
{
#ifdef ENABLE_MFR_WIFI
    int i = 0;
    IARM_BUS_MFRLIB_API_WIFI_Credentials_Param_t param;
    errno_t safec_rc = -1;

    if(argc < 3)
    {
        printf("usage is : ./mfr_wifiSetCredentials <SSID Name> <SSID Password> <Security mode>\n");
        return -1;
    }

    printf("Client Entering\r\n");
    IARM_Bus_Init("wifiSetCredentials");
    IARM_Bus_Connect();

    param.requestType=WIFI_SET_CREDENTIALS;
    safec_rc = strcpy_s(param.wifiCredentials.cSSID, sizeof(param.wifiCredentials.cSSID), argv[1]);
    SAFEC_ERR_CHECK(safec_rc);

    safec_rc = strcpy_s(param.wifiCredentials.cPassword, sizeof(param.wifiCredentials.cPassword), argv[2]);
    SAFEC_ERR_CHECK(safec_rc);

    if(argc < 4){
        param.wifiCredentials.iSecurityMode=-1;
    }
    else{
        param.wifiCredentials.iSecurityMode=atoi(argv[3]);
    }

    if(IARM_RESULT_SUCCESS == IARM_Bus_Call(IARM_BUS_MFRLIB_NAME,IARM_BUS_MFRLIB_API_WIFI_Credentials,(void *)&param,sizeof(param)))
    {
        printf("IARM_Bus_Call Success...\n");
    }
    else
    {
        printf("Problem with IARM_Bus_Call \n");
    }

    while(getchar()!='x') {
        sleep(1);
    }

Error:
    IARM_Bus_Disconnect();
    IARM_Bus_Term();
    printf("Client Exiting\r\n");
#else
    printf("WIFI NOT SUPPORTED IN THIS DEVICE Exiting\r\n");
#endif
}
