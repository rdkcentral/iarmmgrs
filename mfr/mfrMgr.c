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


#include <stdio.h>
#include <memory.h>
#include <string.h>
#include <dlfcn.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>

#include "mfrMgrInternal.h"
#include "mfrMgr.h"
#include "libIARMCore.h"
#include "safec_lib.h"
#include "rdkProfile.h"

/**
* IARM call to set the FSR flag
*/
static IARM_Result_t setFSRflag_(void *arg);
/**
* IARM call to get the FSR flag
*/
static IARM_Result_t getFSRflag_(void *arg);

/**
* IARM call to set the FSR flag
*/
static IARM_Result_t setFSRflag_(void *arg);
/**
* IARM call to get the FSR flag
*/
static IARM_Result_t getFSRflag_(void *arg);

#ifdef MFR_TEMP_CLOCK_READ
/**
*  IARM call to return current thermal state and temperature
*/
static IARM_Result_t getTemperature_(void *arg);
/**
* IARM call to set temperature thresholds
*/
static IARM_Result_t setTemperatureThresholds_(void *arg);
/**
* IARM call to get temperature thresholds
*/
static IARM_Result_t getTemperatureThresholds_(void *arg);
/**
* IARM call to determine cpu clock speed
*/
static IARM_Result_t searchCPUClockSpeeds_(void *arg);
/**
* IARM call to set the cpu clock speed
*/
static IARM_Result_t setCPUClockSpeed_(void *arg);
/**
* IARM call to get the cpu clock speed
*/
static IARM_Result_t getCPUClockSpeed_(void *arg);
#endif

static int is_connected = 0;

static char writeImageCbModule[MAX_BUF] = "";
static mfrUpgradeStatusNotify_t notifyStruct;

static mfrUpgradeStatus_t lastStatus;

static profile_t profileType = PROFILE_INVALID;

/* Mutex for protecting dynamic library loading operations */
static pthread_mutex_t mfr_dlopen_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Keep track of loaded library handles for proper cleanup */
static void *mfr_library_handle = NULL;

/* Control flag for main loop termination */
static volatile int mfr_loop_running = 1;

/* Rate limiting for DoS prevention */
static time_t last_wifi_request_time = 0;
static int wifi_request_count = 0;
static pthread_mutex_t rate_limit_mutex = PTHREAD_MUTEX_INITIALIZER;

#define MAX_WIFI_REQUESTS_PER_SECOND 5
#define RATE_LIMIT_WINDOW 1

/**
 * @brief Validates that a library path is secure
 * @param libpath The library path to validate
 * @return 1 if valid, 0 if invalid
 */
static int validate_library_path(const char *libpath)
{
    if (!libpath) return 0;
    
    /* Ensure path doesn't contain dangerous characters */
    if (strstr(libpath, "..") || strstr(libpath, "//") || 
        strchr(libpath, ';') || strchr(libpath, '|') || 
        strchr(libpath, '&') || strchr(libpath, '$')) {
        LOG("Security: Invalid characters in library path: %s\n", libpath);
        return 0;
    }
    
    /* Ensure path is reasonable length to prevent buffer attacks */
    if (strlen(libpath) > 256) {
        LOG("Security: Library path too long: %zu chars\n", strlen(libpath));
        return 0;
    }
    
    return 1;
}

/**
 * @brief Secure wrapper for dlopen with path validation
 * @param filename Library path to open
 * @param flags dlopen flags  
 * @return Handle to library or NULL if validation fails
 */
static void* secure_dlopen(const char *filename, int flags)
{
    if (!validate_library_path(filename)) {
        LOG("Security: dlopen blocked for unsafe path: %s\n", filename);
        return NULL;
    }
    return dlopen(filename, flags);
}

/**
 * @brief Validates input string for buffer overflow prevention
 * @param input String to validate (may not be null-terminated)
 * @param max_len Maximum allowed length
 * @return 1 if valid, 0 if invalid
 */
static int validate_string_input(const char *input, size_t max_len)
{
    if (!input) return 0;
    
    /* Ensure string is null-terminated within bounds */
    const char *ptr = input;
    for (size_t i = 0; i < max_len; i++) {
        if (*ptr == '\0') {
            return 1;  /* Found null terminator within bounds */
        }
        ptr++;
    }
    
    LOG("Security: String not properly terminated within %zu chars\n", max_len);
    return 0;
}

static IARM_Result_t getSerializedData_(void *arg)
{

    IARM_Result_t retCode = IARM_RESULT_IPCCORE_FAIL;
    IARM_Bus_MFRLib_GetSerializedData_Param_t *param = (IARM_Bus_MFRLib_GetSerializedData_Param_t *)arg;
    mfrError_t err = mfrERR_NONE;
    mfrSerializedData_t data = {0};  /* Initialize struct to zero */
    errno_t safec_rc = -1;
    int i = 0;  /* Initialize to prevent UNINIT usage */
    
    /* Validate input parameter to prevent null pointer dereference */
    if (param == NULL) {
        LOG("Security: NULL parameter passed to getSerializedData_\n");
        return IARM_RESULT_INVALID_PARAM;
    }
    if (PROFILE_INVALID == profileType){
        profileType = searchRdkProfile();
    }
    if((param->type == mfrSERIALIZED_TYPE_PROVISIONED_MODELNAME) &&
          (PROFILE_STB == profileType)){
        LOG(" Querying for sky model name ");
        err = mfrGetSerializedData((mfrSerializedType_t)(mfrSERIALIZED_TYPE_SKYMODELNAME), &(data));
    } else {
         err = mfrGetSerializedData((mfrSerializedType_t)(param->type), &(data));
    }
    if(mfrERR_NONE == err)
    {
        /* Validate data buffer to prevent null pointer dereference */
        if (data.buf == NULL) {
            LOG("Security: NULL buffer returned from mfrGetSerializedData\n");
            return IARM_RESULT_INVALID_PARAM;
        }
        if (data.bufLen == 0 || data.bufLen > sizeof(param->buffer)) {
            LOG("Security: Invalid buffer length %lu from mfrGetSerializedData\n", data.bufLen);
            if(data.freeBuf)
            {
                data.freeBuf(data.buf);
            }
            return IARM_RESULT_INVALID_PARAM;
        }
        
	safec_rc = memcpy_s(param->buffer, sizeof(param->buffer), data.buf, data.bufLen);
    	if(safec_rc != EOK)
        {
                ERR_CHK(safec_rc);
                if(data.freeBuf)
                {
                    data.freeBuf(data.buf);
                }
                return IARM_RESULT_INVALID_PARAM;
         }
         param->bufLen = data.bufLen;
      
	if(data.freeBuf)
        {
             data.freeBuf(data.buf);
        }
	retCode=IARM_RESULT_SUCCESS;
    }
    return retCode;
}

static IARM_Result_t setSerializedData_(void *arg)
{
   typedef mfrError_t (*mfrSerializedData_t)(void);
   LOG("In setSerializedData_\n");
#ifndef RDK_MFRLIB_NAME
    LOG("Please define RDK_MFRLIB_NAME and Exiting setSerializedData_\r\n");
   
    return IARM_RESULT_INVALID_STATE;
#else
    mfrError_t err = mfrERR_NONE;
    /* Remove confusing variable declaration - 'data' is unused */
 
    static mfrSerializedData_t func = 0;
  
    /* Thread-safe check for function pointer initialization */
    pthread_mutex_lock(&mfr_dlopen_mutex);
    if (func == 0) {        
        void *dllib = secure_secure_dlopen(RDK_MFRLIB_NAME, RTLD_LAZY);
        if (dllib) {
            func = (mfrSerializedData_t) dlsym(dllib, "mfrSetSerializedData");
            if (func) {
                LOG("mfrSetSerializedData(void) is defined and loaded\r\n");
                /* Keep handle open while function is in use */
                if (mfr_library_handle == NULL) {
                    mfr_library_handle = dllib;
                } else {
                    /* Handle already tracked, can close this duplicate */
                    dlclose(dllib);
                }
            }
            else {
		    LOG("mfrSetSerializedData(void) is not defined\r\n");
		    dlclose(dllib);
		    pthread_mutex_unlock(&mfr_dlopen_mutex);
		    return IARM_RESULT_INVALID_STATE;
	    }
	}
	else {
		LOG("Opening RDK_MFRLIB_NAME [%s] failed\r\n", RDK_MFRLIB_NAME);
		pthread_mutex_unlock(&mfr_dlopen_mutex);
		return IARM_RESULT_INVALID_STATE;
	}
    }
    pthread_mutex_unlock(&mfr_dlopen_mutex);

    IARM_Result_t retCode = IARM_RESULT_INVALID_STATE;

    if (func) {

	    err = func();

	    if(mfrERR_NONE == err)
	    {
		    LOG("Calling mfrSetSerializedData returned err %d\r\n", err);
		    retCode = IARM_RESULT_SUCCESS;
	    }
    }
    return retCode;
#endif

}

