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
 * @file test_deviceUpdateMgr.cpp
 * @brief L1 unit tests for the deviceUpdateMgr component.
 *
 * Components covered:
 *   - deviceUpdateStart()          — happy path and all early-return branches
 *   - deviceUpdateStop()           — initialized / not-initialized paths
 *   - getXMLTagText()              — pure XML tag extraction logic
 *   - AcceptUpdate()               — session-ID assignment, flag propagation,
 *                                    updatesInProgress map management
 *   - _deviceUpdateEventHandler()  — owner-filter, READY_TO_DOWNLOAD,
 *                                    DOWNLOAD_STATUS, LOAD_STATUS, ERROR
 *
 * Include strategy
 * ----------------
 * The three source files are compiled into this single translation unit so
 * that file-scope globals (initialized, running, interactiveDownload, …) and
 * internal helper functions (getXMLTagText, sendDownLoadInit, …) are
 * directly accessible to the tests without needing extern declarations.
 *
 * Order matters:
 *   1. utils.cpp      — defines _fileExists / _folderExists / getdir
 *   2. jsonParser.cpp — defines JSONParser (uses yajl)
 *   3. deviceUpdateMgrMain.cpp — main logic; requires both above
 *
 * iarmUtil.h is an empty generated stub in the test build.  __TIMESTAMP()
 * is used as a logging macro there; stub it before the sources are included.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <cstring>
#include <map>

/* Iarm.h / IarmBusMock.h before the sources under test */
#include "Iarm.h"
#include "IarmBusMock.h"

/* __TIMESTAMP() is used as a no-op logging call throughout the sources.
 * iarmUtil.h is an empty stub in the test build so it is not defined there;
 * define it here before including the sources. */
#ifndef __TIMESTAMP
#define __TIMESTAMP(...)
#endif

/* Include the source files that form the deviceUpdateMgr binary.
 * jsonParser.cpp is built with USE_YAJL2 defined (see AM_CPPFLAGS) so
 * it compiles against the yajl 2 API provided by libyajl-dev.
 * deviceUpdateMgrMain.cpp contains a main() — rename it so it does not
 * conflict with GTest's main() in test_main.cpp. */
#include "utils.cpp"
#include "jsonParser.cpp"
#define main deviceUpdateMgrMain_unused
#include "deviceUpdateMgrMain.cpp"
#undef main

using ::testing::_;
using ::testing::Return;
using ::testing::NiceMock;

/* -----------------------------------------------------------------------
 * Helper: reset all file-scope globals to their initial values.
 * Called from every fixture's SetUp so tests are isolated.
 * ---------------------------------------------------------------------- */
static void resetGlobals()
{
    initialized          = false;
    running              = true;
    nextID               = 1;
    interactiveDownload  = false;
    interactiveLoad      = false;
    backgroundDownload   = true;
    loadImageImmediately = false;
    requestedPercentIncrement = 10;
    loadDelayType        = 0;
    loadTimeAfterInactive = 0;
    loadBeforeHour       = 4;

    for (auto &kv : *updatesInProgress) {
        delete kv.second->acceptParams;
        delete kv.second;
    }
    updatesInProgress->clear();
}

/* =======================================================================
 * DeviceUpdateStartTest — unit tests for deviceUpdateStart()
 * ====================================================================== */

class DeviceUpdateStartTest : public ::testing::Test
{
protected:
    NiceMock<IarmBusImplMock> iarmMock;

    void SetUp() override
    {
        IarmBus::setImpl(&iarmMock);
        resetGlobals();
    }

    void TearDown() override
    {
        ::testing::Mock::VerifyAndClearExpectations(&iarmMock);
        IarmBus::setImpl(nullptr);
        resetGlobals();
    }
};

