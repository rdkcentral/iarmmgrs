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
* @file irMgr.h
*
* @brief IARM-Bus IR Manager API.
*
 * This API defines the structures and functions for the 
 * IARM-Bus IR Manager interface.
*
* @par Document
* Document reference.is
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

/**
 * @defgroup IARM_MGR IARM Manager
 * IARM (Inter Application Resource Management) is a mechanism for interprocess communication
 * among different RDK applications. Its a platform agnostic inter process
 * communication (IPC) mechanism for the other RDK components. The inter communication is
 * possible between IARM application using events and RPC method.
 *
 * @image html iarm.png
 *
 * Basically there are three IARM entities involved for development
 * <ol>
 * <li> IARM Bus
 * <li> IARM Application (Consumers/Listeners)
 * <li> IARM Manager (Publishers)
 * </ol>
 *
 * @par IARM Bus
 * - Invoke methods in other processes via Remote Procedure Call.
 * - Send inter-process messages.
 * - Manage shared and exclusive access to resources.
 * - Register for event notification.
 * - Publish Event notification to registered listener.
 * - There are two ways for application to use IARM Bus is 'Subscribe for system events' and 'Invoke RPC Methods'
 *
 * @par IARM Application
 * - IARM Application utilize the IARM utilities and it register for event notification.
 * - For example, IR Application registering for the events and the same event appears
 * in the IARM Bus. So that the notification of the IARM Bus event shall be transfer
 * to the IARM Bus application.
 * - IARM application can also invoke the RPC method which has been registered by the other IARM process.
 *
 * @par IARM Manager
 * - IARM Manager is an IARM Application that runs as Linux daemon process.
 * - The IARM Bus Daemon is a Manager Component with Special privileges to manage resources.
 * - The Other IARM Manager components include Power Manager, IR Manager, Disk Manager, Sys Manager, DS Manager, etc
 *
 * @par IARM Publisher and Listeners Concept
 * @image html iarm-publisher.png
 *
 * @defgroup IARM_MGR_RPC Events and Remote Procedure Calls
 * IARM-Bus is a platform agnostic Inter-process communication (IPC) interface. It allows
 * applications to communicate with each other by sending Events or invoking Remote
 * Procedure Calls. The common programming APIs offered by the RDK IARM-Bus interface is
 * independent of the operating system or the underlying IPC mechanism.
 *
 * Two applications connected to the same instance of IARM-Bus are able to exchange events
 * or RPC calls. On a typical system, only one instance of IARM-Bus instance is needed. If
 * desired, it is possible to have multiple IARM-Bus instances. However, applications
 * connected to different buses will not be able to communicate with each other.
 * @ingroup IARM_MGR
 */

/**
 * @defgroup IARMBUS_IR_MGR IR Manager
 * @ingroup IARM_MGR_RPC
 *
 * IR Manager is an application that publishes Remote Key events to other applications.
 * This manager receives IR signals from driver and dispatch it to all registered listeners on IARM Bus.
 * IR manager sends these IR events to other applications.
 *
 * @par IR Manager: Events
 *
 * - @b IARM_BUS_IRMGR_EVENT_IRKEY
 * @n The Event Data contains Key Code and Key Type of the pressed IR key.
 * This enum is used to notify IR Key events.
 * @n Example:
 * @code
 * IARM_Bus_IRMgr_EventData_t eventData;
 * eventData.data.irkey.keyType = KET_KEYUP;
 * eventData.data.irkey.keyCode = prevEventData.data.irkey.keyCode;
 * IARM_Bus_BroadcastEvent(IARM_BUS_IRMGR_NAME, (IARM_EventId_t)IARM_BUS_IRMGR_EVENT_IRKEY,
 *                          (void *)&eventData, sizeof(eventData));
 * @endcode
 * @n
 * - @b IARM_BUS_RF4CEMGR_EVENT_KEY
 * @n The RF4CE fetches the user key and that will pass to UEI's quickset application, that will then fetch the
 * requested TV's IR codeset. UEI's fetch mechanism can be local or non local (Cloud based). Once the requested
 * TV IR codeset has been retrieved , it will then be programmed into the remote via API calls.
 * Ultimately, this will allow the XR device to control the following keys on a given TV. It support for the various
 * keys such as OK, TV input, Volume UP, Volume Down, Mute Toggle power, Discrete Power ON, Discrete Power OFF, and so on.
 * @code
 * IARM_Bus_RegisterEventHandler(IARM_BUS_RF4CEMGR_NAME, IARM_BUS_RF4CEMGR_EVENT_KEY, _rf4ceEventHandler); //Register Callback func
 * void _rf4ceEventHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len); //Function prototype
 * @endcode
 *
 * @par IR Manager: RPC Methods
 *
 * IR manager publishes two RPC Methods such as <b>SetRepeatKeyInterval</b> and <b>GetRepeatKeyInterval</b>.
 * Other application can invoke these methods to determine how often a repeat key is sent when the IR key is
 * held own by the user.
 * @n @n
 * - @b IARM_BUS_IRMGR_API_SetRepeatInterval
 * @n This API is used to set the repeat key event interval.
 * @n Example:
 * @code
 * IARM_Bus_IRMgr_SetRepeatInterval_Param_t param;
 * param. timeout = 200; //200 milisec
 * IARM_Bus_Call(IARM_BUS_IRMGR_NAME, IARM_BUS_IRMGR_API_SetRepeatInterval,
 *               (void *)&param, siizeof(param));
 * @endcode
 * @n @n
 * - @b IARM_BUS_IRMGR_API_GetRepeatInterval
 * @n This is used to retrieves the current key repeat interval.
 * @n Example:
 * @code
 * IARM_Bus_IRMgr_SetRepeatInterval_Param_t param;
 * IARM_Bus_Call (IARM_BUS_IRMGR_NAME, IARM_BUS_IRMGR_API_GetRepeatInterval, \
 *                  (void *)&param, sizeof(param));
 * @endcode
 *
 * @par How to register callback function with HAL to receive the IR keys
 * This function must initialize all the IR specific user input device modules.
 * @code
 * PLAT_API_RegisterIRKeyCallback(PLAT_IrKeyCallback_t func);
 * @endcode
 *
 * @par Callback prototype
 * The event data contains Key type and Key code of the pressed IR Key.
 * @code
 * typedef void (*PLAT_IrKeyCallback_t)(int keyType, int keyCode);
 * @endcode
 *
 */


