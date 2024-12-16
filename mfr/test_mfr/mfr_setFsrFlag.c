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
* @defgroup mfr
* @{
**/

#include <stdio.h>

#include "libIARMCore.h"
#include "libIBus.h"
#include "mfrMgr.h"

int main(int argc, char *argv[])
{
	if( argc == 2 ) {
		printf("The argument supplied is %s\n", argv[1]);
	}
	else if( argc > 2 ) {
		printf("Too many arguments supplied.\n");
		exit(0);
	}
	else {
		printf("Usage : FSR flag is expected (For Set 1 / For reset 2) [./mfr_setFsrFlag <1/2>]\n");
		return 0;
	}

	IARM_Result_t ret;
	IARM_Bus_Init("Tool-mfrSetFsrFlag");
	IARM_Bus_Connect();
	printf("Tool-mfrSetFsrFlag Entering\r\n");

	/* Set FSR Flag */
	IARM_Bus_MFRLib_FsrFlag_Param_t param;

	param = atoi(argv[1]);

	ret = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME,IARM_BUS_MFRLIB_API_SetFsrFlag,&param ,sizeof(IARM_Bus_MFRLib_FsrFlag_Param_t) );

	if(ret != IARM_RESULT_SUCCESS)
	{
		printf("Call failed for %s: error code:%d \n","mfrSetFSRflag",ret);
	}
	else
	{
		printf("Call Success for setFsrFlag : %u \n ", param);
	}

	IARM_Bus_Disconnect();
	IARM_Bus_Term();
	printf("Tool-mfrSetFSRflag  Exiting\r\n");
	return 0;
}
