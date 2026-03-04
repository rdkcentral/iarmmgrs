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
 * @file test_sysMgr.cpp
 * @brief L1 unit tests for the sysmgr/sysMgr.c component.
 *
 * Components covered:
 *   - getKeyCodeLoggingPref()  / setKeyCodeLoggingPref()
 *   - _GetKeyCodeLoggingPref() / _SetKeyCodeLoggingPref()  (IARM RPC wrappers)
 *   - _GetSystemStates()
 *   - _sysEventHandler()       (all modelled state-ID branches)
 *   - CheckHdcpProfile()
 *   - _GetHDCPProfile()        / _SetHDCPProfile()
 *
 * IARM_Bus_* calls route through the testframework's IarmBus singleton.
 * IarmBusMock.h must be included BEFORE sysMgr.c so that Iarm.h defines
 * IARM_Result_t (as an enum) before libIBus.h is processed; this prevents the
 * conflicting-declaration error that would arise if libIBus.h's C-mode
 * "typedef int IARM_Result_t" were seen by the C++ compiler.
 *
 * Filesystem calls (lstat, open, unlink) for the HDCP profile file
 * ("/opt/.hdcp_profile_1") are exercised against the real filesystem; the
 * file is cleaned up in SetUp().
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <cstring>
#include <cstdio>
#include <unistd.h>   /* unlink */

/* IarmBusMock.h / Iarm.h must come BEFORE sysMgr.c so that IARM_Result_t is
 * defined as the enum (not "typedef int") before libIBus.h is processed. */
#include "Iarm.h"
#include "IarmBusMock.h"

/* Pull in the source under test so that static functions and variables are
 * visible in this translation unit. */
extern "C" {
#include "sysMgr.c"
}

using ::testing::_;

/* -----------------------------------------------------------------------
 * Fixture
 * An IarmBusImplMock is installed as the active IarmBus implementation for
 * each test so that IARM_Bus_BroadcastEvent (called by setKeyCodeLoggingPref
 * and _SetHDCPProfile) routes through the mock.  IarmBusImplMock's default
 * ON_CALL stubs return IARM_RESULT_SUCCESS, so tests that don't set explicit
 * expectations still pass cleanly.
 *
 * All file-static state in sysMgr.c is reset before each test so that
 * tests are fully isolated.
 * ---------------------------------------------------------------------- */
class SysMgrTest : public ::testing::Test
{
protected:
    ::testing::NiceMock<IarmBusImplMock> iarmMock;

    void SetUp() override
    {
        IarmBus::setImpl(&iarmMock);

        /* Reset mutable static state to well-known values */
        memset(&systemStates, 0, sizeof(systemStates));
        keyLogStatus = 1;   /* documented default */
        initialized  = 0;

        /* Remove the HDCP profile sentinel file if a previous test left it */
        ::unlink(profile_1_filename);
    }

    void TearDown() override
    {
        ::testing::Mock::VerifyAndClearExpectations(&iarmMock);
        IarmBus::setImpl(nullptr);

        /* Clean up any HDCP file that a test may have created */
        ::unlink(profile_1_filename);
    }
};

/* =======================================================================
 * Section 1 – Key-code logging preference
 * ====================================================================== */

TEST_F(SysMgrTest, GetKeyCodeLoggingPref_DefaultIsOne)
{
    EXPECT_EQ(1, getKeyCodeLoggingPref());
}

TEST_F(SysMgrTest, SetKeyCodeLoggingPref_ZeroChangesState)
{
    setKeyCodeLoggingPref(0);
    EXPECT_EQ(0, getKeyCodeLoggingPref());
}

TEST_F(SysMgrTest, SetKeyCodeLoggingPref_OneWhenAlreadyOne_NoChange)
{
    setKeyCodeLoggingPref(1);   /* same as default */
    EXPECT_EQ(1, getKeyCodeLoggingPref());
}

TEST_F(SysMgrTest, SetKeyCodeLoggingPref_InvalidValueIgnored)
{
    setKeyCodeLoggingPref(5);   /* invalid – should be ignored */
    EXPECT_EQ(1, getKeyCodeLoggingPref());  /* still default */
}

