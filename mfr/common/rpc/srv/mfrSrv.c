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


#include <string.h>
#include <pthread.h>
#include "mfrTypes.h"
#include "mfrMgr.h"
#include "libIARM.h"
#include "libIBus.h"
#include "iarmUtil.h"
#include "mfrMgrInternal.h"
#include <stdio.h>
#include <memory.h>
#include <dlfcn.h>
#include <unistd.h>
#include "libIARMCore.h"
#include "safec_lib.h"

static int m_initialized =0;
static pthread_mutex_t mfrLock = PTHREAD_MUTEX_INITIALIZER;

static char writeImageCbModule[MAX_BUF] = "";

static mfrUpgradeStatus_t lastStatus;


#define IARM_Bus_Lock(lock) pthread_mutex_lock(&mfrLock)
#define IARM_Bus_Unlock(lock) pthread_mutex_unlock(&mfrLock)
#define RDK_MFRCRYPTOLIB_NAME "libmfrcrypto.so"

typedef mfrError_t (*mfrCrypto_init_t)( const mfrSerializedData_t *);
typedef mfrError_t (*mfrCrypto_Encrypt_t)(const mfrSerializedData_t *, mfrSerializedData_t * );
typedef mfrError_t (*mfrCrypto_Decrypt_t)(const mfrSerializedData_t *, mfrSerializedData_t * );
typedef mfrError_t (*mfrCrypto_term_t)();

typedef mfrError_t (*mfr_init_t)();
typedef mfrError_t (*mfrGetSerializedData_t)( mfrSerializedType_t ,  mfrSerializedData_t *, mfrEncryptFunction_t *);
typedef mfrError_t (*mfrSetSerializedData_t)( mfrSerializedType_t ,  mfrSerializedData_t *, mfrDecryptFunction_t *);
typedef mfrError_t (*mfrWriteImage_t)(const char *,  const char *, mfrImageType_t ,  mfrUpgradeStatusNotify_t );
typedef mfrError_t (*mfrSetImageWriteProgress_t)(const char *, mfrImageType_t,  mfrUpgradeProgress_t );
typedef mfrError_t (*mfrGetImageWriteProgress_t)(const char * imageName,mfrImageType_t imageType,mfrUpgradeProgress_t *);
typedef mfrError_t (*mfrDeletePDRI_t)(void);
typedef mfrError_t (*mfrScrubAllBanks_t)(void);
typedef mfrError_t (*mfr_shutdown_t)(void);
typedef mfrError_t (*mfrReboot_t)(const char *);
typedef mfrError_t (*mfrSetCableCardType_t)(mfrCableCardType_t);
typedef mfrError_t (*mfrSetHostFirmwareInfo_t)(const mfrHostFirmwareInfo_t *);
typedef mfrError_t (*mfrGetBootImageName_t)(int , char *, int ,mfrImageType_t );
typedef mfrError_t (*mfrGetPathConfiguration_t)(mfrConfigPathType_t, char *, int *);
typedef mfrError_t (*mfrGetDFAST2Data_t)(mfrDFAST2Params_t *);


IARM_Result_t _mfr_init(void *arg);
IARM_Result_t _mfr_shutdown(void *arg);
IARM_Result_t _mfrGetSerializedData(void *arg);
IARM_Result_t _mfrSetSerializedData(void *arg);
IARM_Result_t _mfrWriteImage(void *arg);
IARM_Result_t _mfrSetImageWriteProgress(void *arg);
IARM_Result_t _mfrGetImageWriteProgress(void *arg);
IARM_Result_t _mfrDeletePDRI(void *arg);
IARM_Result_t _mfrScrubAllBanks(void *arg);
IARM_Result_t _mfrReboot(void *arg);
IARM_Result_t _mfrSetCableCardType(void *arg);
IARM_Result_t _mfrSetHostFirmwareInfo(void *arg);
IARM_Result_t _mfrGetBootImageName(void *arg);
IARM_Result_t _mfrGetPathConfiguration(void *arg);
IARM_Result_t _mfrGetDFAST2Data(void *arg);

void *find_func(const char* lib_name, const char *proc_name)
{
    void *ret = NULL;
    void *dllib = dlopen(lib_name, RTLD_LAZY);
    if (dllib)
    {
	ret = dlsym(dllib, proc_name);
	if (ret)
	{
	    printf("%s is defined and loaded\r\n",proc_name);
	}
	else
	{
	    printf("%s is not defined\r\n",proc_name);
	}
	dlclose(dllib);
    }
    else
    {
	printf("Opening library [%s] failed\r\n", lib_name);
    }
    return ret;
}