TEST_F(DeviceUpdateStartTest, AllCallsSucceed_ReturnsSuccess)
{
    EXPECT_CALL(iarmMock, IARM_Bus_Init(_))
        .WillOnce(Return(IARM_RESULT_SUCCESS));
    EXPECT_CALL(iarmMock, IARM_Bus_Connect())
        .WillOnce(Return(IARM_RESULT_SUCCESS));
    EXPECT_CALL(iarmMock, IARM_Bus_RegisterEvent(IARM_BUS_DEVICE_UPDATE_EVENT_MAX))
        .WillOnce(Return(IARM_RESULT_SUCCESS));
    EXPECT_CALL(iarmMock, IARM_Bus_RegisterCall(_, _))
        .WillOnce(Return(IARM_RESULT_SUCCESS));
    EXPECT_CALL(iarmMock, IARM_Bus_RegisterEventHandler(_, _, _))
        .WillRepeatedly(Return(IARM_RESULT_SUCCESS));

    EXPECT_EQ(IARM_RESULT_SUCCESS, deviceUpdateStart());
    EXPECT_TRUE(initialized);
}

TEST_F(DeviceUpdateStartTest, AlreadyInitialized_ReturnsInvalidState)
{
    /* First call succeeds */
    ON_CALL(iarmMock, IARM_Bus_Init(_)).WillByDefault(Return(IARM_RESULT_SUCCESS));
    ON_CALL(iarmMock, IARM_Bus_Connect()).WillByDefault(Return(IARM_RESULT_SUCCESS));
    ON_CALL(iarmMock, IARM_Bus_RegisterEvent(_)).WillByDefault(Return(IARM_RESULT_SUCCESS));
    ON_CALL(iarmMock, IARM_Bus_RegisterCall(_, _)).WillByDefault(Return(IARM_RESULT_SUCCESS));
    ON_CALL(iarmMock, IARM_Bus_RegisterEventHandler(_, _, _)).WillByDefault(Return(IARM_RESULT_SUCCESS));

    ASSERT_EQ(IARM_RESULT_SUCCESS, deviceUpdateStart());

    /* Second call must indicate already-initialized */
    EXPECT_EQ(IARM_RESULT_INVALID_STATE, deviceUpdateStart());
}

TEST_F(DeviceUpdateStartTest, IarmInitFails_ReturnsEarly)
{
    EXPECT_CALL(iarmMock, IARM_Bus_Init(_))
        .WillOnce(Return(IARM_RESULT_IPCCORE_FAIL));
    EXPECT_CALL(iarmMock, IARM_Bus_Connect()).Times(0);

    EXPECT_EQ(IARM_RESULT_IPCCORE_FAIL, deviceUpdateStart());
    EXPECT_FALSE(initialized);
}

TEST_F(DeviceUpdateStartTest, IarmConnectFails_ReturnsEarly)
{
    EXPECT_CALL(iarmMock, IARM_Bus_Init(_))
        .WillOnce(Return(IARM_RESULT_SUCCESS));
    EXPECT_CALL(iarmMock, IARM_Bus_Connect())
        .WillOnce(Return(IARM_RESULT_IPCCORE_FAIL));
    EXPECT_CALL(iarmMock, IARM_Bus_RegisterEvent(_)).Times(0);

    EXPECT_EQ(IARM_RESULT_IPCCORE_FAIL, deviceUpdateStart());
    EXPECT_FALSE(initialized);
}

TEST_F(DeviceUpdateStartTest, RegisterEventFails_ReturnsEarly)
{
    EXPECT_CALL(iarmMock, IARM_Bus_Init(_))
        .WillOnce(Return(IARM_RESULT_SUCCESS));
    EXPECT_CALL(iarmMock, IARM_Bus_Connect())
        .WillOnce(Return(IARM_RESULT_SUCCESS));
    EXPECT_CALL(iarmMock, IARM_Bus_RegisterEvent(IARM_BUS_DEVICE_UPDATE_EVENT_MAX))
        .WillOnce(Return(IARM_RESULT_IPCCORE_FAIL));
    EXPECT_CALL(iarmMock, IARM_Bus_RegisterCall(_, _)).Times(0);

    EXPECT_EQ(IARM_RESULT_IPCCORE_FAIL, deviceUpdateStart());
    EXPECT_FALSE(initialized);
}

