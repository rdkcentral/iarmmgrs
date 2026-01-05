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
* @file rdmMgr.h
*
* @brief IARM-Bus RDM Manager Public API.
* 
* @section Purpose
* This header defines the Remote Download Manager (RDM) interface for IARM bus
* communication. It provides data structures and event definitions for package
* management operations including download, installation, and status reporting.
* 
* @section Architecture
* This is an interface-only header that defines the communication protocol
* between RDM components. The actual RDM manager implementation is typically
* provided by:
* - Platform-specific package managers
* - RDK services (rdkservices components)
* - Third-party package management daemons
* 
* @section Usage
* Components use this interface to:
* - Register for RDM events via IARM bus
* - Exchange package information and status
* - Coordinate package installation workflows
* 
* @note This header defines the communication interface only, not the
*       implementation logic.
*/

/**
* @defgroup iarmmgrs
* @{
* @defgroup rdmmgr
* @{
**/


#ifndef IARMMGRS_RDM_MGR_H
#define IARMMGRS_RDM_MGR_H


#ifdef __cplusplus
extern "C" 
{
#endif

#define IARM_BUS_RDMMGR_NAME 					    "RDMMgr" /*!< IARM BUS  name for rdm manager */

/* String constants for package information fields - validated against buffer sizes */
#define RDM_PKG_NAME								"pkg_name"          /* Length: 8 chars - well within NAME_MAX_SIZE (128) */
#define RDM_PKG_VERSION								"pkg_version"       /* Length: 11 chars - well within VERSION_MAX_SIZE (64) */
#define RDM_PKG_INST_PATH							"pkg_inst_path"     /* Length: 13 chars - well within INST_PATH_MAX_SIZE (256) */
#define RDM_PKG_INST_STATUS							"pkg_inst_status"   /* Length: 15 chars - used as key identifier */

/* Buffer size definitions - designed to accommodate modern versioning schemes */
#define RDM_PKG_NAME_MAX_SIZE							128
#define RDM_PKG_VERSION_MAX_SIZE						8
#define RDM_PKG_INST_PATH_MAX_SIZE						256

/*! Published Events from rdm manager  */
typedef enum _RDMMgr_EventId_t {
	IARM_BUS_RDMMGR_EVENT_APPDOWNLOADS_CHANGED = 0,		/*!< RDM application download status chnaged*/
	IARM_BUS_RDMMGR_EVENT_APP_INSTALLATION_STATUS,    /*!< RDM application installation status */
	IARM_BUS_RDMMGR_EVENT_MAX				/*!< Max Event Id */
} IARM_Bus_RDMMgr_EventId_t;


typedef enum _RDMMgr_Status_t {
	RDM_PKG_INSTALL_COMPLETE = 0,
	RDM_PKG_INSTALL_ERROR, // 1
	RDM_PKG_DOWNLOAD_COMPLETE, // 2
	RDM_PKG_DOWNLOAD_ERROR, // 3
	RDM_PKG_EXTRACT_COMPLETE, // 4
	RDM_PKG_EXTRACT_ERROR, // 5
	RDM_PKG_VALIDATE_COMPLETE, // 6
	RDM_PKG_VALIDATE_ERROR, // 7
	RDM_PKG_POSTINSTALL_COMPLETE, // 8
	RDM_PKG_POSTINSTALL_ERROR, // 9
	RDM_PKG_UNINSTALL, // 10
	RDM_PKG_INVALID_INPUT // 11
} IARM_RDMMgr_Status_t;


/*
 * RDM Package Information Structure
 * 
 * Security Notes - CRITICAL for buffer overflow prevention:
 * 
 * 1. Input Validation Required:
 *    - Always validate input length before copying to these buffers
 *    - Use strlen() to check input length against MAX_SIZE constants
 * 
 * 2. Safe String Operations:
 *    - Use strncpy() with proper null termination:
 *      strncpy(pkg_info.pkg_name, input, RDM_PKG_NAME_MAX_SIZE - 1);
 *      pkg_info.pkg_name[RDM_PKG_NAME_MAX_SIZE - 1] = '\0';
 *    
 *    - Use snprintf() for formatted strings:
 *      snprintf(pkg_info.pkg_version, RDM_PKG_VERSION_MAX_SIZE, "%s", input);
 * 
 * 3. Validation Example:
 *    if (strlen(input_name) >= RDM_PKG_NAME_MAX_SIZE) {
 *        return RDM_PKG_INVALID_INPUT;
 *    }
 * 
 * WARNING: Failure to validate input can lead to buffer overflows
 *          in critical package management operations.
 */
typedef struct _RDMMgr_EventData_t {
	struct _pkg_info {
	    char pkg_name[RDM_PKG_NAME_MAX_SIZE];           /* Package name buffer - validate input length */
	    char pkg_version[RDM_PKG_VERSION_MAX_SIZE];     /* Version string buffer - supports semantic versioning */
	    char pkg_inst_path[RDM_PKG_INST_PATH_MAX_SIZE]; /* Installation path buffer - validate path length */
	    IARM_RDMMgr_Status_t pkg_inst_status;           /* Installation status enum */
	} rdm_pkg_info;
} IARM_Bus_RDMMgr_EventData_t;


#ifdef __cplusplus
}
#endif

#endif /* IARMMGRS_RDM_MGR_H */

/* End of IARM_BUS_RDMMGR_API doxygen group */
/**
 * @}
 */



/** @} */
/** @} */
