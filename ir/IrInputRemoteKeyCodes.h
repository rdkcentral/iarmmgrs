/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2018 RDK Management
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
* @file IRInputRemoteKeyCodes.h
*
* @brief Map IR Remote Keys to standard Linux Input Keys.
*
* @par Document
* Document reference.is
*
* @par Open Issues (in no particular order)
* -# None
*
* @par Assumptions
* -# None
*
* @par Abbreviations
* - RDK:     Reference Design Kit.
* - _t:      Type (suffix).
*
* @par Implementation Notes
* -# None
*
*/
#ifndef _IARM_IR_KEYCODES_H_
#define _IARM_IR_KEYCODES_H_


#include <linux/input.h>
#include <stdint.h>
#include "comcastIrKeyCodes.h"

#ifdef __cplusplus
extern "C" {
#endif

#define _KEY_INVALID (KEY_RESERVED)
typedef struct
{
    uint32_t            iCode;
    uint32_t	        uCode;
    uint32_t            uModi;
} IARM_keycodes;

#define IARM_TO_LINUX_KEY(iCode, uCode) { iCode, uCode, _KEY_INVALID},
#define IARM_TO_LINUX_CTL(iCode, uCode) { iCode, uCode, KEY_LEFTCTRL},


/*-------------------------------------------------------------------
   End of Macro Defines
-------------------------------------------------------------------*/
static IARM_keycodes kcodesMap_IARM2Linux[] =
{
	IARM_TO_LINUX_CTL( KED_MENU, KEY_M)
	IARM_TO_LINUX_CTL( KED_GUIDE,KEY_G)
	IARM_TO_LINUX_CTL( KED_INFO, KEY_I)
	IARM_TO_LINUX_KEY( KED_ENTER, KEY_ENTER)
	IARM_TO_LINUX_KEY( KED_OK, KEY_OK) 
	IARM_TO_LINUX_KEY( KED_SELECT, KEY_ENTER)
	IARM_TO_LINUX_CTL( KED_EXIT, KEY_E)
	IARM_TO_LINUX_KEY( KED_POWER, KEY_POWER) //FP
	IARM_TO_LINUX_CTL( KED_CHANNELUP, KEY_UP)
	IARM_TO_LINUX_CTL( KED_CHANNELDOWN, KEY_DOWN)
	IARM_TO_LINUX_CTL( KED_VOLUMEUP, KEY_U)
	IARM_TO_LINUX_CTL( KED_VOLUMEDOWN, KEY_D)
	IARM_TO_LINUX_CTL( KED_MUTE, KEY_Y)
	IARM_TO_LINUX_KEY( KED_DIGIT1, KEY_1)
	IARM_TO_LINUX_KEY( KED_DIGIT2, KEY_2)
	IARM_TO_LINUX_KEY( KED_DIGIT3, KEY_3)
	IARM_TO_LINUX_KEY( KED_DIGIT4, KEY_4)
	IARM_TO_LINUX_KEY( KED_DIGIT5, KEY_5)
	IARM_TO_LINUX_KEY( KED_DIGIT6, KEY_6)
	IARM_TO_LINUX_KEY( KED_DIGIT7, KEY_7)
	IARM_TO_LINUX_KEY( KED_DIGIT8, KEY_8)
	IARM_TO_LINUX_KEY( KED_DIGIT9, KEY_9)
	IARM_TO_LINUX_KEY( KED_DIGIT0, KEY_0)
	IARM_TO_LINUX_CTL( KED_FASTFORWARD, KEY_F)
	IARM_TO_LINUX_CTL( KED_REWIND, KEY_W)
	IARM_TO_LINUX_CTL( KED_PAUSE, KEY_P)
	IARM_TO_LINUX_CTL( KED_PLAY, KEY_P)
	IARM_TO_LINUX_CTL( KED_STOP, KEY_S)
	IARM_TO_LINUX_CTL( KED_RECORD, KEY_R)
	IARM_TO_LINUX_KEY( KED_ARROWUP, KEY_UP)
	IARM_TO_LINUX_KEY( KED_ARROWDOWN, KEY_DOWN)
	IARM_TO_LINUX_KEY( KED_ARROWLEFT, KEY_LEFT)
	IARM_TO_LINUX_KEY( KED_ARROWRIGHT, KEY_RIGHT)
	IARM_TO_LINUX_KEY( KED_PAGEUP, KEY_PAGEUP)
	IARM_TO_LINUX_KEY( KED_PAGEDOWN, KEY_PAGEDOWN)
	IARM_TO_LINUX_CTL( KED_LAST, KEY_L)
	IARM_TO_LINUX_CTL( KED_FAVORITE, KEY_N)
	IARM_TO_LINUX_CTL( KED_KEYA, KEY_0)
	IARM_TO_LINUX_CTL( KED_KEYB, KEY_1)
	IARM_TO_LINUX_CTL( KED_KEYC, KEY_2)
	IARM_TO_LINUX_CTL( KED_KEYD, KEY_3)
    IARM_TO_LINUX_KEY( KED_HELP, KEY_HELP)
    IARM_TO_LINUX_KEY( KED_SETUP, KEY_SETUP)
    IARM_TO_LINUX_KEY( KED_NEXT, KEY_NEXT)
    IARM_TO_LINUX_KEY( KED_PREVIOUS, KEY_PREVIOUS)
	IARM_TO_LINUX_CTL( KED_ONDEMAND, KEY_O)
        IARM_TO_LINUX_CTL( KED_REPLAY, KEY_B)
        IARM_TO_LINUX_CTL( KED_SEARCH, KEY_C)
        IARM_TO_LINUX_KEY( KED_PROGRAM, KEY_KPASTERISK)
	IARM_TO_LINUX_KEY( KED_UNDEFINEDKEY, KEY_UNKNOWN) 
};

#ifdef __cplusplus
}
#endif

#endif /* _IARM_IR_KEYCODES_H_ */


/* End of IARM_BUS_IRMGR_API doxygen group */
/**
 * @}
 */


/** @} */
/** @} */