TEST_F(DeviceUpdateStartTest, RegisterCallFails_ReturnsEarly)
{
    EXPECT_CALL(iarmMock, IARM_Bus_Init(_))
        .WillOnce(Return(IARM_RESULT_SUCCESS));
    EXPECT_CALL(iarmMock, IARM_Bus_Connect())
        .WillOnce(Return(IARM_RESULT_SUCCESS));
    EXPECT_CALL(iarmMock, IARM_Bus_RegisterEvent(_))
        .WillOnce(Return(IARM_RESULT_SUCCESS));
    EXPECT_CALL(iarmMock, IARM_Bus_RegisterCall(_, _))
        .WillOnce(Return(IARM_RESULT_IPCCORE_FAIL));
    EXPECT_CALL(iarmMock, IARM_Bus_RegisterEventHandler(_, _, _)).Times(0);

    EXPECT_EQ(IARM_RESULT_IPCCORE_FAIL, deviceUpdateStart());
    EXPECT_FALSE(initialized);
}

TEST_F(DeviceUpdateStartTest, FirstRegisterEventHandlerFails_ReturnsEarly)
{
    EXPECT_CALL(iarmMock, IARM_Bus_Init(_))
        .WillOnce(Return(IARM_RESULT_SUCCESS));
    EXPECT_CALL(iarmMock, IARM_Bus_Connect())
        .WillOnce(Return(IARM_RESULT_SUCCESS));
    EXPECT_CALL(iarmMock, IARM_Bus_RegisterEvent(_))
        .WillOnce(Return(IARM_RESULT_SUCCESS));
    EXPECT_CALL(iarmMock, IARM_Bus_RegisterCall(_, _))
        .WillOnce(Return(IARM_RESULT_SUCCESS));
    /* first RegisterEventHandler (READY_TO_DOWNLOAD) fails */
    EXPECT_CALL(iarmMock, IARM_Bus_RegisterEventHandler(_, _, _))
        .WillOnce(Return(IARM_RESULT_IPCCORE_FAIL));

    EXPECT_EQ(IARM_RESULT_IPCCORE_FAIL, deviceUpdateStart());
    EXPECT_FALSE(initialized);
}

/* =======================================================================
 * DeviceUpdateStopTest — unit tests for deviceUpdateStop()
 * ====================================================================== */

class DeviceUpdateStopTest : public ::testing::Test
{
protected:
    NiceMock<IarmBusImplMock> iarmMock;

    void SetUp() override
    {
        IarmBus::setImpl(&iarmMock);
        resetGlobals();
    }

    void TearDown() override
    {
        ::testing::Mock::VerifyAndClearExpectations(&iarmMock);
        IarmBus::setImpl(nullptr);
        resetGlobals();
    }

    /* Helper: put deviceUpdateMgr into the initialized state without going
     * through the real start (avoids setting up all mock expectations). */
    void forceInitialized()
    {
        initialized = true;
    }
};

TEST_F(DeviceUpdateStopTest, NotInitialized_ReturnsInvalidState)
{
    EXPECT_EQ(IARM_RESULT_INVALID_STATE, deviceUpdateStop());
}

TEST_F(DeviceUpdateStopTest, Initialized_AllCallsSucceed_ReturnsSuccess)
{
    forceInitialized();

    EXPECT_CALL(iarmMock, IARM_Bus_UnRegisterEventHandler(_, _))
        .WillOnce(Return(IARM_RESULT_SUCCESS));
    EXPECT_CALL(iarmMock, IARM_Bus_Disconnect())
        .WillOnce(Return(IARM_RESULT_SUCCESS));
    EXPECT_CALL(iarmMock, IARM_Bus_Term())
        .WillOnce(Return(IARM_RESULT_SUCCESS));

    EXPECT_EQ(IARM_RESULT_SUCCESS, deviceUpdateStop());
    EXPECT_FALSE(initialized);
}

