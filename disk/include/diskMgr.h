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
* @file diskMgr.h
*
* @brief IARM-Bus Disk Manager Public API.
*
* This API defines the public interfaces for Disk manager
*
* @par Document
* Document reference.
*
* @par Open Issues (in no particular order)
* -# None
*
* @par Assumptions
* -# None
*
* @par Abbreviations
* - BE:       ig-Endian.
* - cb:       allback function (suffix).
* - DS:      Device Settings.
* - FPD:     Front-Panel Display.
* - HAL:     Hardware Abstraction Layer.
* - LE:      Little-Endian.
* - LS:      Least Significant.
* - MBZ:     Must be zero.
* - MS:      Most Significant.
* - RDK:     Reference Design Kit.
* - _t:      Type (suffix).
*
* @par Implementation Notes
* -# None
*
*/

/** @defgroup IARM_BUS IARM-Disk Manager API
*   @ingroup IARM_RDK
*
*  IARM-Bus is a platform agnostic Inter-process communication (IPC) interface. It allows
*  applications to communicate with each other by sending Events or invoking Remote
*  Procedure Calls. The common programming APIs offered by the RDK IARM-Bus interface is
*  independent of the operating system or the underlying IPC mechanism.
*
*  Two applications connected to the same instance of IARM-Bus are able to exchange events
*  or RPC calls. On a typical system, only one instance of IARM-Bus instance is needed. If
*  desired, it is possible to have multiple IARM-Bus instances. However, applications
*  connected to different buses will not be able to communicate with each other.
*/

/** @todo complete the implementation with adding required RPCs.
*/

/** @addtogroup IARM_BUS_DISKMGR_API IARM-Disk Manager API.
*  @ingroup IARM_BUS
*
*  Described herein are the Disk Manager events and functions.
*
*  @{
*/




/**
* @defgroup iarmmgrs
* @{
* @defgroup disk
* @{
**/


#ifndef _IARM_BUS_DISKMGR_H
#define _IARM_BUS_DISKMGR_H

#include "libIARM.h"

#ifdef __cplusplus
extern "C" 
{
#endif

#define IARM_BUS_DISKMGR_NAME 						"DISKMgr" /*!< IARM bus name for Disk manager */
#define IARM_BUS_DISKMGR_MAX_LEN					200       /*!< Maximum length for strings in the module */

    /*! Published Events from disk manager  */
    typedef enum _DISKMgr_EventId_t {
        IARM_BUS_DISKMGR_EVENT_HWDISK,    /*!< Harddisk event */
        IARM_BUS_DISKMGR_EVENT_EXTHDD,    /*!< External disk event */
        IARM_BUS_DISKMGR_EVENT_MAX        /*!< Max event id */
    } IARM_Bus_DISKMgr_EventId_t;

    /*! Type of event related to disks */    
    typedef enum _DISKMgr_HDDEvents_t {
        DISKMGR_EVENT_EXTHDD_ON,        /*!< Disk is on */
        DISKMGR_EVENT_EXTHDD_OFF,       /*!< Disk is off */
        DISKMGR_EVENT_EXTHDD_PAIR       /*!< Disk is paired */
    } DISKMgr_HDDEvents_t;

    /*! Data associated with disk events */
    typedef struct _IARM_BUS_DISKMgr_EventData_t{
        IARM_Bus_DISKMgr_EventId_t id;              /*!< Harddisk or external disk event*/
        char eventType;                             /*!< DiskMgr_HDDEvents_t event types*/
        char status;							    /*!< Disk added/paired/removed */
        char model[IARM_BUS_DISKMGR_MAX_LEN];       /*!< Disk model*/
        char modelNumber[IARM_BUS_DISKMGR_MAX_LEN]; /*!< Model number of the disk*/
        char serialNum[IARM_BUS_DISKMGR_MAX_LEN];   /*!< Serial number of the disk*/
        char devicePath[IARM_BUS_DISKMGR_MAX_LEN];  /*!< Device node path*/
        char mountPath[IARM_BUS_DISKMGR_MAX_LEN];   /*!< Path to which disk is mounted*/
    }IARM_BUS_DISKMgr_EventData_t;

#ifdef __cplusplus
}
#endif

#endif

/* End of IARM_BUS_DISKMGR_API doxygen group */
/**
 * @}
 */



/** @} */
/** @} */
