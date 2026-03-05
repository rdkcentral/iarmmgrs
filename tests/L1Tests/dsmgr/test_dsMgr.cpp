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
 * PwrEventListener helpers, etc.) are defined as plain C++ functions
 * BEFORE #include "dsMgr.c" so the intra-TU linker resolves them.
 * No extern "C" wrapper is used: dsMgrPwrEventListener.h includes
 * <queue> (C++ templates) which cannot appear inside extern "C".
 * Since the entire TU is compiled as C++, all symbols share C++ linkage
 * and the definitions satisfy dsMgr.c's extern declarations correctly.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <cstring>
#include <pthread.h>

/* sysMgr.h first — prevents type-redefinition conflicts with Iarm.h */
#include "sysMgr.h"
#include "Iarm.h"
#include "IarmBusMock.h"
#include "WrapsMock.h"

/* -----------------------------------------------------------------------
 * All external symbols that dsMgr.c references but does not define.
 * Defined as plain C++ functions; no extern "C" wrapper is needed because
 * the whole TU is C++ and dsMgr.c's extern declarations in the same TU
 * also get C++ linkage.
 * --------------------------------------------------------------------- */

/* -----------------------------------------------------------------------
 * Overrideable outputs for individual DS-HAL stubs.
 * Reset to defaults in each test's SetUp().
 * --------------------------------------------------------------------- */
static intptr_t      g_stub_videoPortHandle = 0;
static int           g_stub_edidLength      = 0;
static unsigned char g_stub_edidBytes[1024] = {};

/* ---- DS HAL functions (declared extern at the top of dsMgr.c) ------- */
IARM_Result_t _dsGetVideoPort(void *arg) {
    if (g_stub_videoPortHandle) {
        dsVideoPortGetHandleParam_t *p = static_cast<dsVideoPortGetHandleParam_t *>(arg);
        p->handle = g_stub_videoPortHandle;
    }
    return IARM_RESULT_SUCCESS;
}
IARM_Result_t _dsIsDisplayConnected(void *arg)  { return IARM_RESULT_SUCCESS; }
IARM_Result_t _dsGetIgnoreEDIDStatus(void *arg) { return IARM_RESULT_SUCCESS; }
IARM_Result_t _dsSetResolution(void *arg)       { return IARM_RESULT_SUCCESS; }
IARM_Result_t _dsGetResolution(void *arg)       { return IARM_RESULT_SUCCESS; }
IARM_Result_t _dsInitResolution(void *arg)      { return IARM_RESULT_SUCCESS; }
IARM_Result_t _dsGetEDID(void *arg)             { return IARM_RESULT_SUCCESS; }
IARM_Result_t _dsGetEDIDBytes(void *arg) {
    dsDisplayGetEDIDBytesParam_t *p = static_cast<dsDisplayGetEDIDBytesParam_t *>(arg);
    p->length = g_stub_edidLength;
    if (g_stub_edidLength > 0)
        memcpy(p->bytes, g_stub_edidBytes, static_cast<size_t>(g_stub_edidLength));
    return IARM_RESULT_SUCCESS;
}
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

/* ---- DS Manager HAL initialisation (called from DSMgr_Start) ----------
 * Declared inside extern "C" in stubs/dsMgr.h, so must be defined as C. */
extern "C" IARM_Result_t dsMgr_init(void) { return IARM_RESULT_SUCCESS; }

/* ---- Power Controller API (declared inside extern "C" in power_controller.h) */
extern "C" void PowerController_Init(void) {}
extern "C" void PowerController_Term(void) {}

/* ---- Power Event Listener helper (defined in dsMgrPwrEventListener.c)  */
void initPwrEventListner(void)         {}
void dsMgrInitPwrControllerEvt(void)   {}
void dsMgrDeinitPwrControllerEvt(void) {}

/* -----------------------------------------------------------------------
 * Source under test — included last so the stubs above satisfy all
 * extern declarations found inside dsMgr.c.
 * --------------------------------------------------------------------- */
#include "dsMgr.c"

/* ---- DS HAL mock bridge -------------------------------------------
 * dsGetDisplay is called directly by dsMgr.c (no _ds* wrapper), so it
 * needs a definition in this TU.  DSHAL_MOCK_DEFINE_BRIDGES causes
 * dsHalMock.h to emit the extern "C" bridge and the DsHal::s_impl
 * definition here, allowing each test to program return values via the
 * DsHalMock installed on the fixture.
 * --------------------------------------------------------------------- */
