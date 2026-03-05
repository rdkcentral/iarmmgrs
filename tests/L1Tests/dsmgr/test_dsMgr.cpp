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
 * @file test_dsMgr.cpp
 * @brief L1 unit tests for dsmgr/dsMgr.c.
 *
 * Components covered:
 *   - getSecondaryResolution()   — EU secondary-resolution string logic
 *   - parseResolution()          — resolution name base extraction
 *   - getFallBackResolution()    — EU/non-EU fallback fps encoding
 *   - _SysModeChange()           — isEAS state-machine transitions
 *   - _EventHandler()            — owner/event-ID filtering,
 *                                  TUNEREADY dispatch,
 *                                  HDMI hot-plug dispatch,
 *                                  HDCP status broadcast
 *
 * Include order
 * -------------
 * sysMgr.h is included before Iarm.h to set the _IARM_BUS_SYSMGR_H guard
 * before Iarm.h is processed, preventing duplicate-type errors that arise
 * because Iarm.h contains a copy of the sysMgr type block.
 *
 * Link-time stubs
 * ---------------
 * All external symbols referenced by dsMgr.c (DS HAL, PowerController,
 * PwrEventListener helpers, etc.) are defined in the extern "C" block
 * BEFORE #include "dsMgr.c" so the intra-TU linker resolves them without
 * a separate stub translation unit.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <cstring>
#include <pthread.h>

/* sysMgr.h first — prevents type-redefinition conflicts with Iarm.h */
#include "sysMgr.h"
#include "Iarm.h"
#include "IarmBusMock.h"

/* -----------------------------------------------------------------------
 * All external symbols that dsMgr.c references but does not define.
 * Placed inside extern "C" to match the C linkage that dsMgr.c assigns
 * to these symbols through its own extern declarations / include chain.
 * --------------------------------------------------------------------- */
extern "C" {

/* ---- DS HAL functions (declared extern at the top of dsMgr.c) ------- */
IARM_Result_t _dsGetVideoPort(void *arg)        { return IARM_RESULT_SUCCESS; }
IARM_Result_t _dsIsDisplayConnected(void *arg)  { return IARM_RESULT_SUCCESS; }
IARM_Result_t _dsGetIgnoreEDIDStatus(void *arg) { return IARM_RESULT_SUCCESS; }
IARM_Result_t _dsSetResolution(void *arg)       { return IARM_RESULT_SUCCESS; }
IARM_Result_t _dsGetResolution(void *arg)       { return IARM_RESULT_SUCCESS; }
IARM_Result_t _dsInitResolution(void *arg)      { return IARM_RESULT_SUCCESS; }
IARM_Result_t _dsGetEDID(void *arg)             { return IARM_RESULT_SUCCESS; }
IARM_Result_t _dsGetEDIDBytes(void *arg)        { return IARM_RESULT_SUCCESS; }
IARM_Result_t _dsGetForceDisable4K(void *arg)   { return IARM_RESULT_SUCCESS; }
IARM_Result_t _dsSetBackgroundColor(void *arg)  { return IARM_RESULT_SUCCESS; }
IARM_Result_t _dsGetAudioPort(void *arg)        { return IARM_RESULT_SUCCESS; }
IARM_Result_t _dsGetStereoMode(void *arg)       { return IARM_RESULT_SUCCESS; }
IARM_Result_t _dsSetStereoMode(void *arg)       { return IARM_RESULT_SUCCESS; }
IARM_Result_t _dsGetStereoAuto(void *arg)       { return IARM_RESULT_SUCCESS; }
IARM_Result_t _dsIsDisplaySurround(void *arg)   { return IARM_RESULT_SUCCESS; }

/* ---- Miscellaneous helpers ------------------------------------------ */
bool dsGetHDMIDDCLineStatus(void) { return false; }
bool isComponentPortPresent(void) { return true; }

/* ---- DS Manager HAL initialisation (called from DSMgr_Start) --------- */
IARM_Result_t dsMgr_init(void) { return IARM_RESULT_SUCCESS; }

/* ---- Power Controller API (declared in power_controller.h) ----------- */
void     PowerController_Init(void)  {}
void     PowerController_Term(void)  {}

/* ---- Power Event Listener helper (defined in dsMgrPwrEventListener.c)  */
void initPwrEventListner(void)         {}
void dsMgrInitPwrControllerEvt(void)   {}
void dsMgrDeinitPwrControllerEvt(void) {}

/* -----------------------------------------------------------------------
 * Source under test — included last so the stubs above satisfy all
 * extern declarations found inside dsMgr.c.
 * --------------------------------------------------------------------- */
#include "dsMgr.c"

} /* extern "C" */

