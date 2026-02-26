/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2025 RDK Management
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
**/

/**
 * @file WrapsExt.cpp
 * @brief __wrap_fclose / __wrap_fgets implementations + WrapsExt singleton.
 *
 * These wrap functions complement entservices-testframework's Wraps.cpp
 * (which already provides __wrap_fopen and many others).
 *
 * Build with:
 *   -Wl,--wrap,fclose -Wl,--wrap,fgets
 */

#include "WrapsExt.h"
#include <cstdio>

/* Static member initialisation */
WrapsExtImpl* WrapsExt::impl = nullptr;

void WrapsExt::setImpl(WrapsExtImpl* newImpl)
{
    impl = newImpl;
}

/* ------------------------------------------------------------------ */
/*  __wrap_  /  __real_  linker shim functions                        */
/* ------------------------------------------------------------------ */

extern "C" {

/* Real libc symbols — created by the linker when using --wrap */
int   __real_fclose(FILE* stream);
char* __real_fgets(char* s, int size, FILE* stream);

int __wrap_fclose(FILE* stream)
{
    WrapsExtImpl* mock = WrapsExt::getImpl();
    if (mock) {
        return mock->fclose(stream);
    }
    return __real_fclose(stream);
}

char* __wrap_fgets(char* s, int size, FILE* stream)
{
    WrapsExtImpl* mock = WrapsExt::getImpl();
    if (mock) {
        return mock->fgets(s, size, stream);
    }
    return __real_fgets(s, size, stream);
}

} /* extern "C" */
