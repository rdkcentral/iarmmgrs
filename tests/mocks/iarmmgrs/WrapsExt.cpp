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
 * @brief All __wrap_ implementations for the iarmmgrs L1 test binary.
 *
 * Provides null-safe __wrap_ shims for every function intercepted via
 * the linker's --wrap option (fopen, fclose, fgets, setmntent).
 *
 * This file REPLACES the testframework's Wraps.cpp so that we have a
 * single, consistent set of wrappers that always fall back to the real
 * libc function when the mock implementation pointer is nullptr.
 * Without this, calls made after the test fixture is torn down (e.g.
 * gtest's JSON-output writer calling fopen, or gcov's atexit handler
 * flushing .gcda coverage data) would dereference a null impl pointer
 * and segfault.
 *
 * Build with:
 *   -Wl,--wrap,fopen -Wl,--wrap,fclose -Wl,--wrap,fgets -Wl,--wrap,setmntent
 */

#include "WrapsExt.h"
#include <cstdio>

/* ================================================================== */
/*  Wraps singleton  (fopen — normally in testframework's Wraps.cpp)  */
/* ================================================================== */

WrapsImpl* Wraps::impl = nullptr;

void Wraps::setImpl(WrapsImpl* newImpl)
{
    impl = newImpl;
}

/* ================================================================== */
/*  WrapsExt singleton  (fclose, fgets — local extension)             */
/* ================================================================== */

WrapsExtImpl* WrapsExt::impl = nullptr;

void WrapsExt::setImpl(WrapsExtImpl* newImpl)
{
    impl = newImpl;
}

/* ================================================================== */
/*  __wrap_  /  __real_  linker shim functions                        */
/* ================================================================== */

extern "C" {

/* Real libc symbols — created by the linker when using --wrap */
FILE* __real_fopen(const char* path, const char* mode);
int   __real_fclose(FILE* stream);
char* __real_fgets(char* s, int size, FILE* stream);
FILE* __real_setmntent(const char* filename, const char* type);

FILE* __wrap_fopen(const char* path, const char* mode)
{
    WrapsImpl* mock = Wraps::getImpl();
    if (mock) {
        return mock->fopen(path, mode);
    }
    return __real_fopen(path, mode);
}

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

FILE* __wrap_setmntent(const char* filename, const char* type)
{
    WrapsImpl* mock = Wraps::getImpl();
    if (mock) {
        return mock->setmntent(filename, type);
    }
    return __real_setmntent(filename, type);
}

} /* extern "C" */