IARM_Result_t mfrMgr_start( )
{
    IARM_Result_t retCode = IARM_RESULT_INVALID_STATE;
#ifndef RDK_MFRLIB_NAME
    printf("Please define RDK_MFRLIB_NAME\r\n");
    printf("Exiting %s\n",__func__);
    return retCode;
#else

     static mfr_init_t func = 0;
    IARM_Bus_Lock(lock);
    printf("In %s\n",__func__);
    if (func == 0)
    {
    	func = (mfr_init_t) find_func(RDK_MFRLIB_NAME, "mfr_init");
    }
    if (func)
    {
        printf("Calling mfr_init  err %p\r\n", func);
     	  // mfrError_t err = func();
       mfrError_t err = mfr_init();

       printf("Calling mfr_init returned value %d\r\n", err);
        if(mfrERR_NONE != err)
        {
            printf("Calling mfr_init returned err %d\r\n", err);
	         IARM_Bus_Unlock(lock);
	    return IARM_RESULT_IPCCORE_FAIL;
        }
	if(IARM_RESULT_SUCCESS != IARM_Bus_Init(IARM_BUS_MFRLIB_NAME))
	{
	    printf("Error initializing IARM.. \n");
	    IARM_Bus_Unlock(lock);
	    _mfr_shutdown(NULL);
	    return IARM_RESULT_IPCCORE_FAIL;
	}
	IARM_Bus_Connect();

	IARM_Bus_RegisterCall(IARM_BUS_MFRLIB_API_Init,_mfr_init);

    retCode = IARM_RESULT_SUCCESS;
    }

    IARM_Bus_Unlock(lock);
#endif
    return retCode;
}

