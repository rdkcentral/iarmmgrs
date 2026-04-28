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
 * @file test_diskMgr.cpp
 * @brief L1 unit tests for the disk/diskMgr.c component.
 *
 * Components covered:
 *   - DISKMgr_Start()  — happy path and all error-handling branches
 *   - DISKMgr_Stop()   — happy path, cleanup on IARM failures, g_running flag
 *   - DISKMgr_Loop()   — immediate exit when g_running is pre-set to false
 *
 * Include order
 * -------------
 * Iarm.h (from testframework) must be included before diskMgr.c so that
 * IARM_Result_t is defined as the proper C++ enum before the .c file is
 * processed; this avoids "conflicting declaration" errors that arise when
 * the empty libIBus.h / libIARM.h stubs are seen first.
 *
 * diskMgr.c is included directly in this translation unit so that the
 * file-static variable g_running and internal functions are accessible
 * to tests without requiring extra linkage.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <cstring>

/* Iarm.h / IarmBusMock.h before the source under test */
#include "Iarm.h"
#include "IarmBusMock.h"

/* Pull in the source under test — exposes static g_running */
extern "C" {
#include "diskMgr.c"
}

using ::testing::_;
using ::testing::Return;
using ::testing::NiceMock;

/* =======================================================================
 * DiskMgrStartTest — unit tests for DISKMgr_Start()
 * ====================================================================== */

class DiskMgrStartTest : public ::testing::Test
{
protected:
    NiceMock<IarmBusImplMock> iarmMock;

    void SetUp() override
    {
        IarmBus::setImpl(&iarmMock);
        g_running = true;
    }

    void TearDown() override
    {
        ::testing::Mock::VerifyAndClearExpectations(&iarmMock);
        IarmBus::setImpl(nullptr);
    }
};

TEST_F(DiskMgrStartTest, DISKMgrStart_AllIarmCallsSucceed_ReturnsSuccess)
{
    EXPECT_CALL(iarmMock, IARM_Bus_Init(_))
        .WillOnce(Return(IARM_RESULT_SUCCESS));
    EXPECT_CALL(iarmMock, IARM_Bus_Connect())
        .WillOnce(Return(IARM_RESULT_SUCCESS));
    EXPECT_CALL(iarmMock, IARM_Bus_RegisterEvent(IARM_BUS_DISKMGR_EVENT_MAX))
        .WillOnce(Return(IARM_RESULT_SUCCESS));

    EXPECT_EQ(IARM_RESULT_SUCCESS, DISKMgr_Start());
}

TEST_F(DiskMgrStartTest, DISKMgrStart_IarmInitFails_ReturnsFailureImmediately)
{
    EXPECT_CALL(iarmMock, IARM_Bus_Init(_))
        .WillOnce(Return(IARM_RESULT_IPCCORE_FAIL));
    /* Connect and RegisterEvent must NOT be called */
    EXPECT_CALL(iarmMock, IARM_Bus_Connect()).Times(0);
    EXPECT_CALL(iarmMock, IARM_Bus_RegisterEvent(_)).Times(0);

    EXPECT_EQ(IARM_RESULT_IPCCORE_FAIL, DISKMgr_Start());
}

TEST_F(DiskMgrStartTest, DISKMgrStart_IarmConnectFails_TermsAndReturnsFailure)
{
    EXPECT_CALL(iarmMock, IARM_Bus_Init(_))
        .WillOnce(Return(IARM_RESULT_SUCCESS));
    EXPECT_CALL(iarmMock, IARM_Bus_Connect())
        .WillOnce(Return(IARM_RESULT_IPCCORE_FAIL));
    EXPECT_CALL(iarmMock, IARM_Bus_Term())
        .WillOnce(Return(IARM_RESULT_SUCCESS));
    EXPECT_CALL(iarmMock, IARM_Bus_RegisterEvent(_)).Times(0);

    EXPECT_EQ(IARM_RESULT_IPCCORE_FAIL, DISKMgr_Start());
}