/**
* @defgroup iarmmgrs
* @{
* @defgroup ir
* @{
**/

/** @addtogroup IARM_BUS_IRMGR_API IARM-Bus IR Manager API.
 *  @ingroup IARM_BUS
 *
 *  Described herein are the IARM-Bus types and functions that are part of the IR
 *  Manager application. The IR Manager application manages user input.
 *
 *  @{
 */




/**
* @defgroup iarmmgrs
* @{
* @defgroup ir
* @{
**/


#ifndef _IARM_BUS_IRMGR_H
#define _IARM_BUS_IRMGR_H

#include "libIARM.h"

#ifdef __cplusplus
extern "C" 
{
#endif
#define IARM_BUS_IRMGR_NAME 						"IRMgr" /*!< IR Manager IARM -bus name */

/*! Events published from IR Mananger */
typedef enum _IRMgr_EventId_t {
	IARM_BUS_IRMGR_EVENT_IRKEY,           /*!< Key Event  */
    IARM_BUS_IRMGR_EVENT_CONTROL,
    IARM_BUS_IRMGR_EVENT_MAX,             /*!< Maximum event id*/
} IARM_Bus_IRMgr_EventId_t;


/*! Possible IR Manager Key Source */
typedef enum _IRMgr_KeySrc_t {
	IARM_BUS_IRMGR_KEYSRC_FP,	/*!< Key Source FP */
	IARM_BUS_IRMGR_KEYSRC_IR,	/*!< Key Source IR */
	IARM_BUS_IRMGR_KEYSRC_RF,	/*!< Key Source RF */
} IARM_Bus_IRMgr_KeySrc_t;


/*! Key Event Data */
typedef struct _IRMgr_EventData_t {
    union {
        struct _IRKEY_DATA{
        	/* Declare Event Data structure for IRMGR_EVENT_DUMMY0 */
            int keyType;              /*!< Key type (UP/DOWN/REPEAT) */
            int keyCode;              /*!< Key code */ 
            int keyTag;               /*!< key tag (identifies which remote) */
            int keyOwner;             /*!< key owner (normal or pairing op) */
			int isFP;					/*!< Key Source -- Remote/FP - TBD - Need to be removed after changing MPEOS*/ 
			IARM_Bus_IRMgr_KeySrc_t keySrc;					/*!< Key Source -- FP,IR,Remote */
            unsigned int keySourceId;
        } irkey, fpkey;
    } data;
}IARM_Bus_IRMgr_EventData_t;

/*
 * Declare RPC API names and their arguments
 */
#define IARM_BUS_IRMGR_API_SetRepeatInterval 		"SetRepeatInterval" /*!< Set the repeat key event interval*/

/*! Declare Argument Data structure for SetRepeatInterval */
typedef struct _IARM_Bus_IRMgr_SetRepeatInterval_Param_t {
	
	unsigned int timeout;           /*!< Repeat key interval timeout*/
} IARM_Bus_IRMgr_SetRepeatInterval_Param_t;

#define IARM_BUS_IRMGR_API_GetRepeatInterval 		"GetRepeatInterval" /*!< Retrives the current repeat interval*/

/*! Declare Argument Data structure for GetRepeatInterval */
typedef struct _IARM_Bus_IRMgr_GetRepeatInterval_Param_t {
	unsigned int timeout;          /*!< Repeat key interval timeout value retrived*/
} IARM_Bus_IRMgr_GetRepeatInterval_Param_t;

#ifdef __cplusplus
}
#endif
#endif

/* End of IARM_BUS_IRMGR_API doxygen group */
/**
 * @}
 */


/** @} */
/** @} */
