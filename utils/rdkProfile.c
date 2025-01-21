/*
 * If not stated otherwise in this file or this component's Licenses.txt file the
 * following copyright and licenses apply:
 *
 * Copyright 2021 RDK Management
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
* @defgroup dsmgr
* @{
**/


#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "iarmutilslogger.h"
#include "rdkProfile.h"

#define RDK_PROFILE "RDK_PROFILE"
#define PROFILE_STR_TV "TV"
#define PROFILE_STR_STB "STB"

#ifdef __cplusplus
extern "C"
{
#endif

profile_t searchRdkProfile(void) {
    INT_DEBUG("Entering [%s]\r\n", __FUNCTION__);
    const char* devPropPath = "/etc/device.properties";
    char line[256], *rdkProfile = NULL;
    profile_t ret = PROFILE_INVALID;
    FILE* file;

    file = fopen(devPropPath, "r");
    if (file == NULL) {
        INT_ERROR("[%s]: File not found.\n", __FUNCTION__);
        return PROFILE_INVALID;
    }

    while (fgets(line, sizeof(line), file)) {
        rdkProfile = strstr(line, RDK_PROFILE);
        if (rdkProfile != NULL) {
            INT_DEBUG("[%s]: Found RDK_PROFILE\r\n", __FUNCTION__);
            break;
        }
    }

    if(rdkProfile != NULL)
    {
        rdkProfile += strlen(RDK_PROFILE);
        rdkProfile++; // Move past the '=' character
        if(0 == strncmp(rdkProfile, PROFILE_STR_TV, strlen(PROFILE_STR_TV)))
        {
            ret = PROFILE_TV;
            INT_DEBUG("[%s]: Found RDK_PROFILE is TV", __FUNCTION__);
        }
        else if (0 == strncmp(rdkProfile, PROFILE_STR_STB, strlen(PROFILE_STR_STB)))
        {
            ret = PROFILE_STB;
            INT_DEBUG("[%s]: Found RDK_PROFILE is STB", __FUNCTION__);
        }
    }
    else
    {
        INT_ERROR("[%s]: NOT FOUND RDK_PROFILE in device properties file\r\n", __FUNCTION__);
        ret = PROFILE_INVALID;
    }

    fclose(file);
    INT_INFO("Exit [%s]: RDK_PROFILE = %d\r\n", __FUNCTION__, ret);
    return ret;
}

#ifdef __cplusplus
}
#endif

/** @} */
/** @} */