TEST_F(DiskMgrStartTest, DISKMgrStart_RegisterEventFails_DisconnectsTermsAndReturnsFailure)
{
    EXPECT_CALL(iarmMock, IARM_Bus_Init(_))
        .WillOnce(Return(IARM_RESULT_SUCCESS));
    EXPECT_CALL(iarmMock, IARM_Bus_Connect())
        .WillOnce(Return(IARM_RESULT_SUCCESS));
    EXPECT_CALL(iarmMock, IARM_Bus_RegisterEvent(IARM_BUS_DISKMGR_EVENT_MAX))
        .WillOnce(Return(IARM_RESULT_IPCCORE_FAIL));
    EXPECT_CALL(iarmMock, IARM_Bus_Disconnect())
        .WillOnce(Return(IARM_RESULT_SUCCESS));
    EXPECT_CALL(iarmMock, IARM_Bus_Term())
        .WillOnce(Return(IARM_RESULT_SUCCESS));

    EXPECT_EQ(IARM_RESULT_IPCCORE_FAIL, DISKMgr_Start());
}

/* =======================================================================
 * DiskMgrStopTest — unit tests for DISKMgr_Stop()
 * ====================================================================== */

class DiskMgrStopTest : public ::testing::Test
{
protected:
    NiceMock<IarmBusImplMock> iarmMock;

    void SetUp() override
    {
        IarmBus::setImpl(&iarmMock);
        g_running = true;
    }

    void TearDown() override
    {
        ::testing::Mock::VerifyAndClearExpectations(&iarmMock);
        IarmBus::setImpl(nullptr);
    }
};

TEST_F(DiskMgrStopTest, DISKMgrStop_AllIarmCallsSucceed_ReturnsSuccess)
{
    EXPECT_CALL(iarmMock, IARM_Bus_Disconnect())
        .WillOnce(Return(IARM_RESULT_SUCCESS));
    EXPECT_CALL(iarmMock, IARM_Bus_Term())
        .WillOnce(Return(IARM_RESULT_SUCCESS));

    EXPECT_EQ(IARM_RESULT_SUCCESS, DISKMgr_Stop());
}

TEST_F(DiskMgrStopTest, DISKMgrStop_SetsGRunningToFalse)
{
    EXPECT_EQ(true, g_running);

    DISKMgr_Stop();

    EXPECT_EQ(false, g_running);
}

TEST_F(DiskMgrStopTest, DISKMgrStop_DisconnectFails_StillTermsAndReturnsSuccess)
{
    EXPECT_CALL(iarmMock, IARM_Bus_Disconnect())
        .WillOnce(Return(IARM_RESULT_IPCCORE_FAIL));
    EXPECT_CALL(iarmMock, IARM_Bus_Term())
        .WillOnce(Return(IARM_RESULT_SUCCESS));

    EXPECT_EQ(IARM_RESULT_SUCCESS, DISKMgr_Stop());
}

TEST_F(DiskMgrStopTest, DISKMgrStop_TermFails_StillReturnsSuccess)
{
    EXPECT_CALL(iarmMock, IARM_Bus_Disconnect())
        .WillOnce(Return(IARM_RESULT_SUCCESS));
    EXPECT_CALL(iarmMock, IARM_Bus_Term())
        .WillOnce(Return(IARM_RESULT_IPCCORE_FAIL));

    EXPECT_EQ(IARM_RESULT_SUCCESS, DISKMgr_Stop());
}

/* =======================================================================
 * DiskMgrLoopTest — unit tests for DISKMgr_Loop()
 * ====================================================================== */

class DiskMgrLoopTest : public ::testing::Test
{
protected:
    NiceMock<IarmBusImplMock> iarmMock;

    void SetUp() override
    {
        IarmBus::setImpl(&iarmMock);
    }

    void TearDown() override
    {
        ::testing::Mock::VerifyAndClearExpectations(&iarmMock);
        IarmBus::setImpl(nullptr);
        g_running = true;   /* restore for subsequent tests */
    }
};

TEST_F(DiskMgrLoopTest, DISKMgrLoop_GRunningAlreadyFalse_ExitsImmediately)
{
    /* With g_running == false the while-condition is false on entry,
     * so the body (and sleep(300)) is never reached. */
    g_running = false;

    EXPECT_EQ(IARM_RESULT_SUCCESS, DISKMgr_Loop());
}
