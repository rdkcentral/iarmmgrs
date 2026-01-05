/*
 * If not stated otherwise in this file or this component's LICENSE file the
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


#ifndef IARMMGRS_MAINTENANCE_MGR_H
#define IARMMGRS_MAINTENANCE_MGR_H

/*
 * @file maintenanceMGR.h
 * @brief Maintenance Manager IARM Bus Interface Definitions
 * 
 * This is a header-only interface definition for IARM bus communication
 * with maintenance manager services. It provides event types and data
 * structures for inter-process communication but does not implement
 * the actual maintenance logic.
 * 
 * The maintenance manager implementation is typically provided by:
 * - RDK services (rdkservices/MaintenanceManager)
 * - Platform-specific maintenance daemons
 * - Third-party maintenance components
 * 
 * @note This header defines the communication interface only.
 */

#define IARM_BUS_MAINTENANCE_MGR_NAME       "MaintenanceMGR"

/* Event types for maintenance manager communication via IARM bus */
typedef enum {
    IARM_BUS_MAINTENANCEMGR_EVENT_UPDATE=0, /* Event status as data */
    IARM_BUS_DCM_NEW_START_TIME_EVENT,   /* Payload as Time */
}IARM_Bus_MaintMGR_EventId_t;

/* Notification to rdkservice over IARM */
typedef enum {
    MAINT_DCM_COMPLETE=0,
    MAINT_DCM_ERROR, //  1
    MAINT_RFC_COMPLETE, // 2
    MAINT_RFC_ERROR, // 3
    MAINT_LOGUPLOAD_COMPLETE, // 4
    MAINT_LOGUPLOAD_ERROR, // 5
    MAINT_PINGTELEMETRY_COMPLETE, // 6
    MAINT_PINGTELEMETRY_ERROR, // 7
    MAINT_FWDOWNLOAD_COMPLETE, //8
    MAINT_FWDOWNLOAD_ERROR, //9
    MAINT_FWDOWNLOAD_ABORTED, //10
    MAINT_CRITICAL_UPDATE, // 11
    MAINT_REBOOT_REQUIRED, //12
    MAINT_DCM_INPROGRESS, //13
    MAINT_RFC_INPROGRESS, //14
    MAINT_FWDOWNLOAD_INPROGRESS, //15
    MAINT_LOGUPLOAD_INPROGRESS, //16
    MAINT_STATUS_EMPTY //17
} IARM_Maint_module_status_t;

#define MAX_TIME_LEN 32

/*
 * Security Note: When populating start_time buffer, always ensure:
 * 1. Input validation: strlen(input) < MAX_TIME_LEN
 * 2. Use safe string functions: strncpy() with proper null termination
 * 3. Example safe usage:
 *    strncpy(eventData.data.startTimeUpdate.start_time, input, MAX_TIME_LEN - 1);
 *    eventData.data.startTimeUpdate.start_time[MAX_TIME_LEN - 1] = '\0';
 */

/*
 * Event Data for holding the start time and module status
 * 
 * Memory Layout Notes:
 * - Union ensures both structs share same memory space
 * - Padding added to ensure consistent alignment across platforms
 * - Total union size is MAX_TIME_LEN bytes (largest member)
 */
typedef struct {
    union{
        struct _DCM_DATA{
            char start_time[MAX_TIME_LEN]; /* Buffer for time string - ensure bounds checking */
            /* Implicit padding to MAX_TIME_LEN already provided by char array */
        }startTimeUpdate;
        struct _MAINT_STATUS_UPDATE{
            IARM_Maint_module_status_t status;
            char _padding[MAX_TIME_LEN - sizeof(IARM_Maint_module_status_t)]; /* Explicit padding for alignment */
        }maintenance_module_status;
    } data;
}IARM_Bus_MaintMGR_EventData_t;


#endif /* IARMMGRS_MAINTENANCE_MGR_H */