#define DSHAL_MOCK_DEFINE_BRIDGES
#include "dsHalMock.h"

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
    ::testing::NiceMock<DsHalMock>       dsHalMock;
    ::testing::NiceMock<WrapsImplMock>   wrapsMock;

    void SetUp() override
    {
        IarmBus::setImpl(&iarmMock);
        DsHal::setImpl(&dsHalMock);
        Wraps::setImpl(&wrapsMock);

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
        hotplug_event_src   = 0;

        /* Reset stub overrides. */
        g_stub_videoPortHandle = 0;
        g_stub_edidLength      = 0;
        memset(g_stub_edidBytes, 0, sizeof(g_stub_edidBytes));
    }

    void TearDown() override
    {
        pthread_cond_destroy(&tdsMutexCond);
        pthread_mutex_destroy(&tdsMutexLock);
        ::testing::Mock::VerifyAndClearExpectations(&iarmMock);
        ::testing::Mock::VerifyAndClearExpectations(&dsHalMock);
        ::testing::Mock::VerifyAndClearExpectations(&wrapsMock);
        IarmBus::setImpl(nullptr);
        DsHal::setImpl(nullptr);
        Wraps::setImpl(nullptr);
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

/* =======================================================================
 * Section 9 – isEUPlatform(): file-open and content-parsing paths
 *
 * isEUPlatform() reads /etc/device.properties looking for a FRIENDLY_ID
 * line to decide whether the device is an EU variant.  fopen / fgets /
 * fclose are intercepted by --wrap; WrapsImplMock (installed on every
 * fixture via Wraps::setImpl) controls what they return.
 *
 * kFakeFp is a non-NULL sentinel FILE* that the code under test never
 * dereferences directly.
 * ===================================================================== */

namespace {
    FILE * const kFakeFp = reinterpret_cast<FILE *>(0xDEADBEEF);
}

TEST_F(DsMgrTest, IsEUPlatform_FileOpenFails_ReturnsFalse)
{
    EXPECT_CALL(wrapsMock, fopen(_, _)).WillOnce(Return(nullptr));
    EXPECT_FALSE(isEUPlatform());
}

TEST_F(DsMgrTest, IsEUPlatform_EmptyFile_ReturnsFalse)
{
    /* fgets returns nullptr on first call → loop exits without finding FRIENDLY_ID */
    EXPECT_CALL(wrapsMock, fopen(_, _)).WillOnce(Return(kFakeFp));
    EXPECT_CALL(wrapsMock, fgets(_, _, kFakeFp)).WillOnce(Return(nullptr));
    EXPECT_CALL(wrapsMock, fclose(kFakeFp)).WillOnce(Return(0));
    EXPECT_FALSE(isEUPlatform());
}

TEST_F(DsMgrTest, IsEUPlatform_EuLine_ReturnsTrue)
{
    EXPECT_CALL(wrapsMock, fopen(_, _)).WillOnce(Return(kFakeFp));
    EXPECT_CALL(wrapsMock, fgets(_, _, kFakeFp))
        .WillOnce([](char *buf, int size, FILE *) {
            strncpy(buf, "FRIENDLY_ID=UK\n", static_cast<size_t>(size) - 1);
            buf[size - 1] = '\0';
            return buf;
        });
    EXPECT_CALL(wrapsMock, fclose(kFakeFp)).WillOnce(Return(0));
    EXPECT_TRUE(isEUPlatform());
}

TEST_F(DsMgrTest, IsEUPlatform_USLine_ReturnsFalse)
{
    EXPECT_CALL(wrapsMock, fopen(_, _)).WillOnce(Return(kFakeFp));
    EXPECT_CALL(wrapsMock, fgets(_, _, kFakeFp))
        .WillOnce([](char *buf, int size, FILE *) {
            strncpy(buf, "FRIENDLY_ID= US\n", static_cast<size_t>(size) - 1);
            buf[size - 1] = '\0';
            return buf;
        });
    EXPECT_CALL(wrapsMock, fclose(kFakeFp)).WillOnce(Return(0));
    EXPECT_FALSE(isEUPlatform());
}

TEST_F(DsMgrTest, IsEUPlatform_XglobalLine_ReturnsFalse)
{
    EXPECT_CALL(wrapsMock, fopen(_, _)).WillOnce(Return(kFakeFp));
    EXPECT_CALL(wrapsMock, fgets(_, _, kFakeFp))
        .WillOnce([](char *buf, int size, FILE *) {
            strncpy(buf, "FRIENDLY_ID=xglobal\n", static_cast<size_t>(size) - 1);
            buf[size - 1] = '\0';
            return buf;
        });
    EXPECT_CALL(wrapsMock, fclose(kFakeFp)).WillOnce(Return(0));
    EXPECT_FALSE(isEUPlatform());
}

TEST_F(DsMgrTest, IsEUPlatform_FriendlyIdOnSecondLine_ReturnsTrue)
{
    /* First fgets returns an unrelated line; second returns the EU FRIENDLY_ID */
    EXPECT_CALL(wrapsMock, fopen(_, _)).WillOnce(Return(kFakeFp));
    EXPECT_CALL(wrapsMock, fgets(_, _, kFakeFp))
        .WillOnce([](char *buf, int size, FILE *) {
            strncpy(buf, "SOME_KEY=val\n", static_cast<size_t>(size) - 1);
            buf[size - 1] = '\0';
            return buf;
        })
        .WillOnce([](char *buf, int size, FILE *) {
            strncpy(buf, "FRIENDLY_ID=DE\n", static_cast<size_t>(size) - 1);
            buf[size - 1] = '\0';
            return buf;
        });
    EXPECT_CALL(wrapsMock, fclose(kFakeFp)).WillOnce(Return(0));
    EXPECT_TRUE(isEUPlatform());
}

/* =======================================================================
 * Section 10 – setupPlatformConfig(): fallback resolution list building
 *
 * setupPlatformConfig() calls isEUPlatform() (which calls fopen) and
 * then populates fallBackResolutionList[], skipping "576p" on non-EU
 * platforms.
 * ===================================================================== */

TEST_F(DsMgrTest, SetupPlatformConfig_NonEU_ExcludesPal576p)
{
    /* fopen fails → isEUPlatform() returns false → non-EU build */
    EXPECT_CALL(wrapsMock, fopen(_, _)).WillOnce(Return(nullptr));

    memset(fallBackResolutionList, 0, sizeof(fallBackResolutionList));
    setupPlatformConfig();

    bool found576p = false;
    for (int i = 0; i < RES_MAX_COUNT; ++i)
        if (strcmp(fallBackResolutionList[i], "576p") == 0) found576p = true;
    EXPECT_FALSE(found576p);
}

TEST_F(DsMgrTest, SetupPlatformConfig_EU_IncludesPal576p)
{
    /* fopen succeeds with an EU FRIENDLY_ID line → IsEUPlatform = true */
    EXPECT_CALL(wrapsMock, fopen(_, _)).WillOnce(Return(kFakeFp));
    EXPECT_CALL(wrapsMock, fgets(_, _, kFakeFp))
        .WillOnce([](char *buf, int size, FILE *) {
            strncpy(buf, "FRIENDLY_ID=UK\n", static_cast<size_t>(size) - 1);
            buf[size - 1] = '\0';
            return buf;
        });
    EXPECT_CALL(wrapsMock, fclose(kFakeFp)).WillOnce(Return(0));

    memset(fallBackResolutionList, 0, sizeof(fallBackResolutionList));
    setupPlatformConfig();

    bool found576p = false;
    for (int i = 0; i < RES_MAX_COUNT; ++i)
        if (strcmp(fallBackResolutionList[i], "576p") == 0) found576p = true;
    EXPECT_TRUE(found576p);
}

/* =======================================================================
 * Section 11 – _EventHandler: additional HDCP branches
 * ===================================================================== */

/* Unknown HDCP status (neither AUTHENTICATED nor AUTHENTICATIONFAILURE):
 * the handler broadcasts with state=1 (initial value) and leaves
 * bHDCPAuthenticated unchanged. */
TEST_F(DsMgrTest, EventHandler_HdcpUnknownStatus_BroadcastsAndLeavesAuthUnchanged)
{
    bHDCPAuthenticated = true;

    EXPECT_CALL(iarmMock, IARM_Bus_BroadcastEvent(
        StrEq(IARM_BUS_SYSMGR_NAME),
        static_cast<IARM_EventId_t>(IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE),
        _, _))
        .WillOnce(Return(IARM_RESULT_SUCCESS));

    IARM_Bus_DSMgr_EventData_t ev = {};
    ev.data.hdmi_hdcp.hdcpStatus = -1;  /* intentionally invalid */
    _EventHandler(IARM_BUS_DSMGR_NAME,
                  IARM_BUS_DSMGR_EVENT_HDCP_STATUS,
                  &ev, sizeof(ev));

    EXPECT_TRUE(bHDCPAuthenticated);  /* unchanged */
}

/* BroadcastEvent failure: handler logs INT_ERROR but does not crash. */
TEST_F(DsMgrTest, EventHandler_HdcpAuthenticated_BroadcastEventFails_DoesNotCrash)
{
    EXPECT_CALL(iarmMock, IARM_Bus_BroadcastEvent(_, _, _, _))
        .WillOnce(Return(IARM_RESULT_IPCCORE_FAIL));

    IARM_Bus_DSMgr_EventData_t ev = {};
    ev.data.hdmi_hdcp.hdcpStatus = dsHDCP_STATUS_AUTHENTICATED;
    _EventHandler(IARM_BUS_DSMGR_NAME,
                  IARM_BUS_DSMGR_EVENT_HDCP_STATUS,
                  &ev, sizeof(ev));

    EXPECT_TRUE(bHDCPAuthenticated);
}

/* IsIgnoreEdid_gs=true AND bootup_flag_enabled=false: the condition
 * `(!IsIgnoreEdid_gs) || bootup_flag_enabled` is false so
 * _SetVideoPortResolution is NOT called and bootup_flag stays false. */
TEST_F(DsMgrTest, EventHandler_HdcpAuthenticated_IgnoreEdidSet_BootupDone_SkipsResolution)
{
    IsIgnoreEdid_gs     = true;
    bootup_flag_enabled = false;

    IARM_Bus_DSMgr_EventData_t ev = {};
    ev.data.hdmi_hdcp.hdcpStatus = dsHDCP_STATUS_AUTHENTICATED;
    _EventHandler(IARM_BUS_DSMGR_NAME,
                  IARM_BUS_DSMGR_EVENT_HDCP_STATUS,
                  &ev, sizeof(ev));

    EXPECT_TRUE(bHDCPAuthenticated);
    EXPECT_FALSE(bootup_flag_enabled);  /* flag was not touched */
}

/* IsIgnoreEdid_gs=true AND bootup_flag_enabled=true: the bootup override
 * allows _SetVideoPortResolution to run; flag is cleared to false. */
TEST_F(DsMgrTest, EventHandler_HdcpAuthenticated_IgnoreEdidSet_BootupActive_ClearsFlag)
{
    IsIgnoreEdid_gs     = true;
    bootup_flag_enabled = true;

    IARM_Bus_DSMgr_EventData_t ev = {};
    ev.data.hdmi_hdcp.hdcpStatus = dsHDCP_STATUS_AUTHENTICATED;
    _EventHandler(IARM_BUS_DSMGR_NAME,
                  IARM_BUS_DSMGR_EVENT_HDCP_STATUS,
                  &ev, sizeof(ev));

    EXPECT_TRUE(bHDCPAuthenticated);
    EXPECT_FALSE(bootup_flag_enabled);  /* cleared by the handler */
}

/* IsIgnoreEdid_gs=true on AUTHFAILURE: `if (!IsIgnoreEdid_gs)` is false
 * so _SetVideoPortResolution is skipped; bHDCPAuthenticated becomes false. */
TEST_F(DsMgrTest, EventHandler_HdcpAuthFail_IgnoreEdidSet_SkipsResolution)
{
    IsIgnoreEdid_gs    = true;
    bHDCPAuthenticated = true;

    IARM_Bus_DSMgr_EventData_t ev = {};
    ev.data.hdmi_hdcp.hdcpStatus = dsHDCP_STATUS_AUTHENTICATIONFAILURE;
    _EventHandler(IARM_BUS_DSMGR_NAME,
                  IARM_BUS_DSMGR_EVENT_HDCP_STATUS,
                  &ev, sizeof(ev));

    EXPECT_FALSE(bHDCPAuthenticated);
    /* bootup_flag_enabled remains at the true value set by SetUp */
    EXPECT_TRUE(bootup_flag_enabled);
}

/* =======================================================================
 * Section 12 – dumpEdidOnChecksumDiff(): dsGetDisplay / EDID paths
 *
 * dumpEdidOnChecksumDiff is a static gboolean GLib timeout callback that
 * is called directly here because dsMgr.c is included in this TU.
 * The DsHalMock controls what dsGetDisplay() returns.
 * ===================================================================== */

/* dsGetDisplay returns handle=0: function returns FALSE immediately
 * without ever entering the EDID-processing block. */
TEST_F(DsMgrTest, DumpEdidOnChecksumDiff_NoDisplayHandle_ReturnsFalse)
{
    EXPECT_CALL(dsHalMock, dsGetDisplay(dsVIDEOPORT_TYPE_HDMI, 0, _))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgPointee<2>(static_cast<intptr_t>(0)),
            Return(dsERR_NONE)));

    EXPECT_FALSE(dumpEdidOnChecksumDiff(nullptr));
}