IARM_Result_t _mfr_init(void *arg)
{
    IARM_Result_t retVal = IARM_RESULT_IPCCORE_FAIL;
    IARM_Bus_Lock(lock);
    errno_t safec_rc = -1;

    if ( !m_initialized)
    {

        IARM_Bus_RegisterCall(IARM_BUS_MFRLIB_API_GetSerializedData,_mfrGetSerializedData);
        IARM_Bus_RegisterCall(IARM_BUS_MFRLIB_API_SetSerializedData,_mfrSetSerializedData);
        IARM_Bus_RegisterCall(IARM_BUS_MFRLIB_API_Shutdown,_mfr_shutdown);
        IARM_Bus_RegisterCall(IARM_BUS_MFRLIB_API_WriteImage,_mfrWriteImage);
        IARM_Bus_RegisterCall(IARM_BUS_MFRLIB_API_DeletePDRI,_mfrDeletePDRI);
        IARM_Bus_RegisterCall(IARM_BUS_MFRLIB_API_ScrubAllBanks,_mfrScrubAllBanks);
        IARM_Bus_RegisterCall(IARM_BUS_MFRLIB_API_SetImageWriteProgress, _mfrSetImageWriteProgress);
        IARM_Bus_RegisterCall(IARM_BUS_MFRLIB_API_GetImageWriteProgress, _mfrGetImageWriteProgress);
        IARM_Bus_RegisterCall(IARM_BUS_MFRLIB_API_DeletePDRI, _mfrDeletePDRI);
        IARM_Bus_RegisterCall(IARM_BUS_MFRLIB_API_Reboot, _mfrReboot);
        IARM_Bus_RegisterCall(IARM_BUS_MFRLIB_API_SetCableCardType, _mfrSetCableCardType);
        IARM_Bus_RegisterCall(IARM_BUS_MFRLIB_API_SetHostFirmwareInfo, _mfrSetHostFirmwareInfo);
        IARM_Bus_RegisterCall(IARM_BUS_MFRLIB_API_GetBootImageName, _mfrGetBootImageName);
        IARM_Bus_RegisterCall(IARM_BUS_MFRLIB_API_GetPathConfiguration, _mfrGetPathConfiguration);
        IARM_Bus_RegisterCall(IARM_BUS_MFRLIB_API_GetDFAST2Data, _mfrGetDFAST2Data);
        IARM_Bus_RegisterEvent(IARM_BUS_MFRMGR_EVENT_STATUS_UPDATE);

	{
	    mfrSerializedData_t data;
	    IARM_Bus_MFRLib_SerializedData_Param_t param;


        if(IARM_RESULT_SUCCESS == retVal )
	    {

        mfrCrypto_init_t func  = (mfrCrypto_init_t) find_func(RDK_MFRCRYPTOLIB_NAME, "mfrCrypto_init");

        if(func)
		{
		   printf("mfrCrypto_init returned ..... - %d\n",retVal);

                    mfrError_t err;
		    retVal = IARM_RESULT_IPCCORE_FAIL;
		    data.buf = (char *)malloc(param.bufLen);
		    safec_rc = memcpy_s(data.buf, param.bufLen, param.buffer, param.bufLen);
                    if(safec_rc != EOK)
                        {
                            ERR_CHK(safec_rc);
                            if (data.buf)
                                free(data.buf);
                            IARM_Bus_Unlock(lock);
                            return IARM_RESULT_INVALID_PARAM;
                        }
		    data.freeBuf = free;
		    err = func(&data);
		    if(err != mfrERR_NONE)
		    {
                printf("mfrCrypto_init returned failure - %d\n",err);
		    }
		    else
		    {
                retVal = IARM_RESULT_SUCCESS;
		    }
		    free(data.buf);
		}
		else
		    printf("Some issue in finding the function mfrCrypto_init..\n");

	    }
        printf("_mfr_init sucess ..... - %d\n",retVal);

	}

        m_initialized = 1;
    }

    IARM_Bus_Unlock(lock);
    return IARM_RESULT_SUCCESS;
}
IARM_Result_t _mfrGetSerializedData(void *arg)
{
    IARM_Result_t retCode = IARM_RESULT_INVALID_STATE;
    errno_t safec_rc = -1;
    int ind = -1;
#ifndef RDK_MFRLIB_NAME
    printf("Please define RDK_MFRLIB_NAME\r\n");
    printf("Exiting %s\n",__func__);
    return retCode;
#else
    static mfrGetSerializedData_t func = 0;
    static mfrCrypto_Encrypt_t encrypt_func = 0;

    IARM_Bus_Lock(lock);

    printf("In %s\n",__func__);

    if (func == 0)
    {
        func = (mfrGetSerializedData_t) find_func(RDK_MFRLIB_NAME, "mfrGetSerializedData");
    }
    if (func)
    {
	mfrSerializedData_t data;
	mfrCrypto_Encrypt_t *func_ptr = NULL;
	mfrError_t err;
	IARM_Bus_MFRLib_SerializedData_Param_t *param = (IARM_Bus_MFRLib_SerializedData_Param_t*) arg;

        safec_rc = strcmp_s(param->crypto, strlen(param->crypto), "", &ind);
        ERR_CHK(safec_rc);
        if((safec_rc == EOK) && ind))
	{
	    if(encrypt_func == 0)
	    {
		    encrypt_func = (mfrCrypto_Encrypt_t) find_func(RDK_MFRCRYPTOLIB_NAME, param->crypto);
    		if(!encrypt_func)
    		{
    		    printf("Exiting %s\n",__func__);
    		    IARM_Bus_Unlock(lock);
    		    return IARM_RESULT_INVALID_STATE;
    		}
	    }
	    func_ptr = encrypt_func;
	}
	err = func(param->type, &data, func_ptr);
        if(mfrERR_NONE == err)
        {

	    int max_len = MAX_SERIALIZED_BUF;
	    max_len = (max_len < data.bufLen)? max_len:data.bufLen;
	    safec_rc = memcpy_s(((char *)param->buffer), sizeof(param->buffer), data.buf, max_len);
            if(safec_rc != EOK)
            {
               ERR_CHK(safec_rc);
               if (data.freebuf)
               {
                   data.freebuf(data.buf);
               }
               IARM_Bus_Unlock(lock);
               return IARM_RESULT_INVALID_PARAM;
            }
	    param->bufLen = max_len;

	    if(data.freeBuf)
	    {
    		data.freeBuf(data.buf);
	    }
	    retCode = IARM_RESULT_SUCCESS;
        }
    }

    IARM_Bus_Unlock(lock);
#endif
    return retCode;

}