static IARM_Result_t deletePDRI_(void *arg)
{
    typedef mfrError_t (*mfrDeletePDRI_t)(void);

#ifndef RDK_MFRLIB_NAME
    LOG("Please define RDK_MFRLIB_NAME\r\n");
    return IARM_RESULT_INVALID_STATE;
#else
    static mfrDeletePDRI_t func = 0;
    if (func == 0) {
        void *dllib = secure_dlopen(RDK_MFRLIB_NAME, RTLD_LAZY);
        if (dllib) {
            func = (mfrDeletePDRI_t) dlsym(dllib, "mfrDeletePDRI");
            if (func) {
                LOG("mfrDeletePDRI(void) is defined and loaded\r\n");
            }
            else {
                LOG("mfrDeletePDRI(void) is not defined\r\n");
				 dlclose(dllib);
                return IARM_RESULT_INVALID_STATE;
            }
            dlclose(dllib);
        }
        else {
            LOG("Opening RDK_MFRLIB_NAME [%s] failed\r\n", RDK_MFRLIB_NAME);
            return IARM_RESULT_INVALID_STATE;
        }
    }

    IARM_Result_t retCode = IARM_RESULT_INVALID_STATE;
    mfrError_t err = mfrERR_NONE;

    if (func) {
    
        err = func();

        if(mfrERR_NONE == err)
        {
            LOG("Calling mfrDeletePDRI returned err %d\r\n", err);
            retCode = IARM_RESULT_SUCCESS;
        }
    }
    return retCode;
#endif

}

static IARM_Result_t scrubAllBanks_(void *arg)
{
    typedef mfrError_t (*mfrScrubAllBanks_t)(void);

#ifndef RDK_MFRLIB_NAME
    LOG("Please define RDK_MFRLIB_NAME\r\n");
    return IARM_RESULT_INVALID_STATE;
#else
    static mfrScrubAllBanks_t func = 0;
    if (func == 0) {
        void *dllib = secure_dlopen(RDK_MFRLIB_NAME, RTLD_LAZY);
        if (dllib) {
            func = (mfrScrubAllBanks_t) dlsym(dllib, "mfrScrubAllBanks");
            if (func) {
                LOG("mfrScrubAllBanks(void) is defined and loaded\r\n");
            }
            else {
                LOG("mfrScrubAllBanks(void) is not defined\r\n");
                dlclose(dllib);
				return IARM_RESULT_INVALID_STATE;
            }
            dlclose(dllib);
        }
        else {
            LOG("Opening RDK_MFRLIB_NAME [%s] failed\r\n", RDK_MFRLIB_NAME);
            return IARM_RESULT_INVALID_STATE;
        }
    }

    IARM_Result_t retCode = IARM_RESULT_INVALID_STATE;
    mfrError_t err = mfrERR_NONE;

    if (func) {
    
        err = func();

        if(mfrERR_NONE == err)
        {
            LOG("Calling mfrScrubAllBanks returned err %d\r\n", err);
            retCode = IARM_RESULT_SUCCESS;
        }
    }
    return retCode;
#endif

}
#ifdef ENABLE_MFR_WIFI
static IARM_Result_t mfrWifiEraseAllData_(void *arg)
{
    typedef mfrError_t (*mfrWifiEraseAllData_t)(void);

#ifndef RDK_MFRLIB_NAME
    LOG("Please define RDK_MFRLIB_NAME\r\n");
    return IARM_RESULT_INVALID_STATE;
#else
    static mfrWifiEraseAllData_t func = 0;
    if (func == 0) {
        void *dllib = secure_dlopen(RDK_MFRLIB_NAME, RTLD_LAZY);
        if (dllib) {
            func = (mfrWifiEraseAllData_t) dlsym(dllib, "WIFI_EraseAllData");
            if (func) {
                LOG("mfrWifiEraseAllData(void) is defined and loaded\r\n");
            }
            else {
                LOG("mfrWifiEraseAllData(void) is not defined\r\n");
                dlclose(dllib);
                                return IARM_RESULT_INVALID_STATE;
            }
            dlclose(dllib);
        }
        else {
            LOG("Opening RDK_MFRLIB_NAME [%s] failed\r\n", RDK_MFRLIB_NAME);
            return IARM_RESULT_INVALID_STATE;
        }
    }

    IARM_Result_t retCode = IARM_RESULT_INVALID_STATE;
    WIFI_API_RESULT err = WIFI_API_RESULT_SUCCESS;    

    if (func) {

        err = func();

        if(WIFI_API_RESULT_SUCCESS == err)
        {
            LOG("Calling mfrWifiEraseAllData returned err %d\r\n", err);
            retCode = IARM_RESULT_SUCCESS;
        }
    }
    return retCode;
#endif

}
static IARM_Result_t mfrWifiCredentials_(void *arg)
{

    IARM_Result_t retCode = IARM_RESULT_IPCCORE_FAIL;
    IARM_BUS_MFRLIB_API_WIFI_Credentials_Param_t *param = (IARM_BUS_MFRLIB_API_WIFI_Credentials_Param_t *)arg;
    WIFI_API_RESULT err = WIFI_API_RESULT_SUCCESS;
    WIFI_DATA data;
    errno_t safec_rc = -1;

    /* Validate input parameter to prevent null pointer dereference */
    if (param == NULL) {
        LOG("Security: NULL parameter passed to mfrWifiCredentials_\n");
        return IARM_RESULT_INVALID_PARAM;
    }

    /* Rate limiting for DoS prevention */
    pthread_mutex_lock(&rate_limit_mutex);
    time_t current_time = time(NULL);
    if (current_time - last_wifi_request_time >= RATE_LIMIT_WINDOW) {
        /* Reset counter for new time window */
        wifi_request_count = 0;
        last_wifi_request_time = current_time;
    }
    wifi_request_count++;
    if (wifi_request_count > MAX_WIFI_REQUESTS_PER_SECOND) {
        pthread_mutex_unlock(&rate_limit_mutex);
        LOG("Security: Rate limit exceeded for wifi credentials requests\n");
        return IARM_RESULT_INVALID_STATE;
    }
    pthread_mutex_unlock(&rate_limit_mutex);

    if (param->requestType == WIFI_SET_CREDENTIALS)
    {
            /* Validate input strings to prevent buffer overflow */
            if (!validate_string_input(param->wifiCredentials.cSSID, sizeof(data.cSSID))) {
                LOG("Security: Invalid SSID input detected\n");
                return IARM_RESULT_INVALID_PARAM;
            }
            if (!validate_string_input(param->wifiCredentials.cPassword, sizeof(data.cPassword))) {
                LOG("Security: Invalid password input detected\n");
                return IARM_RESULT_INVALID_PARAM;
            }
            
            safec_rc = strcpy_s(data.cSSID, sizeof(data.cSSID), param->wifiCredentials.cSSID);
            if(safec_rc != EOK)
            {
                ERR_CHK(safec_rc);
                return IARM_RESULT_INVALID_PARAM;
            }

            safec_rc = strcpy_s(data.cPassword, sizeof(data.cPassword), param->wifiCredentials.cPassword);
            if(safec_rc != EOK)
            {
                ERR_CHK(safec_rc);
                return IARM_RESULT_INVALID_PARAM;
            }

            data.iSecurityMode = param->wifiCredentials.iSecurityMode;

            LOG("WIFI_SetCredentials ssid = %s \r\n", param->wifiCredentials.cSSID);
            err = WIFI_SetCredentials(&(data));
            if(WIFI_API_RESULT_SUCCESS  == err)
            {
                retCode=IARM_RESULT_SUCCESS;
            }
	    else
		LOG("Calling WIFI_SetCredentials returned err %d\r\n", err);
    }
    else
    {
	data.iSecurityMode = -1;
        err = WIFI_GetCredentials(&(data));

        if(WIFI_API_RESULT_SUCCESS  == err)
        {
            /* Validate output strings from WIFI_GetCredentials to prevent buffer overflow */
            if (!validate_string_input(data.cSSID, sizeof(param->wifiCredentials.cSSID))) {
                LOG("Security: Invalid SSID data from WIFI_GetCredentials\n");
                return IARM_RESULT_INVALID_PARAM;
            }
            if (!validate_string_input(data.cPassword, sizeof(param->wifiCredentials.cPassword))) {
                LOG("Security: Invalid password data from WIFI_GetCredentials\n");
                return IARM_RESULT_INVALID_PARAM;
            }
            
            safec_rc = strcpy_s(param->wifiCredentials.cSSID, sizeof(param->wifiCredentials.cSSID), data.cSSID);
            if(safec_rc != EOK)
            {
                ERR_CHK(safec_rc);
                return IARM_RESULT_INVALID_PARAM;
            }

            safec_rc = strcpy_s(param->wifiCredentials.cPassword, sizeof(param->wifiCredentials.cPassword), data.cPassword);
            if(safec_rc != EOK)
            {
                ERR_CHK(safec_rc);
                return IARM_RESULT_INVALID_PARAM;
            }

	    param->wifiCredentials.iSecurityMode = data.iSecurityMode;

            retCode=IARM_RESULT_SUCCESS;
            LOG("WIFI_GetCredentials ssid = %s SecurityMode = %d \r\n", data.cSSID, data.iSecurityMode);
        }
	else
	    LOG("Calling WIFI_GetCredentials returned err %d\r\n", err);
    }
    param->returnVal=err;
    return retCode;
}
#endif