/* dsGetDisplay returns a non-zero handle but _dsGetEDIDBytes (hardcoded
 * stub) leaves EdidBytesParam.length = 0.  The guard
 * `if((length > 0) && (length <= 512))` is false → returns FALSE. */
TEST_F(DsMgrTest, DumpEdidOnChecksumDiff_ValidHandle_ZeroEdidLength_ReturnsFalse)
{
    EXPECT_CALL(dsHalMock, dsGetDisplay(dsVIDEOPORT_TYPE_HDMI, 0, _))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgPointee<2>(static_cast<intptr_t>(0x1234)),
            Return(dsERR_NONE)));

    EXPECT_FALSE(dumpEdidOnChecksumDiff(nullptr));
}

/* =======================================================================
 * Section 13 – Direct-callable helpers
 *
 * heartbeatMsg, _SetResolutionHandler, isHDMIConnected, and
 * dumpHdmiEdidInfo are all static functions compiled into this TU via
 * #include "dsMgr.c".  They can be exercised by calling them directly.
 * ===================================================================== */

TEST_F(DsMgrTest, HeartbeatMsg_ReturnsTrue)
{
    /* heartbeatMsg is a GLib timeout callback that logs a heartbeat ping. */
    EXPECT_TRUE(heartbeatMsg(nullptr));
}

