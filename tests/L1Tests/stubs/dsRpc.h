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
 * @file tests/L1Tests/stubs/dsRpc.h
 *
 * Minimal stub for devicesettings/rpc/include/dsRpc.h used in the L1 test
 * build.  Provides only the RPC parameter structs and the dsSleepMode_t
 * enum that dsMgr.c and test_dsMgr.cpp reference directly.  The real
 * devicesettings repo is not checked out in CI; this stub avoids that
 * dependency.
 */

#ifndef _DS_RPC_H_
#define _DS_RPC_H_

#include "dsTypes.h"

/* ---- Sleep mode --------------------------------------------------------- */
/* Defined in devicesettings.h (force-included via -include in AM_CPPFLAGS).
 * DRM_DISPLAY_MODE_LEN is a sentinel macro defined at the top of that file;
 * its presence means devicesettings.h has already supplied dsSleepMode_t. */
#ifndef DRM_DISPLAY_MODE_LEN
typedef enum _dsSleepMode_t {
    dsHOST_SLEEP_MODE_LIGHT,
    dsHOST_SLEEP_MODE_DEEP,
    dsHOST_SLEEP_MODE_MAX
} dsSleepMode_t;
#endif /* DRM_DISPLAY_MODE_LEN */

/* ---- Video-port RPC param structs --------------------------------------- */
typedef struct _dsVideoPortGetHandleParam_t {
    dsVideoPortType_t type;
    int               index;
    intptr_t          handle;
} dsVideoPortGetHandleParam_t;

typedef struct _dsVideoPortIsDisplayConnectedParam_t {
    intptr_t handle;
    bool     connected;
} dsVideoPortIsDisplayConnectedParam_t;

typedef struct _dsVideoPortIsDisplaySurroundParam_t {
    intptr_t handle;
    bool     surround;
} dsVideoPortIsDisplaySurroundParam_t;

typedef struct _dsVideoPortGetResolutionParam_t {
    intptr_t                 handle;
    dsVideoPortResolution_t  resolution;
    bool                     toPersist;
} dsVideoPortGetResolutionParam_t;

typedef struct _dsVideoPortSetResolutionParam_t {
    intptr_t                 handle;
    dsVideoPortResolution_t  resolution;
    bool                     toPersist;
    bool                     forceCompatible; /**< dsMgr.c sets this on resolution change */
} dsVideoPortSetResolutionParam_t;

typedef struct _dsForceDisable4KParam_t {
    intptr_t handle;
    bool     disable;
} dsForceDisable4KParam_t;

typedef struct _dsSetBackgroundColorParam_t {
    intptr_t                  handle;
    dsVideoBackgroundColor_t  color;
} dsSetBackgroundColorParam_t;

typedef struct _dsEdidIgnoreParam_t {
    intptr_t handle;
    bool     ignoreEDID; /**< dsMgr.c accesses this field as ignoreEDID */
} dsEdidIgnoreParam_t;

/* ---- Display / EDID RPC param structs ----------------------------------- */
#ifndef dsEEDID_MAX_DATA_SIZE
#define dsEEDID_MAX_DATA_SIZE 512
#endif

/* ---- Video background color --------------------------------------------- */
typedef enum _dsVideoBackgroundColor_t {
    dsVIDEO_BGCOLOR_NONE,
    dsVIDEO_BGCOLOR_BLACK,
    dsVIDEO_BGCOLOR_WHITE,
    dsVIDEO_BGCOLOR_MAX
} dsVideoBackgroundColor_t;

typedef struct _dsEDIData_t {
    unsigned char productCode[2];
    unsigned char serialNumber[4];
    unsigned char weekOfManufacture;
    unsigned char yearOfManufacture;
    unsigned char hdmiDeviceType;
    unsigned char isRepeater;
    unsigned char physicalAddressA;
    unsigned char physicalAddressB;
    unsigned char physicalAddressC;
    unsigned char physicalAddressD;
    int           numSupportedResolution;
    dsVideoPortResolution_t suppResolutionList[64];
    char          monitorName[14];
} dsEDIData_t;

