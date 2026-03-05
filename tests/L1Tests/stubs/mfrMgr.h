/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
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
 * @file mfrMgr.h  (L1 test stub)
 * @brief Drop-in replacement for mfr/include/mfrMgr.h that avoids
 *        redefinition conflicts with entservices-testframework's Iarm.h.
 *
 * The testframework's Iarm.h already defines several MFR types
 * (ThermalState, ThermalSoCTemp, SetBLPattern, SetBLSplashScreen,
 *  GetSerializedData, FsrFlag) WITHOUT include guards.
 * The real mfrMgr.h defines the SAME types, leading to "redefinition"
 * errors when both headers are visible in the same translation unit.
 *
 * This stub:
 *   - Sets the _MFR_MGR_H_ guard so the real mfrMgr.h is never pulled in.
 *   - Pulls in base types via mfrTypes.h / mfr_wifi_types.h / mfr_wifi_api.h.
 *   - Provides ALL #define macros from the real mfrMgr.h.
 *   - Provides ONLY the types that do NOT exist in Iarm.h.
 *   - Lets the conflicting types come from Iarm.h.
 */

#ifndef _MFR_MGR_H_
#define _MFR_MGR_H_

#ifdef __cplusplus
extern "C" {
#endif
#include "mfrTypes.h"
#include "mfr_wifi_types.h"
#include "mfr_wifi_api.h"
#ifdef __cplusplus
}
#endif

/* -----------------------------------------------------------------------
 * Macros — identical to mfr/include/mfrMgr.h.
 * IARM_BUS_MFRLIB_NAME and IARM_BUS_MFRLIB_API_GetSerializedData are also
 * defined in Iarm.h, so guard them to avoid redefinition warnings.
 * ----------------------------------------------------------------------- */
#ifndef IARM_BUS_MFRLIB_NAME
#define IARM_BUS_MFRLIB_NAME                            "MFRLib"
#endif
#define IARM_BUS_MFRLIB_API_SetBootLoaderPattern        "mfrSetBootloaderPattern"
#define IARM_BUS_MFRLIB_API_GetSecureTime               "mfrGetSecureTime"
#define IARM_BUS_MFRLIB_API_SetSecureTime               "mfrSetSecureTime"
#define IARM_BUS_MFRLIB_API_SetFsrFlag                  "mfrSetFSRflag"
#define IARM_BUS_MFRLIB_API_GetFsrFlag                  "mfrGetFSRflag"
#define IARM_BUS_MFRLIB_API_GetTemperature              "mfrGetTemperature"
#define IARM_BUS_MFRLIB_API_SetTemperatureThresholds    "mfrSetTempThresholds"
#define IARM_BUS_MFRLIB_API_GetTemperatureThresholds    "mfrGetTempThresholds"
#define IARM_BUS_MFRLIB_API_SearchCPUClockSpeeds        "mfrDetemineClockSpeeds"
#define IARM_BUS_MFRLIB_API_SetCPUClockSpeed            "mfrSetClockSpeed"
#define IARM_BUS_MFRLIB_API_GetCPUClockSpeed            "mfrGetClockSpeed"
#ifndef IARM_BUS_MFRLIB_API_GetSerializedData
#define IARM_BUS_MFRLIB_API_GetSerializedData           "mfrGetManufacturerData"
#endif
#define IARM_BUS_MFRLIB_API_SetSerializedData           "mfrSetManufacturerData"
#define IARM_BUS_MFRLIB_API_DeletePDRI                  "mfrDeletePDRI"
#define IARM_BUS_MFRLIB_API_ScrubAllBanks               "scrubAllBanks"
#define IARM_BUS_MFRLIB_API_WriteImage                  "mfrWriteImage"
#define IARM_BUS_MFRLIB_API_WIFI_EraseAllData           "mfrWifiEraseAllData"
#define IARM_BUS_MFRLIB_API_WIFI_Credentials            "mfrWifiCredentials"
#define IARM_BUS_MFRLIB_COMMON_API_WriteImageCb         "WriteImageCb"
#define IARM_BUS_MFRLIB_API_validate_image              "mfrValidateImage"
#define IARM_BUS_MFRLIB_API_GetPreDownloadSize          "mfrGetPreDownloadImageSize"
#define IARM_BUS_MFRLIB_API_MirrorImage                 "mfrMirrorImage"
#define IARM_BUS_MFRLIB_API_SetBlSplashScreen           "mfrSetBlSplashScreen"
#define IARM_BUS_MFRLIB_API_ClearBlSplashScreen         "mfrClearBlSplashScreen"
#define IARM_BUS_MFRLIB_API_SetConfigData               "mfr_setConfigData"
#define IARM_BUS_MFRLIB_API_GetConfigData               "mfr_getConfigData"

