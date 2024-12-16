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
////////////////////////////////////////////////////////////////////////////////////
//
// Device Update Manager API
//
// Name: iarm_device_update.h
// Date: 07/24/14
// Author: Sal
//
// This file contains the IARM API definition used by the Device Update Manager, the Device Update Proxy and, potentially,
// an Update Master to find, negotiate, and execute updating an image on a device.
//
// ** Entities **
//
// Device Update Manager - The primary module that is responsible for discovering and managing updating images on various
//                         devices.  On initialization or at some configured times, the Device Update manager will scan
//                         the update sub-filesystem of the rootfs for new images and announce the found images on the IARM
//                         bus.  This can be extended to communicating with a server to identify updates on an update server
//                         that can be pulled down and announced. The Device Update Manager will also be able to aggregate
//                         data about devices that are downloaded and downloaded sessions that are executed.  It will be
//                         able to gate when downloads go to devices and when they are loaded on devices.  And it will be
//                         able to officiate between multiple Device Update Proxy's requesting downloads over low bandwidth
//                         or highly utilized mediums.
//
// Device Update Proxy - Each class of devices will have a single Device Update Proxy responsible for managing updates to
//                       its child devices.  A Device Update Proxy can represent multiple devices as long as they are in
//                       the same class. An example of this is remote controls. The Device Update Proxy will be responsible
//                       for listening for announcements and accepting updates for specific devices that are ready for an
//                       update.  It is assumed the Device Update Proxy will know the version of devices it managers and can
//                       either contact them to notify them of the update or receive polls from them asking about an update.
//                       The Device Update Proxy is responsible for understanding the physical transport connecting devices
//                       to the STB and whether downloads to those devices must be serialized or can be parallelized (and
//                       how many can be parallelized). The Device Update Manager can also restrict parallelized downloads
//                       across the same or different transports. The Device Update Proxy is responsible for accepting
//                       download and load events, informing on the status of these events and negotiating the download with
//                       the device over the device specific protocol.
//
// Update Master - The system can be figured with a config file to allow the Device Update Manager to fully manage device
//                 update activities or to have a third entity, called the Update Master, to initiate either/or downloads
//                 and loads. An example of an Update Master would be a UI which would query the user if they wanted to
//                 load and/or download the code. In a typical usage, the UI would gate on the download, then perform an
//                 immediate download followed by an automatic load or would perform a background download followed by a
//                 gated load.
//
// ** IDs **
//
// deviceID - ID of a particular device marked for download by its Device Update Proxy. The device ID is assigned by the
//            Device Update Proxy and is unique only within that Device Update Proxy.
//
// updateSessionID - The update of a particular image tied to a particular device is described and tracked by the
//                   updateSessionID. All calls after an update has been accepted for a device is tracked by this
//                   session ID.
//
// ** Standard Call/Event sequences **
//
// Background Download and Load during Non-usage Time (middle of night)
//
// #define IARM_BUS_DEVICE_UPDATE_MASTER_DOWNLOAD_ENABLED   0
// #define IARM_BUS_DEVICE_UPDATE_MASTER_LOAD_ENABLED	    0
//
// 1) Device Update Manager performs check and finds new device image in update sub-filesystem in the rootfs.
// 2) Device Update Manager sends an ANNOUNCE event with details of the new image
// 3) The Device Update Proxy receives the Announce, decides a device it manages needs it and calls AcceptUpdate to
//    register its intent with the Device Update Manager and get an updateSessionID.
// 4) When the Device Update Proxy negotiates with its managed device that a download is ready and confirms the device
//    is ready and able to receive the download, it sends the READY_TO_DOWNLOAD event with all of the relevant information
//    about the download it is taking.
// 5) The Device Update Manager sends a DOWNLOAD_INITIATE event to begin the download with backgroundDownload set to true.
// 6) The Device Update Proxy sends DOWNLOAD_STATUS events back for 0, the requested percent increments, and finally 100.
// 7) If, say, its 8 PM, the Device Update Manager sends a LOAD_INITIATE event with a timeToLoad of 25200 and a timeAfterInactive
//    of 300 seconds which will put the download happening at 3 AM and after 5 minute of inactivity (just in case the user is
//    still using it.
// 8) At 3:05 AM (presumably and approximately), the device begins the load causing the Device Update Proxy to send a LOAD_STATUS
//    of BEGIN. The device does what it needs to do to load the new image, notifying the Device Update Proxy when done, which then
//    sends ao LOAD_STATUS of END. This will complete the session.
//
// User confirmed Immediate Download and Load
//
// #define IARM_BUS_DEVICE_UPDATE_MASTER_DOWNLOAD_ENABLED   1
// #define IARM_BUS_DEVICE_UPDATE_MASTER_LOAD_ENABLED	    0
//
// 1) Device Update Manager performs check and finds new device image in update sub-filesystem in the rootfs.
// 2) Device Update Manager sends an ANNOUNCE event with details of the new image
// 3) The Device Update Proxy receives the Announce, decides a device it manages needs it and calls AcceptUpdate to
//    register its intent with the Device Update Manager and get an updateSessionID.
// 4) When the Device Update Proxy negotiates with its managed device that a download is ready and confirms the device
//    is ready and able to receive the download, it sends the READY_TO_DOWNLOAD event with all of the relevant information
//    about the download it is taking.
// 5) A UI agent would be listening for this event and would pop up a window asking the user if he wants to download X update
//    for Y device.
// 6) The user would click yes and the UI agent would send a DOWNLOAD_INITIATE event to begin the download with backgroundDownload
//    set to false with increments as desired to update status bar on the screen.
// 7) The Device Update Proxy sends DOWNLOAD_STATUS events back for 0, the requested percent increments, and finally 100. The UI
//    agent would receive these events and update the status bar on the screen appropriately (also accounting there will be a load
//    at the end).
// 8) When the Device Update Manager sees the 100% DOWNLOAD_STATUS, it would send a LOAD_INITIATE event with a timeToLoad of 0 and
//    a timeAfterInactive of 0 causing the load to Immediately happen
// 9) The device immediately begins the load causing the Device Update Proxy to send a LOAD_STATUS
//    of BEGIN. The device does what it needs to do to load the new image, notifying the Device Update Proxy when done, which then
//    sends ao LOAD_STATUS of END. This will complete the session.
//
//
////////////////////////////////////////////////////////////////////////////////////


