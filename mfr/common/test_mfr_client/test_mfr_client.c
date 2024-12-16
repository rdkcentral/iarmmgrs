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
#include <malloc.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "mfrMgr.h"
#include "mfrTypes.h"
#include "libIBus.h"
#include "safec_lib.h"

mfrSerializedType_t getTypeFromMenu();

int main()
{
	char *pTmpStr;
	int len;
	mfrSerializedType_t type = mfrSERIALIZED_TYPE_MANUFACTURER; 
	mfrSerializedData_t data;
	mfrError_t Err;
        errno_t safec_rc = -1;

	printf("mfrClient Entering %d\r\n", getpid());


    IARM_Bus_Init("MFRClient");
    IARM_Bus_Connect();


	Err = mfr_init();
	if(Err != mfrERR_NONE)
	{
		printf("mfr_init failed:: %d \n",Err);
		goto Error;
	}

	printf("mfr_init success:: %d \n",Err);

	while(1)
    {
		
		type = getTypeFromMenu();
		if(mfrSERIALIZED_TYPE_MAX == type)
		{
			printf("Exiting the APP \r\n");
			goto Error;	
		}

		Err = mfrGetSerializedData(type,&data,NULL);
		if(Err != mfrERR_NONE)
		{
			printf("mfrgetserializedata failed:: %d \n",Err);
			goto Error;
		}
                pTmpStr = (char *)malloc(data.bufLen + 1);
                safec_rc = memset_s(pTmpStr, data.bufLen+1, 0, data.bufLen+1);
                ERR_CHK(safec_rc);
		safec_rc = memcpy_s(pTmpStr, data.bufLen+1, data.buf, data.bufLen);
                if(safec_rc != EOK)
                {
                    ERR_CHK(safec_rc);
                    if(pTmpStr)
                        free(pTmpStr);
                    continue;
                }

		printf("Data length is = %d \r\n",data.bufLen);
		printf("Serial number is = %s \r\n",pTmpStr);
		free(pTmpStr);
		printf("\n\n\n ");
    
	 }
Error:
    IARM_Bus_Disconnect();
    IARM_Bus_Term();
}	
	