using ::testing::_;
using ::testing::StrEq;
using ::testing::Return;

/* -----------------------------------------------------------------------
 * Helper: build a SYSMgr system-state event payload
 * --------------------------------------------------------------------- */
static IARM_Bus_SYSMgr_EventData_t makeSysStateEvent(
        IARM_Bus_SYSMgr_SystemState_t stateId, int state)
{
    IARM_Bus_SYSMgr_EventData_t ev;
    memset(&ev, 0, sizeof(ev));
    ev.data.systemStates.stateId = stateId;
    ev.data.systemStates.state   = state;
    return ev;
}

/* -----------------------------------------------------------------------
 * Helper: build a DSMgr HDMI hot-plug event payload
 * --------------------------------------------------------------------- */
static IARM_Bus_DSMgr_EventData_t makeHpdEvent(dsDisplayEvent_t event)
{
    IARM_Bus_DSMgr_EventData_t ev;
    memset(&ev, 0, sizeof(ev));
    ev.data.hdmi_hpd.event = event;
    return ev;
}

/* -----------------------------------------------------------------------
 * Fixture
 *
 * A NiceMock<IarmBusImplMock> is installed for each test so that
 * IARM_Bus_BroadcastEvent (called by _EventHandler for HDCP status) routes
 * through the mock.  NiceMock returns IARM_RESULT_SUCCESS for all
 * unset expectations, keeping tests focused on the logic under test.
 *
 * The mutex and condvar used by _EventHandler are initialised in SetUp()
 * because DSMgr_Start() (which normally initialises them) is not called.
 * Global state variables from dsMgr.c are reset to defined initial values.
 * --------------------------------------------------------------------- */
class DsMgrTest : public ::testing::Test
{
protected:
    ::testing::NiceMock<IarmBusImplMock> iarmMock;

    void SetUp() override
    {
        IarmBus::setImpl(&iarmMock);

        /* Initialise the mutex / condvar that _EventHandler uses. */
        pthread_mutex_init(&tdsMutexLock, nullptr);
        pthread_cond_init(&tdsMutexCond, nullptr);

        /* Reset module-level statics to known values. */
        isEAS               = IARM_BUS_SYS_MODE_NORMAL;
        iTuneReady          = 0;
        IsEUPlatform        = false;
        edisplayEventStatus = dsDISPLAY_EVENT_MAX;
        bHDCPAuthenticated  = false;
        bootup_flag_enabled = true;
        IsIgnoreEdid_gs     = false;
    }

    void TearDown() override
    {
        pthread_cond_destroy(&tdsMutexCond);
        pthread_mutex_destroy(&tdsMutexLock);
        ::testing::Mock::VerifyAndClearExpectations(&iarmMock);
        IarmBus::setImpl(nullptr);
    }
};

/* =======================================================================
 * Section 1 – getSecondaryResolution
 * ===================================================================== */

TEST_F(DsMgrTest, GetSecondaryResolution_720p_Returns720p)
{
    char secRes[RES_MAX_LEN] = {};
    EXPECT_TRUE(getSecondaryResolution(const_cast<char *>("720p50"), secRes));
    EXPECT_STREQ("720p", secRes);
}

TEST_F(DsMgrTest, GetSecondaryResolution_1080p_Returns1080p60)
{
    char secRes[RES_MAX_LEN] = {};
    EXPECT_TRUE(getSecondaryResolution(const_cast<char *>("1080p50"), secRes));
    EXPECT_STREQ("1080p60", secRes);
}

TEST_F(DsMgrTest, GetSecondaryResolution_1080i_Returns1080i)
{
    char secRes[RES_MAX_LEN] = {};
    EXPECT_TRUE(getSecondaryResolution(const_cast<char *>("1080i25"), secRes));
    EXPECT_STREQ("1080i", secRes);
}

