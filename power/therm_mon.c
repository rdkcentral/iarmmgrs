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
* @defgroup power
* @{
**/


#ifdef __cplusplus
extern "C"
{
#endif
#ifdef ENABLE_THERMAL_PROTECTION
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>

#include "therm_mon.h"
#include "pwrlogger.h"

#include "libIBus.h"
#include "libIARMCore.h"
#include "mfrMgr.h"


int uint32_compare( const void* a, const void* b)
{
    const uint32_t l = * ((const uint32_t*) a);
    const uint32_t r = * ((const uint32_t*) b);

    if ( l == r ) return 0;
    else if ( l < r ) return -1;
    else return 1;
}

#ifdef REALTEK_SPECIFIC
int PLAT_API_DetemineClockSpeeds(uint32_t *cpu_rate_Normal, uint32_t *cpu_rate_Scaled, uint32_t *cpu_rate_Minimal)
{
	int retValue = 0;

	IARM_Result_t iarm_result = IARM_RESULT_IPCCORE_FAIL;
	IARM_Bus_MFRLib_ThermalSoCFreq_Param_t *param = NULL;
	iarm_result = IARM_Malloc(IARM_MEMTYPE_PROCESSLOCAL, sizeof(IARM_Bus_MFRLib_ThermalSoCFreq_Param_t), (void**)&param);

	if ((iarm_result == IARM_RESULT_SUCCESS) && param) {
		iarm_result = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME, IARM_BUS_MFRLIB_API_SearchCPUClockSpeeds, (void *)param, sizeof(IARM_Bus_MFRLib_ThermalSoCFreq_Param_t));
		if (iarm_result == IARM_RESULT_SUCCESS)
		{
			LOG("[pwmgr] Success IARM_BUS_MFRLIB_API_SearchCPUClockSpeeds\n");
			*cpu_rate_Normal   =  param->cpu_rate_Normal;
			*cpu_rate_Scaled   =  param->cpu_rate_Scaled;
			*cpu_rate_Minimal  =  param->cpu_rate_Minimal;
			retValue = 1;
			LOG("[pwmgr] Available CPU Frequencies are: Normal:%u Scaled:%u Minimal:%u\n",*cpu_rate_Normal ,*cpu_rate_Scaled ,*cpu_rate_Minimal);
		}
		else
		{
			LOG("[pwmgr] Failed IARM_BUS_MFRLIB_API_SearchCPUClockSpeeds\n");
		}
		IARM_Free(IARM_MEMTYPE_PROCESSLOCAL,param);
	}
	else
	{
		LOG("PLAT_API_DetemineClockSpeeds: IARM_Malloc() failed\n");
	}

	return retValue;
}

int PLAT_API_SetClockSpeed(uint32_t speed)
{
	int retValue = 0;

	IARM_Result_t iarm_result = IARM_RESULT_IPCCORE_FAIL;
	IARM_Bus_MFRLib_ThermalSoCFreq_Param_t *param = NULL;

	iarm_result = IARM_Malloc(IARM_MEMTYPE_PROCESSLOCAL, sizeof(IARM_Bus_MFRLib_ThermalSoCFreq_Param_t), (void**)&param);
	if ((iarm_result == IARM_RESULT_SUCCESS) && param) {
		param->cpu_clock_speed = speed;
		LOG("[pwmgr2] CPU Clock Speed Setting to [%u]\n",param->cpu_clock_speed);
		iarm_result = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME, IARM_BUS_MFRLIB_API_SetCPUClockSpeed, (void *)param, sizeof(IARM_Bus_MFRLib_ThermalSoCFreq_Param_t));
		if (iarm_result == IARM_RESULT_SUCCESS)
		{
			LOG("[pwmgr2] Success IARM_BUS_MFRLIB_API_SetCPUClockSpeed\n");
			retValue = 1;
		}
		else
		{
			LOG("[pwmgr2] Failed IARM_BUS_MFRLIB_API_SetCPUClockSpeed\n");
		}
		IARM_Free(IARM_MEMTYPE_PROCESSLOCAL,param);
	}
	else
	{
		LOG("PLAT_API_SetClockSpeed: IARM_Malloc() failed\n");
	}

	return retValue;
}