mfrSerializedType_t getTypeFromMenu()
{
	int  input = mfrSERIALIZED_TYPE_MANUFACTURER;
	
	printf("Enter MFR Type for Query :: ");
    scanf ("%d",&input);
	
    
    switch (input)  
    {

		case mfrSERIALIZED_TYPE_MANUFACTURER:
			printf("MFR Query made for  mfrSERIALIZED_TYPE_MANUFACTURER \r\n");
		return mfrSERIALIZED_TYPE_MANUFACTURER;

		case mfrSERIALIZED_TYPE_MANUFACTUREROUI:
			printf("MFR Query made for  mfrSERIALIZED_TYPE_MANUFACTUREROUI \r\n");
		return mfrSERIALIZED_TYPE_MANUFACTUREROUI;

		case mfrSERIALIZED_TYPE_MODELNAME:
			printf("MFR Query made for  mfrSERIALIZED_TYPE_MODELNAME \r\n");
		return mfrSERIALIZED_TYPE_MODELNAME;

		case mfrSERIALIZED_TYPE_DESCRIPTION:
			printf("MFR Query made for  mfrSERIALIZED_TYPE_DESCRIPTION \r\n");
		return mfrSERIALIZED_TYPE_DESCRIPTION;

		case mfrSERIALIZED_TYPE_PRODUCTCLASS:
			printf("MFR Query made for  mfrSERIALIZED_TYPE_PRODUCTCLASS \r\n");
		return mfrSERIALIZED_TYPE_PRODUCTCLASS;	

		case mfrSERIALIZED_TYPE_SERIALNUMBER:
			printf("MFR Query made for  mfrSERIALIZED_TYPE_SERIALNUMBER \r\n");
		return mfrSERIALIZED_TYPE_SERIALNUMBER;	

		case mfrSERIALIZED_TYPE_HARDWAREVERSION:
			printf("MFR Query made for  mfrSERIALIZED_TYPE_HARDWAREVERSION \r\n");
		return mfrSERIALIZED_TYPE_HARDWAREVERSION;	

		case mfrSERIALIZED_TYPE_SOFTWAREVERSION:
			printf("MFR Query made for  mfrSERIALIZED_TYPE_SOFTWAREVERSION \r\n");
		return mfrSERIALIZED_TYPE_SOFTWAREVERSION;

		case mfrSERIALIZED_TYPE_PROVISIONINGCODE:
			printf("MFR Query made for  mfrSERIALIZED_TYPE_PROVISIONINGCODE \r\n");
		return mfrSERIALIZED_TYPE_PROVISIONINGCODE;

		case mfrSERIALIZED_TYPE_FIRSTUSEDATE:
			printf("MFR Query made for  mfrSERIALIZED_TYPE_FIRSTUSEDATE \r\n");
		return mfrSERIALIZED_TYPE_FIRSTUSEDATE;
				
		case mfrSERIALIZED_TYPE_DEVICEMAC:
			printf("MFR Query made for  mfrSERIALIZED_TYPE_DEVICEMAC \r\n");
		return mfrSERIALIZED_TYPE_DEVICEMAC;	

		case mfrSERIALIZED_TYPE_MOCAMAC:
			printf("MFR Query made for  mfrSERIALIZED_TYPE_MOCAMAC \r\n");
		return mfrSERIALIZED_TYPE_MOCAMAC;

    	case mfrSERIALIZED_TYPE_HDMIHDCP:
			printf("MFR Query made for  mfrSERIALIZED_TYPE_HDMIHDCP \r\n");
		return mfrSERIALIZED_TYPE_HDMIHDCP;

		case mfrSERIALIZED_TYPE_BOARDVERSION:
			printf("MFR Query made for  mfrSERIALIZED_TYPE_BOARDVERSION \r\n");
		return mfrSERIALIZED_TYPE_BOARDVERSION;

		case mfrSERIALIZED_TYPE_BOARDSERIALNO:
			printf("MFR Query made for  mfrSERIALIZED_TYPE_BOARDSERIALNO \r\n");
		return mfrSERIALIZED_TYPE_BOARDSERIALNO;
				    	
    	case mfrSERIALIZED_TYPE_CMCHIPVERSION:
			printf("MFR Query made for  mfrSERIALIZED_TYPE_CMCHIPVERSION \r\n");
		return mfrSERIALIZED_TYPE_CMCHIPVERSION;

		case mfrSERIALIZED_TYPE_DECODERSWVERSION:
			printf("MFR Query made for  mfrSERIALIZED_TYPE_DECODERSWVERSION \r\n");
		return mfrSERIALIZED_TYPE_DECODERSWVERSION;

	   	case mfrSERIALIZED_TYPE_OSKERNELVERSION:
			printf("MFR Query made for  mfrSERIALIZED_TYPE_OSKERNELVERSION \r\n");
		return mfrSERIALIZED_TYPE_OSKERNELVERSION;

    	case mfrSERIALIZED_TYPE_MFRLIBVERSION:
			printf("MFR Query made for  mfrSERIALIZED_TYPE_MFRLIBVERSION \r\n");
		return mfrSERIALIZED_TYPE_MFRLIBVERSION;

		case mfrSERIALIZED_TYPE_FRONTPANELVERSION:
			printf("MFR Query made for  mfrSERIALIZED_TYPE_FRONTPANELVERSION \r\n");
		return mfrSERIALIZED_TYPE_FRONTPANELVERSION;

		case mfrSERIALIZED_TYPE_SOFTWAREIMAGEVERSION1:
			printf("MFR Query made for  mfrSERIALIZED_TYPE_SOFTWAREIMAGEVERSION1 \r\n");
		return mfrSERIALIZED_TYPE_SOFTWAREIMAGEVERSION1;

		case mfrSERIALIZED_TYPE_OCHDVERSION:
			printf("MFR Query made for  mfrSERIALIZED_TYPE_OCHDVERSION \r\n");
		return mfrSERIALIZED_TYPE_OCHDVERSION;

		case mfrSERIALIZED_TYPE_OCAPVERSION:
			printf("MFR Query made for  mfrSERIALIZED_TYPE_OCAPVERSION \r\n");
		return mfrSERIALIZED_TYPE_OCAPVERSION;

        
		case mfrSERIALIZED_TYPE_BOOTROMVERSION:
			printf("MFR Query made for  mfrSERIALIZED_TYPE_BOOTROMVERSION \r\n");
		return mfrSERIALIZED_TYPE_BOOTROMVERSION;

		case mfrSERIALIZED_TYPE_MODELNUMBER:
			printf("MFR Query made for  mfrSERIALIZED_TYPE_MODELNUMBER \r\n");
		return mfrSERIALIZED_TYPE_MODELNUMBER;

		case mfrSERIALIZED_TYPE_MODELSERIALNO:
			printf("MFR Query made for  mfrSERIALIZED_TYPE_MODELSERIALNO \r\n");
		return mfrSERIALIZED_TYPE_MODELSERIALNO;

		case mfrSERIALIZED_TYPE_VENDORNAME:
			printf("MFR Query made for  mfrSERIALIZED_TYPE_VENDORNAME \r\n");
		return mfrSERIALIZED_TYPE_VENDORNAME;
	
		//Failing
		case mfrSERIALIZED_TYPE_VENDORSERIALNO:
			printf("MFR Query made for  mfrSERIALIZED_TYPE_VENDORSERIALNO \r\n");
		return mfrSERIALIZED_TYPE_VENDORSERIALNO;
		
		//Failing
    	case mfrSERIALIZED_TYPE_MANUFACTUREDATE:
			printf("MFR Query made for  mfrSERIALIZED_TYPE_MANUFACTUREDATE \r\n");
		return mfrSERIALIZED_TYPE_MANUFACTUREDATE;

		case mfrSERIALIZED_TYPE_CACPHOSTID:
			printf("MFR Query made for  mfrSERIALIZED_TYPE_MANUFACTUREDATE \r\n");
		return mfrSERIALIZED_TYPE_CACPHOSTID;

   		default: 
			printf("Error : Default choosen mfrSERIALIZED_TYPE_SERIALNUMBER \r\n");
			return mfrSERIALIZED_TYPE_MAX;
    }
 }


/** @} */
/** @} */