static void writeImageCb(mfrUpgradeStatus_t * status)
{
    IARM_Bus_MFRLib_CommonAPI_WriteImageCb_Param_t param;

    IARM_Result_t retVal;
    memcpy(&param.status, status, sizeof(mfrUpgradeStatus_t));
    strncpy(param.cbData, notifyStruct.cbData, MAX_BUF);
    param.cbData[MAX_BUF-1] = '\0'; 
    LOG("In writeImage callback: cbData=%s, progress=%d, error = %d, error_str=%s, percentage = %d\n", param.cbData, param.status.progress, param.status.error, param.status.error_string, param.status.percentage/100);


    retVal = IARM_Bus_BroadcastEvent(IARM_BUS_MFRLIB_NAME, IARM_BUS_MFRMGR_EVENT_STATUS_UPDATE, (void *) &param, sizeof(param));
    if (retVal != IARM_RESULT_SUCCESS) {
           LOG("ERROR: IARM_BUS_MFRMGR_EVENT_STATUS_UPDATE failed, cbData=%s, progress=%d, retVal=%d, error=%d, %s\n", param.cbData, param.status.progress, retVal, errno, strerror(errno));
     }
    /* save progress */
    lastStatus.progress = param.status.progress;

    LOG("In writeImage callback: cbData=%s, progress=%d, error = %d, error_str=%s, percentage = %d completed\n", param.cbData, param.status.progress, param.status.error, param.status.error_string, param.status.percentage/100);

}

static IARM_Result_t writeImage_(void *arg)
{
    
    typedef mfrError_t (*mfrWriteImage_)(const char *,  const char *, mfrImageType_t ,  mfrUpgradeStatusNotify_t );
    /* check if back to back calls are being made */
    if (lastStatus.progress == mfrUPGRADE_PROGRESS_VERIFYING || lastStatus.progress == mfrUPGRADE_PROGRESS_FLASHING) {
           /* return error */
           return IARM_RESULT_INVALID_STATE;
    }
    /* OK flashing not in progress, so OK to start new flashing process */

#ifndef RDK_MFRLIB_NAME
    LOG("Please define RDK_MFRLIB_NAME\r\n");
    LOG("Exiting writeImage_\n");
    return IARM_RESULT_INVALID_STATE;
#else
    IARM_Result_t retCode = IARM_RESULT_INVALID_STATE;
    mfrError_t err = mfrERR_NONE;

    static mfrWriteImage_ func = 0;
    LOG("In writeImage_\n");
    
    /* Thread-safe check for function pointer initialization */
    pthread_mutex_lock(&mfr_dlopen_mutex);
    if (func == 0) {        
        void *dllib = secure_secure_dlopen(RDK_MFRLIB_NAME, RTLD_LAZY);
        if (dllib) {
            func = (mfrWriteImage_) dlsym(dllib, "mfrWriteImage");
            if (func) {
                LOG("mfrWriteImage is defined and loaded\r\n");
                /* Keep handle open while function is in use */
                if (mfr_library_handle == NULL) {
                    mfr_library_handle = dllib;
                } else {
                    /* Handle already tracked, can close this duplicate */
                    dlclose(dllib);
                }
            }
            else {
                LOG("mfrWriteImage is not defined\r\n");
                LOG("Exiting writeImage_\n");
				dlclose(dllib);
                pthread_mutex_unlock(&mfr_dlopen_mutex);
                return IARM_RESULT_INVALID_STATE;
            }
        }
        else {
            LOG("Opening RDK_MFRLIB_NAME [%s] failed\r\n", RDK_MFRLIB_NAME);
            LOG("Exiting writeImage_\n");
            pthread_mutex_unlock(&mfr_dlopen_mutex);
            return IARM_RESULT_INVALID_STATE;
        }
    }
    pthread_mutex_unlock(&mfr_dlopen_mutex);

    if (func) {

        IARM_Bus_MFRLib_WriteImage_Param_t *pParam = (IARM_Bus_MFRLib_WriteImage_Param_t *) arg;
        

        notifyStruct.cbData = pParam->cbData;
        notifyStruct.interval = pParam->interval;
        notifyStruct.cb = writeImageCb;


        lastStatus.progress = mfrUPGRADE_PROGRESS_NOT_STARTED;
   
        err = func(pParam->name, pParam->path, pParam->type, notifyStruct);
        
        LOG("Calling mfrWriteImage returned err %d\r\n", err);
        if(mfrERR_NONE != err) {
		retCode = IARM_RESULT_IPCCORE_FAIL;
	} else {
		retCode = IARM_RESULT_SUCCESS;

	}
    }
    LOG("Exiting writeImage_\n");
    return retCode;
#endif

}

