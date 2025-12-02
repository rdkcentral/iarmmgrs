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
* @defgroup iarmmgrs
* @{
* @defgroup test
* @{
**/


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include "libIBus.h"
#include "libIBusDaemon.h"
#include "sysMgr.h"
#include "libIARMCore.h"

/* Global flag for graceful shutdown */
static volatile int keep_running = 1;

static void signal_handler(int sig)
{
	printf("Received signal %d, shutting down gracefully...\n", sig);
	keep_running = 0;
}

static void _evtHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len)
{
	unsigned long messageLength=0;

	if (strcmp(owner, IARM_BUS_SYSMGR_NAME)  == 0) {
		 switch(eventId){
            case IARM_BUS_SYSMGR_EVENT_XUPNP_DATA_UPDATE: 
                {
					IARM_Bus_SYSMgr_EventData_t *eventData = (IARM_Bus_SYSMgr_EventData_t*)data;
					messageLength = eventData->data.xupnpData.deviceInfoLength;
					printf(" >>>>>>>>>>  EVENT_XUPNP_DATA_UPDATE messageLength : %ld \n", messageLength);
					break;
                }
        default:
			break;
		}
	}
}

int main()
{
	IARM_Result_t retCode = IARM_RESULT_SUCCESS;
	IARM_Bus_SYSMgr_EventData_t eventData;

	/* Install signal handlers for graceful shutdown */
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	printf("Client Entering %d\r\n", getpid());
	IARM_Bus_Init("Sys Client");
	IARM_Bus_Connect();

    IARM_Bus_RegisterEventHandler(IARM_BUS_SYSMGR_NAME,IARM_BUS_SYSMGR_EVENT_XUPNP_DATA_UPDATE,_evtHandler);

	/* Fixed infinite loop with proper exit condition */
	while(keep_running){
	
		sleep(30);
		if (!keep_running) break; /* Check exit condition after sleep */
		
	    eventData.data.xupnpData.deviceInfoLength = 0;
        IARM_Bus_BroadcastEvent(IARM_BUS_SYSMGR_NAME,
			(IARM_EventId_t)IARM_BUS_SYSMGR_EVENT_XUPNP_DATA_REQUEST,(void *)&eventData, sizeof(eventData));
		printf(">>>>> Generate IARM_BUS_SYSMGR_NAME EVENT ::: IARM_BUS_SYSMGR_EVENT_XUPNP_DATA_REQUEST \r\n");
		
		sleep(30);
		if (!keep_running) break; /* Check exit condition after second sleep */

	}
	
	/* Cleanup code is now reachable - fixes resource leak */
	if (IARM_Bus_Disconnect() != IARM_RESULT_SUCCESS) {
		printf("Warning: IARM_Bus_Disconnect failed during cleanup\n");
	}
	if (IARM_Bus_Term() != IARM_RESULT_SUCCESS) {
		printf("Warning: IARM_Bus_Term failed during cleanup\n");
	}
	printf("Sys Client Exiting\r\n");
	
	return 0; /* Proper return statement */
}


/** @} */
/** @} */