int PLAT_API_GetClockSpeed(uint32_t *speed)
{
	int retValue = 0;
	IARM_Result_t iarm_result = IARM_RESULT_IPCCORE_FAIL;
	IARM_Bus_MFRLib_ThermalSoCFreq_Param_t *param = NULL;
	iarm_result = IARM_Malloc(IARM_MEMTYPE_PROCESSLOCAL, sizeof(IARM_Bus_MFRLib_ThermalSoCFreq_Param_t), (void**)&param);

	if ((iarm_result == IARM_RESULT_SUCCESS) && param) {
		iarm_result = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME, IARM_BUS_MFRLIB_API_GetCPUClockSpeed, (void *)param, sizeof(IARM_Bus_MFRLib_ThermalSoCFreq_Param_t));
		if (iarm_result == IARM_RESULT_SUCCESS)
		{
			LOG("[pwmgr2] Success IARM_BUS_MFRLIB_API_GetCPUClockSpeed\n");
			*speed = param->cpu_clock_speed;
			retValue = 1;
			LOG("[pwmgr2] Getting CPU Clock Speed  as [%u]\n",*speed);
		}
		else
		{
			LOG("[pwmgr2] Failed IARM_BUS_MFRLIB_API_GetCPUClockSpeed\n");
		}
		IARM_Free(IARM_MEMTYPE_PROCESSLOCAL,param);
	}
	else
	{
		LOG("PLAT_API_GetClockSpeed: IARM_Malloc() failed\n");
	}

	return retValue;
}

int PLAT_API_GetTemperature(mfrTemperatureState_t *curState, float *curTemperature, float *wifiTemperature)
{
	int retValue = 0;

	IARM_Result_t iarm_result = IARM_RESULT_IPCCORE_FAIL;
	IARM_Bus_MFRLib_ThermalSoCTemp_Param_t *param = NULL;
	iarm_result = IARM_Malloc(IARM_MEMTYPE_PROCESSLOCAL, sizeof(IARM_Bus_MFRLib_ThermalSoCTemp_Param_t), (void**)&param);

	if ((iarm_result == IARM_RESULT_SUCCESS) && param) {
		iarm_result = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME, IARM_BUS_MFRLIB_API_GetTemperature, (void *)param, sizeof(IARM_Bus_MFRLib_ThermalSoCTemp_Param_t));
		if (iarm_result == IARM_RESULT_SUCCESS) {
			*curState = (mfrTemperatureState_t)param->curState;
			*curTemperature = param->curSoCTemperature;
			*wifiTemperature = param->curWiFiTemperature;
			retValue = 1;
			LOG("[pwmgr2] SoC Temperature : %d and Wifi Temperature : %d\n",(int)(*curTemperature), (int)(*wifiTemperature));
		} else {
			LOG("[pwmgr2] Failed IARM_BUS_MFRLIB_API_GetTemperature\n");
		}
		IARM_Free(IARM_MEMTYPE_PROCESSLOCAL,param);
	} else {
		LOG("PLAT_API_GetTemperature(): IARM_Malloc() failed\n");
	}

	return retValue;
}

int PLAT_API_SetTempThresholds(float tempHigh, float tempCritical)
{
	int result = 0;

	IARM_Result_t iarm_result = IARM_RESULT_IPCCORE_FAIL;
	IARM_Bus_MFRLib_ThermalSoCTemp_Param_t *param = NULL;

	iarm_result = IARM_Malloc(IARM_MEMTYPE_PROCESSLOCAL, sizeof(IARM_Bus_MFRLib_ThermalSoCTemp_Param_t), (void**)&param);
	if ((iarm_result == IARM_RESULT_SUCCESS) && param) {
		param->highTemp = (int)tempHigh;
		param->criticalTemp = (int)tempCritical;

		LOG("[pwmgr] Setting High Temperature Threshold as : %0.6f and Critical Temperature Threshold as : %0.6f \n",(float)param->highTemp, (float)param->criticalTemp);

		iarm_result = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME, IARM_BUS_MFRLIB_API_SetTemperatureThresholds, (void *)param, sizeof(IARM_Bus_MFRLib_ThermalSoCTemp_Param_t));

		if(iarm_result == IARM_RESULT_SUCCESS)
		{
			LOG("[pwmgr] Success IARM_BUS_MFRLIB_API_SetTemperatureThresholds\n");
			result = 1;
		}
		else
		{
			LOG("[pwmgr] Failed IARM_BUS_MFRLIB_API_SetTemperatureThresholds\n");
		}
		IARM_Free(IARM_MEMTYPE_PROCESSLOCAL,param);
	}
	else
	{
		LOG("PLAT_API_SetTempThresholds(): IARM_Malloc() failed\n");
	}

	return result;
}