/**
* @defgroup iarmmgrs
* @{
* @defgroup deviceUpdateMgr
* @{
**/


#define IARM_BUS_DEVICE_UPDATE_NAME             "DeviceUpdateManager" /*!< IARM BUS  name for Device Update Manager */

#define IARM_BUS_DEVICE_UPDATE_PATH_LENGTH     512
#define IARM_BUS_DEVICE_UPDATE_ERROR_LENGTH    256
#define IARM_BUS_DEVICE_UPDATE_VERSION_LENGTH   32
#define IARM_BUS_DEVICE_UPDATE_DEVICE_NAME_LENGTH 64


//
// Update Master Control
//
// These defines should be moved into a config file in the implementation.  They are used to indicate if the Device
// Update Manager should allow an Update Master, such as the XRE receiver, to initiate either the download or the load.
// This allows the Device Update Manager to defer to a UI based trigger to initiate either a full download or simply a load
// without a Device Update Proxy having any knowledge of the UI and without a code update to the Device Update Manager.
//
#define IARM_BUS_DEVICE_UPDATE_MASTER_DOWNLOAD_ENABLED   0
#define IARM_BUS_DEVICE_UPDATE_MASTER_LOAD_ENABLED	     0

//
// IARM_BUS_DEVICE_UPDATE_API_AcceptUpdate
//
// When a Device Update Proxy has received an announcement that it wants to use to update a device it manages. It
// will call the Device Update Manager with the file path of the update it is interested in, the deviceID it wants to
// update and will receive an updateSessionID that will identify the rest of the transactions in the download.  A Device Update
// Proxy must call the AcceptUpdate RPC BEFORE it sends a ReadyToInstall event.  A Device Update Proxy can call
// AcceptUpdate immediately for all of its devices or it can wait until it wants to download for each one.  It should not send
// the ReadyToDownload event until it is ready to immediately download the device represented by the updateSessionID.
//
// deviceImageFilePath - IN: The download announced by the Device Update Manager when a new image is discovered in the filesystem.
//	                         Allows the particular download to be identified and tracked.
// 
// deviceID - IN: ID of the device accepting the download. Unique only amount devices of a particular type.
//
// updateSessionID - OUT: ID of the download session from here until final loading.
//
// interactiveDownload - OUT: Indicates if the UI will be involved in the download. The DUP will know to poll the device on every
//                            keypress.
//
// interactiveLoad - OUT: Indicates if the UI will be involved in the load. The DUP will know to poll the device on every
//                            keypress.
//
#define IARM_BUS_DEVICE_UPDATE_API_AcceptUpdate "AcceptUpdate"

 typedef struct _IARM_Bus_DeviceUpdate_AcceptUpdate_Param_t
 {
     char		   deviceImageFilePath[IARM_BUS_DEVICE_UPDATE_PATH_LENGTH];
     unsigned char deviceID;
     unsigned int  updateSessionID;
     unsigned char interactiveDownload;
     unsigned char interactiveLoad;

 } IARM_Bus_DeviceUpdate_AcceptUpdate_Param_t;


