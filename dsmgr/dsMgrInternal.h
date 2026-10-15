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
 * @file dsMgrInternal.h
 *
 * @defgroup IARMBUS_DS_MGR DS Manager
 * @ingroup IARM_MGR_RPC
 *
 * DS (Device Settings) Manager is responsible for managing the following operations,
 * - Audio Output Ports (Volume, Mute, etc.)
 * - Video Ouptut Ports (Resolutions, Aspect Ratio, etc.)
 * - Front Panel Indicators
 * - Zoom Settings
 * - Display (Aspect Ratio, EDID data etc.)
 * - General Host configuration (Power managements, event management etc.)
 */

/**
* @defgroup iarmmgrs
* @{
* @defgroup dsmgr
* @{
**/


#ifndef _IARM_DSMGR_INTERNAL_
#define _IARM_DSMGR_INTERNAL_
#include "libIARM.h"
#include "libIBusDaemon.h"

#define IARM_BUS_DSMGR_NAME		"DSMgr"

/* RDK logger module used by the INT_* logging macros (defined in iarmutilslogger.h) for DS Manager */
#define IARMUTILS_LOG_MODULE "LOG.RDK.DSMGR"

extern IARM_Bus_Daemon_SysMode_t isEAS; // Default is Normal Mode

IARM_Result_t DSMgr_Start();
IARM_Result_t DSMgr_Loop();
IARM_Result_t DSMgr_Stop(void);


#endif


/** @} */
/** @} */