int PLAT_API_GetTempThresholds(float *tempHigh, float *tempCritical)
{
	int result = 0;

	IARM_Result_t iarm_result = IARM_RESULT_IPCCORE_FAIL;
	IARM_Bus_MFRLib_ThermalSoCTemp_Param_t *param = NULL;
	iarm_result = IARM_Malloc(IARM_MEMTYPE_PROCESSLOCAL, sizeof(IARM_Bus_MFRLib_ThermalSoCTemp_Param_t), (void**)&param);

	if ((iarm_result == IARM_RESULT_SUCCESS) && param) {
		iarm_result = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME, IARM_BUS_MFRLIB_API_GetTemperatureThresholds, (void *)param, sizeof(IARM_Bus_MFRLib_ThermalSoCTemp_Param_t));

		if(iarm_result == IARM_RESULT_SUCCESS) {
			LOG("[pwmgr] Success IARM_BUS_MFRLIB_API_GetTemperatureThresholds\n");
			result = 1;
			*tempHigh = param->highTemp;
			*tempCritical = param->criticalTemp;
			LOG("[pwmgr] Received High Temperature Threshold as : %0.6f and Critical Temperature Threshold as : %0.6f \n",*tempHigh ,*tempCritical);
		} else {
			LOG("[pwmgr] Failed IARM_BUS_MFRLIB_API_GetTemperatureThresholds\n");
		}
		IARM_Free(IARM_MEMTYPE_PROCESSLOCAL,param);
	} else {
		LOG("PLAT_API_GetTempThresholds(): IARM_Malloc() failed\n");
	}

	return result;
}

#else //REALTEK_SPECIFIC
int PLAT_API_DetemineClockSpeeds(uint32_t *cpu_rate_Normal, uint32_t *cpu_rate_Scaled, uint32_t *cpu_rate_Minimal)
{
    FILE * fp;
    uint32_t normal = 0;
    uint32_t scaled = 0;
    uint32_t minimal = 0;
    uint32_t freqList[32];
    uint32_t numFreqs = 0;
    uint32_t i;

    fp = fopen ("/sys/devices/system/cpu/cpu0/cpufreq/scaling_available_frequencies", "r");
    if (fp == 0) {
        LOG("[%s:%d] Unable to open '/sys/devices/system/cpu/cpu0/cpufreq/scaling_available_frequencies' for reading\n", __FUNCTION__, __LINE__);
        return 0;
    }

    /* Determine available frequencies */
    while (numFreqs < sizeof(freqList)/sizeof(freqList[0]) && (fscanf(fp, "%u", &freqList[numFreqs]) == 1))
        numFreqs++;

    if (numFreqs<=0) {
        LOG("[%s] **ERROR** Unable to read sacaling frequencies!\n", __FUNCTION__);
        fclose(fp);  //CID:158617 - Resource leak
        return -1;
    }

    /* Ensure frequencies are sorted */
    qsort( (void*)freqList, numFreqs, sizeof(freqList[0]), uint32_compare );
    LOG("[%s] Scaling Frequency List:\n", __FUNCTION__);
    for(i=0; i < numFreqs; i++) LOG ("    [%s] %uhz\n", __FUNCTION__, freqList[i]);

    /* Select normal, scaled and minimal from the list */
    minimal=freqList[0];
    scaled=freqList[numFreqs/2];
    normal=freqList[numFreqs-1];
    LOG("[%s] Using -- Normal:%u Scaled:%u Minimal:%u\n", __FUNCTION__, normal, scaled, minimal);

    fclose(fp);

    if (cpu_rate_Normal)  *cpu_rate_Normal = normal;
    if (cpu_rate_Scaled)  *cpu_rate_Scaled = scaled;
    if (cpu_rate_Minimal) *cpu_rate_Minimal = minimal;
    return 1;
}

