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



#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <openssl/aes.h>
#include "mfrCrypto.h"
#include "safec_lib.h"

#define MFR_CRYPTO_KEY_LENGTH    16
#define MFR_CRYPTO_IVEC_LENGTH   16

static mfrSerializedData_t serialNumber;

static void mfr_create_key(const mfrSerializedData_t * pSerialNumber, unsigned char * keybuf, unsigned char * ivec)
{// this function should be static
    
    int i = 0, j = 0, k = 0, keylen = pSerialNumber->bufLen;
    unsigned char normalizedkeybuf[MFR_CRYPTO_KEY_LENGTH] = {0};
    unsigned char refkey[MFR_CRYPTO_KEY_LENGTH] = {0xD6, 0xC1, 0x5C, 0x1E, 0x1D, 0x64, 0x2D, 0x44,
                                                  0x68, 0x5C, 0xCE, 0xA0, 0x8D, 0x9F, 0x85, 0xCB};
    unsigned char digkey[MFR_CRYPTO_KEY_LENGTH] = {0x2D, 0x6C, 0x1B, 0xFB, 0x7B, 0x86, 0x87, 0x35,
                                                  0xD0, 0x81, 0xD9, 0x6D, 0x25, 0x2B, 0xFB, 0xF1};
    errno_t safec_rc = -1;

    if(keylen > MFR_CRYPTO_KEY_LENGTH) keylen = MFR_CRYPTO_KEY_LENGTH;
    if(keylen <= 0) keylen = 1;
    safec_rc = memset_s(keybuf, MFR_CRYPTO_KEY_LENGTH, 0, MFR_CRYPTO_KEY_LENGTH);
    ERR_CHK(safec_rc);
    // normalize the key strength
    for(i = 0, k = 0; i < MFR_CRYPTO_KEY_LENGTH; i+= keylen, k = !k)
    {
        for(j = 0; (j < keylen) && ((j+i) < MFR_CRYPTO_KEY_LENGTH); j++)
        {
            unsigned char ix  = (unsigned char)((j+7)%keylen);
            unsigned char ir  = (unsigned char)((j+3)%MFR_CRYPTO_KEY_LENGTH);
            unsigned char cSN = (pSerialNumber->buf[ix]);
            
            if(0==cSN) cSN = refkey[ir];
            
            if(k)
            {
                normalizedkeybuf[i+j] = (cSN^0xFF);
            }
            else
            {
                normalizedkeybuf[i+j] = (cSN);
            }
        }
    }
    
    // create a digested version of the normalized key
    {
        AES_KEY ctx;
        unsigned char digestkeybuf[MFR_CRYPTO_KEY_LENGTH] = {0};
        unsigned char iv[AES_BLOCK_SIZE] = {0};
        
        safec_rc = memcpy_s(digestkeybuf, keylen, pSerialNumber->buf, keylen);
        if(safec_rc != EOK)
            {
               ERR_CHK(safec_rc);
               return;
            }
        for(i = 0; i < MFR_CRYPTO_KEY_LENGTH; i+= 1)
        {
            digestkeybuf[i] += (unsigned char)(digkey[i]+refkey[i]);
        }
        
        /* set up the AES key structure */
        AES_set_encrypt_key(digestkeybuf, 128, &ctx);
        // generate the digested key
        AES_cbc_encrypt(normalizedkeybuf, keybuf, AES_BLOCK_SIZE, &ctx, iv, AES_ENCRYPT);
        // generate the digested ivec
        AES_cbc_encrypt(keybuf, ivec, AES_BLOCK_SIZE, &ctx, iv, AES_ENCRYPT);
    }
}

mfrError_t mfrCrypto_init( const mfrSerializedData_t * pSerialNumber )
{
    errno_t safec_rc = -1;

    if((NULL == pSerialNumber) || (NULL == pSerialNumber->buf) || (pSerialNumber->bufLen<= 0))
    {
        return mfrERR_INVALID_PARAM;
    }
    safec_rc = memcpy_s(&serialNumber, sizeof( mfrSerializedData_t ), pSerialNumber, sizeof( mfrSerializedData_t ));
    if(safec_rc != EOK)
        {
            ERR_CHK(safec_rc);
            return mfrERR_INVALID_PARAM;
        }

    return mfrERR_NONE; 
}

mfrError_t mfrCrypto_term( )
{
    return mfrERR_NONE;
}