IARM_Result_t _mfrSetSerializedData(void *arg)
{
    IARM_Result_t retCode = IARM_RESULT_INVALID_STATE;
#ifndef RDK_MFRLIB_NAME
    printf("Please define RDK_MFRLIB_NAME\r\n");
    printf("Exiting %s\n",__func__);
    return retCode;
#else
    static mfrSetSerializedData_t func = 0;
    static mfrCrypto_Decrypt_t decrypt_func = 0;
    errno_t safec_rc = -1;
    int ind = -1;

    IARM_Bus_Lock(lock);
    printf("In %s\n",__func__);
    if (func == 0)
    {
        func = (mfrSetSerializedData_t) find_func(RDK_MFRLIB_NAME, "mfrSetSerializedData");
    }
    if (func)
    {
	mfrSerializedData_t data;
	mfrCrypto_Decrypt_t *func_ptr = NULL;
	mfrError_t err;
	IARM_Bus_MFRLib_SerializedData_Param_t *param = (IARM_Bus_MFRLib_SerializedData_Param_t*) arg;
        safec_rc = strcmp_s(param->crypto, strlen(param->crypto), "", &ind);
        ERR_CHK(safec_rc);
        if((safec_rc == EOK) && ind)
        {
	    if(decrypt_func == 0)
	    {
		decrypt_func = (mfrCrypto_Decrypt_t) find_func(RDK_MFRCRYPTOLIB_NAME, param->crypto);
		if(!decrypt_func)
		{
		    printf("Exiting %s\n",__func__);
		    IARM_Bus_Unlock(lock);
		    return IARM_RESULT_INVALID_STATE;
		}
	    }
	    func_ptr = decrypt_func;
	}

	data.buf = (char *) malloc (param->bufLen);
	data.bufLen = param->bufLen;
	data.freeBuf = free;
	safec_rc = memcpy_s(data.buf, param->bufLen, ((char *)param->buffer), param->bufLen);
        if(safec_rc != EOK)
            {
               ERR_CHK(safec_rc);
               if (data.buf)
                   free(data.buf);
               IARM_Bus_Unlock(lock);
               return IARM_RESULT_INVALID_PARAM;
            }
	err = func(param->type, &data, func_ptr);
        if(mfrERR_NONE == err)
        {
	    retCode = IARM_RESULT_SUCCESS;
        }
    }
    IARM_Bus_Unlock(lock);
#endif
    return retCode;
}


static void writeImageCb(mfrUpgradeStatus_t status, void *cbData)
{
    IARM_BUS_MfrMgr_StatusUpdate_EventData_t param;

    param.status = status;

    LOG("In writeImage callback: error = %d, percentage = %02.02\n",status.error,status.percentage/100,status.percentage%100);

    IARM_Bus_BroadcastEvent(IARM_BUS_MFRLIB_NAME,
				IARM_BUS_MFRMGR_EVENT_STATUS_UPDATE,
				(void *) &param, sizeof(param));

    lastStatus = status;
}

IARM_Result_t _mfrWriteImage(void *arg)
{
#ifndef RDK_MFRLIB_NAME
    printf("Please define RDK_MFRLIB_NAME\r\n");
    printf("Exiting writeImage_\n");
    return IARM_RESULT_INVALID_STATE;
#else
    static mfrWriteImage_t func = 0;
    IARM_Bus_Lock(lock);
    printf("In writeImage_\n");
    if (func == 0)
    {
        func = (mfrWriteImage_t) find_func(RDK_MFRLIB_NAME, "mfrWriteImage");
        if (func)
        {
			printf("mfrWriteImage is defined and loaded\r\n");
        }
        else
        {
              printf("mfrWriteImage is not defined\r\n");
			  IARM_Bus_Unlock(lock);
	          return IARM_RESULT_INVALID_STATE;
        }
    }
    IARM_Result_t retCode = IARM_RESULT_INVALID_STATE;
    mfrError_t err = mfrERR_NONE;
    if (func)
    {

        IARM_Bus_MFRLib_WriteImage_Param_t *pParam = (IARM_Bus_MFRLib_WriteImage_Param_t *) arg;
        mfrUpgradeStatusNotify_t notifyStruct;
        notifyStruct.interval = pParam->interval;
        notifyStruct.cb = writeImageCb;
        lastStatus.progress = mfrUPGRADE_PROGRESS_NOT_STARTED;
        err = func(pParam->name, pParam->path, pParam->type, notifyStruct);
        if(mfrERR_NONE == err)
        {
            printf("Calling mfrWriteImage returned err %d\r\n", err);
            retCode = IARM_RESULT_SUCCESS;
            /* Poll for upgrade start */
            while( lastStatus.progress == mfrUPGRADE_PROGRESS_NOT_STARTED )
            {
               printf("Waiting for upgrade to start\n");
               sleep(1);
            }
            /* Poll for completion */
            while( lastStatus.progress == mfrUPGRADE_PROGRESS_STARTED )
            {
               printf("Waiting for upgrade to complete\n");
               sleep(1);
            }
            printf("Update process complete..\n");
            if(lastStatus.progress != mfrUPGRADE_PROGRESS_COMPLETED)
            {
               retCode = IARM_RESULT_IPCCORE_FAIL;
            }
        }
    }
	IARM_Bus_Unlock(lock);
    printf("Exiting writeImage_\n");
    return retCode;
#endif
}