TEST_F(SysMgrTest, SetKeyCodeLoggingPref_InvalidNegativeValueIgnored)
{
    setKeyCodeLoggingPref(-1);
    EXPECT_EQ(1, getKeyCodeLoggingPref());
}

TEST_F(SysMgrTest, SetKeyCodeLoggingPref_ToggleZeroThenOne)
{
    setKeyCodeLoggingPref(0);
    EXPECT_EQ(0, getKeyCodeLoggingPref());

    setKeyCodeLoggingPref(1);
    EXPECT_EQ(1, getKeyCodeLoggingPref());
}

/* =======================================================================
 * Section 2 – IARM RPC wrappers for key-code logging pref
 * ====================================================================== */

TEST_F(SysMgrTest, IarmGetKeyCodeLoggingPref_ReadsDefaultState)
{
    IARM_BUS_SYSMGR_KEYCodeLoggingInfo_Param_t param;
    memset(&param, 0, sizeof(param));

    IARM_Result_t result = _GetKeyCodeLoggingPref(&param);

    EXPECT_EQ(IARM_RESULT_SUCCESS, result);
    EXPECT_EQ(1, param.logStatus);
}

TEST_F(SysMgrTest, IarmGetKeyCodeLoggingPref_ReflectsChangedState)
{
    setKeyCodeLoggingPref(0);

    IARM_BUS_SYSMGR_KEYCodeLoggingInfo_Param_t param;
    memset(&param, 0, sizeof(param));

    IARM_Result_t result = _GetKeyCodeLoggingPref(&param);

    EXPECT_EQ(IARM_RESULT_SUCCESS, result);
    EXPECT_EQ(0, param.logStatus);
}

TEST_F(SysMgrTest, IarmSetKeyCodeLoggingPref_ChangesKeyLogStatus)
{
    IARM_BUS_SYSMGR_KEYCodeLoggingInfo_Param_t param;
    param.logStatus = 0;

    IARM_Result_t result = _SetKeyCodeLoggingPref(&param);

    EXPECT_EQ(IARM_RESULT_SUCCESS, result);
    EXPECT_EQ(0, getKeyCodeLoggingPref());
}

TEST_F(SysMgrTest, IarmSetKeyCodeLoggingPref_InvalidValueIgnored)
{
    IARM_BUS_SYSMGR_KEYCodeLoggingInfo_Param_t param;
    param.logStatus = 99;

    IARM_Result_t result = _SetKeyCodeLoggingPref(&param);

    EXPECT_EQ(IARM_RESULT_SUCCESS, result);
    EXPECT_EQ(1, getKeyCodeLoggingPref());  /* unchanged */
}

/* =======================================================================
 * Section 3 – _GetSystemStates
 * ====================================================================== */

TEST_F(SysMgrTest, GetSystemStates_ReturnsZeroedStateAfterReset)
{
    IARM_Bus_SYSMgr_GetSystemStates_Param_t out;
    memset(&out, 0xff, sizeof(out));  /* fill with garbage */

    IARM_Result_t result = _GetSystemStates(&out);

    EXPECT_EQ(IARM_RESULT_SUCCESS, result);
    EXPECT_EQ(0, out.channel_map.state);
    EXPECT_EQ(0, out.bootup.state);
    EXPECT_EQ(0, out.hdcp_enabled.state);
}

TEST_F(SysMgrTest, GetSystemStates_ReturnsUpdatedChannelMap)
{
    /* Directly set a known value into the static systemStates struct */
    systemStates.channel_map.state = 42;
    systemStates.channel_map.error = 7;

    IARM_Bus_SYSMgr_GetSystemStates_Param_t out;
    memset(&out, 0, sizeof(out));

    EXPECT_EQ(IARM_RESULT_SUCCESS, _GetSystemStates(&out));
    EXPECT_EQ(42, out.channel_map.state);
    EXPECT_EQ(7,  out.channel_map.error);
}

TEST_F(SysMgrTest, GetSystemStates_ReturnsFullCopy)
{
    systemStates.rf_connected.state  = 1;
    systemStates.qam_ready_status.state = 1;
    systemStates.bootup.state        = 3;

    IARM_Bus_SYSMgr_GetSystemStates_Param_t out;
    memset(&out, 0, sizeof(out));

    EXPECT_EQ(IARM_RESULT_SUCCESS, _GetSystemStates(&out));
    EXPECT_EQ(1, out.rf_connected.state);
    EXPECT_EQ(1, out.qam_ready_status.state);
    EXPECT_EQ(3, out.bootup.state);
}

