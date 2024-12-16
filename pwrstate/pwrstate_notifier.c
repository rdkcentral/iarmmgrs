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

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>

#include "libIBus.h"
#include "sysMgr.h"
#include "libIBusDaemon.h"
#include "pwrMgr.h"
#include "pwrlogger.h"

#define IARM_POWER_EVENT "pwrEvent"

/** @brief This API handles power mode change events received from power manager.
 *
 *  @param[in] owner    owner of the event
 *  @param[in] eventId  power manager event ID
 *  @param[in] data     event data
 *  @param[in] len      event size
 */
void powerEventHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len)
{
	if (data == NULL)
	{
	     return;
	}
	/*
        switch(eventId)
        { 
                case IARM_BUS_PWRMGR_EVENT_MODECHANGED:
                {
                        IARM_Bus_PWRMgr_EventData_t *param = (IARM_Bus_PWRMgr_EventData_t *)data;
                        printf("Event IARM_BUS_PWRMGR_EVENT_MODECHANGED: State Changed %d -- > %d\n",
                                param->data.state.curState, param->data.state.newState);
                        switch(param->data.state.newState)
                        {
                                case IARM_BUS_PWRMGR_POWERSTATE_ON:
                                {
					 FILE *fd = NULL;
                                         char buffer[80] = {0};
                                         const char* filename = "/tmp/.fwdnld.pid" ;
                                         fd = fopen(filename, "r");
                                         if (fd == NULL)
                                         {
                                               printf("Error in opening script file \n");
					       return;
                                         }
                                         fgets(buffer,80,fd);
                                         int pid=atoi(buffer);
                                         kill(pid, SIGTERM);
					 fclose(fd);
                                }
                                      break;

                                default:
                                      break;
                        }
                }
                        break;

                default:
                        break;
        }*/
	return;
}


/**
 * @brief To Register required IARM event handlers with appropriate callback function to handle the event.
 */
int init_event_handler()
{       
        IARM_Bus_Init(IARM_POWER_EVENT);
        printf("SUCCESS: IARM_Bus_Init done!\n");
    
        IARM_Bus_Connect();
        printf("SUCCESS: IARM_Bus_Connect done!\n");

  
        if(0 != IARM_Bus_RegisterEventHandler(IARM_BUS_PWRMGR_NAME,  IARM_BUS_PWRMGR_EVENT_MODECHANGED, powerEventHandler))
        {
                IARM_Bus_UnRegisterEventHandler(IARM_BUS_PWRMGR_NAME,  IARM_BUS_PWRMGR_EVENT_MODECHANGED);
                return -1;
        }

        return 0;
}

/**
 * @brief This API UnRegister IARM event handlers in order to release bus-facing resources.
 */
int term_event_handler()
{
        IARM_Bus_UnRegisterEventHandler(IARM_BUS_PWRMGR_NAME,  IARM_BUS_PWRMGR_EVENT_MODECHANGED);
        printf("Successfully terminated all event handlers\n");
        return 0;
}

int main(int argc, char *argv[])
{
    GMainLoop * main_loop = g_main_loop_new(NULL, false);
    
    if(0 != init_event_handler())
    {
        printf("Error: Initializing IARM event handler failed!\n");
        return -1;
    }
    
    printf("SUCCESS: Initialized IARM event handler!\n");
    
    /*Enter event loop */
    g_main_loop_run(main_loop);
    g_main_loop_unref(main_loop);
 
    term_event_handler();
   
    printf("Power Mode Change Client Exiting\n");

    return 0;   
}