IARM_Result_t _mfrGetImageWriteProgress(void *arg)
{
#ifndef RDK_MFRLIB_NAME
    printf("Please define RDK_MFRLIB_NAME\r\n");
    printf("Exiting %s\n",__func__);
    return IARM_RESULT_INVALID_STATE;
#else
    static mfrGetImageWriteProgress_t func = 0;
    IARM_Bus_Lock(lock);
    printf("In %s\n",__func__);
    if (func == 0)
    {
	    func = (mfrGetImageWriteProgress_t) find_func(RDK_MFRLIB_NAME, "mfrGetImageWriteProgress");
	    if (func)
	    {
	        printf("mfrGetImageWriteProgress is defined and loaded\r\n");
        }
	    else
	    {
	        printf("mfrGetImageWriteProgress is not defined\r\n");
			IARM_Bus_Unlock(lock);
	        return IARM_RESULT_INVALID_STATE;
	    }
    }
    IARM_Result_t retCode = IARM_RESULT_INVALID_STATE;
    mfrError_t err = mfrERR_NONE;
    if (func)
    {

        IARM_Bus_MFRLib_GetImageWriteProgress_Param_t *pParam = (IARM_Bus_MFRLib_GetImageWriteProgress_Param_t *) arg;
        err = func(pParam->imageName,pParam->imageType,&(pParam->progress));
        if(mfrERR_NONE == err)
        {
            printf("Calling mfrGetImageWriteProgress returned err %d\r\n", err);
            retCode = IARM_RESULT_SUCCESS;
        }
    }
	IARM_Bus_Unlock(lock);
    printf("Exiting %s\n",__func__);
    return retCode;
#endif
}

IARM_Result_t _mfrSetImageWriteProgress(void *arg)
{
#ifndef RDK_MFRLIB_NAME
    printf("Please define RDK_MFRLIB_NAME\r\n");
    printf("Exiting %s\n",__func__);
    return IARM_RESULT_INVALID_STATE;
#else
    static mfrSetImageWriteProgress_t func = 0;
    IARM_Bus_Lock(lock);
    printf("In %s\n",__func__);
    if (func == 0)
    {
        func = (mfrSetImageWriteProgress_t) find_func(RDK_MFRLIB_NAME, "mfrSetImageWriteProgress");
	    if (func)
	    {
	        printf("mfrSetImageWriteProgress is defined and loaded\r\n");
	    }
	    else
	    {
	        printf("mfrSetImageWriteProgress is not defined\r\n");
			IARM_Bus_Unlock(lock);
	        return IARM_RESULT_INVALID_STATE;
	    }
    }
    IARM_Result_t retCode = IARM_RESULT_INVALID_STATE;
    mfrError_t err = mfrERR_NONE;
    if (func)
    {

        IARM_Bus_MFRLib_SetImageWriteProgress_Param_t *pParam = (IARM_Bus_MFRLib_SetImageWriteProgress_Param_t *) arg;
        err = func(pParam->imageName, pParam->imageType, pParam->progress);
        if(mfrERR_NONE == err)
        {
            printf("Calling mfrSetImageWriteProgress returned err %d\r\n", err);
            retCode = IARM_RESULT_SUCCESS;
        }
    }
	IARM_Bus_Unlock(lock);
    printf("Exiting %s\n",__func__);
    return retCode;
#endif
}
IARM_Result_t _mfrDeletePDRI(void *arg)
{
    mfrError_t Err = mfrERR_NONE;
#ifndef RDK_MFRLIB_NAME
    printf("Please define RDK_MFRLIB_NAME\r\n");
    return IARM_RESULT_INVALID_STATE;
#else
    static mfrDeletePDRI_t func = 0;

    IARM_Bus_Lock(lock);
    if (func == 0)
    {
	    func = (mfrDeletePDRI_t) find_func(RDK_MFRLIB_NAME, "mfrDeletePDRI");
		if (func)
		{
    		printf("mfrDeletePDRI(void) is defined and loaded\r\n");
        }
		else
		{
	   		printf("mfrDeletePDRI(void) is not defined\r\n");
	    	IARM_Bus_Unlock(lock);
	   		return IARM_RESULT_INVALID_STATE;
		}
	}
    IARM_Result_t retCode = IARM_RESULT_INVALID_STATE;
    mfrError_t err = mfrERR_NONE;
    if (func)
    {
        err = func();
        if(mfrERR_NONE == err)
        {
            printf("Calling mfrDeletePDRI returned err %d\r\n", err);
            IARM_Bus_Unlock(lock);
            retCode = IARM_RESULT_SUCCESS;
        }
    }
    IARM_Bus_Unlock(lock);
    return retCode;
#endif
}

