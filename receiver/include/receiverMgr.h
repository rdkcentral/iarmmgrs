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

#ifndef _IARM_BUS_RECEIVERMGR_H
#define _IARM_BUS_RECEIVERMGR_H


#ifdef __cplusplus
extern "C"
{
#endif

#define IARM_RECEIVER_NAME "XRE_native_receiver"

#define IARM_RECEIVER_GET_APP_INFO "XRE_RECEIVER_getAppInfo"
#define IARM_RECEIVER_SET_CONNECTION_RESET "XRE_RECEIVER_setConnectionReset"
#define IARM_RECEIVER_IS_MINI_DIAGNOSTICS_ENABLED "XRE_RECEIVER_isMiniDiagnosticsEnabled"

#define APP_INFO_JSON_MAX 4096

typedef struct _AppInfoData {
    char appInfoJson[APP_INFO_JSON_MAX];
} AppInfoData;

#define CONNECTION_ID_MAX 1024
typedef struct _ConnectionResetData {
    char applicationID[CONNECTION_ID_MAX];
    char connectionID[CONNECTION_ID_MAX];
    char connectionResetLevel[CONNECTION_ID_MAX];
} ConnectionResetData;

typedef struct _IARM_Bus_Receiver_Param_t {
    union {
        AppInfoData appInfoData;
        ConnectionResetData connectionResetData;
        bool isMiniDiagnosticsEnabled;
    } data;
    int status;
} IARM_Bus_Receiver_Param_t;

#ifdef __cplusplus
}
#endif

#endif



