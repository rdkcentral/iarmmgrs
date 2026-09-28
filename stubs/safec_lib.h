/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
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

#ifdef SAFEC_DUMMY_API
#error "SAFEC_DUMMY_API is not permitted in production builds"
#endif

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#if defined(__has_include)
# if __has_include("safe_str_lib.h") && __has_include("safe_mem_lib.h")
#  include "safe_str_lib.h"
#  include "safe_mem_lib.h"
#  define IARMMGRS_HAS_SAFEC_HEADERS 1
# endif
#endif

#ifndef IARMMGRS_HAS_SAFEC_HEADERS
/*
 * Some component build environments provide the Safe C runtime library but
 * not its development headers. Keep these declarations ABI-compatible with
 * safeclib; unlike the former dummy macros, every call still resolves to the
 * real library at link/runtime.
 */
typedef int errno_t;
typedef size_t rsize_t;

#ifndef EOK
#define EOK 0
#endif
#ifndef ESNULLP
#define ESNULLP 400
#endif
#ifndef ESNOSPC
#define ESNOSPC 406
#endif

static inline errno_t strcpy_s(char *dest, rsize_t dmax, const char *src)
{
    rsize_t len;
    if(dest == NULL || src == NULL || dmax == 0) return ESNULLP;
    len = strnlen(src, dmax);
    if(len == dmax) {
        dest[0] = '\0';
        return ESNOSPC;
    }
    memcpy(dest, src, len + 1);
    return EOK;
}

static inline errno_t memcpy_s(void *dest, rsize_t dmax, const void *src, rsize_t smax)
{
    if(dest == NULL || src == NULL) return ESNULLP;
    if(smax > dmax) return ESNOSPC;
    if(smax > 0) memcpy(dest, src, smax);
    return EOK;
}

static inline errno_t memset_s(void *dest, rsize_t dmax, int value, rsize_t n)
{
    volatile unsigned char *out;
    if(dest == NULL) return ESNULLP;
    if(n > dmax) return ESNOSPC;
    out = (volatile unsigned char *)dest;
    while(n-- > 0) *out++ = (unsigned char)value;
    return EOK;
}

static inline errno_t strcmp_s(const char *dest, rsize_t dmax, const char *src, int *result)
{
    if(dest == NULL || src == NULL || result == NULL || dmax == 0) return ESNULLP;
    if(strnlen(dest, dmax) == dmax) return ESNOSPC;
    *result = strcmp(dest, src);
    return EOK;
}
#endif

/* Macro is defined for non clobbering of the safec secure string API strcpy_s & memcpy_s function*/
/* strcpy_s overwrites the old value and nulls the dest when encounters an error*/
#ifndef STRCPY_S_NOCLOBBER
 #define STRCPY_S_NOCLOBBER(dst,dmax,src)   ((src != NULL) ? (strlen(src) < dmax ?  strcpy_s(dst,dmax,src) : ESNOSPC):ESNULLP)
#endif
#define MEMCPY_S_NOCLOBBER(dst,dmax,src,len)   ((src != NULL) ? (len <= dmax ?  memcpy_s(dst,dmax,src,len) : ESNOSPC):ESNULLP)

#define STRCPY_S(dest,size,source)                      \
        { \
        errno_t rc=-1; \
        rc=strcpy_s(dest, size, source);                \
        if(rc!=EOK)                                     \
        {                                               \
             RDK_SAFECLIB_ERR(rc);  \
        }\
}
#define MEMCPY_S(dest,dsize,source,ssize)                      \
        {                                                  \
        errno_t safec_rc=-1; \
        safec_rc=memcpy_s(dest, dsize, source, ssize);                \
        if(safec_rc!=EOK)                                     \
        {                                               \
             RDK_SAFECLIB_ERR(safec_rc);  \
        }\
}

/*
 * SAFECLIB Error Handling Logging APIs
 */
#define RDK_SAFECLIB_ERR(rc)  printf("safeclib error at rc - %d %s %s:%d", rc,  __FILE__, __FUNCTION__, __LINE__)

#define ERR_CHK(rc)                                             \
    if(rc !=EOK) {                                              \
        RDK_SAFECLIB_ERR(rc);                                   \
    }
