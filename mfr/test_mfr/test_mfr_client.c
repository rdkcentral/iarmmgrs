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
* @defgroup mfr
* @{
**/


#include "libIBus.h"
#include "mfrMgr.h"
#include <stdio.h>
#include <malloc.h>
#include "libIARMCore.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "safec_lib.h"

int main()
{
	IARM_Bus_MFRLib_GetSerializedData_Param_t *param = NULL;
	IARM_Result_t ret = IARM_RESULT_IPCCORE_FAIL;
	char *pTmpStr = NULL;
	int len = 0;
#ifdef MFR_TEMP_CLOCK_READ
        IARM_Bus_MFRLib_CurThermalState_t state = IARM_BUS_TEMPERATURE_NORMAL;
        IARM_Bus_MFRLib_ThermalSoCTemp_Param_t *thermalSoCTemp = NULL;
        IARM_Bus_MFRLib_ThermalSoCFreq_Param_t *thermalSoCFreq = NULL;
        IARM_Result_t iarm_result = IARM_RESULT_IPCCORE_FAIL;
        int temperatureValue = 0, wifiTempValue = 0;
        float tempHigh = 0.0, tempCritical = 0.0;
        uint32_t cpu_clock_Normal = 0, cpu_clock_Scaled = 0, cpu_clock_Minimal = 0, clock_speed = 0;
#endif
	printf("mfrClient Entering %d\r\n", getpid());
	if (IARM_Bus_Init("mfrClient") != IARM_RESULT_SUCCESS) {
		printf("mfrClient: IARM_Bus_Init failed.");
		return 0;
	}
	errno_t safec_rc = -1;
	if (IARM_Bus_Connect() != IARM_RESULT_SUCCESS) {
		printf("mfrClient: IARM_Bus_Connect failed.");
		return 0;
	}

	do{

#ifdef MFR_TEMP_CLOCK_READ
		iarm_result = IARM_Malloc(IARM_MEMTYPE_PROCESSLOCAL, sizeof(IARM_Bus_MFRLib_ThermalSoCTemp_Param_t), (void**)&thermalSoCTemp);
		if((iarm_result == IARM_RESULT_SUCCESS) && thermalSoCTemp)
		{
			iarm_result = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME, IARM_BUS_MFRLIB_API_GetTemperature, (void *)thermalSoCTemp, sizeof(IARM_Bus_MFRLib_ThermalSoCTemp_Param_t));

			if(iarm_result == IARM_RESULT_SUCCESS)
			{
				state = thermalSoCTemp->curState;
				temperatureValue = thermalSoCTemp->curSoCTemperature;
				wifiTempValue = thermalSoCTemp->curWiFiTemperature;
				printf("%s returned (%d) - [SUCCESS] Cur Thermal State = %d : SoC Temp = %d : WiFi Temp = %d \n","MFRLIB_API_GetTemperature" ,iarm_result ,state ,temperatureValue ,wifiTempValue);
			}
			else
			{
				printf("Call failed for %s: error code:%d\n","MFRLIB_API_GetTemperature",iarm_result);
			}
			IARM_Free(IARM_MEMTYPE_PROCESSLOCAL,thermalSoCTemp);
		}
		else
		{
			printf("Call to %s: failed due to unsuccessful IARM_Malloc()\n","MFRLIB_API_GetTemperature");
		}

		thermalSoCTemp = NULL;
		iarm_result = IARM_Malloc(IARM_MEMTYPE_PROCESSLOCAL, sizeof(IARM_Bus_MFRLib_ThermalSoCTemp_Param_t), (void**)&thermalSoCTemp);
		if((iarm_result == IARM_RESULT_SUCCESS) && thermalSoCTemp)
		{
			iarm_result = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME, IARM_BUS_MFRLIB_API_GetTemperatureThresholds, (void *)thermalSoCTemp, sizeof(IARM_Bus_MFRLib_ThermalSoCTemp_Param_t));

			if(iarm_result == IARM_RESULT_SUCCESS)
			{
				tempHigh = thermalSoCTemp->highTemp;
				tempCritical = thermalSoCTemp->criticalTemp;
				printf("%s returned (%d) - [SUCCESS] Old Values Returned : Thermal threshold : high = %0.6f, critical = %0.6f \n","MFRLIB_API_GetTemperatureThresholds" ,iarm_result ,tempHigh ,tempCritical);
			}
			else
			{
				printf("Call failed for %s: error code:%d\n","MFRLIB_API_GetTemperatureThresholds",iarm_result);
			}
			IARM_Free(IARM_MEMTYPE_PROCESSLOCAL,thermalSoCTemp);
		}
		else
		{
			printf("Call to %s: failed due to unsuccessful IARM_Malloc()\n","MFRLIB_API_GetTemperatureThresholds");
		}

		thermalSoCTemp = NULL;
		iarm_result = IARM_Malloc(IARM_MEMTYPE_PROCESSLOCAL, sizeof(IARM_Bus_MFRLib_ThermalSoCTemp_Param_t), (void**)&thermalSoCTemp);
		if((iarm_result == IARM_RESULT_SUCCESS) && thermalSoCTemp)
		{
			thermalSoCTemp->highTemp = 110;
			thermalSoCTemp->criticalTemp = 120;

			iarm_result = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME, IARM_BUS_MFRLIB_API_SetTemperatureThresholds, (void *)thermalSoCTemp, sizeof(IARM_Bus_MFRLib_ThermalSoCTemp_Param_t));

			if(iarm_result == IARM_RESULT_SUCCESS)
			{
				printf("%s returned (%d) - [SUCCESS] New Thermal threshold : high = %0.6f, critical = %0.6f \n","MFRLIB_API_SetTemperatureThresholds" ,iarm_result ,(float)thermalSoCTemp->highTemp ,(float)thermalSoCTemp->criticalTemp);
			}
			else
			{
				printf("Call failed for %s: error code:%d\n","MFRLIB_API_SetTemperatureThresholds",iarm_result);
			}
			IARM_Free(IARM_MEMTYPE_PROCESSLOCAL,thermalSoCTemp);
		}
		else
		{
			printf("Call to %s: failed due to unsuccessful IARM_Malloc()\n","MFRLIB_API_SetTemperatureThresholds");
		}

		thermalSoCTemp = NULL;
		iarm_result = IARM_Malloc(IARM_MEMTYPE_PROCESSLOCAL, sizeof(IARM_Bus_MFRLib_ThermalSoCTemp_Param_t), (void**)&thermalSoCTemp);
		if((iarm_result == IARM_RESULT_SUCCESS) && thermalSoCTemp)
		{
			iarm_result = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME, IARM_BUS_MFRLIB_API_GetTemperatureThresholds, (void *)thermalSoCTemp, sizeof(IARM_Bus_MFRLib_ThermalSoCTemp_Param_t));

			if(iarm_result == IARM_RESULT_SUCCESS)
			{
				tempHigh = thermalSoCTemp->highTemp;
				tempCritical = thermalSoCTemp->criticalTemp;
				printf("%s returned (%d) - [SUCCESS] New Values Returned : Thermal threshold : high = %0.6f, critical = %0.6f \n","MFRLIB_API_GetTemperatureThresholds" ,iarm_result ,tempHigh ,tempCritical);
			}
			else
			{
				printf("Call failed for %s: error code:%d\n","MFRLIB_API_GetTemperatureThresholds",iarm_result);
			}
			IARM_Free(IARM_MEMTYPE_PROCESSLOCAL,thermalSoCTemp);
		}
		else
		{
			printf("Call to %s: failed due to unsuccessful IARM_Malloc()\n","MFRLIB_API_GetTemperatureThresholds");
		}

		thermalSoCTemp = NULL;
		iarm_result = IARM_Malloc(IARM_MEMTYPE_PROCESSLOCAL, sizeof(IARM_Bus_MFRLib_ThermalSoCTemp_Param_t), (void**)&thermalSoCTemp);
		if((iarm_result == IARM_RESULT_SUCCESS) && thermalSoCTemp)
		{
			thermalSoCTemp->highTemp = 100;
			thermalSoCTemp->criticalTemp = 110;

			iarm_result = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME, IARM_BUS_MFRLIB_API_SetTemperatureThresholds, (void *)thermalSoCTemp, sizeof(IARM_Bus_MFRLib_ThermalSoCTemp_Param_t));

			if(iarm_result == IARM_RESULT_SUCCESS)
			{
				printf("%s returned (%d) - [SUCCESS] Setting Default Thermal threshold Values : high = %0.6f, critical = %0.6f \n","MFRLIB_API_SetTemperatureThresholds" ,iarm_result ,(float)thermalSoCTemp->highTemp ,(float)thermalSoCTemp->criticalTemp);
			}
			else
			{
				printf("Call failed for %s: error code:%d\n","MFRLIB_API_SetTemperatureThresholds",iarm_result);
			}
			IARM_Free(IARM_MEMTYPE_PROCESSLOCAL,thermalSoCTemp);
		}
		else
		{
			printf("Call to %s: failed due to unsuccessful IARM_Malloc()\n","MFRLIB_API_SetTemperatureThresholds");
		}

		thermalSoCFreq = NULL;
		iarm_result = IARM_Malloc(IARM_MEMTYPE_PROCESSLOCAL, sizeof(IARM_Bus_MFRLib_ThermalSoCFreq_Param_t), (void**)&thermalSoCFreq);
		if((iarm_result == IARM_RESULT_SUCCESS) && thermalSoCFreq)
		{
			iarm_result = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME, IARM_BUS_MFRLIB_API_SearchCPUClockSpeeds, (void *)thermalSoCFreq, sizeof(IARM_Bus_MFRLib_ThermalSoCFreq_Param_t));

			if(iarm_result == IARM_RESULT_SUCCESS)
			{
				cpu_clock_Normal   =  thermalSoCFreq->cpu_rate_Normal;
				cpu_clock_Scaled   =  thermalSoCFreq->cpu_rate_Scaled;
				cpu_clock_Minimal  =  thermalSoCFreq->cpu_rate_Minimal;
				printf("%s returned (%d) - [SUCCESS] Available CPU Frequencies are: Normal:%u Scaled:%u Minimal:%u\n","MFRLIB_API_SearchCPUClockSpeeds" ,iarm_result ,cpu_clock_Normal ,cpu_clock_Scaled ,cpu_clock_Minimal);

			}
			else
			{
				printf("Call failed for %s: error code:%d\n","MFRLIB_API_SearchCPUClockSpeeds",iarm_result);
			}
			IARM_Free(IARM_MEMTYPE_PROCESSLOCAL,thermalSoCFreq);
		}
		else
		{
			printf("Call to %s: failed due to unsuccessful IARM_Malloc()\n","MFRLIB_API_SearchCPUClockSpeeds");
		}

                thermalSoCFreq = NULL;
                iarm_result = IARM_Malloc(IARM_MEMTYPE_PROCESSLOCAL, sizeof(IARM_Bus_MFRLib_ThermalSoCFreq_Param_t), (void**)&thermalSoCFreq);
                if((iarm_result == IARM_RESULT_SUCCESS) && thermalSoCFreq)
                {
                        iarm_result = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME, IARM_BUS_MFRLIB_API_GetCPUClockSpeed, (void *)thermalSoCFreq, sizeof(IARM_Bus_MFRLib_ThermalSoCFreq_Param_t));

                        if(iarm_result == IARM_RESULT_SUCCESS)
                        {
                                clock_speed = thermalSoCFreq->cpu_clock_speed;
                                printf("%s returned (%d) - [SUCCESS] Getting CPU Clock Speed  as [%u] \n","MFRLIB_API_GetCPUClockSpeed" ,iarm_result ,clock_speed);
                        }
                        else
                        {
                                printf("Call failed for %s: error code:%d\n","MFRLIB_API_GetCPUClockSpeed",iarm_result);
                        }
                        IARM_Free(IARM_MEMTYPE_PROCESSLOCAL,thermalSoCFreq);
                }
                else
                {
                    printf("Call to %s: failed due to unsuccessful IARM_Malloc()\n","MFRLIB_API_GetCPUClockSpeed");
                }

		thermalSoCFreq = NULL;
		iarm_result = IARM_Malloc(IARM_MEMTYPE_PROCESSLOCAL, sizeof(IARM_Bus_MFRLib_ThermalSoCFreq_Param_t), (void**)&thermalSoCFreq);
		if((iarm_result == IARM_RESULT_SUCCESS) && thermalSoCFreq)
		{
			thermalSoCFreq->cpu_clock_speed = 700000;

			iarm_result = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME, IARM_BUS_MFRLIB_API_SetCPUClockSpeed, (void *)thermalSoCFreq, sizeof(IARM_Bus_MFRLib_ThermalSoCFreq_Param_t));

			if(iarm_result == IARM_RESULT_SUCCESS)
			{
				printf("%s returned (%d) - [SUCCESS] Setting New CPU Clock Speed to [%u] \n","MFRLIB_API_SetCPUClockSpeed" ,iarm_result ,thermalSoCFreq->cpu_clock_speed);
			}
			else
			{
				printf("Call failed for %s: error code:%d\n","MFRLIB_API_SetCPUClockSpeed",iarm_result);
			}
			IARM_Free(IARM_MEMTYPE_PROCESSLOCAL,thermalSoCFreq);
		}
		else
		{
			printf("Call to %s: failed due to unsuccessful IARM_Malloc()\n","MFRLIB_API_SetCPUClockSpeed");
		}

		thermalSoCFreq = NULL;
		iarm_result = IARM_Malloc(IARM_MEMTYPE_PROCESSLOCAL, sizeof(IARM_Bus_MFRLib_ThermalSoCFreq_Param_t), (void**)&thermalSoCFreq);
		if((iarm_result == IARM_RESULT_SUCCESS) && thermalSoCFreq)
		{
			iarm_result = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME, IARM_BUS_MFRLIB_API_GetCPUClockSpeed, (void *)thermalSoCFreq, sizeof(IARM_Bus_MFRLib_ThermalSoCFreq_Param_t));

			if(iarm_result == IARM_RESULT_SUCCESS)
			{
				clock_speed = thermalSoCFreq->cpu_clock_speed;
				printf("%s returned (%d) - [SUCCESS] Getting New CPU Clock Speed as [%u] \n","MFRLIB_API_GetCPUClockSpeed" ,iarm_result ,clock_speed);
			}
			else
			{
				printf("Call failed for %s: error code:%d\n","MFRLIB_API_GetCPUClockSpeed",iarm_result);
			}
			IARM_Free(IARM_MEMTYPE_PROCESSLOCAL,thermalSoCFreq);
		}
		else
                {
                        printf("Call to %s: failed due to unsuccessful IARM_Malloc()\n","MFRLIB_API_GetCPUClockSpeed");
                }

		thermalSoCFreq = NULL;
                 iarm_result = IARM_Malloc(IARM_MEMTYPE_PROCESSLOCAL, sizeof(IARM_Bus_MFRLib_ThermalSoCFreq_Param_t), (void**)&thermalSoCFreq);
                 if((iarm_result == IARM_RESULT_SUCCESS) && thermalSoCFreq)
                 {
                         thermalSoCFreq->cpu_clock_speed = 1200000;

                         iarm_result = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME, IARM_BUS_MFRLIB_API_SetCPUClockSpeed, (void *)thermalSoCFreq, sizeof(IARM_Bus_MFRLib_ThermalSoCFreq_Param_t));

                         if(iarm_result == IARM_RESULT_SUCCESS)
                         {
                                 printf("%s returned (%d) - [SUCCESS] CPU Clock Speed Setting to default as [%u] \n","MFRLIB_API_SetCPUClockSpeed" ,iarm_result ,thermalSoCFreq->cpu_clock_speed);
                         }
                         else
                         {
                                 printf("Call failed for %s: error code:%d\n","MFRLIB_API_SetCPUClockSpeed",iarm_result);
                         }
                         IARM_Free(IARM_MEMTYPE_PROCESSLOCAL,thermalSoCFreq);
                 }
                 else
                 {
                         printf("Call to %s: failed due to unsuccessful IARM_Malloc()\n","MFRLIB_API_SetCPUClockSpeed");
                 }