TEST_F(DsMgrTest, GetSecondaryResolution_2160p_Returns2160p60)
{
    char secRes[RES_MAX_LEN] = {};
    EXPECT_TRUE(getSecondaryResolution(const_cast<char *>("2160p50"), secRes));
    EXPECT_STREQ("2160p60", secRes);
}

TEST_F(DsMgrTest, GetSecondaryResolution_480p_ReturnsFalse)
{
    char secRes[RES_MAX_LEN] = {};
    EXPECT_FALSE(getSecondaryResolution(const_cast<char *>("480p"), secRes));
}

TEST_F(DsMgrTest, GetSecondaryResolution_576p_ReturnsFalse)
{
    char secRes[RES_MAX_LEN] = {};
    EXPECT_FALSE(getSecondaryResolution(const_cast<char *>("576p"), secRes));
}

/* =======================================================================
 * Section 2 – parseResolution
 * ===================================================================== */

TEST_F(DsMgrTest, ParseResolution_1080p60_ReturnsBase1080p)
{
    char bResn[RES_MAX_LEN] = {};
    parseResolution(const_cast<char *>("1080p60"), bResn);
    EXPECT_STREQ("1080p", bResn);
}

TEST_F(DsMgrTest, ParseResolution_1080i25_ReturnsBase1080i)
{
    char bResn[RES_MAX_LEN] = {};
    parseResolution(const_cast<char *>("1080i25"), bResn);
    EXPECT_STREQ("1080i", bResn);
}

TEST_F(DsMgrTest, ParseResolution_720p50_ReturnsBase720p)
{
    char bResn[RES_MAX_LEN] = {};
    parseResolution(const_cast<char *>("720p50"), bResn);
    EXPECT_STREQ("720p", bResn);
}

TEST_F(DsMgrTest, ParseResolution_2160p60_ReturnsBase2160p)
{
    char bResn[RES_MAX_LEN] = {};
    parseResolution(const_cast<char *>("2160p60"), bResn);
    EXPECT_STREQ("2160p", bResn);
}

TEST_F(DsMgrTest, ParseResolution_480p_ReturnsBase480p)
{
    char bResn[RES_MAX_LEN] = {};
    parseResolution(const_cast<char *>("480p"), bResn);
    EXPECT_STREQ("480p", bResn);
}

/* =======================================================================
 * Section 3 – getFallBackResolution
 * ===================================================================== */

/* ---- EU platform (flag == 1) ---------------------------------------- */

TEST_F(DsMgrTest, GetFallBackResolution_EU_2160p_Appends50)
{
    char fbResn[RES_MAX_LEN] = {};
    getFallBackResolution(const_cast<char *>("2160p"), fbResn, 1);
    EXPECT_STREQ("2160p50", fbResn);
}

TEST_F(DsMgrTest, GetFallBackResolution_EU_1080p_Appends50)
{
    char fbResn[RES_MAX_LEN] = {};
    getFallBackResolution(const_cast<char *>("1080p"), fbResn, 1);
    EXPECT_STREQ("1080p50", fbResn);
}

TEST_F(DsMgrTest, GetFallBackResolution_EU_1080i_Appends25)
{
    char fbResn[RES_MAX_LEN] = {};
    getFallBackResolution(const_cast<char *>("1080i"), fbResn, 1);
    EXPECT_STREQ("1080i25", fbResn);
}

TEST_F(DsMgrTest, GetFallBackResolution_EU_720p_Appends50)
{
    char fbResn[RES_MAX_LEN] = {};
    getFallBackResolution(const_cast<char *>("720p"), fbResn, 1);
    EXPECT_STREQ("720p50", fbResn);
}

TEST_F(DsMgrTest, GetFallBackResolution_EU_576p_NoChange)
{
    char fbResn[RES_MAX_LEN] = {};
    getFallBackResolution(const_cast<char *>("576p"), fbResn, 1);
    EXPECT_STREQ("576p", fbResn);
}

TEST_F(DsMgrTest, GetFallBackResolution_EU_480p_NoChange)
{
    char fbResn[RES_MAX_LEN] = {};
    getFallBackResolution(const_cast<char *>("480p"), fbResn, 1);
    EXPECT_STREQ("480p", fbResn);
}