IARM_Result_t _mfrScrubAllBanks(void *arg)
{
#ifndef RDK_MFRLIB_NAME
    printf("Please define RDK_MFRLIB_NAME\r\n");
    return IARM_RESULT_INVALID_STATE;
#else
    static mfrScrubAllBanks_t func = 0;

    IARM_Bus_Lock(lock);
    if (func == 0)
    {
	    func = (mfrScrubAllBanks_t) find_func(RDK_MFRLIB_NAME, "mfrScrubAllBanks");
	    if (func)
	    {
            printf("mfrScrubAllBanks(void) is defined and loaded\r\n");
	    }
	    else
	    {
	        printf("mfrScrubAllBanks(void) is not defined\r\n");
	        IARM_Bus_Unlock(lock);
	        return IARM_RESULT_INVALID_STATE;
	    }
    }
    IARM_Result_t retCode = IARM_RESULT_INVALID_STATE;
    mfrError_t err = mfrERR_NONE;
    if (func)
    {
        err = func();
        if(mfrERR_NONE == err)
        {
            printf("Calling mfrScrubAllBanks returned err %d\r\n", err);
            IARM_Bus_Unlock(lock);
            retCode = IARM_RESULT_SUCCESS;
        }
    }
    IARM_Bus_Unlock(lock);
    return retCode;

#endif
}

IARM_Result_t _mfr_shutdown(void *arg)
{
    IARM_Bus_Lock(lock);
    IARM_Bus_Disconnect();
    IARM_Bus_Term();
    mfrError_t Err = mfrERR_NONE;
#ifndef RDK_MFRLIB_NAME
    printf("Please define RDK_MFRLIB_NAME\r\n");
    return IARM_RESULT_INVALID_STATE;
#else
    static mfr_shutdown_t func = 0;
    IARM_Result_t retCode = IARM_RESULT_INVALID_STATE;
    IARM_Bus_Lock(lock);
    if (func == 0)
    {
	    func = (mfr_shutdown_t) find_func(RDK_MFRLIB_NAME, "mfr_shutdown");
	    if (func)
	    {
	        printf("mfr_shutdown(void) is defined and loaded\r\n");
	    }
	    else
	    {
	        printf("mfr_shutdown(void) is not defined\r\n");
	        IARM_Bus_Unlock(lock);
	        return IARM_RESULT_INVALID_STATE;
	    }
    }

    if (func)
    {
	mfrError_t err = func();
        if(mfrERR_NONE == err)
        {
            printf("Calling mfr_shutdown returned err %d\r\n", err);
            IARM_Bus_Unlock(lock);
            retCode = IARM_RESULT_SUCCESS;
        }
    }
    IARM_Bus_Unlock(lock);
    return retCode;
#endif

}


IARM_Result_t _mfrReboot(void *arg)
{
#ifndef RDK_MFRLIB_NAME
    printf("Please define RDK_MFRLIB_NAME\r\n");
    return IARM_RESULT_INVALID_STATE;
#else
    static mfrReboot_t func = 0;

    IARM_Bus_Lock(lock);
    if (func == 0)
    {
	func = (mfrReboot_t) find_func(RDK_MFRLIB_NAME, "mfrReboot");
	if (func)
	{
	    printf("mfrReboot (const char*) is defined and loaded\r\n");
	}
	else
	{
	    printf("mfrReboot (const char*) is not defined\r\n");
	    IARM_Bus_Unlock(lock);
	    return IARM_RESULT_INVALID_STATE;
	}
    }
    IARM_Result_t retCode = IARM_RESULT_INVALID_STATE;
    mfrError_t err = mfrERR_NONE;
    if (func)
    {
	IARM_Bus_MFRLib_Reboot_Param_t *pParam = (IARM_Bus_MFRLib_Reboot_Param_t *)arg;

        err = func(pParam->imageName);
        if(mfrERR_NONE == err)
        {
            printf("Calling mfrReboot returned err %d\r\n", err);
            IARM_Bus_Unlock(lock);
            retCode = IARM_RESULT_SUCCESS;
        }
    }
    IARM_Bus_Unlock(lock);
    return retCode;

#endif
}