typedef enum _DEVICE_Update_EventId_t
{
    IARM_BUS_DEVICE_UPDATE_EVENT_ANNOUNCE = 0,
    IARM_BUS_DEVICE_UPDATE_EVENT_READY_TO_DOWNLOAD,
    IARM_BUS_DEVICE_UPDATE_EVENT_DOWNLOAD_INITIATE,
	IARM_BUS_DEVICE_UPDATE_EVENT_DOWNLOAD_STATUS,
	IARM_BUS_DEVICE_UPDATE_EVENT_LOAD_INITIATE,
	IARM_BUS_DEVICE_UPDATE_EVENT_LOAD_STATUS,
    IARM_BUS_DEVICE_UPDATE_EVENT_ERROR,
    IARM_BUS_DEVICE_UPDATE_EVENT_MAX

} IARM_Bus_DEVICE_Update_EventId_t;


//
// IARM_Bus_Device_Update_LoadDelayType_t
//
// LOAD_DEFAULT - Allows the Master or the Device Update Manager to defer to the Device Proxy to determine how to load.
//
// LOAD_NORMAL - Allows the Master or the Device Update Manager to defer to a number of seconds in the future as well as
//               a time after inactivity.  This setting will drive two other parameters: timeToLoad and timeAfterInactivity.
//               If the LoadDelayType is LOAD_NORMAL then the device must wait timeToLoad seconds where the load does
//               not happen.  Immediately after the timeToLoad timer expires, if the timeAfterInactivity is non-zero, the
//               device must wait timeAfterInactivity seconds after all activity on the remote and then attempt to load the
//               image.  If the sender wants the remote to load immediately, it will simply set both time parameters to 0.
//
// LOAD_POLL - Indicates that the device should poll frequently for new instructions on whether or not to load.  For example,
//             a remote control would poll every keypress.  Another device could poll once a second.
//
// LOAD_ABORT - Allows a caller to abort the download on the device if the device is currently waiting for some time or in a
//              polling mode. If abort is called the session is considered over and the updateSessionID is considered invalid.
//
typedef enum _IARM_Bus_Device_Update_LoadDelayType_t
{
    LOAD_DEFAULT,
    LOAD_NORMAL,
    LOAD_POLL,
    LOAD_ABORT

} IARM_Bus_Device_Update_LoadDelayType_t;


//
// IARM_Bus_Device_Update_LoadDelayType_t
//
// LOAD_STATUS_BEGIN - Notifies when the load has commenced on the device.
//
// LOAD_STATUS_END - Notifies that the load has completed and the device is updated and running the new image.  Receiving
//                   this event ends the update and invalidates the updateSessionID
//
typedef enum _IARM_Bus_Device_Update_LoadStatusType_t
{
    LOAD_STATUS_BEGIN,
    LOAD_STATUS_END,

} IARM_Bus_Device_Update_LoadStatusType_t;

//SALTODO: Revisit these.
typedef enum _IARM_Bus_Device_Update_ErrorType_t
{
    IMAGE_NOT_FOUND, // The firmware that was announced could not be found
    IMAGE_INVALID, // The firmware was determined to be invalid prior to sending
    IMAGE_REJECTED, // The firmware was rejected after sending, eg. CRC check etc...
    IMAGE_TIMEOUT, // The request timed out
    IMAGE_GENERIC_ERROR, // See the error message for specific details
    IMAGE_BUSY  //SALADD: If a 2nd update is attempted while one in progress?

} IARM_Bus_Device_Update_ErrorType_t;