TEST_F(DeviceUpdateStopTest, Initialized_DisconnectFails_StillReturnsSuccess)
{
    forceInitialized();

    EXPECT_CALL(iarmMock, IARM_Bus_UnRegisterEventHandler(_, _))
        .WillOnce(Return(IARM_RESULT_SUCCESS));
    EXPECT_CALL(iarmMock, IARM_Bus_Disconnect())
        .WillOnce(Return(IARM_RESULT_IPCCORE_FAIL));
    EXPECT_CALL(iarmMock, IARM_Bus_Term())
        .WillOnce(Return(IARM_RESULT_SUCCESS));

    EXPECT_EQ(IARM_RESULT_SUCCESS, deviceUpdateStop());
}

TEST_F(DeviceUpdateStopTest, Initialized_TermFails_StillReturnsSuccess)
{
    forceInitialized();

    EXPECT_CALL(iarmMock, IARM_Bus_UnRegisterEventHandler(_, _))
        .WillOnce(Return(IARM_RESULT_SUCCESS));
    EXPECT_CALL(iarmMock, IARM_Bus_Disconnect())
        .WillOnce(Return(IARM_RESULT_SUCCESS));
    EXPECT_CALL(iarmMock, IARM_Bus_Term())
        .WillOnce(Return(IARM_RESULT_IPCCORE_FAIL));

    EXPECT_EQ(IARM_RESULT_SUCCESS, deviceUpdateStop());
}

TEST_F(DeviceUpdateStopTest, Initialized_SetsInitializedFalse)
{
    forceInitialized();
    EXPECT_TRUE(initialized);

    deviceUpdateStop();

    EXPECT_FALSE(initialized);
}

/* =======================================================================
 * GetXMLTagText — standalone tests (no fixture needed)
 * ====================================================================== */

TEST(GetXMLTagTextTest, TagFound_ReturnsContent)
{
    std::string xml = "<root><image:softwareVersion>2.1.0</image:softwareVersion></root>";
    EXPECT_EQ("2.1.0", getXMLTagText(xml, "image:softwareVersion"));
}

TEST(GetXMLTagTextTest, TagNotFound_ReturnsEmpty)
{
    std::string xml = "<root><other>value</other></root>";
    EXPECT_EQ("", getXMLTagText(xml, "image:softwareVersion"));
}

TEST(GetXMLTagTextTest, NumericContent_ReturnsString)
{
    std::string xml = "<data><image:type>3</image:type></data>";
    EXPECT_EQ("3", getXMLTagText(xml, "image:type"));
}

TEST(GetXMLTagTextTest, EmptyContent_ReturnsEmpty)
{
    std::string xml = "<data><image:productName></image:productName></data>";
    EXPECT_EQ("", getXMLTagText(xml, "image:productName"));
}

/* =======================================================================
 * AcceptUpdateTest — unit tests for AcceptUpdate()
 * ====================================================================== */

class AcceptUpdateTest : public ::testing::Test
{
protected:
    NiceMock<IarmBusImplMock> iarmMock;

    void SetUp() override
    {
        IarmBus::setImpl(&iarmMock);
        resetGlobals();
    }

    void TearDown() override
    {
        ::testing::Mock::VerifyAndClearExpectations(&iarmMock);
        IarmBus::setImpl(nullptr);
        resetGlobals();
    }
};

TEST_F(AcceptUpdateTest, ValidParam_ReturnsSuccess)
{
    IARM_Bus_DeviceUpdate_AcceptUpdate_Param_t param{};
    param.deviceID = 1;

    EXPECT_EQ(IARM_RESULT_SUCCESS, AcceptUpdate(&param));
}

TEST_F(AcceptUpdateTest, ValidParam_AddsEntryToMap)
{
    IARM_Bus_DeviceUpdate_AcceptUpdate_Param_t param{};
    param.deviceID = 1;

    AcceptUpdate(&param);

    EXPECT_EQ(1u, updatesInProgress->size());
}

