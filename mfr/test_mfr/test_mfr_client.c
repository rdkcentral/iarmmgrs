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
	IARM_Bus_MFRLib_GetSerializedData_Param_t *param;
	IARM_Result_t ret;
	char *pTmpStr;
	int len; 
	printf("mfrClient Entering %d\r\n", getpid());
	IARM_Bus_Init("mfrClient");
	int i = 0;
	errno_t safec_rc = -1;
	IARM_Bus_Connect();

	do{
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
				continue;
			}

			printf("%s returned (%d)::%s ","mfrSERIALIZED_TYPE_MANUFACTURER",ret, pTmpStr);
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
				continue;
			}

			printf("%s returned (%d)::%s","mfrSERIALIZED_TYPE_MANUFACTUREROUI",ret,pTmpStr);
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
				continue;
			}

			printf("%s returned (%d)::%s","mfrSERIALIZED_TYPE_MODELNAME",ret,pTmpStr);
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
				continue;
			}

			printf("%s returned (%d)::%s","mfrSERIALIZED_TYPE_DESCRIPTION",ret,pTmpStr);
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
				continue;
			}

			printf("%s returned (%d)::%s","mfrSERIALIZED_TYPE_PRODUCTCLASS",ret,pTmpStr);
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
				continue;
			}

			printf("%s returned (%d)::%s","mfrSERIALIZED_TYPE_SERIALNUMBER",ret,pTmpStr);
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
				continue;
			}

			printf("%s returned (%d)::%s","mfrSERIALIZED_TYPE_HARDWAREVERSION",ret,pTmpStr);
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
				continue;
			}

			printf("%s returned (%d)::%s","mfrSERIALIZED_TYPE_SOFTWAREVERSION",ret,pTmpStr);
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
				continue;
			}

			printf("%s returned (%d)::%s","mfrSERIALIZED_TYPE_PROVISIONINGCODE",ret,pTmpStr);
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
				continue;
			}

			printf("%s returned (%d)::%s","mfrSERIALIZED_TYPE_FIRSTUSEDATE",ret, pTmpStr);
			free(pTmpStr);
		}
		IARM_Free(IARM_MEMTYPE_PROCESSLOCAL,param);
		IARM_Malloc(IARM_MEMTYPE_PROCESSLOCAL, sizeof(IARM_Bus_MFRLib_GetSerializedData_Param_t), (void**)&param);
		param->type = mfrSERIALIZED_TYPE_PDRIVERSION;

		ret = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME,
			IARM_BUS_MFRLIB_API_GetSerializedData, (void *)param, sizeof(IARM_Bus_MFRLib_GetSerializedData_Param_t));

		if(ret != IARM_RESULT_SUCCESS)
		{
			printf("Call failed for %s: error code:%d\n","mfrSERIALIZED_TYPE_PDRIVERSION",ret);
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
				continue;
			}

			printf("%s returned (%d)::[%s]\r\n","mfrSERIALIZED_TYPE_PDRIVERSION",ret, pTmpStr);
			free(pTmpStr);
		}
		IARM_Free(IARM_MEMTYPE_PROCESSLOCAL,param);

	}while(0);

	IARM_Bus_Disconnect();
	IARM_Bus_Term();
	printf("Client Exiting\r\n");
}


/** @} */
/** @} */
