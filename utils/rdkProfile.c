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
#include <limits.h>     /* For PATH_MAX */

#include "iarmutilslogger.h"
#include "rdkProfile.h"

#define RDK_PROFILE "RDK_PROFILE"
#define PROFILE_STR_TV "TV"
#define PROFILE_STR_STB "STB"

#ifdef __cplusplus
extern "C"
{
#endif

/*
 * Security-hardened RDK profile detection
 * 
 * This function reads device properties from a system configuration file
 * and determines the RDK profile type (TV vs STB). Security measures implemented:
 * - Path validation to prevent traversal attacks
 * - Buffer bounds checking for file operations
 * - Null pointer validation throughout
 * - Safe pointer arithmetic with bounds checking
 */
profile_t searchRdkProfile(void) {
    INT_DEBUG("Entering [%s]\r\n", __FUNCTION__);
    
    /* Issue 4 Fix: Path validation and security
     * Validate that the configuration file path is safe and exists
     * The path is hardcoded to prevent path traversal attacks
     * Future enhancement: Support secure environment variable override
     */
    const char* devPropPath = "/etc/device.properties";
    char line[256];
    char *rdkProfile = NULL;
    profile_t ret = PROFILE_INVALID;
    FILE* file = NULL;
    
    /* Security validation: Ensure path is within expected bounds */
    if (strlen(devPropPath) >= PATH_MAX) {
        INT_ERROR("[%s]: Device properties path too long (potential attack)\n", __FUNCTION__);
        return PROFILE_INVALID;
    }
    
    /* Security validation: Check file access before opening */
    if (access(devPropPath, R_OK) != 0) {
        INT_ERROR("[%s]: Cannot access device properties file safely\n", __FUNCTION__);
        return PROFILE_INVALID;
    }

    file = fopen(devPropPath, "r");
    if (file == NULL) {
        INT_ERROR("[%s]: File not found or access denied.\n", __FUNCTION__);
        return PROFILE_INVALID;
    }

    /* Issue 5 Fix: Buffer overflow protection for file reading
     * Use fgets with explicit buffer size and validate line length
     */
    while (fgets(line, sizeof(line), file) != NULL) {
        /* Ensure line is null-terminated and validate length */
        line[sizeof(line) - 1] = '\0';
        size_t line_len = strlen(line);
        
        if (line_len >= sizeof(line) - 1) {
            INT_WARNING("[%s]: Line too long, may be truncated\n", __FUNCTION__);
            continue; /* Skip potentially malformed lines */
        }
        
        /* Issue 6 Fix: Null pointer validation after strstr */
        rdkProfile = strstr(line, RDK_PROFILE);
        if (rdkProfile != NULL) {
            INT_DEBUG("[%s]: Found RDK_PROFILE\r\n", __FUNCTION__);
            break;
        }
    }

    if(rdkProfile != NULL)
    {
        /* Issue 7 Fix: Safe pointer arithmetic with bounds checking */
        size_t rdk_profile_len = strlen(RDK_PROFILE);
        size_t remaining_len = strlen(rdkProfile);
        
        /* Validate we have enough space for safe pointer advancement */
        if (remaining_len <= rdk_profile_len + 1) { /* +1 for '=' character */
            INT_ERROR("[%s]: Invalid RDK_PROFILE format - insufficient data\n", __FUNCTION__);
            fclose(file);
            return PROFILE_INVALID;
        }
        
        rdkProfile += rdk_profile_len;
        
        /* Additional safety: verify we're at '=' character */
        if (*rdkProfile != '=') {
            INT_ERROR("[%s]: Invalid RDK_PROFILE format - missing '=' delimiter\n", __FUNCTION__);
            fclose(file);
            return PROFILE_INVALID;
        }
        
        rdkProfile++; // Move past the '=' character
        
        /* Final bounds check after pointer arithmetic */
        if (rdkProfile >= line + sizeof(line)) {
            INT_ERROR("[%s]: Pointer arithmetic exceeded buffer bounds\n", __FUNCTION__);
            fclose(file);
            return PROFILE_INVALID;
        }
        
        /* Safe string comparison with validated pointers */
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
