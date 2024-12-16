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


#ifndef __MFR_API_H__
#define __MFR_API_H__
/*-----------------------------------------------------------------*/
/*-------------------------------------------------------------------
   Include Files
-------------------------------------------------------------------*/
#include "mfrTypes.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned long VL_MFR_DEVICE_HANDLE_t;

typedef enum _VL_MFR_API_RESULT
{   // ENUM additions are allowed. Insertions, deletions and value changes are not allowed.
    VL_MFR_API_RESULT_SUCCESS                                   = 0,
    VL_MFR_API_RESULT_FAILED                                    = 1,
    VL_MFR_API_RESULT_NOT_EXISTING                              = 6,
    VL_MFR_API_RESULT_NULL_PARAM                                = 7,
    VL_MFR_API_RESULT_OUT_OF_RANGE                              = 9,
    VL_MFR_API_RESULT_OPEN_FAILED                               = 10,
    VL_MFR_API_RESULT_READ_FAILED                               = 11,
    VL_MFR_API_RESULT_WRITE_FAILED                              = 12,
    VL_MFR_API_RESULT_MALLOC_FAILED                             = 13,
    VL_MFR_API_RESULT_DECRYPTION_FAILED                         = 15,
    VL_MFR_API_RESULT_NULL_KEY                                  = 16,
    VL_MFR_API_RESULT_INVALID_KEY_LENGTH                        = 17,
    VL_MFR_API_RESULT_INVALID_BUFFER_LENGTH                     = 18,
    VL_MFR_API_RESULT_SIZE_MISMATCH                             = 19,

}VL_MFR_API_RESULT;


typedef enum _VL_PLATFORM_VERSION_TYPE
{   // ENUM additions are allowed. Insertions, deletions and value changes are not allowed.
    VL_PLATFORM_VERSION_TYPE_BOARD_VERSION                      = 0,
    VL_PLATFORM_VERSION_TYPE_BOARD_SERIAL_NO                    = 1,
    VL_PLATFORM_VERSION_TYPE_CM_CHIP_VERSION                    = 2,
    VL_PLATFORM_VERSION_TYPE_DECODER_SW_VERSION                 = 7,
    VL_PLATFORM_VERSION_TYPE_OS_KERNEL_VERSION                  = 8,
    VL_PLATFORM_VERSION_TYPE_MFR_LIB_VERSION                    = 11,
    VL_PLATFORM_VERSION_TYPE_FRONT_PANEL_VERSION                = 12,
    VL_PLATFORM_VERSION_TYPE_SOFTWARE_IMAGE_VERSION             = 14,
    VL_PLATFORM_VERSION_TYPE_OCHD_VERSION                       = 16,
    VL_PLATFORM_VERSION_TYPE_OCAP_VERSION                       = 18,
    VL_PLATFORM_VERSION_TYPE_BOOT_ROM_VERSION                   = 20,
    VL_PLATFORM_VERSION_TYPE_MODEL_NUMBER                       = 22,
    VL_PLATFORM_VERSION_TYPE_MODEL_SERIAL_NO                    = 23,
    VL_PLATFORM_VERSION_TYPE_VENDOR_NAME                        = 24,
    VL_PLATFORM_VERSION_TYPE_VENDOR_SERIAL_NO                   = 25,
    VL_PLATFORM_VERSION_TYPE_MANUFACTURE_DATE                   = 26,
} VL_PLATFORM_VERSION_TYPE;


