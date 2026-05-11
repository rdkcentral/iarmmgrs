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
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <cstdio>

/* Iarm.h / IarmBusMock.h before the sources under test */
#include "Iarm.h"
#include "IarmBusMock.h"
#include "WrapsMock.h"

/* Declarations for the real (unwrapped) libc functions used by test helpers
 * that need actual file I/O while Wraps intercepts all fopen/fclose/fgets. */
extern "C" {
    FILE *__real_fopen(const char *pathname, const char *mode);
    int   __real_fclose(FILE *stream);
    char *__real_fgets(char *s, int size, FILE *stream);
}

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
using ::testing::StrEq;
using ::testing::Invoke;

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
    timeToLoad           = 0;
    forceUpdate          = false;
    serverUpdatePath     = "/srv/device_update/";
    tempFilePath         = "/tmp/devUpdate/";
    recheckForUpdatesMin = 0;
    delayTillAnnounceTimeMin = 10;
    oneAnnouncePerRun    = false;
    announceCounter      = 10;

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

TEST_F(DeviceUpdateEventHandlerTest, DownloadStatus_100Percent_InteractiveLoad_NoBroadcast)
{
    /* When interactiveLoad=true the manager must NOT auto-send LOAD_INITIATE */
    interactiveLoad = true;
    int sid = addSession(22);

    IARM_Bus_DeviceUpdate_DownloadStatus_t eventData{};
    eventData.updateSessionID = sid;
    eventData.percentComplete = 100;

    EXPECT_CALL(iarmMock, IARM_Bus_BroadcastEvent(_, _, _, _)).Times(0);

    _deviceUpdateEventHandler(IARM_BUS_DEVICE_UPDATE_NAME,
                              IARM_BUS_DEVICE_UPDATE_EVENT_DOWNLOAD_STATUS,
                              &eventData, sizeof(eventData));

    EXPECT_EQ(100, (*updatesInProgress)[sid]->downloadPercent);
}

TEST_F(DeviceUpdateEventHandlerTest, LoadStatus_Begin_SetsLoadCompleteFalse)
{
    int sid = addSession(31);
    /* Pre-set to true so we can verify it gets written to false */
    (*updatesInProgress)[sid]->loadComplete = true;

    _IARM_Bus_DeviceUpdate_LoadStatus_t eventData{};
    eventData.updateSessionID = sid;
    eventData.loadStatus = LOAD_STATUS_BEGIN; /* 0 */

    _deviceUpdateEventHandler(IARM_BUS_DEVICE_UPDATE_NAME,
                              IARM_BUS_DEVICE_UPDATE_EVENT_LOAD_STATUS,
                              &eventData, sizeof(eventData));

    EXPECT_FALSE((*updatesInProgress)[sid]->loadComplete);
}

TEST_F(DeviceUpdateEventHandlerTest, ReadyToDownload_UnknownSession_NoCrash)
{
    /* Session 999 has never been registered — must not crash */
    interactiveDownload = false;

    IARM_Bus_DeviceUpdate_ReadyToDownload_t eventData{};
    eventData.updateSessionID = 999;

    EXPECT_CALL(iarmMock, IARM_Bus_BroadcastEvent(_, _, _, _)).Times(0);

    EXPECT_NO_FATAL_FAILURE(
        _deviceUpdateEventHandler(IARM_BUS_DEVICE_UPDATE_NAME,
                                  IARM_BUS_DEVICE_UPDATE_EVENT_READY_TO_DOWNLOAD,
                                  &eventData, sizeof(eventData)));
}

TEST_F(DeviceUpdateEventHandlerTest, DownloadStatus_UnknownSession_NoCrash)
{
    IARM_Bus_DeviceUpdate_DownloadStatus_t eventData{};
    eventData.updateSessionID = 888;
    eventData.percentComplete = 100;

    EXPECT_CALL(iarmMock, IARM_Bus_BroadcastEvent(_, _, _, _)).Times(0);

    EXPECT_NO_FATAL_FAILURE(
        _deviceUpdateEventHandler(IARM_BUS_DEVICE_UPDATE_NAME,
                                  IARM_BUS_DEVICE_UPDATE_EVENT_DOWNLOAD_STATUS,
                                  &eventData, sizeof(eventData)));
}