static IARM_Result_t verifyImage_(void *arg)
{
	typedef mfrError_t (*mfrVerifyImage)(const char *,  const char *, mfrImageType_t ,  mfrUpgradeStatusNotify_t );
	/* check if back to back calls are being made */
	if (lastStatus.progress == mfrUPGRADE_PROGRESS_VERIFYING || lastStatus.progress == mfrUPGRADE_PROGRESS_FLASHING) {
		/* return error */
		return IARM_RESULT_INVALID_STATE;
	}

#ifndef RDK_MFRLIB_NAME
	LOG("Please define RDK_MFRLIB_NAME\r\n");
	LOG("Exiting verifyImage_\n");
	return IARM_RESULT_INVALID_STATE;
#else
	IARM_Result_t retCode = IARM_RESULT_INVALID_STATE;
	static mfrVerifyImage func = 0;
	LOG("In verifyImage_\n");
	if (func == 0) {
		void *dllib = secure_dlopen(RDK_MFRLIB_NAME, RTLD_LAZY);
		if (dllib) {
			func = (mfrVerifyImage) dlsym(dllib, "mfrVerifyImage");
			if (func) {
				LOG("mfrVerifyImage is defined and loaded\r\n");
			} else {
				LOG("mfrVerifyImage is not defined\r\n");
				LOG("Exiting verifyImage_\n");
				dlclose(dllib);
				return IARM_RESULT_INVALID_STATE;
			}
			dlclose(dllib);
		} else {
			LOG("Opening RDK_MFRLIB_NAME [%s] failed\r\n", RDK_MFRLIB_NAME);
			LOG("Exiting verifyImage_\n");
			return IARM_RESULT_INVALID_STATE;
		}
	}
	/* check if function symbol was found */
	if (func) {
		mfrError_t err;
		IARM_Bus_MFRLib_WriteImage_Param_t *pParam = (IARM_Bus_MFRLib_WriteImage_Param_t *) arg;
		/* init callback struct */
		notifyStruct.cbData = pParam->cbData;
		notifyStruct.interval = pParam->interval;
		notifyStruct.cb = writeImageCb;

		lastStatus.progress = mfrUPGRADE_PROGRESS_NOT_STARTED;
		/* invoke asynchronous hal function */
		LOG("Calling mfrVerifyImage>>\n");
		err = func(pParam->name, pParam->path, pParam->type, notifyStruct);
		LOG("Calling mfrVerifyImage returned err %d\r\n", err);
		if(mfrERR_NONE != err) {
			retCode = IARM_RESULT_IPCCORE_FAIL;
		} else {
			retCode = IARM_RESULT_SUCCESS;
		}
	}
	LOG("Exiting verifyImage_\n");
	return retCode;
#endif
}

IARM_Result_t setBootloaderPattern_(void *arg)
{
    typedef mfrError_t (*mfrSetBootloaderPattern)(mfrBlPattern_t);
#ifndef RDK_MFRLIB_NAME
    LOG("Please define RDK_MFRLIB_NAME. Cannot resolve mfrSetBootloaderPattern without it.\n");
    return IARM_RESULT_INVALID_STATE;
#else
    static mfrSetBootloaderPattern func = 0;
    static int symbol_lookup_complete = 0;

    if (func == 0) {
        if(0 == symbol_lookup_complete) {
            symbol_lookup_complete = 1;
            void *dllib = secure_dlopen(RDK_MFRLIB_NAME, RTLD_LAZY);
            if (dllib) {
                func = (mfrSetBootloaderPattern) dlsym(dllib, "mfrSetBootloaderPattern");
                dlclose(dllib);
                if (func) {
                    LOG("mfrSetBootloaderPattern is defined and loaded\n");
                }
                else {
                    LOG("mfrSetBootloaderPattern is not defined\n");
                    return IARM_RESULT_INVALID_STATE;
                }
            }
            else {
                LOG("Opening RDK_MFRLIB_NAME [%s] failed\n", RDK_MFRLIB_NAME);
                LOG("Exiting setBootloaderPattern_\n");
                return IARM_RESULT_INVALID_STATE;
            }
        }
        else
        {
            LOG("mfrSetBootloaderPattern is not defined\n");
            return IARM_RESULT_INVALID_STATE;
        }
    }

    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    mfrError_t err = mfrERR_NONE;
    IARM_Bus_MFRLib_SetBLPattern_Param_t *pParam = (IARM_Bus_MFRLib_SetBLPattern_Param_t *)arg;
    
    LOG("Calling mfrSetBootloaderPattern with setting 0x%x\n", pParam->pattern);
    err = func(pParam->pattern);
    if(mfrERR_NONE != err)
    {
        LOG("Calling mfrSetBootloaderPattern returned error 0x%x\n", err);
        retCode = IARM_RESULT_INVALID_PARAM;
    }
    return retCode;      
#endif
}

static IARM_Result_t getSecureTime_(void *arg)
{
    typedef mfrError_t (*getSecureTime_t)(IARM_Bus_MFRLib_SecureTime_Param *);
#ifndef RDK_MFRLIB_NAME
    LOG("Please define RDK_MFRLIB_NAME\r\n");
    return IARM_RESULT_INVALID_STATE;
#else
    static getSecureTime_t func = 0;
    if (func == 0) {
        void *dllib = secure_dlopen(RDK_MFRLIB_NAME, RTLD_LAZY);
        if (dllib) {
            func = (getSecureTime_t) dlsym(dllib, "mfrGetSecureTime");
            if (func) {
                LOG("mfrGetSecureTime(void) is defined and loaded\r\n");
            }
            else {
                LOG("mfrGetSecureTime(void) is not defined\r\n");
                dlclose(dllib);
                return IARM_RESULT_INVALID_STATE;
            }
            dlclose(dllib);
        }
        else {
            LOG("Opening RDK_MFRLIB_NAME [%s] failed\r\n", RDK_MFRLIB_NAME);
            return IARM_RESULT_INVALID_STATE;
        }
    }

    IARM_Result_t retCode = IARM_RESULT_INVALID_STATE;

    if (func && (NULL != arg)) {
        IARM_Bus_MFRLib_SecureTime_Param *param = (IARM_Bus_MFRLib_SecureTime_Param *) arg;
        mfrError_t err = func(param);

        if(mfrERR_NONE == err)
        {
            LOG("Calling mfrGetSecureTime returned err %d\r\n", err);
            retCode = IARM_RESULT_SUCCESS;
        }
    }
    return retCode;
#endif
}

static IARM_Result_t setSecureTime_(void *arg)
{
    typedef mfrError_t (*setSecureTime_t)(IARM_Bus_MFRLib_SecureTime_Param *);
#ifndef RDK_MFRLIB_NAME
    LOG("Please define RDK_MFRLIB_NAME\r\n");
    return IARM_RESULT_INVALID_STATE;
#else
    static setSecureTime_t func = 0;
    if (func == 0) {
        void *dllib = secure_dlopen(RDK_MFRLIB_NAME, RTLD_LAZY);
        if (dllib) {
            func = (setSecureTime_t) dlsym(dllib, "mfrSetSecureTime");
            if (func) {
                LOG("mfrSetSecureTime(void) is defined and loaded\r\n");
            }
            else {
                LOG("mfrSetSecureTime(void) is not defined\r\n");
                dlclose(dllib);
                return IARM_RESULT_INVALID_STATE;
            }
            dlclose(dllib);
        }
        else {
            LOG("Opening RDK_MFRLIB_NAME [%s] failed\r\n", RDK_MFRLIB_NAME);
            return IARM_RESULT_INVALID_STATE;
        }
    }

    IARM_Result_t retCode = IARM_RESULT_INVALID_STATE;

    if (func && (NULL != arg)) {
        IARM_Bus_MFRLib_SecureTime_Param *param = (IARM_Bus_MFRLib_SecureTime_Param *) arg;
        mfrError_t err = func(param);

        if(mfrERR_NONE == err)
        {
            LOG("Calling mfrSetSecureTime returned err %d\r\n", err);
            retCode = IARM_RESULT_SUCCESS;
        }
    }
    return retCode;
#endif
}


static IARM_Result_t mirrorImage(void *arg)
{

    typedef mfrError_t (*mfrMirrorImage)( mfrUpgradeStatusNotify_t );

#ifndef RDK_MFRLIB_NAME
    LOG("Please define RDK_MFRLIB_NAME\r\n");
    LOG("Exiting writeImage_\n");
    return IARM_RESULT_INVALID_STATE;
#else
    IARM_Result_t retCode = IARM_RESULT_INVALID_STATE;
    mfrError_t err = mfrERR_NONE;
    static mfrMirrorImage func = 0;
    LOG("In mfrMirrorImage\n");
    if (func == 0) {
        void *dllib = secure_dlopen(RDK_MFRLIB_NAME, RTLD_LAZY);
        if (dllib) {
            func = (mfrMirrorImage) dlsym(dllib, "mfrMirrorImage");
            if (func) {
                LOG("mfrMirrorImage is defined and loaded\r\n");
            } else {
                LOG("mfrMirrorImage is not defined\r\n");
                LOG("Exiting mfrMirrorImage\n");
                               dlclose(dllib);
                return IARM_RESULT_INVALID_STATE;
            }
           dlclose(dllib);
        } else {
            LOG("Opening RDK_MFRLIB_NAME [%s] failed\r\n", RDK_MFRLIB_NAME);
            LOG("Exiting mfrMirrorImage\n");
            return IARM_RESULT_INVALID_STATE;
        }
    }
    if (func) {

        IARM_Bus_MFRLib_WriteImage_Param_t *pParam = (IARM_Bus_MFRLib_WriteImage_Param_t *) arg;

        notifyStruct.cbData = pParam->cbData;
        notifyStruct.interval = pParam->interval;
        notifyStruct.cb = writeImageCb;

        err = func(notifyStruct);
        LOG("Calling mfrMirrorImage returned err %d\r\n", err);
        if(mfrERR_NONE != err) {
           retCode = IARM_RESULT_IPCCORE_FAIL;
        } else {
            retCode = IARM_RESULT_SUCCESS;
        }
    }
    LOG("Exiting mfrMirrorImage\n");
    return retCode;
#endif
}