#endif
 		IARM_Malloc(IARM_MEMTYPE_PROCESSLOCAL, sizeof(IARM_Bus_MFRLib_GetSerializedData_Param_t), (void**)&param);
 
		param->type = mfrSERIALIZED_TYPE_PRODUCTCLASS;

		ret = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME,
			IARM_BUS_MFRLIB_API_GetSerializedData, (void *)param, sizeof(IARM_Bus_MFRLib_GetSerializedData_Param_t));

		if(ret != IARM_RESULT_SUCCESS)
		{
			printf("Call failed for %s: error code:%d\n","mfrSERIALIZED_TYPE_PRODUCTCLASS",ret);
		}
		else
		{
			len = param->bufLen + 1;
			pTmpStr = (char *)malloc(len);
			memset(pTmpStr,0,len);
			memcpy(pTmpStr,param->buffer,param->bufLen);
			printf("%s returned (%d) - %s\n","mfrSERIALIZED_TYPE_PRODUCTCLASS",ret, pTmpStr);
			free(pTmpStr);
		}
		IARM_Free(IARM_MEMTYPE_PROCESSLOCAL,param);
		
		IARM_Malloc(IARM_MEMTYPE_PROCESSLOCAL, sizeof(IARM_Bus_MFRLib_GetSerializedData_Param_t), (void**)&param);
		param->type = mfrSERIALIZED_TYPE_WPSPIN;

		ret = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME,
			IARM_BUS_MFRLIB_API_GetSerializedData, (void *)param, sizeof(IARM_Bus_MFRLib_GetSerializedData_Param_t));

		if(ret != IARM_RESULT_SUCCESS)
		{
			printf("Call failed for %s: error code:%d\n","mfrSERIALIZED_TYPE_WPSPIN",ret);
		}
		else
		{
			len = param->bufLen + 1;
			pTmpStr = (char *)malloc(len);
			memset(pTmpStr,0,len);
			memcpy(pTmpStr,param->buffer,param->bufLen);
			printf("%s returned (%d) - %s\n","mfrSERIALIZED_TYPE_WPSPIN",ret,pTmpStr);
			free(pTmpStr);
		}
		IARM_Free(IARM_MEMTYPE_PROCESSLOCAL,param);
		
        	IARM_Malloc(IARM_MEMTYPE_PROCESSLOCAL, sizeof(IARM_Bus_MFRLib_GetSerializedData_Param_t), (void**)&param);
		param->type = mfrSERIALIZED_TYPE_DESCRIPTION;

		ret = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME,
			IARM_BUS_MFRLIB_API_GetSerializedData, (void *)param, sizeof(IARM_Bus_MFRLib_GetSerializedData_Param_t));

		if(ret != IARM_RESULT_SUCCESS)
		{
			printf("Call failed for %s: error code:%d\n","mfrSERIALIZED_TYPE_DESCRIPTION",ret);
		}
		else
		{
			len = param->bufLen + 1;
			pTmpStr = (char *)malloc(len);
			memset(pTmpStr,0,len);
			memcpy(pTmpStr,param->buffer,param->bufLen);
			printf("%s returned (%d) - %s\n","mfrSERIALIZED_TYPE_DESCRIPTION",ret, pTmpStr);
			free(pTmpStr);
		}
		IARM_Free(IARM_MEMTYPE_PROCESSLOCAL,param);
		
		IARM_Malloc(IARM_MEMTYPE_PROCESSLOCAL, sizeof(IARM_Bus_MFRLib_GetSerializedData_Param_t), (void**)&param);
		param->type = mfrSERIALIZED_TYPE_ETHERNETMAC;

		ret = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME,
			IARM_BUS_MFRLIB_API_GetSerializedData, (void *)param, sizeof(IARM_Bus_MFRLib_GetSerializedData_Param_t));

		if(ret != IARM_RESULT_SUCCESS)
		{
			printf("Call failed for %s: error code:%d\n","mfrSERIALIZED_TYPE_ETHERNETMAC",ret);
		}
		else
		{
			len = param->bufLen + 1;
			pTmpStr = (char *)malloc(len);
			memset(pTmpStr,0,len);
			memcpy(pTmpStr,param->buffer,param->bufLen);
		        printf("%s returned (%d) - %s\n","mfrSERIALIZED_TYPE_ETHERNETMAC",ret,pTmpStr);
 			free(pTmpStr);
 		}
 		IARM_Free(IARM_MEMTYPE_PROCESSLOCAL,param);


                IARM_Malloc(IARM_MEMTYPE_PROCESSLOCAL, sizeof(IARM_Bus_MFRLib_GetSerializedData_Param_t), (void**)&param);
                param->type = mfrSERIALIZED_TYPE_WIFIMAC;

                ret = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME,
                        IARM_BUS_MFRLIB_API_GetSerializedData, (void *)param, sizeof(IARM_Bus_MFRLib_GetSerializedData_Param_t));

                if(ret != IARM_RESULT_SUCCESS)
                {
                        printf("Call failed for %s: error code:%d\n","mfrSERIALIZED_TYPE_WIFIMAC",ret);
                }
                else
                {
                        len = param->bufLen + 1;
                        pTmpStr = (char *)malloc(len);
                        memset(pTmpStr,0,len);
                        memcpy(pTmpStr,param->buffer,param->bufLen);
                        printf("%s returned (%d) - %s\n","mfrSERIALIZED_TYPE_WIFIMAC",ret,pTmpStr);
                        free(pTmpStr);
                }
                IARM_Free(IARM_MEMTYPE_PROCESSLOCAL,param);

                IARM_Malloc(IARM_MEMTYPE_PROCESSLOCAL, sizeof(IARM_Bus_MFRLib_GetSerializedData_Param_t), (void**)&param);
                param->type = mfrSERIALIZED_TYPE_BLUETOOTHMAC;

                ret = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME,
                        IARM_BUS_MFRLIB_API_GetSerializedData, (void *)param, sizeof(IARM_Bus_MFRLib_GetSerializedData_Param_t));

                if(ret != IARM_RESULT_SUCCESS)
                {
                        printf("Call failed for %s: error code:%d\n","mfrSERIALIZED_TYPE_BLUETOOTHMAC",ret);
                }
                else
                {
                        len = param->bufLen + 1;
                        pTmpStr = (char *)malloc(len);
                        memset(pTmpStr,0,len);
                        memcpy(pTmpStr,param->buffer,param->bufLen);
                        printf("%s returned (%d) - %s\n","mfrSERIALIZED_TYPE_BLUETOOTHMAC",ret,pTmpStr);
                        free(pTmpStr);
                }
                IARM_Free(IARM_MEMTYPE_PROCESSLOCAL,param);


                IARM_Malloc(IARM_MEMTYPE_PROCESSLOCAL, sizeof(IARM_Bus_MFRLib_GetSerializedData_Param_t), (void**)&param);
                param->type = mfrSERIALIZED_TYPE_MANUFACTURER;

		ret = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME,
			IARM_BUS_MFRLIB_API_GetSerializedData, (void *)param, sizeof(IARM_Bus_MFRLib_GetSerializedData_Param_t));

		if(ret != IARM_RESULT_SUCCESS)
		{
			printf("Call failed for %s: error code:%d\n","mfrSERIALIZED_TYPE_MANUFACTURER",ret);
		}
		else
		{
			len = param->bufLen + 1;
			pTmpStr = (char *)malloc(len);
			safec_rc = memset_s(pTmpStr, len, 0, len);
			ERR_CHK(safec_rc);
			safec_rc = memcpy_s(pTmpStr, len, param->buffer, param->bufLen);
			if(safec_rc != EOK)
			{
				ERR_CHK(safec_rc);
				free(pTmpStr);
				break;
			}

			printf("%s returned (%d)::%s \n","mfrSERIALIZED_TYPE_MANUFACTURER",ret, pTmpStr);
			free(pTmpStr);
		}
		IARM_Free(IARM_MEMTYPE_PROCESSLOCAL,param);
		
		IARM_Malloc(IARM_MEMTYPE_PROCESSLOCAL, sizeof(IARM_Bus_MFRLib_GetSerializedData_Param_t), (void**)&param);
		param->type = mfrSERIALIZED_TYPE_MANUFACTUREROUI;

		ret = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME,
			IARM_BUS_MFRLIB_API_GetSerializedData, (void *)param, sizeof(IARM_Bus_MFRLib_GetSerializedData_Param_t));

		if(ret != IARM_RESULT_SUCCESS)
		{
			printf("Call failed for %s: error code:%d\n","mfrSERIALIZED_TYPE_MANUFACTUREROUI",ret);
		}
		else
		{
			len = param->bufLen + 1;
			pTmpStr = (char *)malloc(len);
			safec_rc = memset_s(pTmpStr, len, 0, len);
			ERR_CHK(safec_rc);
			safec_rc = memcpy_s(pTmpStr, len, param->buffer, param->bufLen);
			if(safec_rc != EOK)
			{
				ERR_CHK(safec_rc);
				free(pTmpStr);
				break;
			}

			printf("%s returned (%d)::%s \n","mfrSERIALIZED_TYPE_MANUFACTUREROUI",ret,pTmpStr);
			free(pTmpStr);
		}
		IARM_Free(IARM_MEMTYPE_PROCESSLOCAL,param);
		IARM_Malloc(IARM_MEMTYPE_PROCESSLOCAL, sizeof(IARM_Bus_MFRLib_GetSerializedData_Param_t), (void**)&param);
		param->type = mfrSERIALIZED_TYPE_MODELNAME;

		ret = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME,
			IARM_BUS_MFRLIB_API_GetSerializedData, (void *)param, sizeof(IARM_Bus_MFRLib_GetSerializedData_Param_t));

		if(ret != IARM_RESULT_SUCCESS)
		{
			printf("Call failed for %s: error code:%d\n","mfrSERIALIZED_TYPE_MODELNAME",ret);
		}
		else
		{
			len = param->bufLen + 1;
			pTmpStr = (char *)malloc(len);
			safec_rc = memset_s(pTmpStr, len, 0, len);
			ERR_CHK(safec_rc);
			safec_rc = memcpy_s(pTmpStr, len, param->buffer, param->bufLen);
			if(safec_rc != EOK)
			{
				ERR_CHK(safec_rc);
				free(pTmpStr);
				break;
			}

			printf("%s returned (%d)::%s\n","mfrSERIALIZED_TYPE_MODELNAME",ret,pTmpStr);
			free(pTmpStr);
		}
		IARM_Free(IARM_MEMTYPE_PROCESSLOCAL,param);
		
		IARM_Malloc(IARM_MEMTYPE_PROCESSLOCAL, sizeof(IARM_Bus_MFRLib_GetSerializedData_Param_t), (void**)&param);
		param->type = mfrSERIALIZED_TYPE_DESCRIPTION;

		ret = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME,
			IARM_BUS_MFRLIB_API_GetSerializedData, (void *)param, sizeof(IARM_Bus_MFRLib_GetSerializedData_Param_t));

		if(ret != IARM_RESULT_SUCCESS)
		{
			printf("Call failed for %s: error code:%d\n","mfrSERIALIZED_TYPE_DESCRIPTION",ret);
		}
		else
		{
			len = param->bufLen + 1;
			pTmpStr = (char *)malloc(len);
			safec_rc = memset_s(pTmpStr, len, 0, len);
			ERR_CHK(safec_rc);
			safec_rc = memcpy_s(pTmpStr, len, param->buffer, param->bufLen);
			if(safec_rc != EOK)
			{
				ERR_CHK(safec_rc);
				free(pTmpStr);
				break;
			}

			printf("%s returned (%d)::%s \n","mfrSERIALIZED_TYPE_DESCRIPTION",ret,pTmpStr);
			free(pTmpStr);
		}			
		IARM_Free(IARM_MEMTYPE_PROCESSLOCAL,param);
		IARM_Malloc(IARM_MEMTYPE_PROCESSLOCAL, sizeof(IARM_Bus_MFRLib_GetSerializedData_Param_t), (void**)&param);
		param->type = mfrSERIALIZED_TYPE_PRODUCTCLASS;

		ret = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME,
			IARM_BUS_MFRLIB_API_GetSerializedData, (void *)param, sizeof(IARM_Bus_MFRLib_GetSerializedData_Param_t));

		if(ret != IARM_RESULT_SUCCESS)
		{
			printf("Call failed for %s: error code:%d\n","mfrSERIALIZED_TYPE_PRODUCTCLASS",ret);
		}
		else
		{
			len = param->bufLen + 1;
			pTmpStr = (char *)malloc(len);
			safec_rc = memset_s(pTmpStr, len, 0, len);
			ERR_CHK(safec_rc);
			safec_rc = memcpy_s(pTmpStr, len, param->buffer, param->bufLen);
			if(safec_rc != EOK)
			{
				ERR_CHK(safec_rc);
				free(pTmpStr);
				break;
			}

			printf("%s returned (%d)::%s\n","mfrSERIALIZED_TYPE_PRODUCTCLASS",ret,pTmpStr);
			free(pTmpStr);
		}
		IARM_Free(IARM_MEMTYPE_PROCESSLOCAL,param);
		IARM_Malloc(IARM_MEMTYPE_PROCESSLOCAL, sizeof(IARM_Bus_MFRLib_GetSerializedData_Param_t), (void**)&param);
		param->type = mfrSERIALIZED_TYPE_SERIALNUMBER;

		ret = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME,
			IARM_BUS_MFRLIB_API_GetSerializedData, (void *)param, sizeof(IARM_Bus_MFRLib_GetSerializedData_Param_t));

		if(ret != IARM_RESULT_SUCCESS)
		{
			printf("Call failed for %s: error code:%d\n","mfrSERIALIZED_TYPE_SERIALNUMBER",ret);
		}
		else
		{
			len = param->bufLen + 1;
			pTmpStr = (char *)malloc(len);
			safec_rc = memset_s(pTmpStr, len, 0, len);
			ERR_CHK(safec_rc);
			safec_rc = memcpy_s(pTmpStr, len, param->buffer, param->bufLen);
			if(safec_rc != EOK)
			{
				ERR_CHK(safec_rc);
				free(pTmpStr);
				break;
			}

			printf("%s returned (%d)::%s\n","mfrSERIALIZED_TYPE_SERIALNUMBER",ret,pTmpStr);
			free(pTmpStr);
		}

		IARM_Free(IARM_MEMTYPE_PROCESSLOCAL,param);
		IARM_Malloc(IARM_MEMTYPE_PROCESSLOCAL, sizeof(IARM_Bus_MFRLib_GetSerializedData_Param_t), (void**)&param);
		param->type = mfrSERIALIZED_TYPE_HARDWAREVERSION;

		ret = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME,
			IARM_BUS_MFRLIB_API_GetSerializedData, (void *)param, sizeof(IARM_Bus_MFRLib_GetSerializedData_Param_t));

		if(ret != IARM_RESULT_SUCCESS)
		{
			printf("Call failed for %s: error code:%d\n","mfrSERIALIZED_TYPE_HARDWAREVERSION",ret);
		}
		else
		{
			len = param->bufLen + 1;
			pTmpStr = (char *)malloc(len);
			safec_rc = memset_s(pTmpStr, len, 0, len);
			ERR_CHK(safec_rc);
			safec_rc = memcpy_s(pTmpStr, len, param->buffer, param->bufLen);
			if(safec_rc != EOK)
			{
				ERR_CHK(safec_rc);
				free(pTmpStr);
				break;
			}

			printf("%s returned (%d)::%s\n","mfrSERIALIZED_TYPE_HARDWAREVERSION",ret,pTmpStr);
			free(pTmpStr);
		}
		IARM_Free(IARM_MEMTYPE_PROCESSLOCAL,param);
		IARM_Malloc(IARM_MEMTYPE_PROCESSLOCAL, sizeof(IARM_Bus_MFRLib_GetSerializedData_Param_t), (void**)&param);
		param->type = mfrSERIALIZED_TYPE_SOFTWAREVERSION;

		ret = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME,
			IARM_BUS_MFRLIB_API_GetSerializedData, (void *)param, sizeof(IARM_Bus_MFRLib_GetSerializedData_Param_t));

		if(ret != IARM_RESULT_SUCCESS)
		{
			printf("Call failed for %s: error code:%d\n","mfrSERIALIZED_TYPE_SOFTWAREVERSION",ret);
		}
		else
		{
			len = param->bufLen + 1;
			pTmpStr = (char *)malloc(len);
			safec_rc = memset_s(pTmpStr, len, 0, len);
			ERR_CHK(safec_rc);
			safec_rc = memcpy_s(pTmpStr, len, param->buffer, param->bufLen);
			if(safec_rc != EOK)
			{
				ERR_CHK(safec_rc);
				free(pTmpStr);
				break;
			}

			printf("%s returned (%d)::%s \n","mfrSERIALIZED_TYPE_SOFTWAREVERSION",ret,pTmpStr);
			free(pTmpStr);
		}
		IARM_Free(IARM_MEMTYPE_PROCESSLOCAL,param);
		IARM_Malloc(IARM_MEMTYPE_PROCESSLOCAL, sizeof(IARM_Bus_MFRLib_GetSerializedData_Param_t), (void**)&param);
		param->type = mfrSERIALIZED_TYPE_PROVISIONINGCODE;

		ret = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME,
			IARM_BUS_MFRLIB_API_GetSerializedData, (void *)param, sizeof(IARM_Bus_MFRLib_GetSerializedData_Param_t));

		if(ret != IARM_RESULT_SUCCESS)
		{
			printf("Call failed for %s: error code:%d\n","mfrSERIALIZED_TYPE_PROVISIONINGCODE",ret);
		}
		else
		{
			len = param->bufLen + 1;
			pTmpStr = (char *)malloc(len);
			safec_rc = memset_s(pTmpStr, len, 0, len);
			ERR_CHK(safec_rc);
			safec_rc = memcpy_s(pTmpStr, len, param->buffer, param->bufLen);
			if(safec_rc != EOK)
			{
				ERR_CHK(safec_rc);
				free(pTmpStr);
				break;
			}

			printf("%s returned (%d)::%s \n","mfrSERIALIZED_TYPE_PROVISIONINGCODE",ret,pTmpStr);
			free(pTmpStr);
		}
		IARM_Free(IARM_MEMTYPE_PROCESSLOCAL,param);
		IARM_Malloc(IARM_MEMTYPE_PROCESSLOCAL, sizeof(IARM_Bus_MFRLib_GetSerializedData_Param_t), (void**)&param);
		param->type = mfrSERIALIZED_TYPE_FIRSTUSEDATE;

		ret = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME,
			IARM_BUS_MFRLIB_API_GetSerializedData, (void *)param, sizeof(IARM_Bus_MFRLib_GetSerializedData_Param_t));

		if(ret != IARM_RESULT_SUCCESS)
		{
			printf("Call failed for %s: error code:%d\n","mfrSERIALIZED_TYPE_FIRSTUSEDATE",ret);
		}
		else
		{
			len = param->bufLen + 1;
			pTmpStr = (char *)malloc(len);
			safec_rc = memset_s(pTmpStr, len, 0, len);
			ERR_CHK(safec_rc);
			safec_rc = memcpy_s(pTmpStr, len, param->buffer, param->bufLen);
			if(safec_rc != EOK)
			{
				ERR_CHK(safec_rc);
				free(pTmpStr);
				break;
			}

			printf("%s returned (%d)::%s \n","mfrSERIALIZED_TYPE_FIRSTUSEDATE",ret, pTmpStr);
			free(pTmpStr);
		}
		IARM_Free(IARM_MEMTYPE_PROCESSLOCAL,param);
		IARM_Malloc(IARM_MEMTYPE_PROCESSLOCAL, sizeof(IARM_Bus_MFRLib_GetSerializedData_Param_t), (void**)&param);
		param->type = mfrSERIALIZED_TYPE_PDRIVERSION;

		ret = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME,
			IARM_BUS_MFRLIB_API_GetSerializedData, (void *)param, sizeof(IARM_Bus_MFRLib_GetSerializedData_Param_t));

		if(ret != IARM_RESULT_SUCCESS)
		{
			printf("Call failed for %s: error code:%d\n","mfrSERIALIZED_TYPE_PDRIVERSION \n",ret);
		}
		else
		{
			len = param->bufLen + 1;
			pTmpStr = (char *)malloc(len);
			safec_rc = memset_s(pTmpStr, len, 0, len);
			ERR_CHK(safec_rc);
			safec_rc = memcpy_s(pTmpStr, len, param->buffer, param->bufLen);
			if(safec_rc != EOK)
			{
				ERR_CHK(safec_rc);
				free(pTmpStr);
				break;
			}

			printf("%s returned (%d)::[%s]\r\n","mfrSERIALIZED_TYPE_PDRIVERSION",ret, pTmpStr);
			free(pTmpStr);
		}
		IARM_Free(IARM_MEMTYPE_PROCESSLOCAL,param);

	}while(0);

	if (IARM_Bus_Disconnect() != IARM_RESULT_SUCCESS) {
		printf("Client: IARM_Bus_Disconnect failed.\n");
	}
	if (IARM_Bus_Term() != IARM_RESULT_SUCCESS) {
		printf("Client: IARM_Bus_Term failed.\n");
	}
	printf("Client Exiting\r\n");
}


/** @} */
/** @} */
