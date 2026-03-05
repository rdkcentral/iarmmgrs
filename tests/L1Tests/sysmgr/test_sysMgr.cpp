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

/* sysMgr.h must come first so that _IARM_BUS_SYSMGR_H is defined before
 * Iarm.h is processed.  This prevents the redefinition errors that arise
 * because Iarm.h contains a duplicate copy of the sysMgr type block.
 * libIARM.h (included transitively by sysMgr.h) is an empty stub in the
 * test build, which is fine because sysMgr.h's own type definitions do
 * not require IARM_Result_t. */
#include "sysMgr.h"

/* Iarm.h / IarmBusMock.h must come AFTER sysMgr.h so IARM_Result_t is
 * defined as the proper enum before sysMgr.c's static functions are seen. */
#include "Iarm.h"
#include "IarmBusMock.h"

/* __TIMESTAMP() is used as a no-op logging call throughout sysMgr.c.
 * In the real IARM SDK it is defined in libIARM.h; our stub is empty. */
#ifndef __TIMESTAMP
#define __TIMESTAMP(...)
#endif

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

        /* SYSMgr_Stop() calls pthread_mutex_destroy(); reinitialise the
         * mutex here so SYSMgr_Start() can safely lock it even on the
         * first test or after a previous TearDown destroyed it. */
        pthread_mutex_init(&tMutexLock, nullptr);

        /* Reset mutable static state to well-known values so SYSMgr_Start
         * always takes the !initialized branch. */
        memset(&systemStates, 0, sizeof(systemStates));
        keyLogStatus = 1;   /* documented default */
        initialized  = 0;

        /* Bring the manager up – registers all IARM calls/events. */
        ASSERT_EQ(IARM_RESULT_SUCCESS, SYSMgr_Start());

        /* Remove the HDCP profile sentinel file if a previous test left it */
        ::unlink(profile_1_filename);
    }

    void TearDown() override
    {
        /* Shut the manager down cleanly before validating mock expectations. */
        SYSMgr_Stop();

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

TEST_F(SysMgrTest, GetSystemStates_ReturnsInitialisedStateAfterStart)
{
    IARM_Bus_SYSMgr_GetSystemStates_Param_t out;
    memset(&out, 0xff, sizeof(out));  /* fill with garbage */

    IARM_Result_t result = _GetSystemStates(&out);

    EXPECT_EQ(IARM_RESULT_SUCCESS, result);
    EXPECT_EQ(0, out.channel_map.state);
    EXPECT_EQ(0, out.bootup.state);
    /* SYSMgr_Start() sets hdcp_enabled.state = 1 as the documented default */
    EXPECT_EQ(1, out.hdcp_enabled.state);
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
 * Section 5b – _sysEventHandler: remaining switch-case branches
 * ====================================================================== */

TEST_F(SysMgrTest, SysEventHandler_UpdatesExitOkKeySequence)
{
    auto ev = makeStateEvent(IARM_BUS_SYSMGR_SYSSTATE_EXIT_OK, 1, 0);
    _sysEventHandler(IARM_BUS_SYSMGR_NAME,
                     IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE,
                     &ev, sizeof(ev));

    EXPECT_EQ(1, systemStates.exit_ok_key_sequence.state);
    EXPECT_EQ(0, systemStates.exit_ok_key_sequence.error);
}

TEST_F(SysMgrTest, SysEventHandler_UpdatesCmac)
{
    auto ev = makeStateEvent(IARM_BUS_SYSMGR_SYSSTATE_CMAC, 3, 1);
    _sysEventHandler(IARM_BUS_SYSMGR_NAME,
                     IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE,
                     &ev, sizeof(ev));

    EXPECT_EQ(3, systemStates.cmac.state);
    EXPECT_EQ(1, systemStates.cmac.error);
}

TEST_F(SysMgrTest, SysEventHandler_UpdatesMotoEntitlement)
{
    auto ev = makeStateEvent(IARM_BUS_SYSMGR_SYSSTATE_MOTO_ENTITLEMENT, 2, 0);
    _sysEventHandler(IARM_BUS_SYSMGR_NAME,
                     IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE,
                     &ev, sizeof(ev));

    EXPECT_EQ(2, systemStates.card_moto_entitlements.state);
    EXPECT_EQ(0, systemStates.card_moto_entitlements.error);
}

TEST_F(SysMgrTest, SysEventHandler_UpdatesMotoHrvRx)
{
    auto ev = makeStateEvent(IARM_BUS_SYSMGR_SYSSTATE_MOTO_HRV_RX, 1, 0);
    _sysEventHandler(IARM_BUS_SYSMGR_NAME,
                     IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE,
                     &ev, sizeof(ev));

    EXPECT_EQ(1, systemStates.card_moto_hrv_rx.state);
    EXPECT_EQ(0, systemStates.card_moto_hrv_rx.error);
}

TEST_F(SysMgrTest, SysEventHandler_UpdatesDacInitTimestamp_WithPayload)
{
    auto ev = makeStateEvent(IARM_BUS_SYSMGR_SYSSTATE_DAC_INIT_TIMESTAMP, 1, 0, "1234567890");
    _sysEventHandler(IARM_BUS_SYSMGR_NAME,
                     IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE,
                     &ev, sizeof(ev));

    EXPECT_EQ(1, systemStates.dac_init_timestamp.state);
    EXPECT_EQ(0, systemStates.dac_init_timestamp.error);
    EXPECT_STREQ("1234567890", systemStates.dac_init_timestamp.payload);
}

TEST_F(SysMgrTest, SysEventHandler_UpdatesCardCiscoStatus)
{
    auto ev = makeStateEvent(IARM_BUS_SYSMGR_SYSSTATE_CARD_CISCO_STATUS, 5, 2);
    _sysEventHandler(IARM_BUS_SYSMGR_NAME,
                     IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE,
                     &ev, sizeof(ev));

    EXPECT_EQ(5, systemStates.card_cisco_status.state);
    EXPECT_EQ(2, systemStates.card_cisco_status.error);
}

TEST_F(SysMgrTest, SysEventHandler_UpdatesVideoPresenting)
{
    auto ev = makeStateEvent(IARM_BUS_SYSMGR_SYSSTATE_VIDEO_PRESENTING, 1, 0);
    _sysEventHandler(IARM_BUS_SYSMGR_NAME,
                     IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE,
                     &ev, sizeof(ev));

    EXPECT_EQ(1, systemStates.video_presenting.state);
    EXPECT_EQ(0, systemStates.video_presenting.error);
}

TEST_F(SysMgrTest, SysEventHandler_UpdatesHdmiEdidRead)
{
    auto ev = makeStateEvent(IARM_BUS_SYSMGR_SYSSTATE_HDMI_EDID_READ, 1, 0);
    _sysEventHandler(IARM_BUS_SYSMGR_NAME,
                     IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE,
                     &ev, sizeof(ev));

    EXPECT_EQ(1, systemStates.hdmi_edid_read.state);
    EXPECT_EQ(0, systemStates.hdmi_edid_read.error);
}

TEST_F(SysMgrTest, SysEventHandler_UpdatesFirmwareDownload)
{
    auto ev = makeStateEvent(IARM_BUS_SYSMGR_SYSSTATE_FIRMWARE_DWNLD, 2, 0);
    _sysEventHandler(IARM_BUS_SYSMGR_NAME,
                     IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE,
                     &ev, sizeof(ev));

    EXPECT_EQ(2, systemStates.firmware_download.state);
    EXPECT_EQ(0, systemStates.firmware_download.error);
}

TEST_F(SysMgrTest, SysEventHandler_UpdatesFirmwareUpdateState)
{
    auto ev = makeStateEvent(IARM_BUS_SYSMGR_SYSSTATE_FIRMWARE_UPDATE_STATE, 3, 1);
    _sysEventHandler(IARM_BUS_SYSMGR_NAME,
                     IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE,
                     &ev, sizeof(ev));

    EXPECT_EQ(3, systemStates.firmware_update_state.state);
    EXPECT_EQ(1, systemStates.firmware_update_state.error);
}

TEST_F(SysMgrTest, SysEventHandler_UpdatesRedRecovState)
{
    auto ev = makeStateEvent(IARM_BUS_SYSMGR_SYSSTATE_RED_RECOV_UPDATE_STATE, 1, 0);
    _sysEventHandler(IARM_BUS_SYSMGR_NAME,
                     IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE,
                     &ev, sizeof(ev));

    EXPECT_EQ(1, systemStates.red_recov_state.state);
    EXPECT_EQ(0, systemStates.red_recov_state.error);
}

TEST_F(SysMgrTest, SysEventHandler_UpdatesTimeZone_WithPayload)
{
    auto ev = makeStateEvent(IARM_BUS_SYSMGR_SYSSTATE_TIME_ZONE, 1, 0, "EST05EDT,M3.2.0,M11.1.0");
    _sysEventHandler(IARM_BUS_SYSMGR_NAME,
                     IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE,
                     &ev, sizeof(ev));

    EXPECT_EQ(1, systemStates.time_zone_available.state);
    EXPECT_EQ(0, systemStates.time_zone_available.error);
    EXPECT_STREQ("EST05EDT,M3.2.0,M11.1.0", systemStates.time_zone_available.payload);
}

TEST_F(SysMgrTest, SysEventHandler_UpdatesCaSystem)
{
    auto ev = makeStateEvent(IARM_BUS_SYSMGR_SYSSTATE_CA_SYSTEM, 1, 0);
    _sysEventHandler(IARM_BUS_SYSMGR_NAME,
                     IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE,
                     &ev, sizeof(ev));

    EXPECT_EQ(1, systemStates.ca_system.state);
    EXPECT_EQ(0, systemStates.ca_system.error);
}

TEST_F(SysMgrTest, SysEventHandler_UpdatesDsgBroadcastTunnel)
{
    auto ev = makeStateEvent(IARM_BUS_SYSMGR_SYSSTATE_DSG_BROADCAST_CHANNEL, 1, 0);
    _sysEventHandler(IARM_BUS_SYSMGR_NAME,
                     IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE,
                     &ev, sizeof(ev));

    EXPECT_EQ(1, systemStates.dsg_broadcast_tunnel.state);
    EXPECT_EQ(0, systemStates.dsg_broadcast_tunnel.error);
}

TEST_F(SysMgrTest, SysEventHandler_UpdatesDsgCaTunnel)
{
    auto ev = makeStateEvent(IARM_BUS_SYSMGR_SYSSTATE_DSG_CA_TUNNEL, 1, 0);
    _sysEventHandler(IARM_BUS_SYSMGR_NAME,
                     IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE,
                     &ev, sizeof(ev));

    EXPECT_EQ(1, systemStates.dsg_ca_tunnel.state);
    EXPECT_EQ(0, systemStates.dsg_ca_tunnel.error);
}

TEST_F(SysMgrTest, SysEventHandler_UpdatesCableCard)
{
    auto ev = makeStateEvent(IARM_BUS_SYSMGR_SYSSTATE_CABLE_CARD, 1, 0);
    _sysEventHandler(IARM_BUS_SYSMGR_NAME,
                     IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE,
                     &ev, sizeof(ev));

    EXPECT_EQ(1, systemStates.cable_card.state);
    EXPECT_EQ(0, systemStates.cable_card.error);
}

TEST_F(SysMgrTest, SysEventHandler_UpdatesCableCardDownload)
{
    auto ev = makeStateEvent(IARM_BUS_SYSMGR_SYSSTATE_CABLE_CARD_DWNLD, 2, 1);
    _sysEventHandler(IARM_BUS_SYSMGR_NAME,
                     IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE,
                     &ev, sizeof(ev));

    EXPECT_EQ(2, systemStates.cable_card_download.state);
    EXPECT_EQ(1, systemStates.cable_card_download.error);
}

TEST_F(SysMgrTest, SysEventHandler_UpdatesCvrSubsystem)
{
    auto ev = makeStateEvent(IARM_BUS_SYSMGR_SYSSTATE_CVR_SUBSYSTEM, 1, 0);
    _sysEventHandler(IARM_BUS_SYSMGR_NAME,
                     IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE,
                     &ev, sizeof(ev));

    EXPECT_EQ(1, systemStates.cvr_subsystem.state);
    EXPECT_EQ(0, systemStates.cvr_subsystem.error);
}

TEST_F(SysMgrTest, SysEventHandler_UpdatesDownload)
{
    auto ev = makeStateEvent(IARM_BUS_SYSMGR_SYSSTATE_DOWNLOAD, 1, 0);
    _sysEventHandler(IARM_BUS_SYSMGR_NAME,
                     IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE,
                     &ev, sizeof(ev));

    EXPECT_EQ(1, systemStates.download.state);
    EXPECT_EQ(0, systemStates.download.error);
}

TEST_F(SysMgrTest, SysEventHandler_UpdatesVodAd_WithPayload)
{
    auto ev = makeStateEvent(IARM_BUS_SYSMGR_SYSSTATE_VOD_AD, 1, 0, "vod_data");
    _sysEventHandler(IARM_BUS_SYSMGR_NAME,
                     IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE,
                     &ev, sizeof(ev));

    EXPECT_EQ(1, systemStates.vod_ad.state);
    EXPECT_EQ(0, systemStates.vod_ad.error);
    EXPECT_STREQ("vod_data", systemStates.vod_ad.payload);
}

TEST_F(SysMgrTest, SysEventHandler_UpdatesCableCardSerialNo_WithPayload)
{
    auto ev = makeStateEvent(IARM_BUS_SYSMGR_SYSSTATE_CABLE_CARD_SERIAL_NO, 0, 0, "CARD123");
    _sysEventHandler(IARM_BUS_SYSMGR_NAME,
                     IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE,
                     &ev, sizeof(ev));

    /* CABLE_CARD_SERIAL_NO stores error and payload; state field is not updated */
    EXPECT_EQ(0, systemStates.card_serial_no.error);
    EXPECT_STREQ("CARD123", systemStates.card_serial_no.payload);
}

TEST_F(SysMgrTest, SysEventHandler_UpdatesEcmMac_WithPayload)
{
    auto ev = makeStateEvent(IARM_BUS_SYSMGR_SYSSTATE_ECM_MAC, 0, 0, "AA:BB:CC:DD:EE:FF");
    _sysEventHandler(IARM_BUS_SYSMGR_NAME,
                     IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE,
                     &ev, sizeof(ev));

    /* ECM_MAC stores error and payload; state field is not updated */
    EXPECT_EQ(0, systemStates.ecm_mac.error);
    EXPECT_STREQ("AA:BB:CC:DD:EE:FF", systemStates.ecm_mac.payload);
}

TEST_F(SysMgrTest, SysEventHandler_UpdatesDacId_WithPayload)
{
    auto ev = makeStateEvent(IARM_BUS_SYSMGR_SYSSTATE_DAC_ID, 1, 0, "DAC42");
    _sysEventHandler(IARM_BUS_SYSMGR_NAME,
                     IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE,
                     &ev, sizeof(ev));

    EXPECT_EQ(1, systemStates.dac_id.state);
    EXPECT_EQ(0, systemStates.dac_id.error);
    EXPECT_STREQ("DAC42", systemStates.dac_id.payload);
}

TEST_F(SysMgrTest, SysEventHandler_UpdatesPlantId_WithPayload)
{
    auto ev = makeStateEvent(IARM_BUS_SYSMGR_SYSSTATE_PLANT_ID, 1, 0, "PLANT7");
    _sysEventHandler(IARM_BUS_SYSMGR_NAME,
                     IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE,
                     &ev, sizeof(ev));

    EXPECT_EQ(1, systemStates.plant_id.state);
    EXPECT_EQ(0, systemStates.plant_id.error);
    EXPECT_STREQ("PLANT7", systemStates.plant_id.payload);
}

TEST_F(SysMgrTest, SysEventHandler_UpdatesStbSerialNo_WithPayload)
{
    auto ev = makeStateEvent(IARM_BUS_SYSMGR_SYSSTATE_STB_SERIAL_NO, 0, 0, "STB99");
    _sysEventHandler(IARM_BUS_SYSMGR_NAME,
                     IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE,
                     &ev, sizeof(ev));

    /* STB_SERIAL_NO stores error and payload; state field is not updated */
    EXPECT_EQ(0, systemStates.stb_serial_no.error);
    EXPECT_STREQ("STB99", systemStates.stb_serial_no.payload);
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
