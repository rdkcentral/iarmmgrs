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
 * @addtogroup HPK Hardware Porting Kit
 * @{
 * @par The Hardware Porting Kit
 * HPK is the next evolution of the well-defined Hardware Abstraction Layer
 * (HAL), but augmented with more comprehensive documentation and test suites
 * that OEM or SOC vendors can use to self-certify their ports before taking
 * them to RDKM for validation or to an operator for final integration and
 * deployment. The Hardware Porting Kit effectively enables an OEM and/or SOC
 * vendor to self-certify their own Video Accelerator devices, with minimal RDKM
 * assistance
 *
 */

/** @addtogroup MFR MFR Module
 *  @{
 */
/** @addtogroup MFR_HAL MFR HAL
 *  @{
 * @par Application API Specification
 * MFR HAL provides an interface for wifi data types
 */

/** @defgroup PLAT_MFR_WIFI_DATA PLAT MFR WIFI DATA
 *  @{
 */

/**
 * @file mfr_wifi_types.h
 *
 * @brief Manufacturer Library WIFI Public API
 *
 * This API defines the Types and definitions for WIFI data module
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
 * - WIFI:     Wireless Fidelity
 * - SSID:     Service Set Identifier
 * - WEP:      Wired Equivalent Privacy
 * - WPA:      Wi-Fi Protected Access
 * - WPA2:     Wi-Fi Protected Access Version 2
 * @par Implementation Notes
 * -# None
 *
 */

#ifndef __MFR_WIFI_TYPES_H__
#define __MFR_WIFI_TYPES_H__

#ifdef __cplusplus
extern "C" {
#endif




#define WIFI_MAX_SSID_LEN        32       //!< Maximum SSID name
#define WIFI_MAX_PASSWORD_LEN    128       //!< Maximum password length

#define WIFI_DATA_LENGTH        512

/**
 * @brief WIFI API return status
 * 
 */
typedef enum _WIFI_API_RESULT
{
    WIFI_API_RESULT_SUCCESS = 0,                  ///< operation is successful
    WIFI_API_RESULT_FAILED,                       ///< Operation general error. This enum is deprecated
    WIFI_API_RESULT_NULL_PARAM,                   ///< NULL argument is passed to the module
    WIFI_API_RESULT_INVALID_PARAM,                ///< Invalid argument is passed to the module
    WIFI_API_RESULT_NOT_INITIALIZED,              ///< module not initialized
    WIFI_API_RESULT_OPERATION_NOT_SUPPORTED,      ///< operation not supported in the specific platform
    WIFI_API_RESULT_READ_WRITE_FAILED,            ///< flash read/write failed or crc check failed
    WIFI_API_RESULT_MAX                           ///< Out of range - required to be the last item of the enum

} WIFI_API_RESULT;

/**
 * @brief WIFI data type
 * 
 * @note: This order needs to correspond to whats in DRI code.
 */
typedef enum _WIFI_DATA_TYPE
{
    WIFI_DATA_UNKNOWN = 0,  ///< Unknown error. This enum is deprecated
    WIFI_DATA_SSID,         ///< SSID type
    WIFI_DATA_PASSWORD,     ///< Password type
    WIFI_DATA_MAX           ///< Out of range - required to be the last item of the enum
} WIFI_DATA_TYPE;

/**
 * @brief WIFI credentials data struct
 * 
 */
typedef struct
{
    char cSSID[WIFI_MAX_SSID_LEN+1];         ///< SSID field.
    char cPassword[WIFI_MAX_PASSWORD_LEN+1]; ///< password field
    int  iSecurityMode;                      ///< security mode. Platform dependent and caller is responsible to validate it
} WIFI_DATA;

#ifdef __cplusplus
}
#endif

#endif  //__MFR_WIFI_TYPES_H__

/** @} */ // End of PLAT_MFR_WIFI_DATA
/** @} */ // End of MFR_HAL
/** @} */ // End of MFR Module
/** @} */ // End of HPK