/* ---- Non-EU platform (flag == 0) ------------------------------------ */

TEST_F(DsMgrTest, GetFallBackResolution_NonEU_2160p_Appends60)
{
    char fbResn[RES_MAX_LEN] = {};
    getFallBackResolution(const_cast<char *>("2160p"), fbResn, 0);
    EXPECT_STREQ("2160p60", fbResn);
}

TEST_F(DsMgrTest, GetFallBackResolution_NonEU_1080p_Appends60)
{
    char fbResn[RES_MAX_LEN] = {};
    getFallBackResolution(const_cast<char *>("1080p"), fbResn, 0);
    EXPECT_STREQ("1080p60", fbResn);
}

TEST_F(DsMgrTest, GetFallBackResolution_NonEU_720p_NoChange)
{
    char fbResn[RES_MAX_LEN] = {};
    getFallBackResolution(const_cast<char *>("720p"), fbResn, 0);
    EXPECT_STREQ("720p", fbResn);
}

TEST_F(DsMgrTest, GetFallBackResolution_NonEU_1080i_NoChange)
{
    char fbResn[RES_MAX_LEN] = {};
    getFallBackResolution(const_cast<char *>("1080i"), fbResn, 0);
    EXPECT_STREQ("1080i", fbResn);
}

TEST_F(DsMgrTest, GetFallBackResolution_NonEU_480p_NoChange)
{
    char fbResn[RES_MAX_LEN] = {};
    getFallBackResolution(const_cast<char *>("480p"), fbResn, 0);
    EXPECT_STREQ("480p", fbResn);
}

/* =======================================================================
 * Section 4 – _SysModeChange: isEAS state-machine transitions
 *
 * _SysModeChange() toggles the module-global isEAS variable and triggers
 * _setAudioMode() / _setEASAudioMode() as a side effect.  The DS HAL
 * stubs above absorb those calls, so the tests focus on isEAS state only.
 * ===================================================================== */

TEST_F(DsMgrTest, SysModeChange_NormalToEAS_SetsEASAndReturnsSuccess)
{
    IARM_Bus_CommonAPI_SysModeChange_Param_t param = {};
    param.newMode = IARM_BUS_SYS_MODE_EAS;
    param.oldMode = IARM_BUS_SYS_MODE_NORMAL;

    EXPECT_EQ(IARM_RESULT_SUCCESS, _SysModeChange(&param));
    EXPECT_EQ(IARM_BUS_SYS_MODE_EAS, isEAS);
}

TEST_F(DsMgrTest, SysModeChange_EASToNormal_SetsNormalAndReturnsSuccess)
{
    isEAS = IARM_BUS_SYS_MODE_EAS;

    IARM_Bus_CommonAPI_SysModeChange_Param_t param = {};
    param.newMode = IARM_BUS_SYS_MODE_NORMAL;
    param.oldMode = IARM_BUS_SYS_MODE_EAS;

    EXPECT_EQ(IARM_RESULT_SUCCESS, _SysModeChange(&param));
    EXPECT_EQ(IARM_BUS_SYS_MODE_NORMAL, isEAS);
}

TEST_F(DsMgrTest, SysModeChange_NormalToNormal_NoOpReturnsSuccess)
{
    IARM_Bus_CommonAPI_SysModeChange_Param_t param = {};
    param.newMode = IARM_BUS_SYS_MODE_NORMAL;
    param.oldMode = IARM_BUS_SYS_MODE_NORMAL;

    EXPECT_EQ(IARM_RESULT_SUCCESS, _SysModeChange(&param));
    EXPECT_EQ(IARM_BUS_SYS_MODE_NORMAL, isEAS);  /* unchanged */
}

TEST_F(DsMgrTest, SysModeChange_EASToEAS_NoOpReturnsSuccess)
{
    isEAS = IARM_BUS_SYS_MODE_EAS;

    IARM_Bus_CommonAPI_SysModeChange_Param_t param = {};
    param.newMode = IARM_BUS_SYS_MODE_EAS;
    param.oldMode = IARM_BUS_SYS_MODE_EAS;

    EXPECT_EQ(IARM_RESULT_SUCCESS, _SysModeChange(&param));
    EXPECT_EQ(IARM_BUS_SYS_MODE_EAS, isEAS);  /* unchanged */
}

