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
 * @defgroup power
 * @{
 **/


#include <stdio.h>
#include <stdlib.h>
#include "resetModes.h"
#include "pwrlogger.h"

#ifdef ENABLE_PERSONALITY_SWITCH
extern "C" {
    typedef enum svcs_transPersonalityResult {
        TRANS_PERS_SUCCESS           = 0, /* if personality set/get is successful */
        TRANS_PERS_MOCA_ERROR        = 1, /* If personality check cannot be completed due to network/moca connectivity */
        TRANS_PERS_ACS_CONNECT_ERROR = 2, /* If personality check cannot be completed due to ACS connection error */
        TRANS_PERS_TR069_ERROR       = 3
    } svcs_transPersonalityResult;
    svcs_transPersonalityResult svcs_transSetPersonalityFlag(char * newTransPersonalityFlag);
    unsigned int svcs_transGetPersonalityFlag( char ** personalityFlag );
}
#endif

int processPersonalityReset()
{
    int result = 0;
    char *personalityFlag = NULL;
    LOG("\n Reset: Processing personality reset\n");
    fflush(stdout);
#ifdef ENABLE_PERSONALITY_SWITCH
    svcs_transGetPersonalityFlag(&personalityFlag);
    LOG("Current personality is %s\n", personalityFlag);
    free(personalityFlag);
    result = svcs_transSetPersonalityFlag("DTA");
    LOG("Personality reset returned %d\n", result);
    svcs_transGetPersonalityFlag(&personalityFlag);
    LOG("Current personality is %s\n", personalityFlag);
    free(personalityFlag);
    if(0 == result)
    {
        /*Execute the script for personality reset*/
        system("sh /lib/rdk/deviceReset.sh personality");

        system("echo 0 > /opt/.rebootFlag");
        system(" echo `/bin/timestamp` ------------- Rebooting due to personality reset process --------------- >> /opt/logs/receiver.log");
        system("sleep 5; /rebootNow.sh -s PowerMgr_PersonalityReset -o 'Rebooting the box due to personality reset process ...'");
    }
#else
    LOG("\n Reset: personality reset is not supported on this device.\n");
    fflush(stdout);
#endif //ENABLE_PERSONALITY_SWITCH
    return result;
}



/** @} */
/** @} */