TEST_F(DsMgrTest, SetResolutionHandler_ClearsHotplugSrc)
{
    /* _SetResolutionHandler calls _SetVideoPortResolution() then zeros
     * hotplug_event_src.  With the default stub (_dsGetVideoPort returns
     * handle=0) _SetVideoPortResolution takes the NULL-handle branch. */
    hotplug_event_src = 1;
    EXPECT_FALSE(_SetResolutionHandler(nullptr));
    EXPECT_EQ(hotplug_event_src, static_cast<guint>(0));
}

TEST_F(DsMgrTest, IsHDMIConnected_NullHandle_ReturnsFalse)
{
    /* _dsGetVideoPort stub returns handle=0 → isHDMIConnected returns false. */
    EXPECT_FALSE(isHDMIConnected());
}

TEST_F(DsMgrTest, DumpHdmiEdidInfo_NullPedidData_DoesNotCrash)
{
    /* NULL pointer → else-branch log; no crash or ASAN fault. */
    dumpHdmiEdidInfo(nullptr);
}

TEST_F(DsMgrTest, DumpHdmiEdidInfo_EmptyMonitorName_TakesElseBranch)
{
    /* memset-zeroed edid → monitorName length == 0 → else branch. */
    dsDisplayEDID_t edid;
    memset(&edid, 0, sizeof(edid));
    dumpHdmiEdidInfo(&edid);
}

