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

#include "mfrTypes.h"
#include "mfr_wifi_types.h"
#include "mfr_wifi_api.h"


#define IARM_BUS_MFRLIB_NAME "MFRLib"     /*!< Well-known Name for MFR libarary */

#define IARM_BUS_MFRLIB_API_SetBootLoaderPattern "mfrSetBootloaderPattern" /*!< Sets how the frontpanel LED(s) (and TV backlight on applicable devices) behave when running bootloader.*/
#define IARM_BUS_MFRLIB_API_GetSecureTime "mfrGetSecureTime"          /*!< method to retrive the Secure Time from TEE */
#define IARM_BUS_MFRLIB_API_SetSecureTime "mfrSetSecureTime"          /*!< Method to set Secure Time in TEE */
#define IARM_BUS_MFRLIB_API_SetFsrFlag               "mfrSetFSRflag"      /*!< Method to set FSR flag in FLASH */
#define IARM_BUS_MFRLIB_API_GetFsrFlag               "mfrGetFSRflag"      /*!< Method to get FSR flag from FLASH */
#define IARM_BUS_MFRLIB_API_GetTemperature              "mfrGetTemperature"     /*!< Gets the thermal temperature for the device*/
#define IARM_BUS_MFRLIB_API_SetTemperatureThresholds    "mfrSetTempThresholds"  /*!< Sets the thermal threshold for the device*/
#define IARM_BUS_MFRLIB_API_GetTemperatureThresholds    "mfrGetTempThresholds"  /*!< Gets the thermal threshold for the device*/
#define IARM_BUS_MFRLIB_API_SearchCPUClockSpeeds        "mfrDetemineClockSpeeds"  /*!< List down all the available CPU frequencies for the device*/
#define IARM_BUS_MFRLIB_API_SetCPUClockSpeed            "mfrSetClockSpeed"        /*!< Sets the CPU clock for the device*/
#define IARM_BUS_MFRLIB_API_GetCPUClockSpeed            "mfrGetClockSpeed"        /*!< Gets the CPU clock for the device*/
#define IARM_BUS_MFRLIB_API_GetSerializedData 		"mfrGetManufacturerData" /*!< Retrives manufacturer specific data from the box*/
#define IARM_BUS_MFRLIB_API_SetSerializedData 		"mfrSetManufacturerData" /*!< Retrives manufacturer specific data from the box*/
#define IARM_BUS_MFRLIB_API_DeletePDRI        		"mfrDeletePDRI"          /*!< delete PDRI image from the box*/
#define IARM_BUS_MFRLIB_API_ScrubAllBanks     		"scrubAllBanks"          /*!< scrub all banks   from the box*/
#define IARM_BUS_MFRLIB_API_WriteImage        		"mfrWriteImage"          /*!< Validate and  Write the image into flash*/
#define IARM_BUS_MFRLIB_API_WIFI_EraseAllData  		"mfrWifiEraseAllData"
#define IARM_BUS_MFRLIB_API_WIFI_Credentials   		"mfrWifiCredentials"
#define IARM_BUS_MFRLIB_COMMON_API_WriteImageCb "WriteImageCb"         /*!< This method shall be implemented by the caller calling WriteImage*/
#define IARM_BUS_MFRLIB_API_validate_image                     "mfrValidateImage"
#define IARM_BUS_MFRLIB_API_GetPreDownloadSize         "mfrGetPreDownloadImageSize"
#define IARM_BUS_MFRLIB_API_MirrorImage                        "mfrMirrorImage"
#define IARM_BUS_MFRLIB_API_SetBlSplashScreen       "mfrSetBlSplashScreen"      /*!< Method to flash the BL_OSD splash screen */
#define IARM_BUS_MFRLIB_API_ClearBlSplashScreen     "mfrClearBlSplashScreen"    /*!< Method to clear and use the default BL_OSD splash screen */
/*! Data size management need to be improved */
#define MAX_SERIALIZED_BUF 2048
#define MAX_BUF 255

typedef enum _IARM_Bus_MFRLib_ThermalState_t{
       IARM_BUS_TEMPERATURE_NORMAL = 0,                /* Temp is within normal operating range */
       IARM_BUS_TEMPERATURE_HIGH,                      /* Temp is high, but just a warning as device can still operate */
       IARM_BUS_TEMPERATURE_CRITICAL                   /* Temp is critical, should trigger a thermal reset */
} IARM_Bus_MFRLib_CurThermalState_t;

