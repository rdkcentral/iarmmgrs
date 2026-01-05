/*
 * * @defgroup iarmmgrs
 * * @{
 * * @defgroup mfr
 * * @{
 * **/

#include <stdio.h>

#include "libIARMCore.h"
#include "libIBus.h"
#include "mfrMgr.h"

int main()
{
	IARM_Result_t ret;
	IARM_Bus_Init("Tool-mfrgetConfigdata");
	IARM_Bus_Connect();
	printf("Tool-mfrGetConfigdata Entering\r\n");

	IARM_Bus_MFRLib_Platformblockdata_Param_t param;
	ret = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME,
		IARM_BUS_MFRLIB_API_GetConfigData, &param, sizeof(IARM_Bus_MFRLib_Platformblockdata_Param_t));

	if(ret != IARM_RESULT_SUCCESS)
	{
		printf("Call failed for %s: error code:%d \n","mfrGetConfigdata", ret);

	}
	else
	{
		printf("Call Success: mfrGetConfigdata: %d \n ", param.blocklist);
	}

	IARM_Bus_Disconnect();
	IARM_Bus_Term();
	printf("Tool-mfrGetConfig  Exiting\r\n");
}