/* =======================================================================
 * Section 4 – _sysEventHandler: filter / routing tests
 * ====================================================================== */

/* Helper: build a minimal SYSTEMSTATE event payload */
static IARM_Bus_SYSMgr_EventData_t makeStateEvent(
        IARM_Bus_SYSMgr_SystemState_t stateId, int state, int error,
        const char *payload = "")
{
    IARM_Bus_SYSMgr_EventData_t ev;
    memset(&ev, 0, sizeof(ev));
    ev.data.systemStates.stateId  = stateId;
    ev.data.systemStates.state    = state;
    ev.data.systemStates.error    = error;
    strncpy(ev.data.systemStates.payload, payload,
            sizeof(ev.data.systemStates.payload) - 1);
    return ev;
}

TEST_F(SysMgrTest, SysEventHandler_IgnoresWrongOwner)
{
    auto ev = makeStateEvent(IARM_BUS_SYSMGR_SYSSTATE_CHANNELMAP, 5, 0);
    _sysEventHandler("WrongOwner",
                     IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE,
                     &ev, sizeof(ev));

    EXPECT_EQ(0, systemStates.channel_map.state);  /* unchanged */
}

TEST_F(SysMgrTest, SysEventHandler_IgnoresWrongEventId)
{
    auto ev = makeStateEvent(IARM_BUS_SYSMGR_SYSSTATE_CHANNELMAP, 5, 0);
    _sysEventHandler(IARM_BUS_SYSMGR_NAME,
                     99 /* wrong event ID */,
                     &ev, sizeof(ev));

    EXPECT_EQ(0, systemStates.channel_map.state);  /* unchanged */
}

/* =======================================================================
 * Section 5 – _sysEventHandler: individual stateId switch-case branches
 * ====================================================================== */

TEST_F(SysMgrTest, SysEventHandler_UpdatesChannelMap)
{
    auto ev = makeStateEvent(IARM_BUS_SYSMGR_SYSSTATE_CHANNELMAP, 2, 1, "CM");
    _sysEventHandler(IARM_BUS_SYSMGR_NAME,
                     IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE,
                     &ev, sizeof(ev));

    EXPECT_EQ(2, systemStates.channel_map.state);
    EXPECT_EQ(1, systemStates.channel_map.error);
    EXPECT_STREQ("CM", systemStates.channel_map.payload);
}

TEST_F(SysMgrTest, SysEventHandler_UpdatesDisconnectMgr)
{
    auto ev = makeStateEvent(IARM_BUS_SYSMGR_SYSSTATE_DISCONNECTMGR, 3, 0);
    _sysEventHandler(IARM_BUS_SYSMGR_NAME,
                     IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE,
                     &ev, sizeof(ev));

    EXPECT_EQ(3, systemStates.disconnect_mgr_state.state);
}

TEST_F(SysMgrTest, SysEventHandler_UpdatesTuneReady)
{
    auto ev = makeStateEvent(IARM_BUS_SYSMGR_SYSSTATE_TUNEREADY, 1, 0);
    _sysEventHandler(IARM_BUS_SYSMGR_NAME,
                     IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE,
                     &ev, sizeof(ev));

    EXPECT_EQ(1, systemStates.TuneReadyStatus.state);
}

TEST_F(SysMgrTest, SysEventHandler_UpdatesHdcpEnabled)
{
    auto ev = makeStateEvent(IARM_BUS_SYSMGR_SYSSTATE_HDCP_ENABLED, 1, 0);
    _sysEventHandler(IARM_BUS_SYSMGR_NAME,
                     IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE,
                     &ev, sizeof(ev));

    EXPECT_EQ(1, systemStates.hdcp_enabled.state);
}

TEST_F(SysMgrTest, SysEventHandler_UpdatesHdmiOut)
{
    auto ev = makeStateEvent(IARM_BUS_SYSMGR_SYSSTATE_HDMI_OUT, 1, 0);
    _sysEventHandler(IARM_BUS_SYSMGR_NAME,
                     IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE,
                     &ev, sizeof(ev));

    EXPECT_EQ(1, systemStates.hdmi_out.state);
}

