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
 * @file WrapsExt.h
 * @brief Local extension of entservices-testframework's Wraps mock.
 *
 * The testframework provides the Wraps singleton and WrapsImplMock
 * (fopen, setmntent, system, popen, ...) via Wraps.h / WrapsMock.h.
 * However, it does NOT provide __wrap_fclose or __wrap_fgets.
 *
 * This header adds those two wraps using a parallel WrapsExt singleton.
 * The corresponding WrapsExt.cpp also provides null-safe __wrap_fopen
 * and __wrap_setmntent implementations (replacing the testframework's
 * Wraps.cpp) so that calls made after the test fixture is torn down
 * (e.g. gtest JSON output, gcov atexit handlers) fall through safely
 * to the real libc functions instead of segfaulting.
 *
 * Usage in test fixtures:
 *   WrapsImplExtMock mock;
 *   Wraps::setImpl(&mock);      // fopen, system, popen, ... (testframework)
 *   WrapsExt::setImpl(&mock);   // fclose, fgets              (this file)
 */

#pragma once

#include <cstdio>
#include <gmock/gmock.h>
#include "WrapsMock.h"   /* testframework's WrapsImplMock */

/* ------------------------------------------------------------------ */
/*  Extended interface — fclose / fgets                                */
/* ------------------------------------------------------------------ */

class WrapsExtImpl {
public:
    virtual ~WrapsExtImpl() = default;
    virtual int   fclose(FILE* stream) = 0;
    virtual char* fgets(char* s, int size, FILE* stream) = 0;
};

/* ------------------------------------------------------------------ */
/*  Singleton accessor for the extension                              */
/* ------------------------------------------------------------------ */

class WrapsExt {
protected:
    static WrapsExtImpl* impl;
public:
    static void setImpl(WrapsExtImpl* newImpl);
    static WrapsExtImpl* getImpl() { return impl; }
};

/* ------------------------------------------------------------------ */
/*  Combined GMock: testframework wraps  +  fclose / fgets            */
/*                                                                    */
/*  Inherits from both WrapsImplMock (testframework) and WrapsExtImpl */
/*  so a single mock instance can be passed to both Wraps::setImpl()  */
/*  and WrapsExt::setImpl().                                          */
/* ------------------------------------------------------------------ */

class WrapsImplExtMock : public WrapsImplMock, public WrapsExtImpl {
public:
    using WrapsImplMock::WrapsImplMock;
    ~WrapsImplExtMock() override = default;

    MOCK_METHOD(int,   fclose, (FILE* stream),                    (override));
    MOCK_METHOD(char*, fgets,  (char* s, int size, FILE* stream), (override));
};
