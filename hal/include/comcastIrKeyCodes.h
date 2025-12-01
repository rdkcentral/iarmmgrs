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
 * @addtogroup HPK HPK
 * @{
 * @par The Hardware Porting Kit
 * HPK is the next evolution of the well-defined Hardware Abstraction Layer
 * (HAL), but augmented with more comprehensive documentation and test suites
 * that OEM or SOC vendors can use to self-certify their ports before taking
 * them to RDKM for validation or to an operator for final integration and
 * deployment. The Hardware Porting Kit effectively enables an OEM and/or SOC
 * vendor to self-certify their own Video Accelerator devices, with minimal RDKM
 * assistance.
 *
 */

/** @addtogroup IR_MANAGER_HAL IR MANAGER HAL
 *  @{
 * @par Application API Specification
 * IR HAL provides an interface to register IR events with the low level interfaces, 
 * which will notify the caller based on received IR Key events.
 */


/** @defgroup comcastIrKeyCodes comcastIrKeyCodes
 *  @{
 */


#ifndef _IR_MANAGER_KEYMAPS_H_
#define _IR_MANAGER_KEYMAPS_H_

#ifdef __cplusplus
extern "C" {
#endif



/**
 * @file comcastIrKeyCodes.h
 * 
 * @brief IR key map header
 *
 * This file contains key definitions used by the IR HAL.
 *
 * @par Document
 * Document reference.
 *
 * @par Open Issues (in no particular order)
 * -# None
 *
 * @par Assumptions
 * -# None
 *
 * @par Abbreviations
 * - XR:       Remote type
 * - V:        Version
 * - PIP:      Picture in picture
 * - WPS:      Wi-Fi Protected Setup
 * - DMC:      Digital Media Controller (for activate/deactivate/query operations)
 * - OTR:      One Touch Record (for start/stop recording operations)
 * 
 * @par Implementation Notes
 * -# None
 *
 */

/*-------------------------------------------------------------------
   Defines/Macros

   All definitions are prefixed with IR_ for namespace isolation.
-------------------------------------------------------------------*/
///< Represents the mask for delay and repeat.
#define IR_DELAY_REPEAT_MASK                  0x00000001 
///< Represents the mask for the language selection.
#define IR_LANGUAGE_MASK                      0x00000010
///< Represents the mask for the missed key timeout.
#define IR_MISSED_KEY_TIMEOUT_MASK            0x00000100
///< Represents the mask for enabling key repeat.
#define IR_REPEAT_KEY_ENABLED_MASK            0x00001000
///< Represents the mask for the key repeat frequency.
#define IR_REPEAT_FREQUENCY_MASK              0x00010000
///< Represents the mask for reporting modifiers.
#define IR_REPORT_MODIFIERS_MASK              0x00100000
///< Represents the mask for the number of devices.
#define IR_NUM_OF_DEVICES_MASK                0x01000000

///< Represents the key down event.
#define IR_KET_KEYDOWN		                  0x00008000UL
///< Represents the key up event.
#define IR_KET_KEYUP		                  0x00008100UL
///< Represents the key repeat event.
#define IR_KET_KEYREPEAT	                  0x00008200UL 

/* Numeric keys (common in Remote and Key Board) */
/* Combination of Key + device type (HID_DEVICES) is unique */
///< Represents the key "0".
#define IR_KED_DIGIT0		                  0x00000030UL
///< Represents the key "1".
#define IR_KED_DIGIT1		                  0x00000031UL
///< Represents the key "2".
#define IR_KED_DIGIT2		                  0x00000032UL
///< Represents the key "3".
#define IR_KED_DIGIT3		                  0x00000033UL
///< Represents the key "4".
#define IR_KED_DIGIT4		                  0x00000034UL
///< Represents the key "5".
#define IR_KED_DIGIT5		                  0x00000035UL
///< Represents the key "6".
#define IR_KED_DIGIT6		                  0x00000036UL
///< Represents the key "7".
#define IR_KED_DIGIT7		                  0x00000037UL
///< Represents the key "8".
#define IR_KED_DIGIT8		                  0x00000038UL
///< Represents the key "9".
#define IR_KED_DIGIT9		                  0x00000039UL
///< Represents the key period (decimal point).
#define IR_KED_PERIOD		                  0x00000040UL

///< Represents the key for discrete power on.
#define IR_KED_DISCRETE_POWER_ON              0x00000050UL
///< Represents the key for discrete power standby.
#define IR_KED_DISCRETE_POWER_STANDBY         0x00000051UL

///< Represents the key for search.
#define IR_KED_SEARCH		                  0x000000CFUL
///< Represents the key for setup.
#define IR_KED_SETUP		                  0x00000052UL

///< Represents the key for closed captioning. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_CLOSED_CAPTIONING                 0x00000060UL
///< Represents the key for language selection. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_LANGUAGE                          0x00000061UL
///< Represents the key for voice guidance. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_VOICE_GUIDANCE                    0x00000062UL
///< Represents the key for heartbeat. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_HEARTBEAT                         0x00000063UL
///< Represents the key for push-to-talk. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_PUSH_TO_TALK                      0x00000064UL
///< Represents the key for descriptive audio. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_DESCRIPTIVE_AUDIO                 0x00000065UL
///< Represents the key for volume optimization. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_VOLUME_OPTIMIZE                   0x00000066UL

///< Represents the key for screen bind notify. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_SCREEN_BIND_NOTIFY                0x00000073UL

///< Represents the key for RF power. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_RF_POWER	                         0x0000007FUL
///< Represents the key for power. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_POWER		                         0x00000080UL
///< Represents the key for FP power. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_FP_POWER		                      KED_POWER
///< Represents the key for arrow up. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_ARROWUP		                      0x00000081UL
///< Represents the key for arrow down. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_ARROWDOWN		                   0x00000082UL
///< Represents the key for arrow left. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_ARROWLEFT		                   0x00000083UL
///< Represents the key for arrow right. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_ARROWRIGHT	  	                   0x00000084UL
///< Represents the key for select. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_SELECT		                      0x00000085UL
///< Represents the key for enter. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_ENTER		                         0x00000086UL
///< Represents the key for exit. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_EXIT	  	                         0x00000087UL
///< Represents the key for channel up. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_CHANNELUP	  	                   0x00000088UL
///< Represents the key for channel down. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_CHANNELDOWN	  	                   0x00000089UL
///< Represents the key for volume up. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_VOLUMEUP	  	                      0x0000008AUL
///< Represents the key for volume down. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_VOLUMEDOWN	  	                   0x0000008BUL
///< Represents the key for mute. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_MUTE	  	                         0x0000008CUL
///< Represents the key for guide. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_GUIDE	  	                         0x0000008DUL
///< Represents the key for viewing guide. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_VIEWINGGUIDE  	                   KED_GUIDE
///< Represents the key for info. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_INFO	  	                         0x0000008EUL
///< Represents the key for settings. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_SETTINGS	  	                      0x0000008FUL
///< Represents the key for page up. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_PAGEUP	  	                      0x00000090UL
///< Represents the key for page down. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_PAGEDOWN	  	                      0x00000091UL
///< Represents the key "A". @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_KEYA	  	                         0x00000092UL
///< Represents the key "B". @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_KEYB	  	                         0x00000093UL
///< Represents the key "C". @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_KEYC	  	                         0x00000094UL
///< Represents the key "D". @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_KEYD	  	                         0x0000009FUL	
///< Represents the key for the last channel. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_LAST	  	                         0x00000095UL
///< Represents the key for favorite. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_FAVORITE	  	                      0x00000096UL
///< Represents the key for rewind. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_REWIND	  	                      0x00000097UL
///< Represents the key for fast forward. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_FASTFORWARD	  	                   0x00000098UL
///< Represents the key for play. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_PLAY	  	                         0x00000099UL
///< Represents the key for stop. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_STOP	  	                         0x0000009AUL
///< Represents the key for pause. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_PAUSE		                         0x0000009BUL
///< Represents the key for record. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_RECORD		                      0x0000009CUL
///< Represents the key for bypass. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_BYPASS		                      0x0000009DUL
///< Represents the key for TV/VCR. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_TVVCR		                         0x0000009EUL

///< Represents the key for replay. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_REPLAY                            0x000000A0UL
///< Represents the key for help. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_HELP                              0x000000A1UL
///< Represents the key for recalling favorite 0. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_RECALL_FAVORITE_0                 0x000000A2UL
///< Represents the key for clear. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_CLEAR                             0x000000A3UL
///< Represents the key for delete. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_DELETE                            0x000000A4UL
///< Represents the key for start. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_START                             0x000000A5UL
///< Represents the key for pound. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_POUND                             0x000000A6UL
///< Represents the key for front panel 1. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_FRONTPANEL1                       0x000000A7UL
///< Represents the key for front panel 2. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_FRONTPANEL2                       0x000000A8UL
///< Represents the key for OK. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_OK   		                         0x000000A9UL
///< Represents the key for star. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_STAR                              0x000000AAUL
///< Represents the key for program. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_PROGRAM                           0x000000ABUL

///< Represents the key for TV power (alternate remote). @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_TVPOWER		                      0x000000C1UL
///< Represents the key for previous (alternate remote). @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_PREVIOUS		                      0x000000C3UL
///< Represents the key for next (alternate remote). @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_NEXT		                         0x000000C4UL
///< Represents the key for menu (alternate remote). @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_MENU       		                   0x000000C0UL
///< Represents the key for input key. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_INPUTKEY		                      0x000000D0UL
///< Represents the key for live. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_LIVE		                         0x000000D1UL
///< Represents the key for MyDVR. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_MYDVR		                         0x000000D2UL
///< Represents the key for on-demand. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_ONDEMAND		                      0x000000D3UL
///< Represents the key for STB menu. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_STB_MENU                          0x000000D4UL
///< Represents the key for audio. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_AUDIO                             0x000000D5UL
///< Represents the key for factory. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_FACTORY                           0x000000D6UL
///< Represents the key for RF enable. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_RFENABLE                          0x000000D7UL
///< Represents the key for list. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_LIST		                         0x000000D8UL
///< Represents the key for RF pair ghost. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_RF_PAIR_GHOST	                   0x000000EFUL	/* Ghost code to implement auto pairing in RF remotes */ 
///< Represents the key for WPS. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_WPS                               0x000000F0UL  /*  Key to initiate WiFi WPS pairing */
///< Key to initiate deepsleep wakeup. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_DEEPSLEEP_WAKEUP                  0x000000F1UL  /*  Key to initiate deepsleep wakeup */
///< Signals a battery set was replaced. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_NEW_BATTERIES_INSERTED            0x000000F2UL  /*  Signals a battery set was replaced */
///< Signals an external power supply device is shutting down. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_GRACEFUL_SHUTDOWN                 0x000000F3UL  /*  Signals an external power supply device is shutting down */
///< Use for keys not defined here.  Pass raw code as well. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_UNDEFINEDKEY	                   0x000000FEUL	/*  Use for keys not defined here.  Pass raw code as well. */


///< Represents the key for back. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_BACK		                         0x100000FEUL
///< Represents the key for display swap. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_DISPLAY_SWAP	                   0x300000FEUL
///< Represents the key for PIP move. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_PINP_MOVE		                   0x400000FEUL
///< Represents the key for PIP toggle. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_PINP_TOGGLE		                   0x500000FEUL
///< Represents the key for PIP channel down. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_PINP_CHDOWN		                   0x600000FEUL
///< Represents the key for PIP channel up. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_PINP_CHUP		                   0x700000FEUL
///< Represents the key for DMC activate. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_DMC_ACTIVATE	                   0x800000FEUL
///< Represents the key for DMC deactivate. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_DMC_DEACTIVATE	                   0x900000FEUL
///< Represents the key for DMC query. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_DMC_QUERY		                   0xA00000FEUL
///< Represents the key for OTR start. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_OTR_START		                   0xB00000FEUL
///< Represents the key for OTR stop. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_OTR_STOP		                      0xC00000FEUL
///< Represents the key for test. @todo : All the definitions should be prefixed with IR_. Will do it in the next phase
#define KED_TEST                              0xD00000FEUL

#ifdef __cplusplus
}
#endif

#endif /* _COMCAST_IR_KEYCODES_H_ */


/** @} */ // End of comcastIrKeyCodes
/** @} */ // End of IR_MANAGER_HAL
/** @} */ // End of HPK
