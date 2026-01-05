/*
 * * @defgroup iarmmgrs
 * * @{
 * * @defgroup mfr
 * * @{
 * **/

#include <stdio.h>
#include <stdlib.h>
#include "libIARMCore.h"
#include "libIBus.h"
#include "mfrMgr.h"

int main(int argc, char *argv[] )
{
	if (argc < 2) {
		printf("Usage: %s <unsigned int>\n", argv[0]);
		return 1;
	}

	// Convert input to unsigned int
	char *endptr;
	unsigned long input_ul_data = strtoul(argv[1], &endptr, 10);

	if (*endptr != '\0') {
		printf("Invalid input: not a valid unsigned integer.\n");
		return 1;
	}


	if (input_ul_data > UINT_MAX) {
    		printf("Invalid input: value exceeds unsigned int range (0 to %u).\n", UINT_MAX);
    		return 1;
	}

	unsigned int blocklist_value = (unsigned int)input_ul_data;
        

        // Wrap the integer into the required struct
        IARM_Bus_MFRLib_Platformblockdata_Param_t param;
        param.blocklist = blocklist_value;

	IARM_Result_t ret;
	IARM_Bus_Init("Tool-mfrsetConfigdata");
	IARM_Bus_Connect();
	printf("Tool-mfrSetConfigdata Entering\r\n");

	ret = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME,
		IARM_BUS_MFRLIB_API_SetConfigData, (void *)&param, sizeof(param));

	if(ret != IARM_RESULT_SUCCESS)
	{
		printf("Call failed for %s: error code:%d \n","mfrSetConfigdata", ret);

	}
	else
	{
		printf("Call Success: mfrSetConfigdata \n ");
	}

	IARM_Bus_Disconnect();
	IARM_Bus_Term();
	printf("Tool-mfrSetConfigdata  Exiting\r\n");
	return 0;
}