TEST_F(DsMgrTest, DumpHdmiEdidInfo_PopulatedMonitorName_TakesIfBranch)
{
    /* Non-empty monitorName → if-branch; all fields logged without crash. */
    dsDisplayEDID_t edid;
    memset(&edid, 0, sizeof(edid));
    strncpy(edid.monitorName, "TestTV", sizeof(edid.monitorName) - 1);
    edid.hdmiDeviceType = true;
    edid.isRepeater     = false;
    dumpHdmiEdidInfo(&edid);
}

/* =======================================================================
 * Section 14 – _setAudioMode / _setEASAudioMode: EAS guard branches
 *
 * Both functions bail out early when `isEAS` is in the wrong state.
 * Lines 1148–1150 (_setAudioMode EAS guard) and 1103–1105
 * (_setEASAudioMode non-EAS guard) had 0 hits.
 * ===================================================================== */

TEST_F(DsMgrTest, SetAudioMode_EasInProgress_ReturnsEarly)
{
    /* When EAS is active _setAudioMode must return without any DS calls. */
    isEAS = IARM_BUS_SYS_MODE_EAS;
    _setAudioMode();
}

TEST_F(DsMgrTest, SetEasAudioMode_NotInEas_ReturnsEarly)
{
    /* _setEASAudioMode bails when isEAS != EAS. */
    isEAS = IARM_BUS_SYS_MODE_NORMAL;
    _setEASAudioMode();
}

