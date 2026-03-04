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
 * @file test_rdkProfile.cpp
 * @brief L1 unit tests for the utils/rdkProfile.c component.
 *
 * Tests the searchRdkProfile() function which reads /etc/device.properties
 * and returns PROFILE_TV, PROFILE_STB, or PROFILE_INVALID.
 *
 * External dependencies mocked via __wrap_ linker interception:
 *   - fopen  (controls whether the file "exists")     — testframework Wraps
 *   - fgets  (controls what content is "read" from the file) — testframework Wraps
 *   - fclose (verifies cleanup)                        — testframework Wraps
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <cstring>

#include "WrapsMock.h"

/* Include the source-under-test directly so we can exercise the static
 * internals (there are none in rdkProfile.c, but this keeps the pattern
 * consistent with the other component tests that will follow). */
extern "C" {
#include "rdkProfile.c"
}

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::DoAll;

/* ------------------------------------------------------------------ */
/*  Helper: simulate fgets reading lines from an in-memory buffer     */
/* ------------------------------------------------------------------ */

/**
 * @brief Helper that simulates fgets over an array of lines.
 *
 * Each call to the returned lambda copies the next line into the caller's
 * buffer, exactly as fgets would.  After all lines are consumed it returns
 * nullptr (EOF).
 */
static auto makeFgetsReader(const std::vector<std::string>& lines)
{
    /* Shared mutable index wrapped in a shared_ptr so that copies of the
     * lambda all share the same counter. */
    auto idx = std::make_shared<size_t>(0);
    return [lines, idx](char* s, int size, FILE* /*stream*/) -> char* {
        if (*idx >= lines.size()) {
            return nullptr;  /* EOF */
        }
        std::strncpy(s, lines[*idx].c_str(), static_cast<size_t>(size - 1));
        s[size - 1] = '\0';
        ++(*idx);
        return s;
    };
}

/* ================================================================== */
/*  Test fixture                                                      */
/* ================================================================== */

class RdkProfileTest : public ::testing::Test {
protected:
    WrapsImplMock mock;

    /* Sentinel FILE* value returned by the mocked fopen.  We never
     * dereference it — it just needs to be non-NULL. */
    FILE* fakeFp = reinterpret_cast<FILE*>(0xDEAD);

    void SetUp() override {
        Wraps::setImpl(&mock);
    }

    void TearDown() override {
        ::testing::Mock::VerifyAndClearExpectations(&mock);
        Wraps::setImpl(nullptr);
    }
};

/* ================================================================== */
/*  Tests: file-not-found / fopen failure                             */
/* ================================================================== */

/**
 * @brief Verify PROFILE_INVALID when the device.properties file cannot be
 *        opened (fopen returns NULL).
 */
TEST_F(RdkProfileTest, FileNotFound_ReturnsInvalid)
{
    EXPECT_CALL(mock, fopen(_, _))
        .Times(1)
        .WillOnce(Return(nullptr));

    profile_t result = searchRdkProfile();

    EXPECT_EQ(result, PROFILE_INVALID);
}

/* ================================================================== */
/*  Tests: RDK_PROFILE=TV                                             */
/* ================================================================== */

/**
 * @brief Verify PROFILE_TV when device.properties contains "RDK_PROFILE=TV".
 */
TEST_F(RdkProfileTest, ProfileTV_ReturnsTV)
{
    std::vector<std::string> lines = { "RDK_PROFILE=TV\n" };

    EXPECT_CALL(mock, fopen(_, _))
        .Times(1)
        .WillOnce(Return(fakeFp));

    EXPECT_CALL(mock, fgets(_, _, fakeFp))
        .WillRepeatedly(Invoke(makeFgetsReader(lines)));

    EXPECT_CALL(mock, fclose(fakeFp))
        .Times(1)
        .WillOnce(Return(0));

    profile_t result = searchRdkProfile();

    EXPECT_EQ(result, PROFILE_TV);
}

/**
 * @brief Verify PROFILE_TV when the matching line appears after other
 *        properties in the file.
 */
TEST_F(RdkProfileTest, ProfileTV_FoundAmidOtherProperties)
{
    std::vector<std::string> lines = {
        "MODEL_NUM=PX061AEI\n",
        "FRIENDLY_ID=TV\n",
        "BUILD_TYPE=VBN\n",
        "RDK_PROFILE=TV\n",
        "ANOTHER_PROP=value\n"
    };

    EXPECT_CALL(mock, fopen(_, _))
        .WillOnce(Return(fakeFp));

    EXPECT_CALL(mock, fgets(_, _, fakeFp))
        .WillRepeatedly(Invoke(makeFgetsReader(lines)));

    EXPECT_CALL(mock, fclose(fakeFp))
        .WillOnce(Return(0));

    EXPECT_EQ(searchRdkProfile(), PROFILE_TV);
}

