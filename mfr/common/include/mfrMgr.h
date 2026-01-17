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
* @file
*
* @brief IARM-Bus MFR Manager Public API.
*
* This API defines the operations for the IARM-Bus MFR Manager interface.
*
* @par Document
* Document reference.
*
* @par Open Issues (in no particular order)
* -# None
*
* @par Assumptions
* -# None
*
* @par Abbreviations
* - BE:       ig-Endian.
* - cb:       allback function (suffix).
* - DS:      Device Settings.
* - FPD:     Front-Panel Display.
* - HAL:     Hardware Abstraction Layer.
* - LE:      Little-Endian.
* - LS:      Least Significant.
* - MBZ:     Must be zero.
* - MS:      Most Significant.
* - RDK:     Reference Design Kit.
* - _t:      Type (suffix).
*
* @par Implementation Notes
* -# None
*
*/

/** @defgroup IARM_BUS IARM-Bus HAL API
*   @ingroup IARM_RDK
*
*  IARM-Bus is a platform agnostic Inter-process communication (IPC) interface. It allows
*  applications to communicate with each other by sending Events or invoking Remote
*  Procedure Calls. The common programming APIs offered by the RDK IARM-Bus interface is
*  independent of the operating system or the underlying IPC mechanism.
*
*  Two applications connected to the same instance of IARM-Bus are able to exchange events
*  or RPC calls. On a typical system, only one instance of IARM-Bus instance is needed. If
*  desired, it is possible to have multiple IARM-Bus instances. However, applications
*  connected to different buses will not be able to communicate with each other.
*/

/** @defgroup IARM_BUS MFR Manager MFR lib
*   @ingroup IARM_RDK
*
*/

/** @addtogroup IARM_BUS_MFR_LIB_API IARM-MFR Manager API
*  @ingroup IARM_BUS
*
*  Described herein are functions and structures exposed by MFR library.
*
*  @{
*/



/**
* @defgroup iarmmgrs
* @{
* @defgroup mfr
* @{
**/


#ifndef _MFR_MGR_H_
#define _MFR_MGR_H_

#include "libIBus.h"
#include "libIARM.h"
#include "mfrTypes.h"

#define IARM_BUS_MFRLIB_NAME "MFRLib"     /*!< Well-known Name for MFR libarary */

#define IARM_BUS_MFRLIB_API_GetSerializedData "mfrGetSerializedData" /*!< Retrives manufacturer specific data from the box*/
#define IARM_BUS_MFRLIB_API_SetSerializedData "mfrSetSerializedData"
#define IARM_BUS_MFRLIB_API_WriteImage        "mfrWriteImage"          /*!< Validate and  Write the image into flash*/
#define IARM_BUS_MFRLIB_API_SetImageWriteProgress "mfrSetImageWriteProgress"
#define IARM_BUS_MFRLIB_API_GetImageWriteProgress "mfrGetImageWriteProgress"
#define IARM_BUS_MFRLIB_API_DeletePDRI        "mfrDeletePDRI"          /*!< delete PDRI image from the box*/
#define IARM_BUS_MFRLIB_API_ScrubAllBanks     "mfrScrubAllBanks"       /*!< scrub all banks   from the box*/
#define IARM_BUS_MFRLIB_API_Reboot            "mfrReboot"          
#define IARM_BUS_MFRLIB_API_SetCableCardType  "mfrSetCableCardType" 
#define IARM_BUS_MFRLIB_API_SetHostFirmwareInfo "mfrSetHostFirmwareInfo"
#define IARM_BUS_MFRLIB_API_GetBootImageName  "mfrGetBootImageName"
#define IARM_BUS_MFRLIB_API_GetPathConfiguration "mfrGetPathConfiguration"
#define IARM_BUS_MFRLIB_API_GetDFAST2Data     "mfrGetDFAST2Data"
#define IARM_BUS_MFRLIB_API_GetConfigData       "mfr_getConfigData"
#define IARM_BUS_MFRLIB_API_SetConfigData       "mfr_setConfigData"


#define IARM_BUS_MFRLIB_COMMON_API_WriteImageCb "WriteImageCb"         /*!< This method shall be implemented by the caller calling WriteImage*/

#define IARM_BUS_MFRLIB_API_Init "mfrInit"
#define IARM_BUS_MFRLIB_API_Shutdown "mfrShutdown"