/* Compatibility alias: test code written against an older HAL revision uses
 * numOfSupportedResolution; the canonical field name in this struct is
 * numSupportedResolution.  The #define makes both names resolve identically. */
#ifndef numOfSupportedResolution
#define numOfSupportedResolution numSupportedResolution
#endif

typedef struct _dsDisplayGetEDIDParam_t {
    intptr_t   handle;
    dsEDIData_t edid;
} dsDisplayGetEDIDParam_t;

typedef struct _dsDisplayGetEDIDBytesParam_t {
    intptr_t      handle;
    unsigned char bytes[dsEEDID_MAX_DATA_SIZE];
    int           length;
} dsDisplayGetEDIDBytesParam_t;

/* ---- Audio RPC param structs -------------------------------------------- */
typedef struct _dsAudioGetHandleParam_t {
    dsAudioPortType_t type;
    int               index;
    intptr_t          handle;
} dsAudioGetHandleParam_t;

typedef struct _dsAudioSetStereoModeParam_t {
    intptr_t handle;
    int      mode;
    bool     toPersist;
} dsAudioSetStereoModeParam_t;

typedef struct _dsAudioSetStereoAutoParam_t {
    intptr_t handle;
    bool     autoMode;
} dsAudioSetStereoAutoParam_t;

/* ---- HDCP RPC param structs -------------------------------------------- */
#ifndef HDCP_KEY_MAX_SIZE
#define HDCP_KEY_MAX_SIZE 4096
#endif

typedef struct _dsEnableHDCPParam_t {
    int  keySize;
    char key[HDCP_KEY_MAX_SIZE];
    char hdcpKey[HDCP_KEY_MAX_SIZE];
    char hdcpkey[HDCP_KEY_MAX_SIZE];
} dsEnableHDCPParam_t;

/* ---- API string macros (subset used by dsMgr.c) ------------------------- */
/* Already defined in Iarm.h (testframework) — guard against redefinition. */
#ifndef IARM_BUS_DSMGR_API_dsGetVideoPort
#define IARM_BUS_DSMGR_API_dsGetVideoPort            "dsGetVideoPort"
#define IARM_BUS_DSMGR_API_dsIsDisplayConnected       "dsIsDisplayConnected"
#define IARM_BUS_DSMGR_API_dsGetIgnoreEDIDStatus      "dsGetIgnoreEDIDStatus"
#define IARM_BUS_DSMGR_API_dsSetResolution            "dsSetResolution"
#define IARM_BUS_DSMGR_API_dsGetResolution            "dsGetResolution"
#define IARM_BUS_DSMGR_API_dsGetEDID                  "dsGetEDID"
#define IARM_BUS_DSMGR_API_dsGetEDIDBytes             "dsGetEDIDBytes"
#define IARM_BUS_DSMGR_API_dsGetForceDisable4K        "dsGetForceDisable4K"
#define IARM_BUS_DSMGR_API_dsSetBackgroundColor       "dsSetBackgroundColor"
#define IARM_BUS_DSMGR_API_dsGetAudioPort             "dsGetAudioPort"
#define IARM_BUS_DSMGR_API_dsGetStereoMode            "dsGetStereoMode"
#define IARM_BUS_DSMGR_API_dsSetStereoMode            "dsSetStereoMode"
#define IARM_BUS_DSMGR_API_dsSetStereoAuto            "dsSetStereoAuto"
#define IARM_BUS_DSMGR_API_dsGetVideoPortResolutions  "dsGetVideoPortResolutions"
#define IARM_BUS_DSMGR_API_dsGetDefaultResolutionIndex "dsGetDefaultResolutionIndex"
#define IARM_BUS_DSMGR_API_dsGetAudioTypeConfigs      "dsGetAudioTypeConfigs"
#endif
/* SetStandbyVideoState / GetStandbyVideoState are already defined in Iarm.h */
#ifndef IARM_BUS_DSMGR_API_SetStandbyVideoState
#define IARM_BUS_DSMGR_API_SetStandbyVideoState       "dsSetStandbyVideoState"
#define IARM_BUS_DSMGR_API_GetStandbyVideoState       "dsGetStandbyVideoState"
#endif

#endif /* _DS_RPC_H_ */