/* ================================================================== */
/*  Tests: RDK_PROFILE=STB                                            */
/* ================================================================== */

/**
 * @brief Verify PROFILE_STB when device.properties contains "RDK_PROFILE=STB".
 */
TEST_F(RdkProfileTest, ProfileSTB_ReturnsSTB)
{
    std::vector<std::string> lines = { "RDK_PROFILE=STB\n" };

    EXPECT_CALL(mock, fopen(_, _))
        .WillOnce(Return(fakeFp));

    EXPECT_CALL(mock, fgets(_, _, fakeFp))
        .WillRepeatedly(Invoke(makeFgetsReader(lines)));

    EXPECT_CALL(mock, fclose(fakeFp))
        .WillOnce(Return(0));

    EXPECT_EQ(searchRdkProfile(), PROFILE_STB);
}

/**
 * @brief Verify PROFILE_STB when the profile line appears after several
 *        unrelated lines, demonstrating the while-loop iteration.
 */
TEST_F(RdkProfileTest, ProfileSTB_FoundAfterManyLines)
{
    std::vector<std::string> lines = {
        "# device properties\n",
        "DEVICE_NAME=ARRIS-XG1\n",
        "DEVICE_TYPE=mediaclient\n",
        "RDK_PROFILE=STB\n"
    };

    EXPECT_CALL(mock, fopen(_, _))
        .WillOnce(Return(fakeFp));

    EXPECT_CALL(mock, fgets(_, _, fakeFp))
        .WillRepeatedly(Invoke(makeFgetsReader(lines)));

    EXPECT_CALL(mock, fclose(fakeFp))
        .WillOnce(Return(0));

    EXPECT_EQ(searchRdkProfile(), PROFILE_STB);
}

/* ================================================================== */
/*  Tests: RDK_PROFILE missing → PROFILE_INVALID                     */
/* ================================================================== */

/**
 * @brief Verify PROFILE_INVALID when the file is empty.
 */
TEST_F(RdkProfileTest, EmptyFile_ReturnsInvalid)
{
    std::vector<std::string> lines = {};  /* no lines → immediate EOF */

    EXPECT_CALL(mock, fopen(_, _))
        .WillOnce(Return(fakeFp));

    EXPECT_CALL(mock, fgets(_, _, fakeFp))
        .WillRepeatedly(Invoke(makeFgetsReader(lines)));

    EXPECT_CALL(mock, fclose(fakeFp))
        .WillOnce(Return(0));

    EXPECT_EQ(searchRdkProfile(), PROFILE_INVALID);
}

/**
 * @brief Verify PROFILE_INVALID when device.properties has content but no
 *        RDK_PROFILE line.
 */
TEST_F(RdkProfileTest, NoProfileKey_ReturnsInvalid)
{
    std::vector<std::string> lines = {
        "MODEL_NUM=PX061AEI\n",
        "BUILD_TYPE=VBN\n",
        "FRIENDLY_ID=Pi\n"
    };

    EXPECT_CALL(mock, fopen(_, _))
        .WillOnce(Return(fakeFp));

    EXPECT_CALL(mock, fgets(_, _, fakeFp))
        .WillRepeatedly(Invoke(makeFgetsReader(lines)));

    EXPECT_CALL(mock, fclose(fakeFp))
        .WillOnce(Return(0));

    EXPECT_EQ(searchRdkProfile(), PROFILE_INVALID);
}

/* ================================================================== */
/*  Tests: malformed / edge-case values                               */
/* ================================================================== */

/**
 * @brief Verify PROFILE_INVALID when the value after "RDK_PROFILE=" is
 *        neither "TV" nor "STB".
 */
TEST_F(RdkProfileTest, UnknownProfileValue_ReturnsInvalid)
{
    std::vector<std::string> lines = { "RDK_PROFILE=UNKNOWN\n" };

    EXPECT_CALL(mock, fopen(_, _))
        .WillOnce(Return(fakeFp));

    EXPECT_CALL(mock, fgets(_, _, fakeFp))
        .WillRepeatedly(Invoke(makeFgetsReader(lines)));

    EXPECT_CALL(mock, fclose(fakeFp))
        .WillOnce(Return(0));

    EXPECT_EQ(searchRdkProfile(), PROFILE_INVALID);
}

/**
 * @brief Verify PROFILE_INVALID when "RDK_PROFILE" appears without a value
 *        (missing '=' or nothing after key).
 */
TEST_F(RdkProfileTest, ProfileKeyNoEquals_ReturnsInvalid)
{
    std::vector<std::string> lines = { "RDK_PROFILE\n" };

    EXPECT_CALL(mock, fopen(_, _))
        .WillOnce(Return(fakeFp));

    EXPECT_CALL(mock, fgets(_, _, fakeFp))
        .WillRepeatedly(Invoke(makeFgetsReader(lines)));

    EXPECT_CALL(mock, fclose(fakeFp))
        .WillOnce(Return(0));

    EXPECT_EQ(searchRdkProfile(), PROFILE_INVALID);
}