typedef struct _IARM_Bus_MFRLib_ThermalSoCTemp_Param_t{
       IARM_Bus_MFRLib_CurThermalState_t curState;     /* !< Current Thermal level */
       int curSoCTemperature;                          /* !< Current SoC temperature value */
       int curWiFiTemperature;                         /* !< Current WiFi temperature value */
       int highTemp;                                   /* !< New threshold at which TEMPERATURE_HIGH will be reported  */
       int criticalTemp;                               /* !< New threshold at which TEMPERATURE_CRITICAL will be reported  */
} IARM_Bus_MFRLib_ThermalSoCTemp_Param_t;

typedef struct _IARM_Bus_MFRLib_ThermalSoCFreq_Param_t{
        uint32_t cpu_rate_Normal;                      /* !< Normal/Highest CPU clock available */
        uint32_t cpu_rate_Scaled;                      /* !< Next available scaled CPU clock */
        uint32_t cpu_rate_Minimal;                     /* !< Lowest CPU clock available */
        uint32_t cpu_clock_speed;                      /* !< List of available CPU clocks */
}IARM_Bus_MFRLib_ThermalSoCFreq_Param_t;

typedef struct _IARM_Bus_MFRLib_GetSerializedData_Param_t{
    mfrSerializedType_t type;                        /*!< [in] Type of data to be queried*/	 
    char buffer[MAX_SERIALIZED_BUF];                 /*!< [out] On success, this will be pointing to a local memory location having expected data*/
    int bufLen;                                      /*!< [out] Indicates length of buffer pointed by pBuffer */
}IARM_Bus_MFRLib_GetSerializedData_Param_t;

typedef struct _IARM_Bus_MFRLib_SetSerializedData_Param_t{
    mfrSerializedType_t type;                        /*!< [in] Type of data to be queried*/
    char buffer[MAX_SERIALIZED_BUF];                 /*!< [out] On success, this will be pointing to a local memory location having expected data*/
    int bufLen;
}IARM_Bus_MFRLib_SetSerializedData_Param_t;

typedef struct _IARM_Bus_MFRLib_WriteImage_Param_t{
    char name[MAX_BUF];                             /*!< [in] the path of the image file in the STB file system. */
    char path[MAX_BUF];                             /*!< [in] the filename of the image file. */
    mfrImageType_t type;	                    /*!< [in] the type (e.g. format, signature type) of the image.*/
    char callerModuleName[MAX_BUF];                 /*!< [in] Name registered IARMBus by the caller module*/	
    int interval;                                   /*!< [in] number of seconds between two callbacks */
    char cbData[MAX_BUF];                           /*!< [inout] data that could be passed through this call and returned back through callback*/
}IARM_Bus_MFRLib_WriteImage_Param_t;

/*! Data associated with WriteImage callback call*/
typedef struct _IARM_Bus_MFRLib_CommonAPI_WriteImageCb_Param_t{
    mfrUpgradeStatus_t status;                     /*! upgrade status, set by WriteImage callback*/ 
    char cbData[MAX_BUF];                          /*! callback data, which was passed through WriteImage call*/ 
} IARM_Bus_MFRLib_CommonAPI_WriteImageCb_Param_t;

typedef enum _MfrMgr_EventId_t {
    IARM_BUS_MFRMGR_EVENT_STATUS_UPDATE= 0,         /*!< Event to notify status update change */
    IARM_BUS_MFRMGR_EVENT_MAX,                      /*!< Max event id from this module */
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

typedef struct _IARM_Bus_MFRLib_SetBLPattern_Param_t{
    mfrBlPattern_t pattern;
} IARM_Bus_MFRLib_SetBLPattern_Param_t;

/* Splash Screen File path as parameter for apis to set bl osd */
typedef struct _IARM_Bus_MFRLib_SetBLSplashScreen_Param{
	char path[MAX_BUF];
} IARM_Bus_MFRLib_SetBLSplashScreen_Param_t;

typedef unsigned long IARM_Bus_MFRLib_SecureTime_Param;  /* UTC time in seconds used as parameter for apis set/get Secure Time*/


typedef bool IARM_Bus_MFRLib_FsrFlag_Param_t;  /* Used to set/get the flag for fullSystem reset under the FLASH area */


#endif //_MFR_MGR_H_


/* End of IARM_BUS_MFR_LIB_API doxygen group */
/**
 * @}
 */


/** @} */
/** @} */