typedef enum _VL_NORMAL_NVRAM_DATA_TYPE
{   // ENUM additions are allowed. Insertions, deletions and value changes are not allowed.
   
    VL_NORMAL_NVRAM_DATA_BOOT_IMAGE_NAME                    = 0x70000001,// max: 256 bytes : typically 128 bytes : name of current monolithic image
    VL_NORMAL_NVRAM_DATA_BOOT_FIRMWARE_IMAGE_NAME           = 0x70000010,// max: 256 bytes : typically 128 bytes : name of current firmware image name
    VL_NORMAL_NVRAM_DATA_BOOT_APPLICATION_IMAGE_NAME        = 0x70000011,// max: 256 bytes : typically 128 bytes : name of current application image name
    VL_NORMAL_NVRAM_DATA_BOOT_DATA_IMAGE_NAME               = 0x70000012,// max: 256 bytes : typically 128 bytes : name of current data image name
   
    VL_NORMAL_NVRAM_DATA_CACP_AuthStatus                    = 0x70000100,// max: 16  bytes : typically 1   bytes :
    VL_NORMAL_NVRAM_DATA_CACP_HostId                        = 0x70000101,// max: 16  bytes : typically 5   bytes :
    VL_NORMAL_NVRAM_DATA_COM_DWNLD_CO_SIGN_NAME             = 0x70000211,// max: 256 bytes : typically 128 bytes : Co-Signer Name of the device
    VL_NORMAL_NVRAM_DATA_COM_DWNL_VEN_ID                    = 0x70000212,// max: 16  bytes : typically 3   bytes : Vendor Id of the device which
    VL_NORMAL_NVRAM_DATA_COM_DWNL_HW_ID                     = 0x70000213,// max: 16  bytes : typically 4   bytes : Hardware ID of the dvice which
    //VL_NORMAL_NVRAM_DATA_COM_DWNL_CODE_FILE_NAME            = 0x70000214,// max: 256 bytes : typically 128 bytes : Current running Code file name of the device, identical to VL_NORMAL_NVRAM_DATA_BOOT_IMAGE_NAME, (not used)
    VL_NORMAL_NVRAM_DATA_COM_DWNL_MFR_CODE_ACC_STR_TIME     = 0x70000215,// max: 32  bytes : typically 12  bytes : Manufacturer Code Access Start Time Register
    VL_NORMAL_NVRAM_DATA_COM_DWNL_CO_SIGN_CODE_ACC_STR_TIME = 0x70000216,// max: 32  bytes : typically 12  bytes : Co-Signer Code Access Start Time Register
    VL_NORMAL_NVRAM_DATA_COM_DWNL_MFR_CVC_ACC_STR_TIME      = 0x70000217,// max: 32  bytes : typically 12  bytes : Manufacturer CVC Access Start Time Register
    VL_NORMAL_NVRAM_DATA_COM_DWNL_CO_SIGN_CVC_ACC_STR_TIME  = 0x70000218,// max: 32  bytes : typically 12  bytes : Co-Signer CVC Start Time Register
    VL_NORMAL_NVRAM_DATA_COM_DWNLD_MGR_STATUS               = 0x70000219,// max: 16  bytes : typically 4   bytes : This is the  code image down load status maintained by the Common Download Manager
    VL_NORMAL_NVRAM_DATA_CDL_MFR_CODE_ACC_UPG_STR_TIME      = 0x7000021C,// max: 32  bytes : typically 12  bytes : Manufacturer Code Access Start Time Register
    VL_NORMAL_NVRAM_DATA_CDL_CO_SIGN_CODE_ACC_UPG_STR_TIME  = 0x7000021D,// max: 32  bytes : typically 12  bytes : Co-Signer Code Access Start Time Register

    VL_NORMAL_NVRAM_DATA_CDL_FW_MFR_CODE_ACC_UPG_STR_TIME       = 0x70000300,// max: 32  bytes : typically 12  bytes : Manufacturer Code Access Start Time Register
    VL_NORMAL_NVRAM_DATA_CDL_FW_CO_SIGN_CODE_ACC_UPG_STR_TIME   = 0x70000301,// max: 32  bytes : typically 12  bytes : Co-Signer Code Access Start Time Register
    VL_NORMAL_NVRAM_DATA_CDL_APP_MFR_CODE_ACC_UPG_STR_TIME      = 0x70000302,// max: 32  bytes : typically 12  bytes : Manufacturer Code Access Start Time Register
    VL_NORMAL_NVRAM_DATA_CDL_APP_CO_SIGN_CODE_ACC_UPG_STR_TIME  = 0x70000303,// max: 32  bytes : typically 12  bytes : Co-Signer Code Access Start Time Register
    VL_NORMAL_NVRAM_DATA_CDL_DATA_MFR_CODE_ACC_UPG_STR_TIME     = 0x70000304,// max: 32  bytes : typically 12  bytes : Manufacturer Code Access Start Time Register
    VL_NORMAL_NVRAM_DATA_CDL_DATA_CO_SIGN_CODE_ACC_UPG_STR_TIME = 0x70000305,// max: 32  bytes : typically 12  bytes : Co-Signer Code Access Start Time Register

    VL_NORMAL_NVRAM_DATA_CDL_FW_MFR_CODE_ACC_BOOT_STR_TIME      = 0x70000320,// max: 32  bytes : typically 12  bytes : Manufacturer Code Access Start Time Register
    VL_NORMAL_NVRAM_DATA_CDL_FW_CO_SIGN_CODE_ACC_BOOT_STR_TIME  = 0x70000321,// max: 32  bytes : typically 12  bytes : Co-Signer Code Access Start Time Register
    VL_NORMAL_NVRAM_DATA_CDL_APP_MFR_CODE_ACC_BOOT_STR_TIME     = 0x70000322,// max: 32  bytes : typically 12  bytes : Manufacturer Code Access Start Time Register
    VL_NORMAL_NVRAM_DATA_CDL_APP_CO_SIGN_CODE_ACC_BOOT_STR_TIME = 0x70000323,// max: 32  bytes : typically 12  bytes : Co-Signer Code Access Start Time Register
    VL_NORMAL_NVRAM_DATA_CDL_DATA_MFR_CODE_ACC_BOOT_STR_TIME    = 0x70000324,// max: 32  bytes : typically 12  bytes : Manufacturer Code Access Start Time Register
    VL_NORMAL_NVRAM_DATA_CDL_DATA_CO_SIGN_CODE_ACC_BOOT_STR_TIME= 0x70000325,// max: 32  bytes : typically 12  bytes : Co-Signer Code Access Start Time Register

    VL_NORMAL_NVRAM_DATA_IEEE1394_DTCP_KEY_TYPE             = 0x70000500,// max: 16  bytes : typically 1   bytes : DTCP key type
    VL_NORMAL_NVRAM_DATA_IEEE1394_DTCP_KEY_STATUS           = 0x70000501,// max: 16  bytes : typically 1   bytes : DTCP key status

}VL_NORMAL_NVRAM_DATA_TYPE;



