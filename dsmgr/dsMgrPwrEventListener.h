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



/**
 * @file dsMgrPwrEventListener.h
 *
 * @defgroup IARMBUS_DS_MGR DS Manager
 * @ingroup IARM_MGR_RPC
 *
 * DS (Device Settings) Manager is responsible for listening Power manager event and Set,
 * - LED/Front Panel Indicators
 * - Display (Aspect Ratio, EDID data etc.)
 * - General Host configuration (Power managements, event management etc.)
 */

/**
* @defgroup iarmmgrs
* @{
* @defgroup dsmgr
* @{
**/
#ifndef DSMGR_DSMGRPWREVENTLISTENER_H_
#define DSMGR_DSMGRPWREVENTLISTENER_H_

#include "libIARM.h"
#include "libIBusDaemon.h"
#include "dsMgrInternal.h"
#include "powermanager_client.h"
#include "dsMgr.h"
#include "libIBus.h"
#include "iarmUtil.h"
#include "plat_power.h"


void initPwrEventListner(void);



#endif /* DSMGR_DSMGRPWREVENTLISTENER_H_ */