TEST_F(DeviceUpdateEventHandlerTest, ErrorEvent_UnknownSession_NoCrash)
{
    IARM_Bus_Device_Update_Error_t eventData{};
    eventData.updateSessionID = 777;
    eventData.errorType = IMAGE_NOT_FOUND;

    EXPECT_NO_FATAL_FAILURE(
        _deviceUpdateEventHandler(IARM_BUS_DEVICE_UPDATE_NAME,
                                  IARM_BUS_DEVICE_UPDATE_EVENT_ERROR,
                                  &eventData, sizeof(eventData)));
}

TEST_F(DeviceUpdateEventHandlerTest, DefaultEvent_NoCrash)
{
    /* An unrecognised event ID must fall into the default branch without crashing */
    EXPECT_NO_FATAL_FAILURE(
        _deviceUpdateEventHandler(IARM_BUS_DEVICE_UPDATE_NAME,
                                  (IARM_EventId_t)0xDEAD,
                                  nullptr, 0));
}

TEST_F(DeviceUpdateEventHandlerTest, ReadyToDownload_InteractiveAndNoSession_NoBroadcast)
{
    interactiveDownload = true;

    IARM_Bus_DeviceUpdate_ReadyToDownload_t eventData{};
    eventData.updateSessionID = 555;

    EXPECT_CALL(iarmMock, IARM_Bus_BroadcastEvent(_, _, _, _)).Times(0);

    _deviceUpdateEventHandler(IARM_BUS_DEVICE_UPDATE_NAME,
                              IARM_BUS_DEVICE_UPDATE_EVENT_READY_TO_DOWNLOAD,
                              &eventData, sizeof(eventData));
}

/* =======================================================================
 * SendDownLoadInitTest — direct tests for sendDownLoadInit()
 * ====================================================================== */

class SendDownLoadInitTest : public ::testing::Test
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

TEST_F(SendDownLoadInitTest, BroadcastsDownloadInitiateEvent)
{
    EXPECT_CALL(iarmMock, IARM_Bus_BroadcastEvent(
        StrEq(IARM_BUS_DEVICE_UPDATE_NAME),
        (IARM_EventId_t)IARM_BUS_DEVICE_UPDATE_EVENT_DOWNLOAD_INITIATE,
        _, _))
        .WillOnce(Return(IARM_RESULT_SUCCESS));

    sendDownLoadInit(42);
}

TEST_F(SendDownLoadInitTest, PropagatesBackgroundDownload)
{
    backgroundDownload = true;
    requestedPercentIncrement = 20;
    loadImageImmediately = true;

    _IARM_Bus_DeviceUpdate_DownloadInitiate_t captured{};
    EXPECT_CALL(iarmMock, IARM_Bus_BroadcastEvent(
        _, (IARM_EventId_t)IARM_BUS_DEVICE_UPDATE_EVENT_DOWNLOAD_INITIATE, _, _))
        .WillOnce([&](const char *, IARM_EventId_t, void *data, size_t) {
            auto *evt = static_cast<_IARM_Bus_DeviceUpdate_DownloadInitiate_t *>(data);
            captured = *evt;
            return IARM_RESULT_SUCCESS;
        });

    sendDownLoadInit(1);

    EXPECT_TRUE(captured.backgroundDownload);
    EXPECT_EQ(20, captured.requestedPercentIncrement);
    EXPECT_TRUE(captured.loadImageImmediately);
    EXPECT_EQ(1, (int)captured.updateSessionID);
}

TEST_F(SendDownLoadInitTest, BroadcastFails_NoFatalError)
{
    ON_CALL(iarmMock, IARM_Bus_BroadcastEvent(_, _, _, _))
        .WillByDefault(Return(IARM_RESULT_IPCCORE_FAIL));

    EXPECT_NO_FATAL_FAILURE(sendDownLoadInit(5));
}

/* =======================================================================
 * SendLoadInitTest — direct tests for sendLoadInit()
 * ====================================================================== */

class SendLoadInitTest : public ::testing::Test
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

TEST_F(SendLoadInitTest, BroadcastsLoadInitiateEvent)
{
    EXPECT_CALL(iarmMock, IARM_Bus_BroadcastEvent(
        StrEq(IARM_BUS_DEVICE_UPDATE_NAME),
        (IARM_EventId_t)IARM_BUS_DEVICE_UPDATE_EVENT_LOAD_INITIATE,
        _, _))
        .WillOnce(Return(IARM_RESULT_SUCCESS));

    sendLoadInit(7);
}