//
// IARM_Bus_DeviceUpdate_Announce_t
//
// Event: IARM_BUS_DEVICE_UPDATE_EVENT_ANNOUNCE
//
// The Device Update Manager will scan the filesystem for image updates for different devices at configurable times.  When a new
// image is found, the Device Update Manager will send an Announce event for the particular image on the IARM bus.  A Device Proxy
// for an updatable device should register for this event and look for updates meant for it.  The Device Update Proxy is responsible for
// managing all devices it is proxying for to update each in a reasonable way.  An example is a Remote Control Proxy which has
// multiple entries in its pairing table.  It should take care to update them in a metered way, i.e., one at a time, or all
// background downloads, etc.
//
// deviceName - Device Name pulled from the XML file of the received device update package. Devices will know what name they are
//              listening for and want information about.
//
// deviceImageVersion - Version of the new image to update.
//
// deviceImageType - Type of the new image. Pulled from the XML file of the received device update package.
//
// deviceImageFilePath - Absolute path of the image in the rootFS.
//
// forceUpdate - Boolean describing if the device update package is requesting to be forced onto devices regardless of version
//
typedef struct _IARM_Bus_DeviceUpdate_Announce_t
{
	char			deviceName[IARM_BUS_DEVICE_UPDATE_DEVICE_NAME_LENGTH];
	char			deviceImageVersion[IARM_BUS_DEVICE_UPDATE_VERSION_LENGTH];
	unsigned int	deviceImageType;
    char			deviceImageFilePath[IARM_BUS_DEVICE_UPDATE_PATH_LENGTH];
    unsigned char	forceUpdate;

} IARM_Bus_DeviceUpdate_Announce_t;


//
// IARM_Bus_DeviceUpdate_ReadyToDownload_t
//
// Event: IARM_BUS_DEVICE_UPDATE_EVENT_READY_TO_DOWNLOAD
//
// After the Device Update Proxy Manager has called the AcceptUpdate RPC for a particular device, it can transmit a
// ReadyToDownload event out to the IARM bus.  This event will notify the Device Update Manager and any other interested
// parties (such as the XRE receiver) that a particular device represented by the Device Update Proxy is ready to
// begin a download.
//
// updateSessionID - The session ID of the download. Corresponds to a specific download and a specific device.
//
// deviceCurrentSWVersion - Version that is currently installed on the device targeted for the image update.
//
// deviceNewSoftwareVersion - Version to update the device to.
//
// deviceHWVersion - Version of the device HW
//
// deviceBootloaderVersion - Version of the bootloader running on the device
//
// deviceName - Null terminated String containing the name of the device.
//
// totalSize - The size of the image accepted for download in bytes
//
// deviceImageType - Type of the new image. Pulled from the XML file of the received device update package.
//
typedef struct _IARM_Bus_DeviceUpdate_ReadyToDownload_t
{
    unsigned int  updateSessionID;
    unsigned char deviceCurrentSWVersion[IARM_BUS_DEVICE_UPDATE_VERSION_LENGTH];
    unsigned char deviceNewSoftwareVersion[IARM_BUS_DEVICE_UPDATE_VERSION_LENGTH];
    unsigned char deviceHWVersion[IARM_BUS_DEVICE_UPDATE_VERSION_LENGTH];
    unsigned char deviceBootloaderVersion[IARM_BUS_DEVICE_UPDATE_VERSION_LENGTH];
    char          deviceName[IARM_BUS_DEVICE_UPDATE_DEVICE_NAME_LENGTH];
    unsigned int  totalSize;
    unsigned int  deviceImageType;

} IARM_Bus_DeviceUpdate_ReadyToDownload_t;


//
// IARM_Bus_DeviceUpdate_DownloadInitiate_t
//
// Event: IARM_BUS_DEVICE_UPDATE_EVENT_DOWNLOAD_INITIATE
//
// This event can be sent from either the Device Update Manager or Update Master to tell the Device Update Proxy to initiate
// a download for a particular device.  An Update Master would have seen the ReadyToDownload event from a Device Update Proxy
// before sending this and would have obtained the updateSessionID from that event.  The download can be initiated as a background
// download (meaning no impact to the device's normal operation) or an immediate download (move data to the device as fast as
// possible.
//
// updateSessionID - The session ID of the download. Corresponds to a specific download and a specific device.
//
// backgroundDownload - Whether the background should be full speed or in the background
//
// requestedPercentIncrement - Allows the Device Update Manager or Update Master to request what increments the Device Update
//                             Proxy will provide status update increments. This should be a best effort implementation by the
//                             Device Update Proxy.  If it is set at 0x0, then it should only send at 0 and 100.  if it is 0xFF
//                             then it should determined by the Device Update Proxy.
//
typedef struct _IARM_Bus_DeviceUpdate_DownloadInitiate_t
{
    unsigned int  updateSessionID;
    unsigned char backgroundDownload;
    unsigned char requestedPercentIncrement;
    unsigned char loadImageImmediately;

} IARM_Bus_DeviceUpdate_DownloadInitiate_t;