static IARM_Result_t mfrSetBlSplashScreen_(void *arg)
{
    typedef mfrError_t (*mfrSetBlSplashScreen_t)(const char *);

#ifndef RDK_MFRLIB_NAME
    LOG("Please define RDK_MFRLIB_NAME\r\n");
    return IARM_RESULT_INVALID_STATE;
#else

    static mfrSetBlSplashScreen_t func = 0;

    if (func == 0)
    {
        void *dllib = secure_dlopen(RDK_MFRLIB_NAME, RTLD_LAZY);
        if (dllib)
        {
            func = (mfrSetBlSplashScreen_t) dlsym(dllib, "mfrSetBlSplashScreen");
            if (func)
            {
                LOG("mfrSetBlSplashScreen is defined and loaded\r\n");
            }
            else
            {
                LOG("mfrSetBlSplashScreen is not  defined \r\n");
                dlclose(dllib);
                return IARM_RESULT_INVALID_STATE;
            }
            dlclose(dllib);
        }
        else
        {
            LOG("Opening RDK_MFRLIB_NAME [%s] failed\r\n", RDK_MFRLIB_NAME);
            return IARM_RESULT_INVALID_STATE;
        }
    }

    IARM_Result_t retCode = IARM_RESULT_INVALID_STATE;
    mfrError_t err = mfrERR_NONE;
    if(NULL != arg)
    {

         IARM_Bus_MFRLib_SetBLSplashScreen_Param_t *param = (IARM_Bus_MFRLib_SetBLSplashScreen_Param_t *) arg;
         if (func)
         {
             err = func(param->path);
             if(mfrERR_NONE == err)
             {
                 LOG("[mfrMgr] IARM_Bus_MFRLib_SetBLSplashScreen_Param success with param is %s \n", param->path);
                 retCode = IARM_RESULT_SUCCESS;
             }
         }
    }
    else
    {
        LOG("[mfrMgr] IARM_Bus_MFRLib_SplashScreen_Param IARM_RESULT_INVALID_PARAM \n");
        retCode = IARM_RESULT_INVALID_PARAM;
    }

    return retCode;
#endif
}

static IARM_Result_t mfrClearBlSplashScreen_(void *arg)
{
    typedef mfrError_t (*mfrClearBlSplashScreen_t)(void);

#ifndef RDK_MFRLIB_NAME
    LOG("Please define RDK_MFRLIB_NAME\r\n");
    return IARM_RESULT_INVALID_STATE;
#else

    static mfrClearBlSplashScreen_t func = 0;

    if (func == 0)
    {
        void *dllib = secure_dlopen(RDK_MFRLIB_NAME, RTLD_LAZY);
        if (dllib)
        {
            func = (mfrClearBlSplashScreen_t) dlsym(dllib, "mfrClearBlSplashScreen");
            if (func)
            {
                LOG("mfrClearBlSplashScreen is defined and loaded\r\n");
            }
            else
            {
                LOG("mfrClearBlSplashScreen is not  defined \r\n");
                dlclose(dllib);
                return IARM_RESULT_INVALID_STATE;
            }
            dlclose(dllib);
        }
        else
        {
            LOG("Opening RDK_MFRLIB_NAME [%s] failed\r\n", RDK_MFRLIB_NAME);
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
            LOG("[mfrMgr] mfrClearBlSplashScreen success\n");
            retCode = IARM_RESULT_SUCCESS;
        }
    }
    return retCode;
#endif
}

#ifdef MFR_TEMP_CLOCK_READ
static IARM_Result_t getTemperature_(void *arg)
{
       IARM_Result_t retCode = IARM_RESULT_IPCCORE_FAIL;
       mfrError_t err = mfrERR_NONE;
       typedef mfrError_t (*mfrGetTemperature_t)(IARM_Bus_MFRLib_CurThermalState_t*, int*, int*);
       static mfrGetTemperature_t func = 0;

        if (func == 0)
        {
            void *dllib = secure_dlopen(RDK_MFRLIB_NAME, RTLD_LAZY);
            if (dllib)
            {
                func = (mfrGetTemperature_t) dlsym(dllib, "mfrGetTemperature");
                if (func)
                {
                    LOG("mfrGetTemperature is defined and loaded\r\n");
                }
                else
                {
                    LOG("mfrGetTemperature is not defined\r\n");
                    dlclose(dllib);
                    return IARM_RESULT_INVALID_STATE;
                }
                dlclose(dllib);
            }
            else
            {
                LOG("Opening RDK_MFRLIB_NAME [%s] failed\r\n", RDK_MFRLIB_NAME);
                return IARM_RESULT_INVALID_STATE;
            }
        }

       if(NULL != arg && func)
       {
               IARM_Bus_MFRLib_ThermalSoCTemp_Param_t *param = (IARM_Bus_MFRLib_ThermalSoCTemp_Param_t *)arg;
               IARM_Bus_MFRLib_CurThermalState_t state;
               int temperatureValue, wifiTempValue;

               err = func(&state, &temperatureValue, &wifiTempValue);

               if(mfrERR_NONE == err)
               {
                       retCode = IARM_RESULT_SUCCESS;
                       param->curState = state;
                       param->curSoCTemperature = temperatureValue;
                       param->curWiFiTemperature = wifiTempValue;

                      /* Reducing thermal temperature logs which occurs at every 30 seconds */
                      #if 0
                       LOG("[mfrMgr] thermal current state is queried: returning %d \n", param->curState);
                       LOG("[mfrMgr] thermal SoC temperature is queried: returning %d \n", param->curSoCTemperature);
                       LOG("[mfrMgr] WiFi temperature is queried: returning %d \n", param->curWiFiTemperature);
                      #endif
               }
       }
       else
       {
               LOG("[mfrMgr] mfrGetTemperature : IARM_RESULT_INVALID_PARAM \n");
               retCode = IARM_RESULT_INVALID_PARAM;
       }
       return retCode;
}

static IARM_Result_t setTemperatureThresholds_(void *arg)
{
       IARM_Result_t retCode = IARM_RESULT_IPCCORE_FAIL;
       mfrError_t err = mfrERR_NONE;
       typedef mfrError_t (*mfrSetTempThresholds_t)(int, int);
       static mfrSetTempThresholds_t func = 0;

         if (func == 0)
         {
              void *dllib = secure_dlopen(RDK_MFRLIB_NAME, RTLD_LAZY);
              if (dllib)
              {
                func = (mfrSetTempThresholds_t) dlsym(dllib, "mfrSetTempThresholds");
                if (func)
                {
                     LOG("mfrSetTempThresholds is defined and loaded\r\n");
                }
                else
                {
                     LOG("mfrSetTempThresholds is not defined\r\n");
                     dlclose(dllib);
                     return IARM_RESULT_INVALID_STATE;
                }
                dlclose(dllib);
              }
              else
              {
                LOG("Opening RDK_MFRLIB_NAME [%s] failed\r\n", RDK_MFRLIB_NAME);
                return IARM_RESULT_INVALID_STATE;
              }
         }

       if(NULL != arg && func)
       {
               IARM_Bus_MFRLib_ThermalSoCTemp_Param_t * param = (IARM_Bus_MFRLib_ThermalSoCTemp_Param_t *) arg;

               err = func(param->highTemp ,param->criticalTemp);
               if (mfrERR_NONE == err)
               {
                       retCode = IARM_RESULT_SUCCESS;
                       LOG("[mfrMgr] New Thermal threshold : High = %0.6f , Critical = %0.6f \n",(float)param->highTemp ,(float)param->criticalTemp);
               }
       }
       else
       {
               LOG("[mfrMgr] mfrSetTempThresholds : IARM_RESULT_INVALID_PARAM \n");
               retCode = IARM_RESULT_INVALID_PARAM;
       }
       return retCode;

}

