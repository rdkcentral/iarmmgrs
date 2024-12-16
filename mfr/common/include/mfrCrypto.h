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


#ifndef _MFR_CRYPTO_H
#define _MFR_CRYPTO_H

#include <stdlib.h>
#include <stdint.h>
#include "mfrTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

mfrError_t mfrCrypto_init( const mfrSerializedData_t * pSerialNumber );
mfrError_t mfrCrypto_Encrypt(const mfrSerializedData_t * pPlainText, mfrSerializedData_t * pCipherText);
mfrError_t mfrCrypto_Decrypt(const mfrSerializedData_t * pCipherText, mfrSerializedData_t * pPlainText);
mfrError_t mfrCrypto_term();
#ifdef __cplusplus
};
#endif


#endif //_MFR_CRYPTO_H



/** @} */
/** @} */