TEST_F(SysMgrTest, SysEventHandler_UpdatesBootup)
{
    auto ev = makeStateEvent(IARM_BUS_SYSMGR_SYSSTATE_BOOTUP, 4, 0);
    _sysEventHandler(IARM_BUS_SYSMGR_NAME,
                     IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE,
                     &ev, sizeof(ev));

    EXPECT_EQ(4, systemStates.bootup.state);
}

TEST_F(SysMgrTest, SysEventHandler_UpdatesRfConnected)
{
    auto ev = makeStateEvent(IARM_BUS_SYSMGR_SYSSTATE_RF_CONNECTED, 1, 0);
    _sysEventHandler(IARM_BUS_SYSMGR_NAME,
                     IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE,
                     &ev, sizeof(ev));

    EXPECT_EQ(1, systemStates.rf_connected.state);
}

TEST_F(SysMgrTest, SysEventHandler_UpdatesPartneridChange)
{
    auto ev = makeStateEvent(IARM_BUS_SYSMGR_SYSSTATE_PARTNERID_CHANGE, 7, 0);
    _sysEventHandler(IARM_BUS_SYSMGR_NAME,
                     IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE,
                     &ev, sizeof(ev));

    EXPECT_EQ(7, systemStates.partnerid_changed.state);
}

TEST_F(SysMgrTest, SysEventHandler_UpdatesQamReady)
{
    auto ev = makeStateEvent(IARM_BUS_SYSMGR_SYSSTATE_QAM_READY, 1, 0);
    _sysEventHandler(IARM_BUS_SYSMGR_NAME,
                     IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE,
                     &ev, sizeof(ev));

    EXPECT_EQ(1, systemStates.qam_ready_status.state);
}

TEST_F(SysMgrTest, SysEventHandler_UpdatesIpMode_WithPayload)
{
    auto ev = makeStateEvent(IARM_BUS_SYSMGR_SYSSTATE_IP_MODE, 2, 0, "v6");
    _sysEventHandler(IARM_BUS_SYSMGR_NAME,
                     IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE,
                     &ev, sizeof(ev));

    EXPECT_EQ(2, systemStates.ip_mode.state);
    EXPECT_STREQ("v6", systemStates.ip_mode.payload);
}

TEST_F(SysMgrTest, SysEventHandler_UpdatesDstOffset_WithPayload)
{
    auto ev = makeStateEvent(IARM_BUS_SYSMGR_SYSSTATE_DST_OFFSET, 1, 0, "+5:30");
    _sysEventHandler(IARM_BUS_SYSMGR_NAME,
                     IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE,
                     &ev, sizeof(ev));

    EXPECT_EQ(1, systemStates.dst_offset.state);
    EXPECT_STREQ("+5:30", systemStates.dst_offset.payload);
}

TEST_F(SysMgrTest, SysEventHandler_UpdatesEstbIp)
{
    auto ev = makeStateEvent(IARM_BUS_SYSMGR_SYSSTATE_ESTB_IP, 1, 0);
    _sysEventHandler(IARM_BUS_SYSMGR_NAME,
                     IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE,
                     &ev, sizeof(ev));

    EXPECT_EQ(1, systemStates.estb_ip.state);
}

TEST_F(SysMgrTest, SysEventHandler_UpdatesEcmIp)
{
    auto ev = makeStateEvent(IARM_BUS_SYSMGR_SYSSTATE_ECM_IP, 1, 0);
    _sysEventHandler(IARM_BUS_SYSMGR_NAME,
                     IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE,
                     &ev, sizeof(ev));

    EXPECT_EQ(1, systemStates.ecm_ip.state);
}

TEST_F(SysMgrTest, SysEventHandler_UpdatesLanIp)
{
    auto ev = makeStateEvent(IARM_BUS_SYSMGR_SYSSTATE_LAN_IP, 1, 0);
    _sysEventHandler(IARM_BUS_SYSMGR_NAME,
                     IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE,
                     &ev, sizeof(ev));

    EXPECT_EQ(1, systemStates.lan_ip.state);
}

TEST_F(SysMgrTest, SysEventHandler_UpdatesMoca)
{
    auto ev = makeStateEvent(IARM_BUS_SYSMGR_SYSSTATE_MOCA, 1, 0);
    _sysEventHandler(IARM_BUS_SYSMGR_NAME,
                     IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE,
                     &ev, sizeof(ev));

    EXPECT_EQ(1, systemStates.moca.state);
}