static IARM_Result_t getTemperatureThresholds_(void *arg)
{
       IARM_Result_t retCode = IARM_RESULT_IPCCORE_FAIL;
       mfrError_t err = mfrERR_NONE;
       typedef mfrError_t (*mfrGetTempThresholds_t)(int*, int*);
       static mfrGetTempThresholds_t func = 0;

         if (func == 0)
         {
                  void *dllib = secure_dlopen(RDK_MFRLIB_NAME, RTLD_LAZY);
                  if (dllib)
                  {
                 func = (mfrGetTempThresholds_t) dlsym(dllib, "mfrGetTempThresholds");
                 if (func)
                 {
                        LOG("mfrGetTempThresholds is defined and loaded\r\n");
                 }
                 else
                 {
                        LOG("mfrGetTempThresholds is not defined\r\n");
                        dlclose(dllib);
                        return IARM_RESULT_INVALID_STATE;
                 }
                 dlclose(dllib);
                  }
                  else
                  {
                 LOG("Opening RDK_MFRLIB_NAME [%s] failed\r\n", RDK_MFRLIB_NAME);
                 return IARM_RESULT_INVALID_STATE;
                  }
         }

       if(NULL != arg && func)
       {
               IARM_Bus_MFRLib_ThermalSoCTemp_Param_t * param = (IARM_Bus_MFRLib_ThermalSoCTemp_Param_t *) arg;
               int high, critical;

               err = func(&high, &critical);
               if (mfrERR_NONE == err)
               {
                       retCode = IARM_RESULT_SUCCESS;
                      param->highTemp = high;
                       param->criticalTemp = critical;
                       LOG("[mfrMgr] Current thermal threshold : High = %0.6f , Critical = %0.6f \n",(float)param->highTemp ,(float)param->criticalTemp);
               }
       }
       else
       {
               LOG("[mfrMgr] mfrGetTempThresholds : IARM_RESULT_INVALID_PARAM \n");
               retCode = IARM_RESULT_INVALID_PARAM;
       }
       return retCode;
}

static IARM_Result_t searchCPUClockSpeeds_(void *arg)
{
       IARM_Result_t retCode = IARM_RESULT_IPCCORE_FAIL;
       mfrError_t err = mfrERR_NONE;
       typedef mfrError_t (*mfrDetemineClockSpeeds_t)(uint32_t *, uint32_t *, uint32_t *);
       static mfrDetemineClockSpeeds_t func = 0;

       if (func == 0)
       {
               void *dllib = secure_dlopen(RDK_MFRLIB_NAME, RTLD_LAZY);
               if (dllib)
               {
                       func = (mfrDetemineClockSpeeds_t) dlsym(dllib, "mfrDetemineClockSpeeds");
                       if (func)
                       {
                               LOG("mfrDetemineClockSpeeds is defined and loaded\r\n");
                       }
                       else
                       {
                               LOG("mfrDetemineClockSpeeds is not defined\r\n");
                               dlclose(dllib);
                               return IARM_RESULT_INVALID_STATE;
                       }
                       dlclose(dllib);
               }
               else
               {
                       LOG("Opening RDK_MFRLIB_NAME [%s] failed\r\n", RDK_MFRLIB_NAME);
                       return IARM_RESULT_INVALID_STATE;
               }
       }

       if(NULL != arg && func)
       {
               IARM_Bus_MFRLib_ThermalSoCFreq_Param_t * param = (IARM_Bus_MFRLib_ThermalSoCFreq_Param_t *) arg;
               uint32_t cpu_clock_Normal ,cpu_clock_Scaled ,cpu_clock_Minimal;

               err = func(&cpu_clock_Normal ,&cpu_clock_Scaled ,&cpu_clock_Minimal);
               if(mfrERR_NONE == err)
               {
                       retCode = IARM_RESULT_SUCCESS;
                       param->cpu_rate_Normal   =  cpu_clock_Normal;
                       param->cpu_rate_Scaled   =  cpu_clock_Scaled;
                       param->cpu_rate_Minimal  =  cpu_clock_Minimal;
                       LOG("[mfrMgr] Available CPU Clocks - Normal:%u Scaled:%u Minimal:%u\n",param->cpu_rate_Normal ,param->cpu_rate_Scaled ,param->cpu_rate_Minimal);
               }
               else
               {
                       LOG("Calling mfrDetemineClockSpeeds returned err %d\r\n", err);
               }
       }
       else
       {
               LOG("[mfrMgr] mfrDetemineClockSpeeds : IARM_RESULT_INVALID_PARAM \n");
               retCode = IARM_RESULT_INVALID_PARAM;
       }
       return retCode;
}

static IARM_Result_t setCPUClockSpeed_(void *arg)
{
       IARM_Result_t retCode = IARM_RESULT_IPCCORE_FAIL;
       mfrError_t err = mfrERR_NONE;
       typedef mfrError_t (*mfrSetClockSpeed_t)(uint32_t);
       static mfrSetClockSpeed_t func = 0;

       if (func == 0)
       {
               void *dllib = secure_dlopen(RDK_MFRLIB_NAME, RTLD_LAZY);
               if (dllib)
               {
                       func = (mfrSetClockSpeed_t) dlsym(dllib, "mfrSetClockSpeed");
                       if (func)
                       {
                               LOG("mfrSetClockSpeed is defined and loaded\r\n");
                       }
                       else
                       {
                               LOG("mfrSetClockSpeed is not defined\r\n");
                               dlclose(dllib);
                               return IARM_RESULT_INVALID_STATE;
                       }
                       dlclose(dllib);
               }
               else
               {
                       LOG("Opening RDK_MFRLIB_NAME [%s] failed\r\n", RDK_MFRLIB_NAME);
                       return IARM_RESULT_INVALID_STATE;
               }
       }

       if(NULL != arg && func)
       {
               IARM_Bus_MFRLib_ThermalSoCFreq_Param_t * param = (IARM_Bus_MFRLib_ThermalSoCFreq_Param_t *) arg;

               err = func(param->cpu_clock_speed);
               if(mfrERR_NONE == err)
               {
                       retCode = IARM_RESULT_SUCCESS;
                       LOG("[mfrMgr] CPU Clock Speed Set to [%u]\n",param->cpu_clock_speed);
               }
               else
               {
                       LOG("Calling mfrDetemineClockSpeeds returned err %d\r\n", err);
               }
       }
       else
       {
               LOG("[mfrMgr] mfrSetClockSpeed : IARM_RESULT_INVALID_PARAM \n");
               retCode = IARM_RESULT_INVALID_PARAM;
       }
       return retCode;
}