int PLAT_API_GetClockSpeed(uint32_t *speed)
{
    FILE* fp = NULL;

#ifdef ENABLE_LLAMA_PLATCO
    fp = fopen ("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq", "r");
    if (fp == 0) {
        LOG("[%s:%d] Unable to open '/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq' for writing\n", __FUNCTION__, __LINE__);
        return 0;
    }
#else
    fp = fopen ("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_cur_freq", "r");
    if (fp == 0) {
        LOG("[%s:%d] Unable to open '/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_cur_freq' for writing\n", __FUNCTION__, __LINE__);
        return 0;
    }
#endif

    if(0 >= fscanf(fp, "%u", speed)) {
        LOG("[%s:%d] Unable to get the speed \n", __FUNCTION__, __LINE__);
    }
    fclose(fp);  //CID:103784 - checked return

    return 1;
}

int PLAT_API_GetTemperature(mfrTemperatureState_t *curState, float *curTemperature, float *wifiTemperature)
{
    mfrTemperatureState_t state;
    int temperatureValue;
    int wifiTempValue;
    int retValue = 0;

    mfrError_t result = mfrGetTemperature(&state, &temperatureValue, &wifiTempValue);

#if 0
    /* Leave this debug code here commented out (or otherwise disabled by default). This is used in testing to allow manually controlling the returned temperature.
       This helps test functionallity without actually haveing to heat up the box */
    {
        FILE *fp;
        state = (mfrTemperatureState_t)IARM_BUS_PWRMGR_TEMPERATURE_NORMAL;
        temperatureValue=50.0;
        wifiTempValue=50.0;
        result = mfrERR_NONE;

        fp = fopen ("/opt/force_temp.soc", "r");
        if (fp) {
            fscanf(fp, "%d", &temperatureValue);
            fclose(fp);
        }

        fp = fopen ("/opt/force_temp.wifi", "r");
        if (fp) {
            fscanf(fp, "%d", &wifiTempValue);
            fclose(fp);
        }

        fp = fopen ("/opt/force_temp.state", "r");
        if (fp) {
            fscanf(fp, "%d", &state);
            fclose(fp);
        }
    }
#endif

    if (result == mfrERR_NONE)
    {
        LOG("[%s] Got MFR Temperatures SoC:%d Wifi:%d\n", __FUNCTION__, temperatureValue, wifiTempValue);
        *curState = (mfrTemperatureState_t )state;
        *curTemperature = temperatureValue;
        *wifiTemperature = wifiTempValue;
        retValue = 1;
    }

    return retValue;
}

int PLAT_API_SetClockSpeed(uint32_t speed)
{
    FILE* fp = NULL;
    uint32_t cur_speed = 0;
    LOG("[%s]Setting clock speed to [%d]\n", __FUNCTION__, speed );
    //Opening the clock speed adjusting

    fp = fopen ("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor", "w");
    if (fp == 0) {
        LOG("[%s:%d] Unable to open '/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor' for writing\n", __FUNCTION__, __LINE__);
        return 0;
    }

    /* Switch to 'userspace' mode */
    fprintf(fp, "userspace");
    fclose(fp);

    fp = fopen ("/sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed", "w");
    if (fp == 0) {
        LOG("[%s:%d] Unable to open '/sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed' for writing\n", __FUNCTION__, __LINE__);
        return 0;
    }

    /* Set the desired speed */
    fprintf(fp, "%u", speed);
    fclose(fp);

    if (PLAT_API_GetClockSpeed(&cur_speed) != 1 ) {
        LOG("[%s:%d] Failed to read current CPU speed\n", __FUNCTION__, __LINE__);
        return 1;
    }

    LOG("[%s] Clock speed set to [%d]\n", __FUNCTION__, cur_speed );

    return (speed == cur_speed) ? 1 : 0;
}

int PLAT_API_SetTempThresholds(float tempHigh, float tempCritical)
{
    mfrError_t response = mfrSetTempThresholds(tempHigh,tempCritical);
    int result = (response == mfrERR_NONE) ?1:0;

    return result;
}

int PLAT_API_GetTempThresholds(float *tempHigh, float *tempCritical)
{
    int result = 0;
    int highTemp = 0, criticalTemp= 0;

    mfrError_t response = mfrGetTempThresholds(&highTemp, &criticalTemp);
    if(mfrERR_NONE == response)
    {
        result = 1;
        *tempHigh = highTemp;
        *tempCritical = criticalTemp;
    }

    return result;
}
#endif //REALTEK_SPECIFIC
#endif //ENABLE_THERMAL_PROTECTION
#ifdef __cplusplus
}
#endif


/** @} */
/** @} */