IARM_Result_t _mfrSetCableCardType(void *arg)
{
#ifndef RDK_MFRLIB_NAME
    printf("Please define RDK_MFRLIB_NAME\r\n");
    return IARM_RESULT_INVALID_STATE;
#else
    static mfrSetCableCardType_t func = 0;

    IARM_Bus_Lock(lock);
    if (func == 0)
    {
	func = (mfrSetCableCardType_t) find_func(RDK_MFRLIB_NAME, "mfrSetCableCardType");
	if (func)
	{
	    printf("mfrSetCableCardType is defined and loaded\r\n");
	}
	else
	{
	    printf("mfrSetCableCardType is not defined\r\n");
	    IARM_Bus_Unlock(lock);
	    return IARM_RESULT_INVALID_STATE;
	}
    }
    IARM_Result_t retCode = IARM_RESULT_INVALID_STATE;
    mfrError_t err = mfrERR_NONE;
    if (func)
    {
	IARM_Bus_MFRLib_SetCableCardType_Param_t *pParam = (IARM_Bus_MFRLib_SetCableCardType_Param_t *)arg;

        err = func(pParam->type);
        if(mfrERR_NONE == err)
        {
            printf("Calling mfrSetCableCardType returned err %d\r\n", err);
            retCode = IARM_RESULT_SUCCESS;
        }
    }
    IARM_Bus_Unlock(lock);
    return retCode;

#endif
}

IARM_Result_t _mfrSetHostFirmwareInfo(void *arg)
{
#ifndef RDK_MFRLIB_NAME
    printf("Please define RDK_MFRLIB_NAME\r\n");
    return IARM_RESULT_INVALID_STATE;
#else
    static mfrSetHostFirmwareInfo_t func = 0;
    errno_t safec_rc = -1;

    IARM_Bus_Lock(lock);
    if (func == 0)
    {
	func = (mfrSetHostFirmwareInfo_t) find_func(RDK_MFRLIB_NAME, "mfrSetHostFirmwareInfo");
	if (func)
	{
	    printf("mfrSetHostFirmwareInfo is defined and loaded\r\n");
	}
	else
	{
	    printf("mfrSetHostFirmwareInfo is not defined\r\n");
	    IARM_Bus_Unlock(lock);
	    return IARM_RESULT_INVALID_STATE;
	}
    }
    IARM_Result_t retCode = IARM_RESULT_INVALID_STATE;
    mfrError_t err = mfrERR_NONE;
    if (func)
    {
	IARM_Bus_MFRLib_SetHostFirmwareInfo_Param_t *pParam = (IARM_Bus_MFRLib_SetHostFirmwareInfo_Param_t *)arg;
	mfrHostFirmwareInfo_t host_firmware_info;

	host_firmware_info.firmwareDay = pParam->day;
	host_firmware_info.firmwareMonth = pParam->month;
	host_firmware_info.firmwareYear = pParam->year;
	safec_rc = memcpy_s(&(host_firmware_info.firmwareVersion), sizeof(host_firmware_info.firmwareVersion), pParam->version, sizeof(host_firmware_info.firmwareVersion));
        if(safec_rc != EOK)
            {
               ERR_CHK(safec_rc);
               IARM_Bus_Unlock(lock);
               return IARM_RESULT_INVALID_PARAM;
            }
        err = func(&host_firmware_info);
        if(mfrERR_NONE == err)
        {
            printf("Calling mfrSetHostFirmwareInfo returned err %d\r\n", err);
            retCode = IARM_RESULT_SUCCESS;
        }
    }
    IARM_Bus_Unlock(lock);
    return retCode;

#endif
}