TEST_F(AcceptUpdateTest, ValidParam_AssignsNextID)
{
    IARM_Bus_DeviceUpdate_AcceptUpdate_Param_t param{};
    nextID = 7;

    AcceptUpdate(&param);

    EXPECT_EQ(7u, param.updateSessionID);
    EXPECT_EQ(8, nextID);         /* nextID incremented */
}

TEST_F(AcceptUpdateTest, ValidParam_PropagatesInteractiveDownload)
{
    interactiveDownload = true;
    IARM_Bus_DeviceUpdate_AcceptUpdate_Param_t param{};

    AcceptUpdate(&param);

    EXPECT_EQ(1, static_cast<int>(param.interactiveDownload));
}

TEST_F(AcceptUpdateTest, ValidParam_PropagatesInteractiveLoad)
{
    interactiveLoad = true;
    IARM_Bus_DeviceUpdate_AcceptUpdate_Param_t param{};

    AcceptUpdate(&param);

    EXPECT_EQ(1, static_cast<int>(param.interactiveLoad));
}

TEST_F(AcceptUpdateTest, MultipleAccepts_SessionIDsAreUnique)
{
    IARM_Bus_DeviceUpdate_AcceptUpdate_Param_t p1{}, p2{};

    AcceptUpdate(&p1);
    AcceptUpdate(&p2);

    EXPECT_NE(p1.updateSessionID, p2.updateSessionID);
    EXPECT_EQ(2u, updatesInProgress->size());
}

/* =======================================================================
 * DeviceUpdateEventHandlerTest — tests for _deviceUpdateEventHandler()
 * ====================================================================== */

class DeviceUpdateEventHandlerTest : public ::testing::Test
{
protected:
    NiceMock<IarmBusImplMock> iarmMock;

    void SetUp() override
    {
        IarmBus::setImpl(&iarmMock);
        resetGlobals();
    }

    void TearDown() override
    {
        ::testing::Mock::VerifyAndClearExpectations(&iarmMock);
        IarmBus::setImpl(nullptr);
        resetGlobals();
    }

    /* Insert a fake in-progress update session into the map. */
    int addSession(int sessionID)
    {
        auto *params = new _IARM_Bus_DeviceUpdate_AcceptUpdate_Param_t{};
        params->updateSessionID = sessionID;
        auto *uip = new updateInProgress_t{};
        uip->acceptParams = params;
        uip->downloadPercent = 0;
        uip->loadComplete = false;
        uip->errorCode = 0;
        (*updatesInProgress)[sessionID] = uip;
        return sessionID;
    }
};

TEST_F(DeviceUpdateEventHandlerTest, WrongOwner_NoBroadcastOccurs)
{
    /* BroadcastEvent must NOT be called when owner doesn't match */
    EXPECT_CALL(iarmMock, IARM_Bus_BroadcastEvent(_, _, _, _)).Times(0);

    _deviceUpdateEventHandler("SomeOtherBus", IARM_BUS_DEVICE_UPDATE_EVENT_READY_TO_DOWNLOAD,
                              nullptr, 0);
}

TEST_F(DeviceUpdateEventHandlerTest, ReadyToDownload_NonInteractive_SendsDownloadInit)
{
    interactiveDownload = false;
    int sid = addSession(10);

    IARM_Bus_DeviceUpdate_ReadyToDownload_t eventData{};
    eventData.updateSessionID = sid;

    EXPECT_CALL(iarmMock, IARM_Bus_BroadcastEvent(
        _, (IARM_EventId_t)IARM_BUS_DEVICE_UPDATE_EVENT_DOWNLOAD_INITIATE, _, _))
        .WillOnce(Return(IARM_RESULT_SUCCESS));

    _deviceUpdateEventHandler(IARM_BUS_DEVICE_UPDATE_NAME,
                              IARM_BUS_DEVICE_UPDATE_EVENT_READY_TO_DOWNLOAD,
                              &eventData, sizeof(eventData));
}