#define MAX_SERIALIZED_BUF 2048
#define MAX_BUF            255

/* -----------------------------------------------------------------------
 * CONFLICTING types — provided by Iarm.h (testframework), NOT here.
 *
 *   IARM_Bus_MFRLib_CurThermalState_t        (Iarm.h ~907)
 *   IARM_Bus_MFRLib_ThermalSoCTemp_Param_t   (Iarm.h ~922)
 *   IARM_Bus_MFRLib_SetBLPattern_Param_t     (Iarm.h ~251)
 *   IARM_Bus_MFRLib_SetBLSplashScreen_Param_t(Iarm.h ~918)
 *   IARM_Bus_MFRLib_FsrFlag_Param_t          (Iarm.h ~930)
 *
 *   IARM_Bus_MFRLib_GetSerializedData_Param_t is guarded in Iarm.h by
 *   IARM_BUS_MFRLIB_GETSERIALIZEDDATA_DEFINED — we provide our own below
 *   so that whichever header is included first wins.
 * ----------------------------------------------------------------------- */

/* GetSerializedData — guarded so it coexists with Iarm.h's copy */
#ifndef IARM_BUS_MFRLIB_GETSERIALIZEDDATA_DEFINED
#define IARM_BUS_MFRLIB_GETSERIALIZEDDATA_DEFINED
typedef struct _IARM_Bus_MFRLib_GetSerializedData_Param_t{
    mfrSerializedType_t type;
    char buffer[MAX_SERIALIZED_BUF];
    int bufLen;
}IARM_Bus_MFRLib_GetSerializedData_Param_t;
#endif /* IARM_BUS_MFRLIB_GETSERIALIZEDDATA_DEFINED */

/* -----------------------------------------------------------------------
 * NON-CONFLICTING types — only in mfrMgr.h, NOT in Iarm.h
 * ----------------------------------------------------------------------- */

typedef struct _IARM_Bus_MFRLib_ThermalSoCFreq_Param_t{
    uint32_t cpu_rate_Normal;
    uint32_t cpu_rate_Scaled;
    uint32_t cpu_rate_Minimal;
    uint32_t cpu_clock_speed;
}IARM_Bus_MFRLib_ThermalSoCFreq_Param_t;

typedef struct _IARM_Bus_MFRLib_SetSerializedData_Param_t{
    mfrSerializedType_t type;
    char buffer[MAX_SERIALIZED_BUF];
    int bufLen;
}IARM_Bus_MFRLib_SetSerializedData_Param_t;

typedef struct _IARM_Bus_MFRLib_WriteImage_Param_t{
    char name[MAX_BUF];
    char path[MAX_BUF];
    mfrImageType_t type;
    char callerModuleName[MAX_BUF];
    int interval;
    char cbData[MAX_BUF];
}IARM_Bus_MFRLib_WriteImage_Param_t;

typedef struct _IARM_Bus_MFRLib_CommonAPI_WriteImageCb_Param_t{
    mfrUpgradeStatus_t status;
    char cbData[MAX_BUF];
} IARM_Bus_MFRLib_CommonAPI_WriteImageCb_Param_t;

typedef enum _MfrMgr_EventId_t {
    IARM_BUS_MFRMGR_EVENT_STATUS_UPDATE = 0,
    IARM_BUS_MFRMGR_EVENT_MAX,
} IARM_Bus_MfrMgr_EventId_t;

typedef enum _WifiRequestType
{
    WIFI_GET_CREDENTIALS = 0,
    WIFI_SET_CREDENTIALS = 1
} WifiRequestType_t;

typedef struct _IARM_BUS_MFRLIB_API_WIFI_Credentials_Param_t{
    WIFI_DATA wifiCredentials;
    WifiRequestType_t requestType;
    WIFI_API_RESULT returnVal;
}IARM_BUS_MFRLIB_API_WIFI_Credentials_Param_t;

typedef unsigned long IARM_Bus_MFRLib_SecureTime_Param;

typedef struct _IARM_Bus_MFRLib_Platformblockdata_Param_t{
    unsigned int blocklist;
}IARM_Bus_MFRLib_Platformblockdata_Param_t;

#endif /* _MFR_MGR_H_ */