TEST_F(SendLoadInitTest, PropagatesLoadDelayType)
{
    loadDelayType = (int)LOAD_NORMAL;

    _IARM_Bus_DeviceUpdate_LoadInitiate_t captured{};
    EXPECT_CALL(iarmMock, IARM_Bus_BroadcastEvent(
        _, (IARM_EventId_t)IARM_BUS_DEVICE_UPDATE_EVENT_LOAD_INITIATE, _, _))
        .WillOnce([&](const char *, IARM_EventId_t, void *data, size_t) {
            auto *evt = static_cast<_IARM_Bus_DeviceUpdate_LoadInitiate_t *>(data);
            captured = *evt;
            return IARM_RESULT_SUCCESS;
        });

    sendLoadInit(3);

    EXPECT_EQ(LOAD_NORMAL, captured.loadDelayType);
    EXPECT_EQ(3, (int)captured.updateSessionID);
}

TEST_F(SendLoadInitTest, BroadcastFails_NoFatalError)
{
    ON_CALL(iarmMock, IARM_Bus_BroadcastEvent(_, _, _, _))
        .WillByDefault(Return(IARM_RESULT_IPCCORE_FAIL));

    EXPECT_NO_FATAL_FAILURE(sendLoadInit(2));
}

/* =======================================================================
 * GetEventDataTest — tests for getEventData()
 * Creates temporary XML files in /tmp for each test.
 * ====================================================================== */

static const char *k_testXmlDir = "/tmp/iarmmgrs_test_getEventData/";

class GetEventDataTest : public ::testing::Test
{
protected:
    std::string tmpDir;
    ::testing::NiceMock<WrapsImplMock> wrapsMock;

    void SetUp() override
    {
        Wraps::setImpl(&wrapsMock);
        ON_CALL(wrapsMock, fopen(_, _)).WillByDefault(::testing::Invoke(__real_fopen));
        ON_CALL(wrapsMock, fclose(_)).WillByDefault(::testing::Invoke(__real_fclose));
        ON_CALL(wrapsMock, fgets(_, _, _)).WillByDefault(::testing::Invoke(__real_fgets));
        resetGlobals();
        tmpDir = k_testXmlDir;
        mkdir(tmpDir.c_str(), 0755);
    }

    void TearDown() override
    {
        /* Remove all files we created */
        DIR *dp = opendir(tmpDir.c_str());
        if (dp) {
            struct dirent *de;
            while ((de = readdir(dp)) != nullptr) {
                if (de->d_type == DT_REG) {
                    std::string path = tmpDir + de->d_name;
                    unlink(path.c_str());
                }
            }
            closedir(dp);
        }
        rmdir(tmpDir.c_str());
        ::testing::Mock::VerifyAndClearExpectations(&wrapsMock);
        Wraps::setImpl(nullptr);
        resetGlobals();
    }

    std::string writeFile(const std::string &name, const std::string &content)
    {
        std::string path = tmpDir + name;
        FILE *f = fopen(path.c_str(), "w");
        if (f) {
            fwrite(content.c_str(), 1, content.size(), f);
            fclose(f);
        }
        return path;
    }
};

TEST_F(GetEventDataTest, FileNotFound_ReturnsFalse)
{
    _IARM_Bus_DeviceUpdate_Announce_t data{};
    EXPECT_FALSE(getEventData("/tmp/no_such_file_xyz_12345.xml", &data));
}

TEST_F(GetEventDataTest, MissingSoftwareVersionTag_ReturnsFalse)
{
    std::string xml =
        "<device>"
        "<image:type>1</image:type>"
        "<image:productName>TestDevice</image:productName>"
        "</device>";
    std::string path = writeFile("missing_ver.xml", xml);

    _IARM_Bus_DeviceUpdate_Announce_t data{};
    EXPECT_FALSE(getEventData(path, &data));
}

TEST_F(GetEventDataTest, ValidXml_ReturnsTrue)
{
    std::string xml =
        "<device>"
        "<image:softwareVersion>3.0.1</image:softwareVersion>"
        "<image:type>2</image:type>"
        "<image:productName>XR15</image:productName>"
        "</device>";
    std::string path = writeFile("valid.xml", xml);

    _IARM_Bus_DeviceUpdate_Announce_t data{};
    EXPECT_TRUE(getEventData(path, &data));
}

TEST_F(GetEventDataTest, ValidXml_PopulatesVersion)
{
    std::string xml =
        "<device>"
        "<image:softwareVersion>4.2.0</image:softwareVersion>"
        "<image:type>0</image:type>"
        "<image:productName>TestDev</image:productName>"
        "</device>";
    std::string path = writeFile("ver.xml", xml);

    _IARM_Bus_DeviceUpdate_Announce_t data{};
    getEventData(path, &data);

    EXPECT_STREQ("4.2.0", data.deviceImageVersion);
}

