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
#include "libIBus.h"
#include "pwrMgr.h"
#include <string.h>
static void _eventHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len)
{
    

    printf("Event handler invoked :: \n");
    if (strcmp(owner,IARM_BUS_PWRMGR_NAME) == 0)  
    {
        switch (eventId)  
        {
            case IARM_BUS_PWRMGR_EVENT_DEEPSLEEP_TIMEOUT:
            {
                //IARM_Bus_PWRMgr_SetDeepSleepTimeOut_Param_t *param = (IARM_Bus_PWRMgr_SetDeepSleepTimeOut_Param_t *)data;
                printf("Time out event called \n");
                printf("Deep Sleep time out occured \n");
            }
              break;
            case IARM_BUS_PWRMGR_EVENT_MODECHANGED:
            {
                  IARM_Bus_PWRMgr_EventData_t *param = (IARM_Bus_PWRMgr_EventData_t *)data;
                        printf("Event IARM_BUS_PWRMGR_EVENT_MODECHANGED: State Changed %d to %d\n",
                                        param->data.state.curState, param->data.state.newState);
            }
              break;
#ifdef ENABLE_THERMAL_PROTECTION

             case IARM_BUS_PWRMGR_EVENT_THERMAL_MODECHANGED:
            {
                  IARM_Bus_PWRMgr_EventData_t *param = (IARM_Bus_PWRMgr_EventData_t *)data;
                        printf("Event IARM_BUS_PWRMGR_EVENT_THERMAL_MODECHANGED: State Changed %d to %d\n \
				Current temperature is %f ", param->data.therm.curLevel, param->data.therm.newLevel,
				param->data.therm.curTemperature);
            }
              break;
#endif //ENABLE_THERMAL_PROTECTION

          default:
              break;

        }
    }


}

