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
 * assistance
 *
 */

/** @defgroup IR_MANAGER_HAL IR MANAGER HAL
 *  @{
 * @par Application API Specification
 * IR HAL provides an interface to register IR events with the low level interfaces, 
 * which will notify the caller based on received IR Key events.
 */

/** @defgroup PLAT_IR_API PLAT_IR_API
 *  @{
 */


#ifndef _MOD_IRHAL_PLATFORM_
#define _MOD_IRHAL_PLATFORM_
#ifdef __cplusplus 
extern "C" {
#endif

/**
 * @file plat_ir.h
 * 
 * @brief IR HAL header
 *
 * This file defines APIs, datatypes and error codes used by the IR HAL.
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
 * - IR:      Infra red
 * - XMP:     IR protocol. Platform specific
 * - HAL:     Hardware abstraction layer
 * - XR       Remote type
 * @par Implementation Notes
 * -# None
 *
 */


/**
 * @brief IR status.
 * @todo need to rename irError_t to irStatus_t. will do it in the next phase
 */
typedef enum {
    irERR_NONE = 0,             ///< Operation is successful
    irERR_INVALID_PARAM,        ///< Invalid argument is passed to the module
    irERR_NOT_INITIALIZED,      ///< Module already intialised
    irERR_ALREADY_INITIALIZED,  ///< Module is not  intialised
    irERR_ALREADY_EXECUTED,     ///< API is already executed
    irERR_MEMORY_ERR,           ///< Memory error
    irERR_MAX                   ///< Out of range - required to be the last item of the enum
} irError_t;


/**
 * @brief xmp tag type. This is used to specify the different remote types
 * @todo need to remove the values since it is ordered. Values are given for readability
 * @todo need to define enums like typedef enum {} name_t; first name is not required
 * Need to do it in other places also. But since code is shared for multiple SOCs C file
 * will do the modification as a part of next phase
 */
typedef enum PLAT_xmp_tag {
   XMP_TAG_COMCAST = 0x00,   ///< comcast remote 
   XMP_TAG_PLATCO  = 0x01,   ///< platco remote
   XMP_TAG_XR11V2  = 0x02,   ///< XR11V2 remote
   XMP_TAG_XR15V1  = 0x03,   ///< XR15V1 remote
   XMP_TAG_XR15V2  = 0x04,   ///< XR15V2 remote
   XMP_TAG_XR16V1  = 0x05,   ///< XR16V1 remote
   XMP_TAG_XRAV1   = 0x06,   ///< XRAV1 remote
   XMP_TAG_XR20V1  = 0x07,   ///< XR20V1 remote
   XMP_TAG_PLATCOV2  = 0x08, ///< PLATCOV2 remote
   XMP_TAG_UNDEFINED,        ///< Undefined 
   XMP_TAG_MAX               ///< Out of range - required to be the last item of the enum
} PLAT_xmp_tag_t;

/**
 * @brief xmp owner type describes whether key received in pairing or normal mode
 * 
 *  @todo need to define enums like typedef enum {} name_t; first name is not required
 * Need to do it in other places also. But since code is shared for multiple SOCs C file
 * will do the modification as a part of next phase
 */
typedef enum PLAT_xmp_owner {
   XMP_OWNER_NORMAL  = 0x00,   ///< normal mode 
   XMP_OWNER_PAIRING = 0x01,   ///< paring mode
   XMP_OWNER_UNDEFINED,        ///< undefined mode
   XMP_OWNER_MAX               ///< Out of range - required to be the last item of the enum
} PLAT_xmp_owner_t;

/**
 * @brief IR Key struct to store the IR key meta data
 * 
 * @todo need to change the arguments type and code to enum. We will handle it in next phase.
 * Platform specific key maping should be handled in platform layer. But there should be generic mapping in
 * the HAL layer
 * 
 */
typedef struct PLAT_irKey_metadata {
   int              type;  ///< Event type of key press
   ///< Platform specific. See macros KET_KEYDOWN,  KET_KEYUP, KET_KEYREPEAT etc 
   int              code;  ///< Code of the pressed key
   ///< See macros KED_POWER,  KED_SELECT, KED_CHANNELUP etc 
   PLAT_xmp_tag_t   tag;   ///< Designates which device key belongs. See PLAT_xmp_tag_t
   PLAT_xmp_owner_t owner; ///< Designates how key should be handled. See PLAT_xmp_owner_t
} PLAT_irKey_metadata_t;

/**
* @brief HAL must call this function when an extended IR key is received
*
* The Event meta data contains Key Type, Code, Owner, and Tag of the IR key pressed
*
* @param[in] irKey contains data on IR key pressed. See PLAT_irKey_metadata_t
*
* @return None
*
* @note This function must not suspend and must not invoke any blocking system
* calls. It shall send a message to a HAL IR event handler task.
*/
typedef void (*PLAT_IrKeyCallback_Extended_t)(PLAT_irKey_metadata_t *irKey);

/**
 * @brief This function is used to register for Extended IR key events
 *
 * The Caller must be notified with the extended IR Key events using the callback function
 *
 * @param[in] func callback function. See PLAT_IrKeyCallback_Extended_t
 * 
 * @return None
 * 
 * @pre  PLAT_API_INIT() should be called before calling this API
 * @warning  This API is NOT thread safe. Caller shall handle the concurrency.
 * @see PLAT_API_INIT()
 * 
 * @todo: Need to return irError_t for failure. irERR_INVALID_STATE should be returned if 
 * called with out initialising.
 * 
 * @note: PLAT_API_RegisterIRKeyCallbackExtended is used in bcom. PLAT_API_RegisterIRKeyCallback is used in realtek.
 * 
 */
void PLAT_API_RegisterIRKeyCallbackExtended(PLAT_IrKeyCallback_Extended_t func);

/**
 * @brief HAL must call this function when an IR key is received
 *
 * The Event Data contains Key Type and Key Code of the IR key pressed
 *
 * @param[in] keyType  Key Type (e.g. Key Down, Key Up, Key Repeat) of the IR key pressed.
 *     Platform specific. See macros KET_KEYDOWN,  KET_KEYUP, KET_KEYREPEAT etc 
 * 
 * @param[in] keyCode  Key Code of the the IR key. Platform specific. See macros KED_POWER,  KED_SELECT, KED_CHANNELUP etc 
 *
 * @return None
 *
 * @note This function must not suspend and must not invoke any blocking system
* calls. It shall send a message to a HAL IR event handler task.
 */
typedef void (*PLAT_IrKeyCallback_t)(int keyType, int keyCode);

/**
 * @brief This function is used to register for IR key events
 *
 * The Caller must be notified with the IR Key events using the callback function
 *
 * @param[in] func - callback function ; @see PLAT_IrKeyCallback_t
 *
 * @return None
 * 
 * @pre  PLAT_API_INIT() should be called before calling this API
 * @warning  This API is NOT thread safe. Caller shall handle the concurrency
 * @see PLAT_API_INIT()
 * @todo: Need to return irError_t for failure. irERR_INVALID_STATE should be returned if 
 * called without initialising
 * 
 */
void PLAT_API_RegisterIRKeyCallback(PLAT_IrKeyCallback_t func);

/**
 * @brief This API initializes the underlying IR module
 *
 * This function must initialize all the IR specific user input device modules
 *
 * @param None
 * 
 * @return irError_t                  - Status
 * @retval irERR_NONE                 - Success
 * @retval irERR_MEMORY_ERR           - Memory error
 * @retval irERR_ALREADY_INITIALIZED  - Module is already open
 * @warning  This API is NOT thread safe. Caller shall handle the concurrency
 * @see PLAT_API_TERM()
 * 
 * @todo: Will be renamed later. To avoid compilation issues, time being keeping the name unchanged
 * 
 * @todo: in all open/init scenarios. Change the irERR_INVALID_STATE 
 *   to irERR_ALREADY_OPEN. Will do it in the next phase
 * 
 */
int  PLAT_API_INIT(void);

/**
 * @brief This API is used to terminate the IR device module
 *
 * This function must terminate all the IR specific user input device modules. It must
 * reset any data structures used within IR module and release any IR specific handles
 * and resources
 * 
 * @param None
 * 
 * @return None
 * 
 * @pre  PLAT_API_INIT() should be called before calling this API
 * @warning  This API is NOT thread safe. Caller shall handle the concurrency
 * @see PLAT_API_INIT()
 * @todo: Need to return irError_t for failure
 */
void PLAT_API_TERM(void);

/**
 * @brief This function executes the  key event loop
 *
 * This function executes the platform-specific key event loop. This will generally
 * translate between platform-specific key codes and Comcast standard keycode definitions
 * 
 * @param None
 * 
 * @return None
 * 
 * @pre  PLAT_API_INIT() should be called before calling this API
 * @warning  This API is NOT thread safe. Caller shall handle the concurrency
 * 
 * @todo: Need to return irError_t for failure
 * 
 */
void PLAT_API_LOOP();

/** @} */ //End of Doxygen Tag

#ifdef __cplusplus 
}
#endif /* __cplusplus */
#endif


/** @} */ // End of PLAT_IR_API
/** @} */ // End of IR_MANAGER_HAL
/** @} */ // End of HPK