TEST_F(GetEventDataTest, ValidXml_PopulatesDeviceType)
{
    std::string xml =
        "<device>"
        "<image:softwareVersion>1.0</image:softwareVersion>"
        "<image:type>5</image:type>"
        "<image:productName>Dev</image:productName>"
        "</device>";
    std::string path = writeFile("type.xml", xml);

    _IARM_Bus_DeviceUpdate_Announce_t data{};
    getEventData(path, &data);

    EXPECT_EQ(5u, data.deviceImageType);
}

TEST_F(GetEventDataTest, ValidXml_PopulatesDeviceName)
{
    std::string xml =
        "<device>"
        "<image:softwareVersion>1.0</image:softwareVersion>"
        "<image:type>0</image:type>"
        "<image:productName>MyRemote</image:productName>"
        "</device>";
    std::string path = writeFile("name.xml", xml);

    _IARM_Bus_DeviceUpdate_Announce_t data{};
    getEventData(path, &data);

    EXPECT_STREQ("MyRemote", data.deviceName);
}

TEST_F(GetEventDataTest, ValidXml_PropagatesForceUpdate)
{
    forceUpdate = true;
    std::string xml =
        "<device>"
        "<image:softwareVersion>1.0</image:softwareVersion>"
        "<image:type>0</image:type>"
        "<image:productName>Dev</image:productName>"
        "</device>";
    std::string path = writeFile("force.xml", xml);

    _IARM_Bus_DeviceUpdate_Announce_t data{};
    getEventData(path, &data);

    EXPECT_TRUE(data.forceUpdate);
}

/* =======================================================================
 * LoadConfigTest — tests for loadConfig()
 * Uses a temporary directory as the CWD so the "current directory"
 * fallback path is predictable and isolated from real system files.
 * ====================================================================== */

static const char *k_testConfigDir = "/tmp/iarmmgrs_test_loadConfig/";
static const char *k_configFileName = "deviceUpdateConfig.json";

class LoadConfigTest : public ::testing::Test
{
protected:
    char savedCwd[4096];
    ::testing::NiceMock<WrapsImplMock> wrapsMock;

    void SetUp() override
    {
        Wraps::setImpl(&wrapsMock);
        ON_CALL(wrapsMock, fopen(_, _)).WillByDefault(::testing::Invoke(__real_fopen));
        ON_CALL(wrapsMock, fclose(_)).WillByDefault(::testing::Invoke(__real_fclose));
        ON_CALL(wrapsMock, fgets(_, _, _)).WillByDefault(::testing::Invoke(__real_fgets));
        /* Save real CWD and switch into our scratch directory */
        getcwd(savedCwd, sizeof(savedCwd));
        mkdir(k_testConfigDir, 0755);
        chdir(k_testConfigDir);
        resetGlobals();
    }

    void TearDown() override
    {
        /* Remove the config file if it was created */
        unlink(k_configFileName);
        chdir(savedCwd);
        rmdir(k_testConfigDir);
        ::testing::Mock::VerifyAndClearExpectations(&wrapsMock);
        Wraps::setImpl(nullptr);
        resetGlobals();
    }

    void writeConfig(const std::string &json)
    {
        FILE *f = fopen(k_configFileName, "w");
        if (f) {
            fwrite(json.c_str(), 1, json.size(), f);
            fclose(f);
        }
    }
};

TEST_F(LoadConfigTest, NoFileFound_ReturnsFalse)
{
    /* The scratch directory has no deviceUpdateConfig.json and the standard
     * system paths (/opt, /mnt/nfs/env, /etc) won't have it in CI either */
    EXPECT_FALSE(loadConfig());
}

TEST_F(LoadConfigTest, ValidConfig_ReturnsTrue)
{
    writeConfig(R"({
        "serverUpdatePath": "/tmp/test_srv/",
        "tempFilePath": "/tmp/test_tmp/",
        "forceUpdate": "false",
        "interactiveDownload": "false",
        "interactiveLoad": "false",
        "loadDelayType": "2",
        "loadTimeAfterInactive": "30",
        "timeToLoad": "0",
        "backgroundDownload": "true",
        "loadImageImmediately": "false",
        "loadBeforeHour": "6",
        "requestedPercentIncrement": "25",
        "recheckForUpdatesMin": "5",
        "delayTillAnnounceTimeMin": "15",
        "oneAnnouncePerRun": "true",
        "deviceFoldersToWatch": ["XR11v2"]
    })");

    EXPECT_TRUE(loadConfig());
}

