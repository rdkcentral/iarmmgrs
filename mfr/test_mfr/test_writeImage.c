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

#define SAFEC_ERR_CHECK(safec_rc)    if(safec_rc != EOK) {\
ERR_CHK(safec_rc);\
goto Error; \
}


IARM_Result_t _callback(void *arg)
{
    IARM_Bus_MFRLib_CommonAPI_WriteImageCb_Param_t *param = (IARM_Bus_MFRLib_CommonAPI_WriteImageCb_Param_t *)arg;
    printf("Result :: %s \n",param->cbData);
    return (IARM_Result_t)0;
}

int main(int argc, char** argv)
{
    int i = 0;
    IARM_Bus_MFRLib_WriteImage_Param_t param;
    errno_t safec_rc = -1;
    if(argc < 3)
    {
        printf("usage is : ./test_writeImage <image_path> <signed_image_name>\n");
        return -1;
    }

    printf("Client Entering\r\n");
    IARM_Bus_Init("WriteImageTest");
    IARM_Bus_Connect();


    safec_rc = strcpy_s(param.name, sizeof(param.name), argv[2]);
    SAFEC_ERR_CHECK(safec_rc);

    safec_rc = strcpy_s(param.path, sizeof(param.path), argv[1]);
    SAFEC_ERR_CHECK(safec_rc);

    safec_rc = strcpy_s(param.callerModuleName, sizeof(param.callerModuleName), "WriteImageTest");
    SAFEC_ERR_CHECK(safec_rc);

    param.interval = 2;
    param.type = mfrIMAGE_TYPE_CDL;
    safec_rc = strcpy_s(param.cbData, sizeof(param.cbData), "Test Success");
    SAFEC_ERR_CHECK(safec_rc);

    if(IARM_RESULT_SUCCESS == IARM_Bus_RegisterCall(IARM_BUS_MFRLIB_COMMON_API_WriteImageCb,_callback))
    {
        printf("Register Call Success...\n");
    }
    else
    {
        printf("Problem with registering callback \n");
    }
    if(IARM_RESULT_SUCCESS == IARM_Bus_Call(IARM_BUS_MFRLIB_NAME,IARM_BUS_MFRLIB_API_WriteImage,(void *)&param,sizeof(param)))
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
}



/** @} */
/** @} */
