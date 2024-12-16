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
 * MFR HAL provides an interface for setting and getting temperature thresholds
 */

/** @defgroup PLAT_MFR_TEMP_API PLAT MFR TEMP API
 *  @{
 */

/**
 * @file mfr_temperature.h
 *
 * @brief Manufacturer Library temperature Public API.
 *
 * This API defines the Types and definitions for temperature data module
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
 * -# None
 * 
 * @par Implementation Notes
 * -# None
 *
 */

#ifndef __MFR_TEMPERATURE_API_H__
#define __MFR_TEMPERATURE_API_H__

#ifdef __cplusplus
extern "C" {
#endif
#include "mfrTypes.h"

/**
 * @brief Represents temperature states
 * 
 */
typedef enum _mfrTemperatureState_t
{
    mfrTEMPERATURE_NORMAL = 0,  ///< Temp is within normal operating range. 
    mfrTEMPERATURE_HIGH,        ///< Temp is high, but just a warning as device can still operate. 
    mfrTEMPERATURE_CRITICAL,    ///< Temp is critical, should trigger a thermal reset. 
    mfrTEMPERATURE_MAX          ///< Out of range - required to be the last item of the enum.
} mfrTemperatureState_t;

/**
 * @brief Retrieves the current temperature of the core and wifi chip
 *
 * @param [out] state              - The current state of the cabinet (platform) temperature 
 *                                    @note the highest state will be considered from cpu and wifi chip @see mfrTemperatureState_t
 * @param [out] temperatureValue   - Raw temperature value of the core
 *                                   in degrees Celsius
 * @param [out] wifiTemp           - Raw temperature value of the wifi chip
 *                                   in degrees Celsius
 *
 * @return    mfrError_t              - Status
 * @retval    mfrERR_NONE             - Success
 * @retval    mfrERR_NOT_INITIALIZED  - Module is not initialised
 * @retval    mfrERR_INVALID_PARAM    - Parameter passed to this function is invalid
 * @retval    mfrERR_TEMP_READ_FAILED - Temperature read failed
 * 
 * @pre  mfr_init() should be called before calling this API. If this precondition is not met, the API will return mfrERR_NOT_INITIALIZED. 
 * @warning  This API is Not thread safe
 * 
 */
mfrError_t mfrGetTemperature(mfrTemperatureState_t *state, int *temperatureValue, int *wifiTemp);


/**
 * @brief Sets the high and critical temperature threshold in the MFR persistance
 *
 * @param [in] tempHigh       - Temperature threshold at which mfrTEMPERATURE_HIGH
 *                              state will be reported. The unit is in degrees Celsius
 * @param [in] tempCritical   - Temperature threshold at which mfrTEMPERATURE_CRITICAL
 *                              state will be reported. The unit is in degrees Celsius
 * 
 * @return    mfrError_t                 - Status
 * @retval    mfrERR_NONE                - Success
 * @retval    mfrERR_NOT_INITIALIZED     - Module is not initialised
 * @retval    mfrERR_SRC_FILE_ERROR      - file errors
 * @retval    mfrERR_WRITE_FLASH_FAILED  - Flash write failed
 * 
 * @pre  mfr_init() should be called before calling this API. If this precondition is not met, the API will return mfrERR_NOT_INITIALIZED. 
 * @warning  This API is Not thread safe
 * 
 */
mfrError_t mfrSetTempThresholds(int tempHigh, int tempCritical);


/**
 * @brief Gets the high and critical temperature threshold from the MFR persistance
 *
 * @param [out] tempHigh       - Temperature threshold at which mfrTEMPERATURE_HIGH
 *                               state will be reported. The unit is in degrees Celsius
 * @param [out] tempCritical   - Temperature threshold at which mfrTEMPERATURE_CRITICAL
 *                               state will be reported. The unit is in degrees Celsius
 *
 * @return    mfrError_t                 - Status
 * @retval    mfrERR_NONE                - Success
 * @retval    mfrERR_NOT_INITIALIZED     - Module is not initialised
 * @retval    mfrERR_INVALID_PARAM       - Parameter passed to this function is invalid also this code will be returned if no value exists in the sys entry
 * @retval    mfrERR_SRC_FILE_ERROR      - file errors
 * @retval    mfrERR_FLASH_READ_FAILED   - Flash read failed
 * 
 * @pre  mfr_init() should be called before calling this API. If this precondition is not met, the API will return mfrERR_NOT_INITIALIZED. 
 * @warning  This API is Not thread safe
 * 
 */
mfrError_t mfrGetTempThresholds(int *tempHigh, int *tempCritical);


/**
 * @brief Get clock speeds for this device for the given states
 *
 * @param [out] cpu_rate_Normal  The clock rate to be used when in the 'normal' state
 * @param [out] cpu_rate_Scaled  The clock rate to be used when in the 'scaled' state
 * @param [out] cpu_rate_Minimal The clock rate to be used when in the 'minimal' state
 *
 * @return Error Code
 */
mfrError_t mfrDetemineClockSpeeds(uint32_t *cpu_rate_Normal, uint32_t *cpu_rate_Scaled, uint32_t *cpu_rate_Minimal);


/**
 * @brief This API sets the clock speed of the CPU.
 * @param [in] speed  One of the predefined parameters to set the clock speed.
 *
 * @return Error Code
 */
mfrError_t mfrSetClockSpeed(uint32_t speed);


/**
 * @brief This API returns the clock speed of the CPU
 * @param [out] speed One of the predefined parameters
 *
 * @return Error Code
 */
mfrError_t mfrGetClockSpeed(uint32_t *speed);


#ifdef __cplusplus
}
#endif

#endif /*__MFR_TEMPERATURE_API_H__*/


/** @} */ // End of PLAT_MFR_TEMP_API
/** @} */ // End of MFR_HAL
/** @} */ // End of MFR Module
/** @} */ // End of HPK