TEST_F(LoadConfigTest, ValidConfig_SetsServerUpdatePath)
{
    writeConfig(R"({
        "serverUpdatePath": "/tmp/test_srv/",
        "tempFilePath": "/tmp/test_tmp/",
        "deviceFoldersToWatch": ["XR11v2"]
    })");

    loadConfig();

    EXPECT_EQ("/tmp/test_srv/", serverUpdatePath);
}

TEST_F(LoadConfigTest, ValidConfig_SetsTempFilePath)
{
    writeConfig(R"({
        "serverUpdatePath": "/tmp/test_srv/",
        "tempFilePath": "/tmp/test_tmp/",
        "deviceFoldersToWatch": ["XR11v2"]
    })");

    loadConfig();

    EXPECT_EQ("/tmp/test_tmp/", tempFilePath);
}

TEST_F(LoadConfigTest, ValidConfig_SetsLoadDelayType)
{
    writeConfig(R"({
        "serverUpdatePath": "/tmp/test_srv/",
        "tempFilePath": "/tmp/test_tmp/",
        "loadDelayType": "2",
        "deviceFoldersToWatch": ["XR11v2"]
    })");

    loadConfig();

    EXPECT_EQ(2, loadDelayType);
}

TEST_F(LoadConfigTest, ValidConfig_SetsRequestedPercentIncrement)
{
    writeConfig(R"({
        "serverUpdatePath": "/tmp/test_srv/",
        "tempFilePath": "/tmp/test_tmp/",
        "requestedPercentIncrement": "25",
        "deviceFoldersToWatch": ["XR11v2"]
    })");

    loadConfig();

    EXPECT_EQ(25, requestedPercentIncrement);
}

TEST_F(LoadConfigTest, ValidConfig_SetsBackgroundDownload)
{
    writeConfig(R"({
        "serverUpdatePath": "/tmp/test_srv/",
        "tempFilePath": "/tmp/test_tmp/",
        "backgroundDownload": "true",
        "deviceFoldersToWatch": ["XR11v2"]
    })");

    loadConfig();

    EXPECT_TRUE(backgroundDownload);
}

TEST_F(LoadConfigTest, ValidConfig_SetsDelayTillAnnounceTimeMin)
{
    writeConfig(R"({
        "serverUpdatePath": "/tmp/test_srv/",
        "tempFilePath": "/tmp/test_tmp/",
        "delayTillAnnounceTimeMin": "20",
        "deviceFoldersToWatch": ["XR11v2"]
    })");

    loadConfig();

    EXPECT_EQ(20, delayTillAnnounceTimeMin);
}

TEST_F(LoadConfigTest, ValidConfig_SetsLoadBeforeHour)
{
    writeConfig(R"({
        "serverUpdatePath": "/tmp/test_srv/",
        "tempFilePath": "/tmp/test_tmp/",
        "loadBeforeHour": "3",
        "deviceFoldersToWatch": ["XR11v2"]
    })");

    loadConfig();

    EXPECT_EQ(3, loadBeforeHour);
}

TEST_F(LoadConfigTest, ValidConfig_SetsInteractiveDownloadTrue)
{
    writeConfig(R"({
        "serverUpdatePath": "/tmp/test_srv/",
        "tempFilePath": "/tmp/test_tmp/",
        "interactiveDownload": "true",
        "deviceFoldersToWatch": ["XR11v2"]
    })");

    loadConfig();

    EXPECT_TRUE(interactiveDownload);
}

TEST_F(LoadConfigTest, ValidConfig_SetsOneAnnouncePerRun)
{
    writeConfig(R"({
        "serverUpdatePath": "/tmp/test_srv/",
        "tempFilePath": "/tmp/test_tmp/",
        "oneAnnouncePerRun": "true",
        "deviceFoldersToWatch": ["XR11v2"]
    })");

    loadConfig();

    EXPECT_TRUE(oneAnnouncePerRun);
}

TEST_F(LoadConfigTest, ValidConfig_SetsRecheckForUpdatesMin)
{
    writeConfig(R"({
        "serverUpdatePath": "/tmp/test_srv/",
        "tempFilePath": "/tmp/test_tmp/",
        "recheckForUpdatesMin": "10",
        "deviceFoldersToWatch": ["XR11v2"]
    })");

    loadConfig();

    EXPECT_EQ(10, recheckForUpdatesMin);
}
