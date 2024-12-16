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
 * @file pwrMgr.h
 *
 * @brief IARM-Bus Power Manager Public API.
 *
 * This API defines the structures and functions for the IARM-Bus Power Manager interface.
 */

/**
* @defgroup iarmmgrs
* @{
* @defgroup hal
* @{
**/

#ifndef _IARM_BUS_THERMON_H
#define _IARM_BUS_THERMON_H
#ifdef __cplusplus
extern "C"
{
#endif
#include <stdbool.h>
#include <stdint.h>
#include "mfr_temperature.h"

#ifdef ENABLE_THERMAL_PROTECTION
/**
 *  @brief Enumerator which represents the  possible temeperature states
 */
typedef enum _PWRMgr_ThermalState_t{
    PWRMGR_TEMPERATURE_NORMAL = 0,    /* Temp is within normal operating range */
    PWRMGR_TEMPERATURE_HIGH,          /* Temp is high, but just a warning as device can still operate */
    PWRMGR_TEMPERATURE_CRITICAL       /* Temp is critical, should trigger a thermal reset */
} PWRMgr_ThermalState_t;


/**
 * @brief Get the  current temperature of the core.
 *
 * @param[out] state            The current state of the core temperature
 * @param[out] curTemperature   Raw temperature value of the core
 *                              in degrees Celsius
 * @param[out] wifiTemperature  Raw temperature value of the wifi chip
 *                              in degrees Celsius
 *
 * @return Returns the status of the operation.
 * @retval 0 if successful, appropiate error code otherwise.
*/
int PLAT_API_GetTemperature(mfrTemperatureState_t *curState, float *curTemperature, float *wifiTemperature);
/**
 * @brief Set temperature thresholds which will determine the state returned from a call to mfrGetTemperature.
 *
 * @param[in] tempHigh       Temperature threshold at which mfrTEMPERATURE_HIGH
 *                           state will be reported.
 * @param[in] tempCritical   Temperature threshold at which mfrTEMPERATURE_CRITICAL
 *                           state will be reported.
 *
 * @return  Returns the status of the operation.
 * @retval  0 if successful, appropiate error code otherwise.
 */
int PLAT_API_SetTempThresholds(float tempHigh, float tempCritical);
/**
 * @brief Get temperature thresholds which will determine the state returned from a call to mfrGetTemperature.
 *
 * @param[out] tempHigh      Temperature threshold at which mfrTEMPERATURE_HIGH
 *                           state will be reported.
 * @param[out] tempCritical  Temperature threshold at which mfrTEMPERATURE_CRITICAL
 *                           state will be reported.
 *
 * @return Returns the status of the operation.
 * @retval 0 if successful, appropiate error code otherwise.
 */
int PLAT_API_GetTempThresholds(float *tempHigh, float *tempCritical);
/**
 * @brief Get clock speeds for this device for the given states
 *
 * @param [out] cpu_rate_Normal  The clock rate to be used when in the 'normal' state
 * @param [out] cpu_rate_Scaled  The clock rate to be used when in the 'scaled' state
 * @param [out] cpu_rate_Minimal The clock rate to be used when in the 'minimal' state
 *
 * @return 1 if operation is attempted 0 otherwise
 */
int PLAT_API_DetemineClockSpeeds(uint32_t *cpu_rate_Normal, uint32_t *cpu_rate_Scaled, uint32_t *cpu_rate_Minimal);
/**
 * @brief This API sets the clock speed of the CPU.
 * @param [in] speed  One of the predefined parameters to set the clock speed.
 *
 * @return Returns the status of the operation
 * @retval returns 1, if operation is attempted and 0 otherwise
 */
int PLAT_API_SetClockSpeed(uint32_t speed);
/**
 * @brief This API returns the clock speed of the CPU
 *
 * @param [out] speed One of the predefined parameters
 * @return Returns the current clock speed.
 */
int PLAT_API_GetClockSpeed(uint32_t *speed);
#endif //ENABLE_THERMAL_PROTECTION

#ifdef __cplusplus
}
#endif
#endif 
/** @} */ // End of Doxygen Tag

/** @} */
/** @} */