static IARM_Result_t getCPUClockSpeed_(void *arg)
{
       IARM_Result_t retCode = IARM_RESULT_IPCCORE_FAIL;
       mfrError_t err = mfrERR_NONE;
       typedef mfrError_t (*mfrGetClockSpeed_t)(uint32_t *);
       static mfrGetClockSpeed_t func = 0;

       if (func == 0)
       {
               void *dllib = secure_dlopen(RDK_MFRLIB_NAME, RTLD_LAZY);
               if (dllib)
               {
                       func = (mfrGetClockSpeed_t) dlsym(dllib, "mfrGetClockSpeed");
                       if (func)
                       {
                               LOG("mfrGetClockSpeed is defined and loaded\r\n");
                       }
                       else
                       {
                               LOG("mfrGetClockSpeed is not defined\r\n");
                               dlclose(dllib);
                               return IARM_RESULT_INVALID_STATE;
                       }
                       dlclose(dllib);
               }
               else
               {
                       LOG("Opening RDK_MFRLIB_NAME [%s] failed\r\n", RDK_MFRLIB_NAME);
                       return IARM_RESULT_INVALID_STATE;
               }
       }

       if(NULL != arg && func)
       {
               IARM_Bus_MFRLib_ThermalSoCFreq_Param_t * param = (IARM_Bus_MFRLib_ThermalSoCFreq_Param_t *) arg;
               uint32_t clock_speed;

               err = func(&clock_speed);
               if(mfrERR_NONE == err)
               {
                       retCode = IARM_RESULT_SUCCESS;
                       param->cpu_clock_speed = clock_speed;
                       LOG("[mfrMgr] Getting CPU Clock Speed as [%u]\n",param->cpu_clock_speed);
               }
               else
               {
                       LOG("Calling mfrGetClockSpeed returned err %d\r\n", err);
               }
       }
       else
       {
               LOG("[mfrMgr] mfrGetClockSpeed : IARM_RESULT_INVALID_PARAM \n");
               retCode = IARM_RESULT_INVALID_PARAM;
       }
       return retCode;
}

#endif

IARM_Result_t MFRLib_Start(void)
{
    IARM_Result_t err = IARM_RESULT_SUCCESS;

    LOG("Entering [%s] - [%s] - disabling io redirect buf\r\n", __FUNCTION__, IARM_BUS_MFRLIB_NAME);
    setvbuf(stdout, NULL, _IOLBF, 0);

    do{

        if(mfr_init()!= mfrERR_NONE)
        {
            LOG("Error initializing MFR library..\n");
	    err = IARM_RESULT_INVALID_STATE;
	    break;
        }

        err = IARM_Bus_Init(IARM_BUS_MFRLIB_NAME);

        if(IARM_RESULT_SUCCESS != err)
        {
            LOG("Error initializing IARM.. error code : %d\n",err);
            break;
        }

        err = IARM_Bus_Connect();

        if(IARM_RESULT_SUCCESS != err)
        {
            LOG("Error connecting to IARM.. error code : %d\n",err);
            break;
        }
        is_connected = 1;
        err = IARM_Bus_RegisterCall(IARM_BUS_MFRLIB_API_GetSerializedData,getSerializedData_);
        if(IARM_RESULT_SUCCESS != err)
        {
            LOG("Error registering call(getSerializedData) in IARM.. error code : %d\n",err);
            break;
        }
	err = IARM_Bus_RegisterCall(IARM_BUS_MFRLIB_API_SetSerializedData,setSerializedData_);
	if(IARM_RESULT_SUCCESS != err)
	{
		LOG("Error registering call(setSerializedData) in IARM.. error code : %d\n",err);
		break;
	}

	err = IARM_Bus_RegisterCall(IARM_BUS_MFRLIB_API_DeletePDRI, deletePDRI_);
        if(IARM_RESULT_SUCCESS != err)
        {
            LOG("Error registering call(deletePDRI) in IARM.. error code : %d\n",err);
            break;
        }

        err = IARM_Bus_RegisterCall(IARM_BUS_MFRLIB_API_ScrubAllBanks, scrubAllBanks_);

        if(IARM_RESULT_SUCCESS != err)
        {
            LOG("Error registering call(scrubAllBanks) in IARM.. error code : %d\n",err);
            break;
        }
        err = IARM_Bus_RegisterCall(IARM_BUS_MFRLIB_API_WriteImage, writeImage_);

        if(IARM_RESULT_SUCCESS != err)
        {
            LOG("Error registering call(writeImage) in IARM.. error code : %d\n",err);
            break;
        }

        err = IARM_Bus_RegisterCall(IARM_BUS_MFRLIB_API_SetBootLoaderPattern, setBootloaderPattern_);
        if(IARM_RESULT_SUCCESS != err)
        {
            LOG("Error registering call(setBootloaderPattern_) in IARM.. error code : %d\n",err);
            break;
        }

       err = IARM_Bus_RegisterCall(IARM_BUS_MFRLIB_API_MirrorImage, mirrorImage);
       if(IARM_RESULT_SUCCESS != err)
       {
               LOG("Error registering call(mirrorImage) in IARM.. error code : %d\n",err);
               break;
       }

	err = IARM_Bus_RegisterCall(IARM_BUS_MFRLIB_API_SetBlSplashScreen, mfrSetBlSplashScreen_);
	if(IARM_RESULT_SUCCESS != err)
	{
		LOG("Error registering call(mfrSetBlSplashScreen) in IARM.. error code : %d\n",err);
		break;
	}

	err = IARM_Bus_RegisterCall(IARM_BUS_MFRLIB_API_ClearBlSplashScreen, mfrClearBlSplashScreen_);
	if(IARM_RESULT_SUCCESS != err)
	{
		LOG("Error registering call(mfrClearBlSplashScreen) in IARM.. error code : %d\n",err);
		break;
	}
#ifdef ENABLE_MFR_WIFI
	err = IARM_Bus_RegisterCall(IARM_BUS_MFRLIB_API_WIFI_EraseAllData, mfrWifiEraseAllData_);

        if(IARM_RESULT_SUCCESS != err) {
            LOG("Error registering call(wifiEraseAllData) in IARM.. error code : %d\n",err);
            break;
        }
        err = IARM_Bus_RegisterCall(IARM_BUS_MFRLIB_API_WIFI_Credentials, mfrWifiCredentials_);

        if(IARM_RESULT_SUCCESS != err) {
            LOG("Error registering call(mfrWifiGetCredentials) in IARM.. error code : %d\n",err);
            break;
        }
#endif 
	/* registering verify image call */
	err = IARM_Bus_RegisterCall(IARM_BUS_MFRLIB_API_validate_image, verifyImage_);
	if(IARM_RESULT_SUCCESS != err)
	{
		LOG("Error registering call(IARM_BUS_MFRLIB_API_validate_image) in IARM.. error code : %d\n",err);
		break;
	}
	/* register the write image callback event */
	err = IARM_Bus_RegisterEvent(IARM_BUS_MFRMGR_EVENT_MAX);
	if(IARM_RESULT_SUCCESS != err)
	{
		LOG("Error registering event(IARM_BUS_MFRMGR_EVENT_MAX) in IARM.. error code : %d\n",err);
		break;
	}

    err = IARM_Bus_RegisterCall(IARM_BUS_MFRLIB_API_GetSecureTime, getSecureTime_);

    if(IARM_RESULT_SUCCESS != err)
    {
        LOG("Error registering call(mfrGetSecureTime) in IARM.. error code : %d\n",err);
        break;
    }
    err = IARM_Bus_RegisterCall(IARM_BUS_MFRLIB_API_SetSecureTime, setSecureTime_);

    if(IARM_RESULT_SUCCESS != err)
    {
        LOG("Error registering call(mfrGetSecureTime) in IARM.. error code : %d\n",err);
        break;
    }

	err = IARM_Bus_RegisterCall(IARM_BUS_MFRLIB_API_SetFsrFlag, setFSRflag_);

    if(IARM_RESULT_SUCCESS != err)
    {
        LOG("Error registering call(mfrSetFSRflag) in IARM.. error code : %d\n",err);
        break;
    }

    err = IARM_Bus_RegisterCall(IARM_BUS_MFRLIB_API_GetFsrFlag, getFSRflag_);

    if(IARM_RESULT_SUCCESS != err)
    {
        LOG("Error registering call(mfrGetFSRflag) in IARM.. error code : %d\n",err);
        break;
    }
#ifdef MFR_TEMP_CLOCK_READ
    err = IARM_Bus_RegisterCall(IARM_BUS_MFRLIB_API_GetTemperature, getTemperature_);

    if(IARM_RESULT_SUCCESS != err)
    {
        LOG("Error registering call(mfrGetTemperature) in IARM.. error code : %d\n",err);
        break;
    }

    err = IARM_Bus_RegisterCall(IARM_BUS_MFRLIB_API_SetTemperatureThresholds, setTemperatureThresholds_);

    if(IARM_RESULT_SUCCESS != err)
    {
        LOG("Error registering call(mfrSetTempThresholds) in IARM.. error code : %d\n",err);
        break;
    }

    err = IARM_Bus_RegisterCall(IARM_BUS_MFRLIB_API_GetTemperatureThresholds, getTemperatureThresholds_);

    if(IARM_RESULT_SUCCESS != err)
    {
         LOG("Error registering call(mfrGetTempThresholds) in IARM.. error code : %d\n",err);
         break;
    }

    err = IARM_Bus_RegisterCall(IARM_BUS_MFRLIB_API_SearchCPUClockSpeeds, searchCPUClockSpeeds_);

    if(IARM_RESULT_SUCCESS != err)
    {
         LOG("Error registering call(mfrDetemineClockSpeeds) in IARM.. error code : %d\n",err);
         break;
    }

    err = IARM_Bus_RegisterCall(IARM_BUS_MFRLIB_API_SetCPUClockSpeed, setCPUClockSpeed_);

    if(IARM_RESULT_SUCCESS != err)
    {
         LOG("Error registering call(mfrSetClockSpeed) in IARM.. error code : %d\n",err);
         break;
    }

    err = IARM_Bus_RegisterCall(IARM_BUS_MFRLIB_API_GetCPUClockSpeed, getCPUClockSpeed_);

    if(IARM_RESULT_SUCCESS != err)
    {
         LOG("Error registering call(mfrGetClockSpeed) in IARM.. error code : %d\n",err);
         break;
    }
#endif
	LOG("All IARM Bus calls and events registered successfully\n");
    }while(0);

    if(err != IARM_RESULT_SUCCESS)
    {
        if(is_connected)
        {
            if (IARM_Bus_Disconnect() != IARM_RESULT_SUCCESS) {
                LOG("Warning: IARM_Bus_Disconnect failed during error cleanup\n");
            }
        }
        if (IARM_Bus_Term() != IARM_RESULT_SUCCESS) {
            LOG("Warning: IARM_Bus_Term failed during error cleanup\n");
        }
    }

    return err;

}