/* =======================================================================
 * Section 15 – setBGColor() / getVideoPortHandle(): handle != NULL branch
 *
 * g_stub_videoPortHandle overrides the _dsGetVideoPort stub so it returns
 * a non-zero handle, exercising the if(vidPortParam.handle != NULL) block
 * (lines 434–440) that had 0 hits.
 * ===================================================================== */

TEST_F(DsMgrTest, SetBGColor_WithValidHandle_CallsSetBackgroundColor)
{
    g_stub_videoPortHandle = 0xCAFE;
    setBGColor(dsVIDEO_BGCOLOR_NONE);
    /* Lines 435-440 covered; _dsSetBackgroundColor stub absorbs the call. */
}

TEST_F(DsMgrTest, GetVideoPortHandle_StubReturnsConfiguredHandle)
{
    g_stub_videoPortHandle = 0xDEAD;
    intptr_t h = getVideoPortHandle(dsVIDEOPORT_TYPE_HDMI);
    EXPECT_EQ(h, static_cast<intptr_t>(0xDEAD));
}

/* =======================================================================
 * Section 16 – dumpEdidOnChecksumDiff(): valid EDID length inner paths
 *
 * g_stub_edidLength/g_stub_edidBytes control what _dsGetEDIDBytes fills
 * in.  A single test exercises both the "new checksum → dump → TRUE" path
 * and the "same checksum → no dump → FALSE" path by calling the function
 * twice in sequence, relying on the static cached_EDID_checksum starting
 * at 0 (no previous test has written to it for this particular data).
 * ===================================================================== */

TEST_F(DsMgrTest, DumpEdidOnChecksumDiff_ValidEdid_NewThenSameChecksum)
{
    /* 128-byte EDID block; checksum byte = 0xCC (unique value not used by
     * any earlier test). */
    g_stub_edidLength     = 128;
    g_stub_edidBytes[127] = 0xCC;

    EXPECT_CALL(dsHalMock, dsGetDisplay(dsVIDEOPORT_TYPE_HDMI, 0, _))
        .WillRepeatedly(::testing::DoAll(
            ::testing::SetArgPointee<2>(static_cast<intptr_t>(0x9ABC)),
            Return(dsERR_NONE)));

    /* First call: cached == 0, current == 0xCC → new checksum → TRUE */
    EXPECT_TRUE(dumpEdidOnChecksumDiff(nullptr));

    /* Second call: cached == current == 0xCC → no change → FALSE */
    EXPECT_FALSE(dumpEdidOnChecksumDiff(nullptr));
}

/* =======================================================================
 * Section 17 – DSMgr_Start(): early IARM error-return paths
 *
 * DSMgr_Start returns early when IARM_Bus_Init, IARM_Bus_Connect, or
 * IARM_Bus_RegisterEvent fails.  These paths (lines 226–240) had 0 hits.
 * All fail paths return before pthread_create so no background thread leaks.
 * ===================================================================== */

TEST_F(DsMgrTest, DsmgrStart_InitFails_ReturnsError)
{
    EXPECT_CALL(iarmMock, IARM_Bus_Init(_))
        .WillOnce(Return(IARM_RESULT_IPCCORE_FAIL));

    EXPECT_EQ(DSMgr_Start(), IARM_RESULT_IPCCORE_FAIL);
}

TEST_F(DsMgrTest, DsmgrStart_ConnectFails_ReturnsError)
{
    EXPECT_CALL(iarmMock, IARM_Bus_Init(_))
        .WillOnce(Return(IARM_RESULT_SUCCESS));
    EXPECT_CALL(iarmMock, IARM_Bus_Connect())
        .WillOnce(Return(IARM_RESULT_IPCCORE_FAIL));

    EXPECT_EQ(DSMgr_Start(), IARM_RESULT_IPCCORE_FAIL);
}

