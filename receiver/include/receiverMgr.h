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
 * @file receiverMgr.h
 * @brief XRE Receiver Manager IARM Bus Interface
 * 
 * @section Purpose
 * This header defines the XRE (Xfinity Receiver Experience) receiver manager
 * interface for IARM bus communication. It provides data structures and 
 * definitions for managing application information, network connections, and
 * diagnostic capabilities in XRE receiver environments.
 * 
 * @section Architecture
 * This is an interface-only header that defines the communication protocol
 * for XRE receiver operations. The actual implementation is typically provided by:
 * - XRE native receiver applications
 * - Platform-specific receiver daemons
 * - RDK XRE service components
 * 
 * @section Security
 * CRITICAL: This component handles network communication and must implement
 * strict input validation. All network parameters and JSON data must be
 * validated before processing to prevent buffer overflow attacks and
 * injection vulnerabilities.
 * 
 * @section Usage
 * Components use this interface to:
 * - Retrieve application information via JSON
 * - Manage network connection parameters
 * - Control diagnostic features
 * - Coordinate XRE receiver operations
 * 
 * @note This header defines the communication interface only. Implementation
 *       must be provided by XRE receiver components.
 */

#ifndef IARMMGRS_RECEIVER_MGR_H
#define IARMMGRS_RECEIVER_MGR_H


#ifdef __cplusplus
extern "C"
{
#endif

/*
 * XRE Receiver Manager - Network Communication Interface
 * 
 * CRITICAL SECURITY NOTES:
 * This component handles network communication for XRE (Xfinity Receiver Experience).
 * All buffers must be validated to prevent buffer overflow attacks from network sources.
 * 
 * Buffer Security Guidelines:
 * 1. Always validate input length before copying to ANY buffer
 * 2. Use secure string functions (strncpy, snprintf) with proper null termination
 * 3. Never trust network input - treat all external data as potentially malicious
 * 4. Implement rate limiting for network operations
 */

#define IARM_RECEIVER_NAME "XRE_native_receiver"

#define IARM_RECEIVER_GET_APP_INFO "XRE_RECEIVER_getAppInfo"
#define IARM_RECEIVER_SET_CONNECTION_RESET "XRE_RECEIVER_setConnectionReset"
#define IARM_RECEIVER_IS_MINI_DIAGNOSTICS_ENABLED "XRE_RECEIVER_isMiniDiagnosticsEnabled"

/* JSON Application Information Buffer */
#define APP_INFO_JSON_MAX 4096

/*
 * SECURITY WARNING: Large JSON buffer for network communication
 * 
 * Safe Usage Pattern:
 *   if (strlen(json_input) >= APP_INFO_JSON_MAX) {
 *       return -1; // Reject oversized input
 *   }
 *   strncpy(appData.appInfoJson, json_input, APP_INFO_JSON_MAX - 1);
 *   appData.appInfoJson[APP_INFO_JSON_MAX - 1] = '\0';
 */
typedef struct _AppInfoData {
    char appInfoJson[APP_INFO_JSON_MAX]; /* JSON buffer - MUST validate input length */
} AppInfoData;

/* Network Connection Parameters Buffer Size */
#define CONNECTION_ID_MAX 1024

/*
 * SECURITY CRITICAL: Network connection parameter validation
 * 
 * These buffers store network identifiers that could be exploited if not validated.
 * All connection parameters MUST be validated for:
 * - Length (< CONNECTION_ID_MAX)
 * - Character set (alphanumeric + allowed symbols only)
 * - Format compliance (no injection patterns)
 * 
 * Safe Usage Example:
 *   if (validate_connection_param(input_id, CONNECTION_ID_MAX) != 0) {
 *       return -1; // Reject invalid input
 *   }
 *   strncpy(resetData.applicationID, input_id, CONNECTION_ID_MAX - 1);
 *   resetData.applicationID[CONNECTION_ID_MAX - 1] = '\0';
 */
typedef struct _ConnectionResetData {
    char applicationID[CONNECTION_ID_MAX];      /* App identifier - validate format and length */
    char connectionID[CONNECTION_ID_MAX];       /* Connection ID - validate format and length */
    char connectionResetLevel[CONNECTION_ID_MAX]; /* Reset level - validate against allowed values */
} ConnectionResetData;

/*
 * XRE Receiver Parameter Union
 * 
 * Memory Layout Considerations:
 * - AppInfoData: 4096 bytes (largest member)
 * - ConnectionResetData: 3072 bytes (3 * 1024)
 * - bool: typically 1 byte
 * 
 * The union size will be 4096 bytes (size of largest member).
 * Proper alignment is ensured by the compiler for the largest member.
 */
typedef struct _IARM_Bus_Receiver_Param_t {
    union {
        AppInfoData appInfoData;             /* 4096 bytes - largest member, determines union size */
        ConnectionResetData connectionResetData; /* 3072 bytes - padded to union size */
        struct {
            bool isMiniDiagnosticsEnabled;   /* 1 byte + padding to union size */
            char _padding[APP_INFO_JSON_MAX - sizeof(bool)]; /* Explicit padding for consistency */
        } diagnosticsData;
    } data;
    int status;                              /* Return status for operations */
} IARM_Bus_Receiver_Param_t;

#ifdef __cplusplus
}
#endif

#endif /* IARMMGRS_RECEIVER_MGR_H */