typedef enum _VL_SECURE_NVRAM_DATA_TYPE
{   // ENUM additions are allowed. Insertions, deletions and value changes are not allowed.
    // Certificates and Keys for Cable Card Conditional Access and Copy Protection
    VL_SECURE_NVRAM_DATA_CACP_HOST_ROOT_CERT                = 0x100,// max: 2048 bytes : typically 1024 bytes : Cable Labs Root certificate
    VL_SECURE_NVRAM_DATA_CACP_HOST_MFR_CA_CERT              = 0x101,// max: 2048 bytes : typically 1024 bytes : Cable Labs Manufacturer CA Certificate
    VL_SECURE_NVRAM_DATA_CACP_HOST_DEV_CERT                 = 0x102,// max: 2048 bytes : typically 1024 bytes : Host Device Certificate
    VL_SECURE_NVRAM_DATA_CACP_HOST_DEV_PRIVATE_KEY          = 0x103,// max: 2048 bytes : typically 1024 bytes : Host Device Private Key
    VL_SECURE_NVRAM_DATA_CACP_DH_B_G1                       = 0x104,// max: 256  bytes : typically 128  bytes : Diffie-Hellman base   ( g )
    VL_SECURE_NVRAM_DATA_CACP_DH_P_N1                       = 0x105,// max: 256  bytes : typically 128  bytes : Diffie-Hellman prime  ( n )
    VL_SECURE_NVRAM_DATA_CACP_DHKey                         = 0x106,// max: 256  bytes : typically 128  bytes :
    VL_SECURE_NVRAM_DATA_CACP_AuthKeyH                      = 0x107,// max: 64   bytes : typically 20   bytes :
    //Cable Labs Code Verification Certificates for Common Download
    VL_SECURE_NVRAM_DATA_COM_DWNLD_CL_CODE_VER_ROOT_CA      = 0x200,// max: 2048 bytes : typically 1024 bytes : Cable Labs Code Verification Root CA
    VL_SECURE_NVRAM_DATA_COM_DWNLD_CL_CVC_CA                = 0x201,// max: 2048 bytes : typically 1024 bytes : Cable Labs CVC CA
    VL_SECURE_NVRAM_DATA_COM_DWNLD_MFR_CVC                  = 0x202,// max: 2048 bytes : typically 1024 bytes : Manufacturer CVC

    //Common Download ( code image download NV data)
    VL_SECURE_NVRAM_DATA_COM_DWNLD_CVC_CA_PUB_KEY           = 0x219,// max: 2048 bytes : typically 1024 bytes : CVC CA public Key stored by the host device

    //Cable Labs Application Code verification Certificates
    //VL_SECURE_NVRAM_DATA_COM_DWNLD_CL_CODE_VER_ROOT_CA     = 0x300,// max: 2048 bytes : typically 1024 bytes : Cable Labs Code Verification Root CA same as above 0x100
    VL_SECURE_NVRAM_DATA_APP_CL_APP_CVC_CA                  = 0x301,// max: 2048 bytes : typically 1024 bytes : Cable Labs Application CVC CA
    VL_SECURE_NVRAM_DATA_APP_MAN_CVC                        = 0x302,// max: 2048 bytes : typically 1024 bytes : Cable Labs Application Manufacturer CVCs

    VL_SECURE_NVRAM_DATA_SERIAL_NUMBER                      = 0x400,// max: 32   bytes : typically 16   bytes :

    //Ieee1394 DTCP certificates and Keys
    VL_SECURE_NVRAM_DATA_IEEE1394_DTCP_CERT_KEYS            = 0x500,// max: 2048 bytes : typically 1024 bytes : DTCP keys for 1394 CP
    VL_SECURE_NVRAM_DATA_IEEE1394_DTCP_SRM                  = 0x501,// max: 4096 bytes : typically 1024 bytes : DTCP SRM
    VL_SECURE_NVRAM_DATA_IEEE1394_DTCP_SEED                 = 0x502,// max: 64   bytes : typically 24   bytes : DTCP SEED

    // VL certificates and Keys
    VL_SECURE_NVRAM_DATA_VL_CERT_KEYS                       = 0x600,// max: 1024 bytes : typically 512  bytes : VividLogic certificates

}VL_SECURE_NVRAM_DATA_TYPE;