#define MAX_BUF 255
#define MAX_SERIALIZED_BUF 1280
typedef struct _IARM_Bus_MFRLib_SerializedData_Param_t{
    mfrSerializedType_t type;                        /*!< [in] Type of data to be queried*/	 
    char crypto[64];
    int bufLen;                                      /*!< [out] Indicates length of buffer pointed by pBuffer */
    char buffer[MAX_SERIALIZED_BUF];
}IARM_Bus_MFRLib_SerializedData_Param_t;

typedef struct _IARM_Bus_MFRLib_WriteImage_Param_t{
    char name[MAX_BUF];                             /*!< [in] the path of the image file in the STB file system. */
    char path[MAX_BUF];                             /*!< [in] the filename of the image file. */
    mfrImageType_t type;	                    /*!< [in] the type (e.g. format, signature type) of the image.*/
    int interval;                                   /*!< [in] number of seconds between two callbacks */
}IARM_Bus_MFRLib_WriteImage_Param_t;

typedef struct _IARM_Bus_MFRLib_GetImageWriteProgress_Param_t{
    char imageName[MAX_BUF];
    mfrImageType_t imageType;
    mfrUpgradeProgress_t progress;
}IARM_Bus_MFRLib_GetImageWriteProgress_Param_t;

typedef struct _IARM_Bus_MFRLib_SetImageWriteProgress_Param_t{
    char imageName[MAX_BUF];
    mfrImageType_t imageType;
    mfrUpgradeProgress_t progress;
}IARM_Bus_MFRLib_SetImageWriteProgress_Param_t;

typedef struct _IARM_Bus_MFRLib_Reboot_Param_t{
    char imageName[MAX_BUF];
}IARM_Bus_MFRLib_Reboot_Param_t;

typedef struct _IARM_Bus_MFRLib_SetCableCardType_Param_t{
    mfrCableCardType_t type;
}IARM_Bus_MFRLib_SetCableCardType_Param_t;

typedef struct _IARM_Bus_MFRLib_SetHostFirmwareInfo_Param_t{
    char version[MFR_MAX_STR_SIZE];
    int day;
    int month;
    int year;
}IARM_Bus_MFRLib_SetHostFirmwareInfo_Param_t;

typedef struct _IARM_Bus_MFRLib_GetBootImageName_Param_t{
    int bootInstance;
    char imageName[MAX_BUF];
    int len;
    mfrImageType_t type;
}IARM_Bus_MFRLib_GetBootImageName_Param_t;

typedef struct _IARM_Bus_MFRLib_GetPathConfiguration_Param_t{
    mfrConfigPathType_t type;
    char path[MFR_MAX_STR_SIZE];
    int len;
}IARM_Bus_MFRLib_GetPathConfiguration_Param_t;

typedef struct _IARM_Bus_MFRLib_GetDFAST2Data_Param_t{
    unsigned int seedIn [MFR_DFAST_INPUT_BYTES];
    unsigned int keyOut [MFR_DFAST_OUTPUT_BYTES];
}IARM_Bus_MFRLib_GetDFAST2Data_Param_t;

/*! Data associated with WriteImage callback call*/
typedef struct _IARM_Bus_MFRLib_CommonAPI_WriteImageCb_Param_t{
    mfrUpgradeStatus_t status;                     /*! upgrade status, set by WriteImage callback*/ 
    char cbData[MAX_BUF];                          /*! callback data, which was passed through WriteImage call*/ 
} IARM_Bus_MFRLib_CommonAPI_WriteImageCb_Param_t;

typedef enum _MfrMgr_EventId_t {
    IARM_BUS_MFRMGR_EVENT_STATUS_UPDATE= 0,         /*!< Event to notify status update change */
    IARM_BUS_MFRMGR_EVENT_MAX,                      /*!< Max event id from this module */
} IARM_Bus_MfrMgr_EventId_t;

/*! Event data*/
typedef struct _IARM_BUS_MfrMgr_StatusUpdate_EventData_t {
    mfrUpgradeStatus_t status;        /*!< New status*/
} IARM_BUS_MfrMgr_StatusUpdate_EventData_t;


#endif //_MFR_MGR_H_


/* End of IARM_BUS_MFR_LIB_API doxygen group */
/**
 * @}
 */


/** @} */
/** @} */