TEST_F(DeviceUpdateEventHandlerTest, ReadyToDownload_Interactive_NoBroadcast)
{
    interactiveDownload = true;
    int sid = addSession(11);

    IARM_Bus_DeviceUpdate_ReadyToDownload_t eventData{};
    eventData.updateSessionID = sid;

    EXPECT_CALL(iarmMock, IARM_Bus_BroadcastEvent(_, _, _, _)).Times(0);

    _deviceUpdateEventHandler(IARM_BUS_DEVICE_UPDATE_NAME,
                              IARM_BUS_DEVICE_UPDATE_EVENT_READY_TO_DOWNLOAD,
                              &eventData, sizeof(eventData));
}

TEST_F(DeviceUpdateEventHandlerTest, DownloadStatus_At100Percent_NonInteractiveLoad_SendsLoadInit)
{
    interactiveLoad = false;
    int sid = addSession(20);

    IARM_Bus_DeviceUpdate_DownloadStatus_t eventData{};
    eventData.updateSessionID = sid;
    eventData.percentComplete = 100;

    EXPECT_CALL(iarmMock, IARM_Bus_BroadcastEvent(
        _, (IARM_EventId_t)IARM_BUS_DEVICE_UPDATE_EVENT_LOAD_INITIATE, _, _))
        .WillOnce(Return(IARM_RESULT_SUCCESS));

    _deviceUpdateEventHandler(IARM_BUS_DEVICE_UPDATE_NAME,
                              IARM_BUS_DEVICE_UPDATE_EVENT_DOWNLOAD_STATUS,
                              &eventData, sizeof(eventData));

    EXPECT_EQ(100, (*updatesInProgress)[sid]->downloadPercent);
}

TEST_F(DeviceUpdateEventHandlerTest, DownloadStatus_Below100Percent_NoBroadcast)
{
    interactiveLoad = false;
    int sid = addSession(21);

    IARM_Bus_DeviceUpdate_DownloadStatus_t eventData{};
    eventData.updateSessionID = sid;
    eventData.percentComplete = 50;

    EXPECT_CALL(iarmMock, IARM_Bus_BroadcastEvent(_, _, _, _)).Times(0);

    _deviceUpdateEventHandler(IARM_BUS_DEVICE_UPDATE_NAME,
                              IARM_BUS_DEVICE_UPDATE_EVENT_DOWNLOAD_STATUS,
                              &eventData, sizeof(eventData));

    EXPECT_EQ(50, (*updatesInProgress)[sid]->downloadPercent);
}

TEST_F(DeviceUpdateEventHandlerTest, LoadStatus_Complete_SetsLoadCompleteFlag)
{
    int sid = addSession(30);

    _IARM_Bus_DeviceUpdate_LoadStatus_t eventData{};
    eventData.updateSessionID = sid;
    eventData.loadStatus = LOAD_STATUS_END;

    _deviceUpdateEventHandler(IARM_BUS_DEVICE_UPDATE_NAME,
                              IARM_BUS_DEVICE_UPDATE_EVENT_LOAD_STATUS,
                              &eventData, sizeof(eventData));

    EXPECT_TRUE((*updatesInProgress)[sid]->loadComplete);
}

TEST_F(DeviceUpdateEventHandlerTest, ErrorEvent_SetsErrorCodeAndMessage)
{
    int sid = addSession(40);

    IARM_Bus_Device_Update_Error_t eventData{};
    eventData.updateSessionID = sid;
    eventData.errorType = IMAGE_INVALID;
    strncpy(reinterpret_cast<char *>(eventData.errorMessage), "bad image",
            sizeof(eventData.errorMessage) - 1);

    _deviceUpdateEventHandler(IARM_BUS_DEVICE_UPDATE_NAME,
                              IARM_BUS_DEVICE_UPDATE_EVENT_ERROR,
                              &eventData, sizeof(eventData));

    EXPECT_EQ(IMAGE_INVALID, (*updatesInProgress)[sid]->errorCode);
    EXPECT_EQ("bad image", (*updatesInProgress)[sid]->errorMsg);
}