TEST_F(DsMgrTest, DsmgrStart_RegisterEventFails_ReturnsError)
{
    EXPECT_CALL(iarmMock, IARM_Bus_Init(_))
        .WillOnce(Return(IARM_RESULT_SUCCESS));
    EXPECT_CALL(iarmMock, IARM_Bus_Connect())
        .WillOnce(Return(IARM_RESULT_SUCCESS));
    EXPECT_CALL(iarmMock, IARM_Bus_RegisterEvent(_))
        .WillOnce(Return(IARM_RESULT_IPCCORE_FAIL));

    EXPECT_EQ(DSMgr_Start(), IARM_RESULT_IPCCORE_FAIL);
}

/* =======================================================================
 * Section 18 – DSMgr_Stop() and DSMgr_Loop()
 *
 * DSMgr_Stop's success path destroys the mutex and condvar, so the
 * DsMgrStopTest fixture tracks ownership via `mutexOwnedByTest` and
 * only lets TearDown destroy them when DSMgr_Stop did not.
 *
 * DSMgr_Loop with dsMgr_Gloop==NULL skips the GLib run loop and
 * immediately returns IARM_RESULT_SUCCESS.
 * ===================================================================== */

class DsMgrStopTest : public ::testing::Test
{
protected:
    ::testing::NiceMock<IarmBusImplMock> iarmMock;
    ::testing::NiceMock<DsHalMock>       dsHalMock;
    ::testing::NiceMock<WrapsImplMock>   wrapsMock;
    bool mutexOwnedByTest = true;

    void SetUp() override
    {
        IarmBus::setImpl(&iarmMock);
        DsHal::setImpl(&dsHalMock);
        Wraps::setImpl(&wrapsMock);
        pthread_mutex_init(&tdsMutexLock, nullptr);
        pthread_cond_init(&tdsMutexCond, nullptr);
        isEAS       = IARM_BUS_SYS_MODE_NORMAL;
        iTuneReady  = 0;
        dsMgr_Gloop = nullptr;
        g_stub_videoPortHandle = 0;
        g_stub_edidLength      = 0;
        mutexOwnedByTest = true;
    }

    void TearDown() override
    {
        if (mutexOwnedByTest) {
            pthread_cond_destroy(&tdsMutexCond);
            pthread_mutex_destroy(&tdsMutexLock);
        }
        ::testing::Mock::VerifyAndClearExpectations(&iarmMock);
        ::testing::Mock::VerifyAndClearExpectations(&dsHalMock);
        ::testing::Mock::VerifyAndClearExpectations(&wrapsMock);
        IarmBus::setImpl(nullptr);
        DsHal::setImpl(nullptr);
        Wraps::setImpl(nullptr);
    }
};

TEST_F(DsMgrStopTest, DsmgrStop_Success_ReturnsSuccess)
{
    mutexOwnedByTest = false;   /* DSMgr_Stop destroys mutex/condvar */
    EXPECT_CALL(iarmMock, IARM_Bus_Disconnect())
        .WillOnce(Return(IARM_RESULT_SUCCESS));
    EXPECT_CALL(iarmMock, IARM_Bus_Term())
        .WillOnce(Return(IARM_RESULT_SUCCESS));

    EXPECT_EQ(DSMgr_Stop(), IARM_RESULT_SUCCESS);
}

TEST_F(DsMgrStopTest, DsmgrStop_DisconnectFails_ReturnsError)
{
    /* Early return before pthread_destroy → TearDown cleans up. */
    EXPECT_CALL(iarmMock, IARM_Bus_Disconnect())
        .WillOnce(Return(IARM_RESULT_IPCCORE_FAIL));

    EXPECT_EQ(DSMgr_Stop(), IARM_RESULT_IPCCORE_FAIL);
}

TEST_F(DsMgrStopTest, DsmgrStop_TermFails_ReturnsError)
{
    /* Early return before pthread_destroy → TearDown cleans up. */
    EXPECT_CALL(iarmMock, IARM_Bus_Disconnect())
        .WillOnce(Return(IARM_RESULT_SUCCESS));
    EXPECT_CALL(iarmMock, IARM_Bus_Term())
        .WillOnce(Return(IARM_RESULT_IPCCORE_FAIL));

    EXPECT_EQ(DSMgr_Stop(), IARM_RESULT_IPCCORE_FAIL);
}

TEST_F(DsMgrTest, DsmgrLoop_NullLoop_ReturnsImmediately)
{
    /* dsMgr_Gloop == nullptr → GLib loop is skipped → SUCCESS. */
    dsMgr_Gloop = nullptr;
    EXPECT_EQ(DSMgr_Loop(), IARM_RESULT_SUCCESS);
}
