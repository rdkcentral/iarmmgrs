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
 * @see the License for the specific language governing permissions and
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

/** @defgroup PLAT_MFR_WIFI_API PLAT MFR WIFI API
 *  @{
 */

/**
 * @file mfr_wifi_api.h
 *
 * @brief Manufacturer Library WIFI Public API.
 *
 * This API defines the Manufacturer API for the WIFI credentials access
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
 *
 * @par Implementation Notes
 * -# None
 *
 */

#ifndef __MFR_WIFI_API_H__
#define __MFR_WIFI_API_H__


#ifdef __cplusplus
extern "C" {
#endif

#include "mfr_wifi_types.h"


/**
 * @brief Retrieves the saved SSID name, password, and security mode from the MFR persistence
 * 
 * @param pData [out] : out parameter to get the saved wifi credentials. @see WIFI_DATA
 * 
 * @return    WIFI_API_RESULT                            - Status
 * @retval    WIFI_API_RESULT_SUCCESS                    - Success
 * @retval    WIFI_API_RESULT_NOT_INITIALIZED            - Not initialized
 * @retval    WIFI_API_RESULT_OPERATION_NOT_SUPPORTED    - Operation not supported
 * @retval    WIFI_API_RESULT_NULL_PARAM                 - Null param
 * @retval    WIFI_API_RESULT_READ_WRITE_FAILED          - flash operation failed
 * 
 * @pre  mfr_init() should be called before calling this API. If this precondition is not met, the API will return WIFI_API_RESULT_NOT_INITIALIZED. 
 * @warning  This API is NOT thread safe. Caller shall handle the concurrency
 * @see  WIFI_SetCredentials()
 * 
 */
WIFI_API_RESULT WIFI_GetCredentials(WIFI_DATA *pData);

/**
 * @brief Sets wifi ssid name, password and the security mode in the MFR persistance storage
 *
 * @param pData [in] : Sets the ssid credentials. @see WIFI_DATA
 * 
 * @return    WIFI_API_RESULT                            - Status
 * @retval    WIFI_API_RESULT_SUCCESS                    - Success
 * @retval    WIFI_API_RESULT_NOT_INITIALIZED            - Not initialized
 * @retval    WIFI_API_RESULT_OPERATION_NOT_SUPPORTED    - Operation not supported
 * @retval    WIFI_API_RESULT_NULL_PARAM                 - Null param
 * @retval    WIFI_API_RESULT_INVALID_PARAM              - Invalid param
 * @retval    WIFI_API_RESULT_READ_WRITE_FAILED          - flash operation failed
 * 
 * @pre  mfr_init() should be called before calling this API. If this precondition is not met, the API will return WIFI_API_RESULT_NOT_INITIALIZED. 
 * @warning  This API is NOT thread safe. Caller shall handle the concurrency
 * @see  WIFI_GetCredentials()
 * 
 */
WIFI_API_RESULT WIFI_SetCredentials(WIFI_DATA *pData);

/**
 * @brief Clears the wifi credentials saved in the  MFR persistance storage @see WIFI_DATA
 * 
 * @return    WIFI_API_RESULT                     - Status
 * @retval    WIFI_API_RESULT_SUCCESS             - Success
 * @retval    WIFI_API_RESULT_NOT_INITIALIZED     - Not initialized
 * @retval    WIFI_ERR_OPERATION_NOT_SUPPORTED    - Operation not supported
 * @retval    WIFI_API_RESULT_READ_WRITE_FAILED   - flash operation failed
 * 
 * @pre  mfr_init() should be called before calling this API. If this precondition is not met, the API will return WIFI_API_RESULT_NOT_INITIALIZED. 
 * @warning  This API is NOT thread safe. Caller shall handle the concurrency
 * 
 */
WIFI_API_RESULT WIFI_EraseAllData (void);


#ifdef __cplusplus
}
#endif

#endif // __MFR_WIFI_API_H__

/** @} */ // End of PLAT_MFR_WIFI_API
/** @} */ // End of MFR_HAL
/** @} */ // End of MFR Module
/** @} */ // End of HPK