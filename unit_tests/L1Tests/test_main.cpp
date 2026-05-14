/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
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
*/

/**
 * @file test_main.cpp
 * @brief Main entry point for the iarmmgrs L1 unit test runner.
 *
 * Initialises Google Test / Google Mock and runs all registered test suites.
 * Individual component tests live under subdirectories (e.g. utils/, dsmgr/)
 * and are compiled into this single binary.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "WrapsMock.h"

/* The linker's --wrap,fopen/fclose/fgets redirects ALL fopen calls —
 * including GTest's own (e.g. writing --gtest_output=json:… results) —
 * through Wraps::impl.  Individual test fixtures install and release
 * their own WrapsImplMock around each test.  After the last fixture
 * TearDown clears impl, GTest still needs to open the output file.
 *
 * WrapsGlobalEnvironment fixes this: its TearDown() reinstalls a
 * passthrough mock (forwarding to the real libc functions) so that any
 * framework-level I/O that follows all-tests teardown works correctly.
 */
extern "C" {
    FILE *__real_fopen(const char *pathname, const char *mode);
    int   __real_fclose(FILE *stream);
    char *__real_fgets(char *s, int size, FILE *stream);
}

class WrapsGlobalEnvironment : public ::testing::Environment
{
public:
    void SetUp() override {}

    void TearDown() override
    {
        /* Allocate once, then intentionally leak.  A heap mock that is
         * never deleted has no destructor races with GMock's internal
         * registry; the OS reclaims the memory when the process exits.
         * Mock::AllowLeak() suppresses GMock's "leaked mock" warning. */
        static ::testing::NiceMock<WrapsImplMock>* s_mock = []() {
            auto* m = new ::testing::NiceMock<WrapsImplMock>();
            ON_CALL(*m, fopen(::testing::_, ::testing::_))
                .WillByDefault(::testing::Invoke(__real_fopen));
            ON_CALL(*m, fclose(::testing::_))
                .WillByDefault(::testing::Invoke(__real_fclose));
            ON_CALL(*m, fgets(::testing::_, ::testing::_, ::testing::_))
                .WillByDefault(::testing::Invoke(__real_fgets));
            ::testing::Mock::AllowLeak(m);
            return m;
        }();

        /* All test fixtures have finished and left impl == nullptr.
         * Reinstall the passthrough so GTest can open the JSON/XML output
         * file without hitting EXPECT_NE(impl, nullptr) inside Wraps::fopen. */
        Wraps::setImpl(nullptr);   /* safe no-op if impl is already null */
        Wraps::setImpl(s_mock);
    }
};

int main(int argc, char** argv)
{
    ::testing::AddGlobalTestEnvironment(new WrapsGlobalEnvironment());
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