//
// IARM_Bus_DeviceUpdate_DownloadStatus_t
//
// Event: IARM_BUS_DEVICE_UPDATE_EVENT_DOWNLOAD_STATUS
//
// This event is sent from the Device Update Proxy to the IARM bus to inform listeners of the status of the download.
// The Device Update Proxy must sent an initial status with percentComplete == 0 and a final status with percentComplete == 100.
// This will notify listeners of the the beginning and end, respectively of the download.  The Device Update Proxy should
// send status updates at periods defined by the requestedPercentIncrement in the Download Initiate command.  If it is set at
// 0x0, then it should only send at 0 and 100.  if it is 0xFF then it should determined by the Device Update Proxy.
//
// updateSessionID - The session ID of the download. Corresponds to a specific download and a specific device.
//
// percentComplete - Percent of the download that has completed. 0x0 means only start/end.  0xFF means let the DUP decide.
//
typedef struct _IARM_Bus_DeviceUpdate_DownloadStatus_t
{
    unsigned int   updateSessionID;
    unsigned int   percentComplete; // 0-100

} IARM_Bus_DeviceUpdate_DownloadStatus_t;


//
// IARM_Bus_DeviceUpdate_LoadInitiate_t
//
// Event: IARM_BUS_DEVICE_UPDATE_EVENT_LOAD_INITIATE
//
// This event can be sent from either the Device Update Manager or Update Master to tell the Device Update Proxy to initiate
// a load for a particular device. The initiator can tell the Device Update Proxy to load immediately (this would be expected if
// the download was immediate), load after a given time and a given amount of inactivity, to poll repeatedly for updated
// instructions, or to abort an existing download session.
//
// updateSessionID - The session ID of the download. Corresponds to a specific download and a specific device.
//
// loadDelayType - The delay type/strategy for loading
//
// timeToLoad - The delta in time on the remote from receiving the command to monitoring the inactivity and starting the
//              inactivity timer in seconds.
//
// timeAfterInactive
//
typedef struct _IARM_Bus_DeviceUpdate_LoadInitiate_t
{
    unsigned int                             updateSessionID;
    IARM_Bus_Device_Update_LoadDelayType_t   loadDelayType;
    unsigned int							 timeToLoad;
    unsigned int							 timeAfterInactive;

} IARM_Bus_DeviceUpdate_LoadInitiate_t;


//
// IARM_Bus_DeviceUpdate_LoadStatus_t
//
// Event: IARM_BUS_DEVICE_UPDATE_EVENT_LOAD_STATUS
//
// This event is sent from the Device Update Proxy to the IARM bus to inform listeners of the status of the load.  The
// Device Update Proxy should know when the load gets initiated as well as wend it is complete and finished.  When this
// event is sent with a status of LOAD_STATUS_END, the session should be considered complete and the updateSessionID
// invalidated.
//
// updateSessionID - The session ID of the download. Corresponds to a specific download and a specific device.
//
// loadStatus - Status of the load.  Will signify the beginning and the end.
//
typedef struct _IARM_Bus_DeviceUpdate_LoadStatus_t
{
    unsigned int                            updateSessionID;
    IARM_Bus_Device_Update_LoadStatusType_t loadStatus;

} IARM_Bus_DeviceUpdate_LoadStatus_t;


//
// IARM_Bus_Device_Update_Error_t
//
// IARM_BUS_DEVICE_UPDATE_EVENT_ERROR
//
// This event is sent from the Device Update Proxy to the IARM bus to inform listeners of the status of the load.  The
// Device Update Proxy should know when the load gets initiated as well as wend it is complete and finished.  When this
// event is sent with a status of LOAD_STATUS_END, the session should be considered complete and the updateSessionID
// invalidated.
//
// updateSessionID - The session ID of the download. Corresponds to a specific download and a specific device.
//
// errorMessage - Null terminated string describing error.
//
// errorType - Type of error.
//
typedef struct _IARM_Bus_Device_Update_Error_t
{
    unsigned int 					     updateSessionID;
    unsigned char 						 errorMessage[IARM_BUS_DEVICE_UPDATE_ERROR_LENGTH];
    IARM_Bus_Device_Update_ErrorType_t   errorType;

} IARM_Bus_Device_Update_Error_t;


/** @} */
/** @} */