int main()
{

    IARM_Result_t err;
    int input;
    unsigned int timeOut;

    do{
        printf("Tring to initialize IARM..\n");

        err = IARM_Bus_Init("PowerMgrTest");

        if(IARM_RESULT_SUCCESS != err)
        {
            printf("Error initialing IARM bus()... error code : %d\n",err);
            break;
        }

        printf("Trying to connect..\n");

        err = IARM_Bus_Connect();

        if(IARM_RESULT_SUCCESS != err)
        {
            printf("Error connecting to  IARM bus()... error code : %d\n",err);
            break;
        }
    }while(0);

    IARM_Bus_RegisterEventHandler(IARM_BUS_PWRMGR_NAME,  IARM_BUS_PWRMGR_EVENT_DEEPSLEEP_TIMEOUT, _eventHandler);
#ifdef ENABLE_THERMAL_PROTECTION
    IARM_Bus_RegisterEventHandler(IARM_BUS_PWRMGR_NAME, IARM_BUS_PWRMGR_EVENT_THERMAL_MODECHANGED , _eventHandler);
#endif    
    IARM_Bus_RegisterEventHandler(IARM_BUS_PWRMGR_NAME,  IARM_BUS_PWRMGR_EVENT_MODECHANGED, _eventHandler);
    
    do{
        printf("Enter command..\n");
        printf("c - check power status\n");
        printf("s - set power status\n");
        printf("w - warehouse reset the box\n");
        printf("t - set time out \n");	
#ifdef ENABLE_THERMAL_PROTECTION
        printf("v - get current temperature \n");	
        printf("u - set temperature thresholds:  \n");	
#endif
	printf("x - exit..\n");

    
        input = getchar();

        switch(input)
        {
	        IARM_Result_t err;
            /*coverity[unterminated_case]*/
            case 't':
            {
                IARM_Bus_PWRMgr_SetDeepSleepTimeOut_Param_t param;
                printf("Enter time out \n :");
                scanf("%d",&timeOut);
                param.timeout = timeOut;
                err = IARM_Bus_Call(IARM_BUS_PWRMGR_NAME,
                            IARM_BUS_PWRMGR_API_SetDeepSleepTimeOut,
                            (void *)&param,
                            sizeof(param));
                if( err == IARM_RESULT_SUCCESS)
                {
                   printf("Successfully set the time out \n");
                } 
            }
#ifdef ENABLE_THERMAL_PROTECTION
             case 'v':
            {
                IARM_Bus_PWRMgr_GetThermalState_Param_t  param;
                err = IARM_Bus_Call(IARM_BUS_PWRMGR_NAME,
                            IARM_BUS_PWRMGR_API_GetThermalState,
                            (void *)&param,
                            sizeof(param));
                if(err == IARM_RESULT_SUCCESS)
                {
                    printf("Current level is : (%d), temperature : %f \n",param.curLevel,param.curTemperature);
                }
            }
            break;
            case 'u':
            {
                IARM_Bus_PWRMgr_SetTempThresholds_Param_t param;
               int temperature;
                printf("Enter high temperature level \n :");
                scanf("%d",&temperature);
                param.tempHigh = temperature;
                printf("Enter critical temperature level \n :");
                scanf("%d",&temperature);
                param.tempCritical = temperature;

                err = IARM_Bus_Call(IARM_BUS_PWRMGR_NAME,
                            IARM_BUS_PWRMGR_API_SetTemperatureThresholds,
                            (void *)&param,
                            sizeof(param));
                if(err == IARM_RESULT_SUCCESS)
                {
                    printf("Threshold temperature levels updated \n");
                }
            }
            break;
#endif


            /*coverity[fallthrough]*/
            case 'c':
            {
                IARM_Bus_PWRMgr_GetPowerState_Param_t param;
                err = IARM_Bus_Call(IARM_BUS_PWRMGR_NAME,
                            IARM_BUS_PWRMGR_API_GetPowerState,
                            (void *)&param,
                            sizeof(param));
                if(err == IARM_RESULT_SUCCESS)
                {
                    printf("Current state is : (%d)\n",param.curState);
                }                
            }
            break;
            case 's':
            {
                IARM_Bus_PWRMgr_SetPowerState_Param_t param;
                int state = 'x';
                printf("Give a state to set..\n");
                printf("n - Power on\n");
                printf("f - Power off\n");
                printf("b - Standby\n");
                printf("l - light sleep \n");
                printf("d - deep sleep \n");
                
                while(state != 'n' && state != 'f' && state != 'b' && state != 'l' && state != 'd')
                {
                    printf("Give a valid state n/f/b/l/d\n");
                    state = getchar();
                } 

                switch(state)
                {
                    case 'n': param.newState = IARM_BUS_PWRMGR_POWERSTATE_ON;break;
                    case 'f': param.newState = IARM_BUS_PWRMGR_POWERSTATE_OFF;break;
                    case 'b': param.newState = IARM_BUS_PWRMGR_POWERSTATE_STANDBY;break;
                    case 'l': param.newState = IARM_BUS_PWRMGR_POWERSTATE_STANDBY_LIGHT_SLEEP;break;
                    case 'd': param.newState = IARM_BUS_PWRMGR_POWERSTATE_STANDBY_DEEP_SLEEP;break;
                }
                
                err = IARM_Bus_Call(IARM_BUS_PWRMGR_NAME,
                            IARM_BUS_PWRMGR_API_SetPowerState,
                            (void *)&param,
                            sizeof(param));
                if(err == IARM_RESULT_SUCCESS)
                {
                    printf("Successfully set the state..\n");
                }
            }
            break;
            case 'w':
            {
                err = IARM_Bus_Call(IARM_BUS_PWRMGR_NAME,
                            IARM_BUS_PWRMGR_API_WareHouseReset,
                            NULL, 0);
                if(err == IARM_RESULT_SUCCESS)
                {
                    printf("Successfully executed Warehouse reset command..\n");
                }

            }
            break;
        }

    }while(input != 'x');
    IARM_Bus_UnRegisterEventHandler(IARM_BUS_PWRMGR_NAME, IARM_BUS_PWRMGR_EVENT_MODECHANGED);
    IARM_Bus_UnRegisterEventHandler(IARM_BUS_PWRMGR_NAME, IARM_BUS_PWRMGR_EVENT_DEEPSLEEP_TIMEOUT);
#ifdef ENABLE_THERMAL_PROTECTION
    IARM_Bus_UnRegisterEventHandler(IARM_BUS_PWRMGR_NAME, IARM_BUS_PWRMGR_EVENT_THERMAL_MODECHANGED );
#endif
    IARM_Bus_Disconnect();
    IARM_Bus_Term();
}


/** @} */
/** @} */