IARM_Result_t _mfrGetBootImageName(void *arg)
{
#ifndef RDK_MFRLIB_NAME
    printf("Please define RDK_MFRLIB_NAME\r\n");
    return IARM_RESULT_INVALID_STATE;
#else
    static mfrGetBootImageName_t func = 0;

    IARM_Bus_Lock(lock);
    if (func == 0)
    {
	func = (mfrGetBootImageName_t) find_func(RDK_MFRLIB_NAME, "mfrGetBootImageName");
	if (func)
	{
	    printf("mfrGetBootImageName is defined and loaded\r\n");
	}
	else
	{
	    printf("mfrGetBootImageName is not defined\r\n");
	    IARM_Bus_Unlock(lock);
	    return IARM_RESULT_INVALID_STATE;
	}
    }
    IARM_Result_t retCode = IARM_RESULT_INVALID_STATE;
    mfrError_t err = mfrERR_NONE;
    if (func)
    {
	IARM_Bus_MFRLib_GetBootImageName_Param_t *pParam = (IARM_Bus_MFRLib_GetBootImageName_Param_t *)arg;

        err = func(pParam->bootInstance, pParam->imageName, &(pParam->len), pParam->type);
        if(mfrERR_NONE == err)
        {
            printf("Calling mfrGetBootImageName returned err %d\r\n", err);
            retCode = IARM_RESULT_SUCCESS;
        }
    }
    IARM_Bus_Unlock(lock);
    return retCode;

#endif
}

IARM_Result_t _mfrGetPathConfiguration(void *arg)
{
#ifndef RDK_MFRLIB_NAME
    printf("Please define RDK_MFRLIB_NAME\r\n");
    return IARM_RESULT_INVALID_STATE;
#else
    static mfrGetPathConfiguration_t func = 0;
    printf("In %s \n",__func__);
    IARM_Bus_Lock(lock);
    if (func == 0)
    {
	func = (mfrGetPathConfiguration_t) find_func(RDK_MFRLIB_NAME, "mfrGetPathConfiguration");
	if (func)
	{
	    printf("mfrGetPathConfiguration is defined and loaded\r\n");
	}
	else
	{
	    printf("mfrGetPathConfiguration is not defined\r\n");
	    IARM_Bus_Unlock(lock);
	    return IARM_RESULT_INVALID_STATE;
	}
    }
    IARM_Result_t retCode = IARM_RESULT_INVALID_STATE;
    mfrError_t err = mfrERR_NONE;
    if (func)
    {
	IARM_Bus_MFRLib_GetPathConfiguration_Param_t *pParam = (IARM_Bus_MFRLib_GetPathConfiguration_Param_t *)arg;

        err = func(pParam->type, pParam->path, &(pParam->len));
        if(mfrERR_NONE == err)
        {
            printf("Calling mfrGetPathConfiguration returned err %d\r\n", err);
            retCode = IARM_RESULT_SUCCESS;
        }
    }
    IARM_Bus_Unlock(lock);
    return retCode;

#endif
}

IARM_Result_t _mfrGetDFAST2Data(void *arg)
{
#ifndef RDK_MFRLIB_NAME
    printf("Please define RDK_MFRLIB_NAME\r\n");
    return IARM_RESULT_INVALID_STATE;
#else
    static mfrGetDFAST2Data_t func = 0;
    errno_t safec_rc = -1;

    IARM_Bus_Lock(lock);
    if (func == 0)
    {
	func = (mfrGetDFAST2Data_t) find_func(RDK_MFRLIB_NAME, "mfrGetDFAST2Data");
	if (func)
	{
	    printf("mfrGetDFAST2Data is defined and loaded\r\n");
	}
	else
	{
	    printf("mfrGetDFAST2Data is not defined\r\n");
	    IARM_Bus_Unlock(lock);
	    return IARM_RESULT_INVALID_STATE;
	}
    }
    IARM_Result_t retCode = IARM_RESULT_INVALID_STATE;
    mfrError_t err = mfrERR_NONE;
    if (func)
    {
	IARM_Bus_MFRLib_GetDFAST2Data_Param_t *pParam = (IARM_Bus_MFRLib_GetDFAST2Data_Param_t *)arg;
	mfrDFAST2Params_t dfast_params;

	safec_rc = memcpy_s(&dfast_params,sizeof(mfrDFAST2Params_t), pParam,sizeof(mfrDFAST2Params_t));
        if(safec_rc != EOK)
            {
               ERR_CHK(safec_rc);
               IARM_Bus_Unlock(lock);
               return IARM_RESULT_INVALID_PARAM;
            }
        err = func(&dfast_params);
        if(mfrERR_NONE == err)
        {
            printf("Calling mfrGetDFAST2Data returned err %d\r\n", err);
            retCode = IARM_RESULT_SUCCESS;
        }
    }
    IARM_Bus_Unlock(lock);
    return retCode;

#endif
}



/** @} */
/** @} */