/**
 * @brief Verify PROFILE_INVALID when "RDK_PROFILE=" is present but the
 *        value is empty.
 */
TEST_F(RdkProfileTest, ProfileKeyEmptyValue_ReturnsInvalid)
{
    std::vector<std::string> lines = { "RDK_PROFILE=\n" };

    EXPECT_CALL(mock, fopen(_, _))
        .WillOnce(Return(fakeFp));

    EXPECT_CALL(mock, fgets(_, _, fakeFp))
        .WillRepeatedly(Invoke(makeFgetsReader(lines)));

    EXPECT_CALL(mock, fclose(fakeFp))
        .WillOnce(Return(0));

    EXPECT_EQ(searchRdkProfile(), PROFILE_INVALID);
}

/**
 * @brief Verify that a partial match like "NOT_RDK_PROFILE=TV" does NOT
 *        trigger a false positive — strstr("NOT_RDK_PROFILE=TV", "RDK_PROFILE")
 *        matches, so the code will still find it and return PROFILE_TV.
 *
 *        This documents a known behavioral quirk: the code uses strstr
 *        rather than an exact key match, so any line *containing*
 *        "RDK_PROFILE" will match.
 */
TEST_F(RdkProfileTest, SubstringMatch_DocumentedBehavior)
{
    std::vector<std::string> lines = { "NOT_RDK_PROFILE=TV\n" };

    EXPECT_CALL(mock, fopen(_, _))
        .WillOnce(Return(fakeFp));

    EXPECT_CALL(mock, fgets(_, _, fakeFp))
        .WillRepeatedly(Invoke(makeFgetsReader(lines)));

    EXPECT_CALL(mock, fclose(fakeFp))
        .WillOnce(Return(0));

    /* strstr finds "RDK_PROFILE" inside "NOT_RDK_PROFILE=TV", advances
     * past "RDK_PROFILE", hits '=', and compares "TV" → PROFILE_TV.  */
    EXPECT_EQ(searchRdkProfile(), PROFILE_TV);
}

/**
 * @brief Verify PROFILE_TV even when the value has trailing whitespace.
 *        strncmp only compares the first strlen("TV") chars so "TV  \n"
 *        still matches.
 */
TEST_F(RdkProfileTest, ProfileTV_TrailingWhitespace)
{
    std::vector<std::string> lines = { "RDK_PROFILE=TV  \n" };

    EXPECT_CALL(mock, fopen(_, _))
        .WillOnce(Return(fakeFp));

    EXPECT_CALL(mock, fgets(_, _, fakeFp))
        .WillRepeatedly(Invoke(makeFgetsReader(lines)));

    EXPECT_CALL(mock, fclose(fakeFp))
        .WillOnce(Return(0));

    EXPECT_EQ(searchRdkProfile(), PROFILE_TV);
}

/**
 * @brief Verify PROFILE_STB with no trailing newline (last line of file
 *        may not have one).
 */
TEST_F(RdkProfileTest, ProfileSTB_NoTrailingNewline)
{
    std::vector<std::string> lines = { "RDK_PROFILE=STB" };

    EXPECT_CALL(mock, fopen(_, _))
        .WillOnce(Return(fakeFp));

    EXPECT_CALL(mock, fgets(_, _, fakeFp))
        .WillRepeatedly(Invoke(makeFgetsReader(lines)));

    EXPECT_CALL(mock, fclose(fakeFp))
        .WillOnce(Return(0));

    EXPECT_EQ(searchRdkProfile(), PROFILE_STB);
}

/**
 * @brief Verify fclose is always called when fopen succeeds, even when
 *        the profile key is not found.
 */
TEST_F(RdkProfileTest, FcloseCalledOnSuccess)
{
    std::vector<std::string> lines = { "SOME_OTHER=VALUE\n" };

    EXPECT_CALL(mock, fopen(_, _))
        .WillOnce(Return(fakeFp));

    EXPECT_CALL(mock, fgets(_, _, fakeFp))
        .WillRepeatedly(Invoke(makeFgetsReader(lines)));

    /* The critical assertion: fclose must be called exactly once */
    EXPECT_CALL(mock, fclose(fakeFp))
        .Times(1)
        .WillOnce(Return(0));

    searchRdkProfile();
}

/**
 * @brief Verify fclose is NOT called when fopen fails.
 */
TEST_F(RdkProfileTest, FcloseNotCalledOnFopenFailure)
{
    EXPECT_CALL(mock, fopen(_, _))
        .WillOnce(Return(nullptr));

    EXPECT_CALL(mock, fclose(_))
        .Times(0);

    searchRdkProfile();
}