typedef enum _VL_STACK_2_MFR_EVENT_TYPE
{   // ENUM additions are allowed. Insertions, deletions and value changes are not allowed.

    VL_STACK_2_MFR_EVENT_SET_UPGRADE_TO_IMAGE               = 0x20000100,
    VL_STACK_2_MFR_EVENT_SET_REBOOT_WITH_UPGRADED_IMAGE     = 0x20000101,
    VL_STACK_2_MFR_EVENT_SET_UPGRADE_IMAGE_NAME             = 0x20000102,
    VL_STACK_2_MFR_EVENT_SET_UPGRADE_SUCCEEDED              = 0x20000103,
    VL_STACK_2_MFR_EVENT_SET_UPGRADE_FAILED                 = 0x20000104,

    VL_STACK_2_MFR_EVENT_get_CONFIG_PATH                    = 0x20000200,
    VL_STACK_2_MFR_EVENT_get_SNMP_ENTERPRISE_ID             = 0x20000201,

    VL_STACK_2_MFR_EVENT_get_CALL_DFAST2                    = 0x20000301,

    VL_STACK_2_MFR_EVENT_get_PREVIOUS_BOOT_IMAGE_NAME       = 0x20000400,
    VL_STACK_2_MFR_EVENT_get_CURRENT_BOOT_IMAGE_NAME        = 0x20000401,
    VL_STACK_2_MFR_EVENT_get_UPGRADE_IMAGE_NAME             = 0x20000402,
    VL_STACK_2_MFR_EVENT_get_UPGRADE_STATUS                 = 0x20000403,

    VL_STACK_2_MFR_EVENT_SET_CARD_TYPE                      = 0x20000700,
    VL_STACK_2_MFR_EVENT_SET_FIRMWARE_VERSION_INFO          = 0x20000701,

} VL_STACK_2_MFR_EVENT_TYPE;

/*
struct VL_NVRAM_DATA
nSize is the size of the data read from the NVRAM.
pData is the data buffer of the data read from the NVRAM.
*/
typedef struct _VL_NVRAM_DATA
{   // STRUCT member additions are allowed. Insertions, deletions, datatype and name changes are not allowed.
    unsigned long       nActualBytes;
    int                 nBytes;
    unsigned char   *   pData;
}VL_NVRAM_DATA;

int MFR_Init (void);
int MFR_Shutdown (void);
    

VL_MFR_API_RESULT HAL_MFR_get_version     ( VL_MFR_DEVICE_HANDLE_t hMFRHandle, VL_PLATFORM_VERSION_TYPE eVersionType, char ** ppString);
    
VL_MFR_API_RESULT HAL_MFR_read_normal_nvram ( VL_MFR_DEVICE_HANDLE_t hMFRHandle, VL_NORMAL_NVRAM_DATA_TYPE eType, VL_NVRAM_DATA * pNvRamData);
VL_MFR_API_RESULT HAL_MFR_write_normal_nvram( VL_MFR_DEVICE_HANDLE_t hMFRHandle, VL_NORMAL_NVRAM_DATA_TYPE eType, const VL_NVRAM_DATA * pNvRamData);

VL_MFR_API_RESULT HAL_MFR_read_secure_nvram ( VL_MFR_DEVICE_HANDLE_t hMFRHandle, VL_SECURE_NVRAM_DATA_TYPE eType, VL_NVRAM_DATA * pNvRamData);
VL_MFR_API_RESULT HAL_MFR_write_secure_nvram( VL_MFR_DEVICE_HANDLE_t hMFRHandle, VL_SECURE_NVRAM_DATA_TYPE eType, const VL_NVRAM_DATA * pNvRamData);

VL_MFR_API_RESULT HAL_MFR_set_mfr_data( VL_MFR_DEVICE_HANDLE_t hMFRHandle, VL_STACK_2_MFR_EVENT_TYPE eEvent, void * _pvData);
VL_MFR_API_RESULT HAL_MFR_get_mfr_data( VL_MFR_DEVICE_HANDLE_t hMFRHandle, VL_STACK_2_MFR_EVENT_TYPE eEvent, void * _pvData);
/*-----------------------------------------------------------------*/
#ifdef __cplusplus
}
#endif
/*-----------------------------------------------------------------*/
#endif //__MFR_API_H__
/*-----------------------------------------------------------------*/


/** @} */
/** @} */