TEST_F(DsMgrTest, SysModeChange_WarehouseMode_IsIgnoredReturnsSuccess)
{
    IARM_Bus_CommonAPI_SysModeChange_Param_t param = {};
    param.newMode = IARM_BUS_SYS_MODE_WAREHOUSE;
    param.oldMode = IARM_BUS_SYS_MODE_NORMAL;

    EXPECT_EQ(IARM_RESULT_SUCCESS, _SysModeChange(&param));
    EXPECT_EQ(IARM_BUS_SYS_MODE_NORMAL, isEAS);  /* unchanged */
}

/* =======================================================================
 * Section 5 – _EventHandler: owner / event-ID filtering
 * ===================================================================== */

TEST_F(DsMgrTest, EventHandler_WrongOwner_IgnoresSysMgrStateEvent)
{
    auto ev = makeSysStateEvent(IARM_BUS_SYSMGR_SYSSTATE_TUNEREADY, 1);
    _EventHandler("WrongOwner",
                  IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE,
                  &ev, sizeof(ev));

    EXPECT_EQ(0, iTuneReady);  /* unchanged */
}

TEST_F(DsMgrTest, EventHandler_SysMgrOwner_WrongEventId_Ignored)
{
    auto ev = makeSysStateEvent(IARM_BUS_SYSMGR_SYSSTATE_TUNEREADY, 1);
    /* Event ID 99 is not IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE */
    _EventHandler(IARM_BUS_SYSMGR_NAME,
                  99,
                  &ev, sizeof(ev));

    EXPECT_EQ(0, iTuneReady);
}

TEST_F(DsMgrTest, EventHandler_DsMgrOwner_WrongEventId_Ignored)
{
    auto ev = makeHpdEvent(dsDISPLAY_EVENT_CONNECTED);
    /* Event ID 99 is neither HDMI_HOTPLUG nor HDCP_STATUS */
    _EventHandler(IARM_BUS_DSMGR_NAME,
                  99,
                  &ev, sizeof(ev));

    /* edisplayEventStatus must remain at its initial MAX sentinel */
    EXPECT_EQ(dsDISPLAY_EVENT_MAX, edisplayEventStatus);
}

/* =======================================================================
 * Section 6 – _EventHandler: TUNEREADY dispatch
 * ===================================================================== */

TEST_F(DsMgrTest, EventHandler_TuneReady_WhenNotYetSet_SetsTuneReadyFlag)
{
    ASSERT_EQ(0, iTuneReady);

    auto ev = makeSysStateEvent(IARM_BUS_SYSMGR_SYSSTATE_TUNEREADY, 1);
    _EventHandler(IARM_BUS_SYSMGR_NAME,
                  IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE,
                  &ev, sizeof(ev));

    EXPECT_EQ(1, iTuneReady);
}

TEST_F(DsMgrTest, EventHandler_TuneReady_WhenAlreadySet_DoesNotReenter)
{
    /* Guard: if iTuneReady is already 1 the inner if-block is skipped */
    iTuneReady = 1;

    auto ev = makeSysStateEvent(IARM_BUS_SYSMGR_SYSSTATE_TUNEREADY, 1);
    _EventHandler(IARM_BUS_SYSMGR_NAME,
                  IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE,
                  &ev, sizeof(ev));

    EXPECT_EQ(1, iTuneReady);
}

TEST_F(DsMgrTest, EventHandler_SysMgr_NonTuneReadyStateId_NoTuneReadyChange)
{
    /* CHANNELMAP is a SYSMGR event but not handled by _EventHandler switch */
    auto ev = makeSysStateEvent(IARM_BUS_SYSMGR_SYSSTATE_CHANNELMAP, 1);
    _EventHandler(IARM_BUS_SYSMGR_NAME,
                  IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE,
                  &ev, sizeof(ev));

    EXPECT_EQ(0, iTuneReady);
}

/* =======================================================================
 * Section 7 – _EventHandler: HDMI hot-plug dispatch
 * ===================================================================== */