TEST_F(SysMgrTest, SysEventHandler_UpdatesDocsis)
{
    auto ev = makeStateEvent(IARM_BUS_SYSMGR_SYSSTATE_DOCSIS, 1, 0);
    _sysEventHandler(IARM_BUS_SYSMGR_NAME,
                     IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE,
                     &ev, sizeof(ev));

    EXPECT_EQ(1, systemStates.docsis.state);
}

TEST_F(SysMgrTest, SysEventHandler_UpdatesTimeSource)
{
    auto ev = makeStateEvent(IARM_BUS_SYSMGR_SYSSTATE_TIME_SOURCE, 1, 0);
    _sysEventHandler(IARM_BUS_SYSMGR_NAME,
                     IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE,
                     &ev, sizeof(ev));

    EXPECT_EQ(1, systemStates.time_source.state);
}

TEST_F(SysMgrTest, SysEventHandler_MultipleUpdates_LastOneWins)
{
    /* Drive the same field twice; the second value should stick */
    auto ev1 = makeStateEvent(IARM_BUS_SYSMGR_SYSSTATE_BOOTUP, 1, 0);
    _sysEventHandler(IARM_BUS_SYSMGR_NAME,
                     IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE,
                     &ev1, sizeof(ev1));

    auto ev2 = makeStateEvent(IARM_BUS_SYSMGR_SYSSTATE_BOOTUP, 9, 2);
    _sysEventHandler(IARM_BUS_SYSMGR_NAME,
                     IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE,
                     &ev2, sizeof(ev2));

    EXPECT_EQ(9, systemStates.bootup.state);
    EXPECT_EQ(2, systemStates.bootup.error);
}

/* =======================================================================
 * Section 6 – HDCP profile
 *
 * CheckHdcpProfile() calls lstat("/opt/.hdcp_profile_1", …).
 * The SetUp() fixture removes the file so each test starts with
 * no file present, making CheckHdcpProfile() deterministically return 0.
 * ====================================================================== */

TEST_F(SysMgrTest, CheckHdcpProfile_WhenFileAbsent_ReturnsZero)
{
    /* File was deleted in SetUp() */
    EXPECT_EQ(0, CheckHdcpProfile());
}

TEST_F(SysMgrTest, GetHDCPProfile_WhenFileAbsent_ReturnsZero)
{
    IARM_BUS_SYSMGR_HDCPProfileInfo_Param_t param;
    memset(&param, 0xff, sizeof(param));

    IARM_Result_t result = _GetHDCPProfile(&param);

    EXPECT_EQ(IARM_RESULT_SUCCESS, result);
    EXPECT_EQ(0, param.HdcpProfile);
}

TEST_F(SysMgrTest, SetHDCPProfile_SameAsCurrentProfile_ReturnsInvalidState)
{
    /* current_profile = 0 (file absent), new_profile = 0 → no change needed */
    IARM_BUS_SYSMGR_HDCPProfileInfo_Param_t param;
    param.HdcpProfile = 0;

    EXPECT_EQ(IARM_RESULT_INVALID_STATE, _SetHDCPProfile(&param));
}

TEST_F(SysMgrTest, SetHDCPProfile_InvalidProfileValue_ReturnsInvalidState)
{
    IARM_BUS_SYSMGR_HDCPProfileInfo_Param_t param;
    param.HdcpProfile = 2;  /* only 0 and 1 are valid */

    EXPECT_EQ(IARM_RESULT_INVALID_STATE, _SetHDCPProfile(&param));
}

TEST_F(SysMgrTest, SetHDCPProfile_Profile1WhenCurrentIs0_ReturnsSuccess)
{
    /* current_profile = 0, new_profile = 1 → change is triggered.
     * open() to create the file may fail if /opt is not writable (CI),
     * but the function still completes and returns IARM_RESULT_SUCCESS. */
    IARM_BUS_SYSMGR_HDCPProfileInfo_Param_t param;
    param.HdcpProfile = 1;

    EXPECT_EQ(IARM_RESULT_SUCCESS, _SetHDCPProfile(&param));
}