mfrError_t mfrCrypto_Encrypt(const mfrSerializedData_t * pPlainText, mfrSerializedData_t * pCipherText)
{// this function should be static
    
    AES_KEY ctx;
    int count = 0;
    unsigned char keybuf[MFR_CRYPTO_KEY_LENGTH];
    unsigned char iv[AES_BLOCK_SIZE];
    int nBytes           = pPlainText->bufLen;
    unsigned char * pbuf = (unsigned char *) pPlainText->buf;
    unsigned char * cbuf = NULL;
    errno_t safec_rc = -1;

    pCipherText->buf = NULL;
    
    // parameter check
    if(NULL == pbuf) return mfrERR_INVALID_PARAM;
    
    // block size check
    if(0 != (nBytes%AES_BLOCK_SIZE))
    {
        // fix block size
        nBytes = ((nBytes/AES_BLOCK_SIZE)+1)*AES_BLOCK_SIZE;
        pbuf = (unsigned char *)malloc(nBytes);
        if(NULL == pbuf) return mfrERR_GENERAL;
        safec_rc = memset_s(pbuf, nBytes, 0, nBytes);
        ERR_CHK(safec_rc);

        safec_rc = memcpy_s(pbuf, nBytes, pPlainText->buf, pPlainText->bufLen);
        if(safec_rc != EOK)
            {
               ERR_CHK(safec_rc);
               if (pbuf)
                   free (pbuf);
               return mfrERR_GENERAL;
            }

    }
    // allocate output buffer
    pCipherText->buf = (char *)malloc(nBytes);
    cbuf = (unsigned char *) pCipherText->buf; 
    // check for alloc failure
	/* coverity fix - 13/5 */
    if(NULL == cbuf) 
	{
		/* coverity fix  */
		if( pbuf != (unsigned char *) pPlainText->buf ) free(pbuf);
		return mfrERR_GENERAL;		
    	}
    
    // redundant check for block size
    if(0 != (nBytes%AES_BLOCK_SIZE))
    {
        return mfrERR_INVALID_PARAM;
    }
    
    /* set up the AES key structure */
    mfr_create_key(&serialNumber, keybuf, iv);
    AES_set_encrypt_key(keybuf, 128, &ctx);
    
    /* encrypt the data */
    while (count < nBytes)
    {
        /* encrypt the data */
        AES_cbc_encrypt(pbuf+count, cbuf+count, AES_BLOCK_SIZE, &ctx, iv, AES_ENCRYPT);
        count += AES_BLOCK_SIZE;
    }
    
    // free any duplicate pbuf
    if(pbuf != (unsigned char *) pPlainText->buf)
    {
        free(pbuf);
    }

    pCipherText->bufLen = nBytes;
    
    // return success
    return mfrERR_NONE;
}

mfrError_t mfrCrypto_Decrypt(const mfrSerializedData_t * pCipherText, mfrSerializedData_t * pPlainText)
{// this function should be static
    
    AES_KEY ctx;
    unsigned char keybuf[MFR_CRYPTO_KEY_LENGTH];
    int count = 0;
    unsigned char iv[AES_BLOCK_SIZE];
    int nBytes           = pCipherText->bufLen;
    unsigned char * cbuf = (unsigned char *) pCipherText->buf;
    unsigned char * pbuf = NULL;
    errno_t safec_rc = -1;

    pPlainText->buf = NULL;
    
    // parameter check
    if(NULL == cbuf) return mfrERR_INVALID_PARAM;
    
    // block size check
    if(0 != (nBytes%AES_BLOCK_SIZE))
    {
        // fix block size
        nBytes = ((nBytes/AES_BLOCK_SIZE)+1)*AES_BLOCK_SIZE;
        cbuf = (unsigned char *)malloc(nBytes);
        if(NULL == cbuf) return mfrERR_GENERAL;
        safec_rc = memset_s(cbuf, nBytes, 0, nBytes);
        ERR_CHK(safec_rc);

        safec_rc = memcpy_s(cbuf, nBytes, pCipherText->buf, pCipherText->bufLen);
        if(safec_rc != EOK)
            {
               ERR_CHK(safec_rc);
               if(cbuf)
                   free(cbuf);
               return mfrERR_GENERAL;
            }

    }
    // allocate output buffer
    pPlainText->buf = (char *)malloc(nBytes);
    pbuf = (unsigned char*) pPlainText->buf;
 
    // check for alloc failure
	/* coverity fix - 13/5 */
    if(NULL == pbuf) 
	{
		if( cbuf != (unsigned char*) pCipherText->buf ) free( cbuf );
		return mfrERR_GENERAL;
	}
    
    // redundant check for block size
    if(0 != (nBytes%AES_BLOCK_SIZE))
    {
        return mfrERR_INVALID_PARAM;
    }
    
    /* set up the AES key structure */
    mfr_create_key(&serialNumber, keybuf, iv);
    AES_set_decrypt_key(keybuf, 128, &ctx);
    
    /* decrypt the data */
    while (count < nBytes)
    {
        /* decrypt the data */
        AES_cbc_encrypt(cbuf+count, pbuf+count, AES_BLOCK_SIZE, &ctx, iv, AES_DECRYPT);
        count += AES_BLOCK_SIZE;
    }
    
    // free any duplicate cbuf
    if(cbuf != (unsigned char*) pCipherText->buf)
    {
        free(cbuf);
    }
    
    pPlainText->bufLen = nBytes;
    
    // return success
    return mfrERR_NONE;
}


/** @} */
/** @} */