TEST_F(DsMgrTest, EventHandler_HdmiHotplugConnect_SetsDisplayEventStatusConnected)
{
    auto ev = makeHpdEvent(dsDISPLAY_EVENT_CONNECTED);
    _EventHandler(IARM_BUS_DSMGR_NAME,
                  IARM_BUS_DSMGR_EVENT_HDMI_HOTPLUG,
                  &ev, sizeof(ev));

    EXPECT_EQ(dsDISPLAY_EVENT_CONNECTED, edisplayEventStatus);
}

TEST_F(DsMgrTest, EventHandler_HdmiHotplugDisconnect_SetsDisplayEventStatusDisconnected)
{
    auto ev = makeHpdEvent(dsDISPLAY_EVENT_DISCONNECTED);
    _EventHandler(IARM_BUS_DSMGR_NAME,
                  IARM_BUS_DSMGR_EVENT_HDMI_HOTPLUG,
                  &ev, sizeof(ev));

    EXPECT_EQ(dsDISPLAY_EVENT_DISCONNECTED, edisplayEventStatus);
}

/* =======================================================================
 * Section 8 – _EventHandler: HDCP status broadcast
 *
 * On both AUTHENTICATED and AUTHENTICATIONFAILURE the handler must
 * broadcast a SYSMGR SYSTEMSTATE event carrying the new HDCP state.
 * The mock verifies the broadcast is issued exactly once with the
 * correct bus name and event ID.
 * ===================================================================== */

TEST_F(DsMgrTest, EventHandler_HdcpAuthenticated_BroadcastsHdcpEnabledEvent)
{
    EXPECT_CALL(iarmMock, IARM_Bus_BroadcastEvent(
        StrEq(IARM_BUS_SYSMGR_NAME),
        static_cast<IARM_EventId_t>(IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE),
        _, _))
        .WillOnce(Return(IARM_RESULT_SUCCESS));

    IARM_Bus_DSMgr_EventData_t ev = {};
    ev.data.hdmi_hdcp.hdcpStatus = dsHDCP_STATUS_AUTHENTICATED;
    _EventHandler(IARM_BUS_DSMGR_NAME,
                  IARM_BUS_DSMGR_EVENT_HDCP_STATUS,
                  &ev, sizeof(ev));
}

TEST_F(DsMgrTest, EventHandler_HdcpAuthFail_BroadcastsHdcpDisabledEvent)
{
    EXPECT_CALL(iarmMock, IARM_Bus_BroadcastEvent(
        StrEq(IARM_BUS_SYSMGR_NAME),
        static_cast<IARM_EventId_t>(IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE),
        _, _))
        .WillOnce(Return(IARM_RESULT_SUCCESS));

    IARM_Bus_DSMgr_EventData_t ev = {};
    ev.data.hdmi_hdcp.hdcpStatus = dsHDCP_STATUS_AUTHENTICATIONFAILURE;
    _EventHandler(IARM_BUS_DSMGR_NAME,
                  IARM_BUS_DSMGR_EVENT_HDCP_STATUS,
                  &ev, sizeof(ev));
}

TEST_F(DsMgrTest, EventHandler_HdcpAuthenticated_SetsBHDCPAuthenticatedTrue)
{
    IARM_Bus_DSMgr_EventData_t ev = {};
    ev.data.hdmi_hdcp.hdcpStatus = dsHDCP_STATUS_AUTHENTICATED;
    _EventHandler(IARM_BUS_DSMGR_NAME,
                  IARM_BUS_DSMGR_EVENT_HDCP_STATUS,
                  &ev, sizeof(ev));

    EXPECT_TRUE(bHDCPAuthenticated);
}

TEST_F(DsMgrTest, EventHandler_HdcpAuthFail_SetsBHDCPAuthenticatedFalse)
{
    bHDCPAuthenticated = true;  /* start authenticated */

    IARM_Bus_DSMgr_EventData_t ev = {};
    ev.data.hdmi_hdcp.hdcpStatus = dsHDCP_STATUS_AUTHENTICATIONFAILURE;
    _EventHandler(IARM_BUS_DSMGR_NAME,
                  IARM_BUS_DSMGR_EVENT_HDCP_STATUS,
                  &ev, sizeof(ev));

    EXPECT_FALSE(bHDCPAuthenticated);
}
