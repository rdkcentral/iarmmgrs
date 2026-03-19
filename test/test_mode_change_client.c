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


#include "libIBus.h"
#include "libIBusDaemon.h"
#include "libIARMCore.h" 
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

IARM_Bus_Daemon_SysMode_t currentMode = IARM_BUS_SYS_MODE_NORMAL;
 #define IARM_BUS_DAEMON_API_SysModeChange               "DaemonSysModeChange" /*!< Well known name for Sys mode change*/

IARM_Result_t _SysModeChange(void *arg)
{
	IARM_Bus_CommonAPI_SysModeChange_Param_t *param = (IARM_Bus_CommonAPI_SysModeChange_Param_t *)arg;
	printf("Sys Mode Change::New mode --> %d, Old mode --> %d\n",param->newMode,param->oldMode);
	return IARM_RESULT_SUCCESS;
}

int main()
{
	int x = 0; /* Initialize properly instead of 'y' */
	IARM_Bus_CommonAPI_SysModeChange_Param_t sysModeParam;

	printf("SysClient Entering %d\r\n", getpid());
	if (IARM_Bus_Init("Client") != IARM_RESULT_SUCCESS) {
		printf("Failed to initialize IARM Bus\n");
		return -1;
	}
	if (IARM_Bus_Connect() != IARM_RESULT_SUCCESS) {
		printf("Failed to connect to IARM Bus\n");
		return -1;
	}
	if (IARM_Bus_RegisterCall(IARM_BUS_COMMON_API_SysModeChange,_SysModeChange) != IARM_RESULT_SUCCESS) {
		printf("Failed to register SysModeChange call\n");
		return -1;
	}
	printf("Enter 'x' to exit, 'e' to send EAS and 'w' to send warehouse and 'n' to send normal Sys mode changes\n");
	
	/* Fix logic error: read user input before entering switch */
	while((x = getchar()) != 'x') {

		switch(x)
		{
			case 'e':
				sysModeParam.oldMode = currentMode;
				sysModeParam.newMode = IARM_BUS_SYS_MODE_EAS;
				if (IARM_Bus_Call(IARM_BUS_DAEMON_NAME,
                			IARM_BUS_DAEMON_API_SysModeChange,
                			&sysModeParam,
                			sizeof(sysModeParam)) != IARM_RESULT_SUCCESS) {
					printf("Failed to send SysModeChange EAS\n");
				} else {
					currentMode = IARM_BUS_SYS_MODE_EAS;
				}
			break;
			case 'w':
				sysModeParam.oldMode = currentMode;
				sysModeParam.newMode = IARM_BUS_SYS_MODE_WAREHOUSE;
				if (IARM_Bus_Call(IARM_BUS_DAEMON_NAME,
                			IARM_BUS_DAEMON_API_SysModeChange,
                			&sysModeParam,
                			sizeof(sysModeParam)) != IARM_RESULT_SUCCESS) {
					printf("Failed to send SysModeChange Warehouse\n");
				} else {
					currentMode = IARM_BUS_SYS_MODE_WAREHOUSE;
				}
			break;
			case 'n':
				sysModeParam.oldMode = currentMode;
				sysModeParam.newMode = IARM_BUS_SYS_MODE_NORMAL;
				if (IARM_Bus_Call(IARM_BUS_DAEMON_NAME,
                			IARM_BUS_DAEMON_API_SysModeChange,
                			&sysModeParam,
                			sizeof(sysModeParam)) != IARM_RESULT_SUCCESS) {
					printf("Failed to send SysModeChange Normal\n");
				} else {
					currentMode = IARM_BUS_SYS_MODE_NORMAL;
				}
			break;
			default:
				/* Handle invalid input */
				if (x != '\n' && x != ' ' && x != '\t') {
					printf("Invalid input '%c'. Enter 'e', 'w', 'n', or 'x'\n", x);
				}
			break;
		}
	}
	if (IARM_Bus_Disconnect() != IARM_RESULT_SUCCESS) {
		printf("Failed to disconnect from IARM Bus\n");
	}
	if (IARM_Bus_Term() != IARM_RESULT_SUCCESS) {
		printf("Failed to terminate IARM Bus\n");
	}
	printf("Client Exiting\r\n");
	return 0; /* Add proper return statement */
}


/** @} */
/** @} */
