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
* @defgroup disk
* @{
**/


#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>
#include <stdbool.h>

#include "libIBus.h"
#include "diskMgr.h"
#include "diskMgrInternal.h"

/* Global flag for graceful shutdown */
static volatile bool g_running = true;

/* Signal handler for graceful shutdown */
static void signal_handler(int signal) {
    if (signal == SIGTERM || signal == SIGINT) {
        LOG("Disk Manager: Received signal %d, initiating graceful shutdown\n", signal);
        g_running = false;
    }
}


IARM_Result_t DISKMgr_Start()
{
    IARM_Result_t rc = IARM_RESULT_SUCCESS;

    LOG("Entering [%s] - [%s] - disabling io redirect buf\r\n", __FUNCTION__, IARM_BUS_DISKMGR_NAME);
    setvbuf(stdout, NULL, _IOLBF, 0);

    /* Set up signal handlers for graceful shutdown */
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);

    /* Initialize IARM Bus with error checking */
    rc = IARM_Bus_Init(IARM_BUS_DISKMGR_NAME);
    if (rc != IARM_RESULT_SUCCESS) {
        LOG("ERROR: IARM_Bus_Init failed with error code %d\n", rc);
        return rc;
    }

    /* Connect to IARM Bus with error checking */
    rc = IARM_Bus_Connect();
    if (rc != IARM_RESULT_SUCCESS) {
        LOG("ERROR: IARM_Bus_Connect failed with error code %d\n", rc);
        IARM_Bus_Term();  /* Cleanup on failure */
        return rc;
    }

    /* Register events with error checking */
    rc = IARM_Bus_RegisterEvent(IARM_BUS_DISKMGR_EVENT_MAX);
    if (rc != IARM_RESULT_SUCCESS) {
        LOG("ERROR: IARM_Bus_RegisterEvent failed with error code %d\n", rc);
        IARM_Bus_Disconnect();  /* Cleanup on failure */
        IARM_Bus_Term();
        return rc;
    }

    LOG("Disk Manager started successfully\n");
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t DISKMgr_Loop()
{
    LOG("Disk Manager: Starting main loop\n");
    
    /* Main loop with proper termination mechanism */
    while(g_running)
    {
        LOG("I-ARM Disk Mgr: HeartBeat ping.\r\n");
        
        /* Fix critical issue: sleep takes seconds, not milliseconds
         * sleep(2000) would sleep for 2000 seconds (33+ minutes!)
         * Change to sleep(2) for 2-second delay as intended
         */
        sleep(2);
        
        /* Check if we need to shutdown gracefully */
        if (!g_running) {
            LOG("Disk Manager: Shutdown signal received, exiting main loop\n");
            break;
        }
    }
    
    LOG("Disk Manager: Main loop terminated\n");
    return IARM_RESULT_SUCCESS;
}


IARM_Result_t DISKMgr_Stop(void)
{
    LOG("Disk Manager: Stopping...\n");
    
    /* Signal the main loop to stop */
    g_running = false;
    
    /* Clean up IARM connections */
    IARM_Bus_Disconnect();
    IARM_Bus_Term();
    
    LOG("Disk Manager: Stopped successfully\n");
    return IARM_RESULT_SUCCESS;
}




/** @} */
/** @} */