static IARM_Result_t setFSRflag_(void *arg)
{
    typedef mfrError_t (*mfrSetFSRflag_t)(uint16_t *);

#ifndef RDK_MFRLIB_NAME
    LOG("Please define RDK_MFRLIB_NAME\r\n");
    return IARM_RESULT_INVALID_STATE;
#else

    static mfrSetFSRflag_t func = 0;

    if (func == 0)
    {
        void *dllib = secure_dlopen(RDK_MFRLIB_NAME, RTLD_LAZY);
        if (dllib)
        {
            func = (mfrSetFSRflag_t) dlsym(dllib, "mfrSetFSRflag");
            if (func)
            {
                LOG("mfrSetFSRflag is defined and loaded\r\n");
            }
            else
            {
                LOG("mfrSetFSRflag is not  defined \r\n");
                dlclose(dllib);
                return IARM_RESULT_INVALID_STATE;
            }
            dlclose(dllib);
        }
        else
        {
            LOG("Opening RDK_MFRLIB_NAME [%s] failed\r\n", RDK_MFRLIB_NAME);
            return IARM_RESULT_INVALID_STATE;
        }
    }

    IARM_Result_t retCode = IARM_RESULT_INVALID_STATE;
    mfrError_t err = mfrERR_NONE;
    if(NULL != arg)
    {
         IARM_Bus_MFRLib_FsrFlag_Param_t *tmp = (IARM_Bus_MFRLib_FsrFlag_Param_t *) arg;
         uint16_t param = *tmp ? 1 : 0;
         if (func)
         {
             err = func(&param);
             if(mfrERR_NONE == err)
             {
                 LOG("[mfrMgr][%s] IARM_Bus_MFRLib_FsrFlag_Param_t success with param is %d \n",__FUNCTION__, param);
                 retCode = IARM_RESULT_SUCCESS;
             }
             else
             {
                 LOG("[mfrMgr][%s] mfrSetFSRflag failed with error code return:%d \n",__FUNCTION__, err);
             }
         }
    }
    else
    {
        LOG("[mfrMgr] IARM_Bus_MFRLib_FsrFlag_Param_t IARM_RESULT_INVALID_PARAM \n");
        retCode = IARM_RESULT_INVALID_PARAM;
    }

    return retCode;
  
#endif

}

static IARM_Result_t getFSRflag_(void *arg)
{

    typedef mfrError_t (*mfrGetFSRflag_t)(uint16_t *);

#ifndef RDK_MFRLIB_NAME
    LOG("Please define RDK_MFRLIB_NAME\r\n");
    return IARM_RESULT_INVALID_STATE;
#else

    static mfrGetFSRflag_t func = 0;

    if (func == 0)
    {
        void *dllib = secure_dlopen(RDK_MFRLIB_NAME, RTLD_LAZY);
        if (dllib)
        {
            func = (mfrGetFSRflag_t) dlsym(dllib, "mfrGetFSRflag");
            if (func)
            {
                LOG("mfrGetFSRflag is defined and loaded\r\n");
            }
            else
            {
                LOG("mfrGetFSRflag is not  defined \r\n");
                dlclose(dllib);
                return IARM_RESULT_INVALID_STATE;
            }
            dlclose(dllib);
        }
        else
        {
            LOG("Opening RDK_MFRLIB_NAME [%s] failed\r\n", RDK_MFRLIB_NAME);
            return IARM_RESULT_INVALID_STATE;
        }
    }

    IARM_Result_t retCode = IARM_RESULT_INVALID_STATE;
    mfrError_t err = mfrERR_NONE;
    if(NULL != arg)
    {

         IARM_Bus_MFRLib_FsrFlag_Param_t *tmp = (IARM_Bus_MFRLib_FsrFlag_Param_t *) arg;
         uint16_t param = -1;
         if (func)
         {
             err = func(&param);
             if(mfrERR_NONE == err && param != (uint16_t)-1)
             {
                 LOG("[mfrMgr][%s] IARM_Bus_MFRLib_FsrFlag_Param_t success with param is %d \n",__FUNCTION__, param);
                 *tmp = (IARM_Bus_MFRLib_FsrFlag_Param_t)param?true:false;
                 retCode = IARM_RESULT_SUCCESS;
             }  
             else
             {
                 LOG("[mfrMgr][%s] mfrSetFSRflag failed with error code return:%d and FSRFlag:%d \n",__FUNCTION__, err, param);
             }
             
         }
    }
    else
    {
        LOG("[mfrMgr] IARM_Bus_MFRLib_FsrFlag_Param_t IARM_RESULT_INVALID_PARAM \n");
        retCode = IARM_RESULT_INVALID_PARAM;
    }

    return retCode;
#endif
}

IARM_Result_t MFRLib_Stop(void)
{
    /* Signal main loop to terminate */
    mfr_loop_running = 0;
    
    if(is_connected)
    {
	if (IARM_Bus_Disconnect() != IARM_RESULT_SUCCESS) {
		LOG("Warning: IARM_Bus_Disconnect failed during MFRLib_Stop cleanup\n");
	}
	if (IARM_Bus_Term() != IARM_RESULT_SUCCESS) {
		LOG("Warning: IARM_Bus_Term failed during MFRLib_Stop cleanup\n");
	}
    }
    
    /* Cleanup dynamic library resources */
    pthread_mutex_lock(&mfr_dlopen_mutex);
    if (mfr_library_handle != NULL) {
        dlclose(mfr_library_handle);
        mfr_library_handle = NULL;
    }
    pthread_mutex_unlock(&mfr_dlopen_mutex);
    
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t MFRLib_Loop()
{
    while(mfr_loop_running)
    {
        LOG("I-ARM MFR Lib: HeartBeat ping.\r\n");
        sleep(300);
    }
    LOG("I-ARM MFR Lib: Loop terminated gracefully.\r\n");
    return IARM_RESULT_SUCCESS;
}





/** @} */
/** @} */
