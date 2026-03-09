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
#include <cerrno>
#include <pthread.h>
#include <unistd.h>
#include <glib.h>

/* sysMgr.h first — prevents type-redefinition conflicts with Iarm.h */
#include "sysMgr.h"
#include "Iarm.h"
#include "IarmBusMock.h"
#include "WrapsMock.h"

/* dsMgr.h (stub) pulls in dsRpc.h which defines the RPC param structs
 * (dsVideoPortGetHandleParam_t, dsDisplayGetEDIDBytesParam_t, …) needed
 * by the DS-HAL stub functions defined below.  Must come before those. */
#include "dsMgr.h"

/* dsHalMock.h defines the DsHal abstract interface + DsHalMock.  Must be
 * included here — before the _ds* bridge functions below — because those
 * functions call DsHal::getInstance().  DSHAL_MOCK_DEFINE_BRIDGES also
 * emits the extern "C" dsGetDisplay() bridge and DsHal::s_impl in this TU. */
#define DSHAL_MOCK_DEFINE_BRIDGES
#include "dsHalMock.h"

/* -----------------------------------------------------------------------
 * All external symbols that dsMgr.c references but does not define.
 * Defined as plain C++ functions; no extern "C" wrapper is needed because
 * the whole TU is C++ and dsMgr.c's extern declarations in the same TU
 * also get C++ linkage.
 * --------------------------------------------------------------------- */

/* ---- DS HAL functions (declared extern at the top of dsMgr.c) -------
 * These bridge functions cast void*arg to the appropriate typed struct
 * pointer and delegate to the DsHalMock installed on the fixture.
 * NiceMock returns IARM_RESULT_SUCCESS (= 0) by default and leaves output
 * fields zeroed, matching the previous no-op stub behaviour.
 * Individual tests use ON_CALL / EXPECT_CALL to inject specific values.
 * --------------------------------------------------------------------- */
IARM_Result_t _dsGetVideoPort(void *arg) {
    DsHal *impl = DsHal::getInstance();
    return impl ? impl->dsGetVideoPort(
                      static_cast<dsVideoPortGetHandleParam_t *>(arg))
                : IARM_RESULT_SUCCESS;
}
IARM_Result_t _dsIsDisplayConnected(void *arg) {
    DsHal *impl = DsHal::getInstance();
    return impl ? impl->dsIsDisplayConnected(
                      static_cast<dsVideoPortIsDisplayConnectedParam_t *>(arg))
                : IARM_RESULT_SUCCESS;
}
IARM_Result_t _dsGetIgnoreEDIDStatus(void *arg) { return IARM_RESULT_SUCCESS; }
IARM_Result_t _dsSetResolution(void *arg)       { return IARM_RESULT_SUCCESS; }
IARM_Result_t _dsGetResolution(void *arg) {
    DsHal *impl = DsHal::getInstance();
    return impl ? impl->dsGetResolution(
                      static_cast<dsVideoPortGetResolutionParam_t *>(arg))
                : IARM_RESULT_SUCCESS;
}
IARM_Result_t _dsInitResolution(void *arg)      { return IARM_RESULT_SUCCESS; }
IARM_Result_t _dsGetEDID(void *arg) {
    DsHal *impl = DsHal::getInstance();
    return impl ? impl->dsGetEDID(
                      static_cast<dsDisplayGetEDIDParam_t *>(arg))
                : IARM_RESULT_SUCCESS;
}
IARM_Result_t _dsGetEDIDBytes(void *arg) {
    DsHal *impl = DsHal::getInstance();
    return impl ? impl->dsGetEDIDBytes(
                      static_cast<dsDisplayGetEDIDBytesParam_t *>(arg))
                : IARM_RESULT_SUCCESS;
}
IARM_Result_t _dsGetForceDisable4K(void *arg)   { return IARM_RESULT_SUCCESS; }
IARM_Result_t _dsSetBackgroundColor(void *arg)  { return IARM_RESULT_SUCCESS; }
IARM_Result_t _dsGetAudioPort(void *arg)        { return IARM_RESULT_SUCCESS; }
IARM_Result_t _dsGetStereoMode(void *arg) {
    DsHal *impl = DsHal::getInstance();
    return impl ? impl->dsGetStereoMode(
                      static_cast<dsAudioSetStereoModeParam_t *>(arg))
                : IARM_RESULT_SUCCESS;
}
IARM_Result_t _dsSetStereoMode(void *arg)       { return IARM_RESULT_SUCCESS; }
IARM_Result_t _dsGetStereoAuto(void *arg)       { return IARM_RESULT_SUCCESS; }
IARM_Result_t _dsIsDisplaySurround(void *arg)   { return IARM_RESULT_SUCCESS; }

/* ---- Miscellaneous helpers ------------------------------------------ */
bool dsGetHDMIDDCLineStatus(void) { return false; }
bool isComponentPortPresent(void) { return true; }

/* ---- DS Manager HAL initialisation (called from DSMgr_Start) ----------
 * Declared inside extern "C" in stubs/dsMgr.h, so must be defined as C.
 * The global lets individual tests inject failure. */
static IARM_Result_t g_stub_dsMgr_init_result = IARM_RESULT_SUCCESS;
extern "C" IARM_Result_t dsMgr_init(void) { return g_stub_dsMgr_init_result; }

/* ---- Power Controller API (declared inside extern "C" in power_controller.h) */
extern "C" void PowerController_Init(void) {}
extern "C" void PowerController_Term(void) {}

/* ---- Power Event Listener helper (defined in dsMgrPwrEventListener.c)  */
void initPwrEventListner(void)         {}
void dsMgrInitPwrControllerEvt(void)   {}
void dsMgrDeinitPwrControllerEvt(void) {}

/* -----------------------------------------------------------------------
 * __wrap_ functions for DSMgr_Start() branch coverage.
 *
 * pthread_cond_init / pthread_create / fscanf / g_main_loop_new /
 * g_timeout_add_seconds / g_main_loop_unref are intercepted via
 * --wrap so we can inject failures and avoid spawning real threads
 * or GLib loops in unit tests.
 * --------------------------------------------------------------------- */
static int  g_stub_pthread_cond_init_ret   = 0;
static int  g_stub_pthread_create_ret      = 0;
static bool g_stub_gloop_null              = false;

/* __real_fclose lets us release real FILE* objects that the fclose mock
 * would otherwise swallow without actually closing.                      */
extern "C" int __real_fclose(FILE *);

extern "C" {

/* usleep is called inside _SetVideoPortResolution when the HDMI handle is
 * non-NULL (100 ms wait).  Wrap it to a no-op so tests run instantly.   */
void __wrap_usleep(useconds_t us) { (void)us; }

/* Wrapped libc / pthread symbols */
int __real_pthread_cond_init(pthread_cond_t *, const pthread_condattr_t *);
int __wrap_pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr)
{
    if (g_stub_pthread_cond_init_ret != 0)
        return g_stub_pthread_cond_init_ret;
    return __real_pthread_cond_init(cond, attr);
}

int __real_pthread_create(pthread_t *, const pthread_attr_t *,
                          void *(*)(void *), void *);
int __wrap_pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                          void *(*start)(void *), void *arg)
{
    if (g_stub_pthread_create_ret != 0)
        return g_stub_pthread_create_ret;
    /* In success-path tests we still avoid spawning the real infinite-loop
     * thread.  Return 0 (success) but don't actually create anything.     */
    *thread = 0;
    return 0;
}

/* Wrapped GLib symbols */
GMainLoop *__real_g_main_loop_new(GMainContext *, gboolean);
GMainLoop *__wrap_g_main_loop_new(GMainContext *ctx, gboolean is_running)
{
    if (g_stub_gloop_null)
        return nullptr;
    /* Return a distinguishable non-NULL sentinel.  We never run the loop
     * so the pointer is never dereferenced by GLib internals.            */
    return reinterpret_cast<GMainLoop *>(0xBEEF);
}

guint __wrap_g_timeout_add_seconds(guint interval, GSourceFunc func, gpointer data)
{
    (void)interval; (void)func; (void)data;
    return 1;  /* fake source id */
}

void __wrap_g_main_loop_unref(GMainLoop *loop)
{
    (void)loop; /* no-op — we never allocated a real GMainLoop */
}

} /* extern "C" */

/* -----------------------------------------------------------------------
 * Source under test — included last so the stubs above satisfy all
 * extern declarations found inside dsMgr.c.
 * --------------------------------------------------------------------- */
#include "dsMgr.c"


using ::testing::_;
using ::testing::Invoke;
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

        /* Default: any fopen call not matched by a specific EXPECT_CALL
         * returns nullptr.  Without this, setting EXPECT_CALL for one
         * fopen path (e.g. "/opt/ddcDelay") makes every other fopen path
         * (e.g. "/etc/device.properties" inside isEUPlatform) "unexpected"
         * even with NiceMock, causing spurious test failures. */
        ON_CALL(wrapsMock, fopen(_, _)).WillByDefault(Return(nullptr));
        /* Default: fclose any pointer safely without calling real fclose. */
        ON_CALL(wrapsMock, fclose(_)).WillByDefault(Return(0));

        /* Reset DSMgr_Start stub controls FIRST so that the pthread init
         * calls below pass through to the real implementations. */
        g_stub_dsMgr_init_result     = IARM_RESULT_SUCCESS;
        g_stub_pthread_cond_init_ret = 0;
        g_stub_pthread_create_ret    = 0;
        g_stub_gloop_null            = false;

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
        iResnCount          = 5;
        iInitResnFlag       = 0;
        dsMgr_Gloop         = nullptr;

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
 * ON_CALL on dsGetVideoPort returns a non-zero handle, exercising the
 * if(vidPortParam.handle != NULL) block (lines 434–440) that had 0 hits.
 * ===================================================================== */

TEST_F(DsMgrTest, SetBGColor_WithValidHandle_CallsSetBackgroundColor)
{
    ON_CALL(dsHalMock, dsGetVideoPort(_))
        .WillByDefault(Invoke([](dsVideoPortGetHandleParam_t *p) {
            p->handle = 0xCAFE;
            return IARM_RESULT_SUCCESS;
        }));
    setBGColor(dsVIDEO_BGCOLOR_NONE);
    /* Lines 435-440 covered; _dsSetBackgroundColor stub absorbs the call. */
}

TEST_F(DsMgrTest, GetVideoPortHandle_StubReturnsConfiguredHandle)
{
    ON_CALL(dsHalMock, dsGetVideoPort(_))
        .WillByDefault(Invoke([](dsVideoPortGetHandleParam_t *p) {
            p->handle = 0xDEAD;
            return IARM_RESULT_SUCCESS;
        }));
    intptr_t h = getVideoPortHandle(dsVIDEOPORT_TYPE_HDMI);
    EXPECT_EQ(h, static_cast<intptr_t>(0xDEAD));
}

/* =======================================================================
 * Section 16 – dumpEdidOnChecksumDiff(): valid EDID length inner paths
 *
 * ON_CALL on dsGetEDIDBytes fills in length and the checksum byte.
 * A single test exercises both the "new checksum → dump → TRUE" path
 * and the "same checksum → no dump → FALSE" path by calling the function
 * twice in sequence, relying on the static cached_EDID_checksum starting
 * at 0 (no previous test has written to it for this particular data).
 * ===================================================================== */

TEST_F(DsMgrTest, DumpEdidOnChecksumDiff_ValidEdid_NewThenSameChecksum)
{
    /* 128-byte EDID block; checksum byte = 0xCC (unique value not used by
     * any earlier test). */
    ON_CALL(dsHalMock, dsGetEDIDBytes(_))
        .WillByDefault(Invoke([](dsDisplayGetEDIDBytesParam_t *p) {
            p->length = 128;
            p->bytes[127] = 0xCC;
            return IARM_RESULT_SUCCESS;
        }));

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
 * Section 17b – DSMgr_Start(): deeper branch coverage
 *
 * Tests below cover every remaining if-statement inside DSMgr_Start.
 * The __wrap_ functions defined at the top of this file let us control
 * pthread_cond_init, pthread_create, fscanf, and g_main_loop_new without
 * spawning real threads or GLib loops.
 * ===================================================================== */

/* Helper: program the IarmBusMock so the first four IARM calls succeed
 * (Init → Connect → RegisterEvent → dsMgr_init). */
static void stubIarmPassInit(IarmBusImplMock &m)
{
    EXPECT_CALL(m, IARM_Bus_Init(_))
        .WillOnce(Return(IARM_RESULT_SUCCESS));
    EXPECT_CALL(m, IARM_Bus_Connect())
        .WillOnce(Return(IARM_RESULT_SUCCESS));
    EXPECT_CALL(m, IARM_Bus_RegisterEvent(_))
        .WillOnce(Return(IARM_RESULT_SUCCESS));
}

/* Helper: after dsMgr_init succeeds, the next five IARM calls are
 * RegisterEventHandler(×3), RegisterCall(×1).  Program them to succeed
 * by default and optionally override one later. */
static void stubIarmPassRegister(IarmBusImplMock &m)
{
    ON_CALL(m, IARM_Bus_RegisterEventHandler(_, _, _))
        .WillByDefault(Return(IARM_RESULT_SUCCESS));
    ON_CALL(m, IARM_Bus_RegisterCall(_, _))
        .WillByDefault(Return(IARM_RESULT_SUCCESS));
}

/* -- 17b-1: dsMgr_init returns failure -------------------------------- */
TEST_F(DsMgrTest, DsmgrStart_DsMgrInitFails_ReturnsError)
{
    stubIarmPassInit(iarmMock);
    g_stub_dsMgr_init_result = IARM_RESULT_IPCCORE_FAIL;

    EXPECT_EQ(DSMgr_Start(), IARM_RESULT_IPCCORE_FAIL);
}

/* -- 17b-2: first RegisterEventHandler (SYSMGR) fails ---------------- */
TEST_F(DsMgrTest, DsmgrStart_RegEvtSysMgrFails_ReturnsError)
{
    stubIarmPassInit(iarmMock);
    /* First RegisterEventHandler → FAIL; the rest are never called. */
    EXPECT_CALL(iarmMock, IARM_Bus_RegisterEventHandler(_, _, _))
        .WillOnce(Return(IARM_RESULT_IPCCORE_FAIL));

    EXPECT_EQ(DSMgr_Start(), IARM_RESULT_IPCCORE_FAIL);
}

/* -- 17b-3: second RegisterEventHandler (HDMI_HOTPLUG) fails ---------- */
TEST_F(DsMgrTest, DsmgrStart_RegEvtHdmiHpFails_ReturnsError)
{
    stubIarmPassInit(iarmMock);
    EXPECT_CALL(iarmMock, IARM_Bus_RegisterEventHandler(_, _, _))
        .WillOnce(Return(IARM_RESULT_SUCCESS))      /* SYSMGR */
        .WillOnce(Return(IARM_RESULT_IPCCORE_FAIL)); /* HDMI_HOTPLUG */

    EXPECT_EQ(DSMgr_Start(), IARM_RESULT_IPCCORE_FAIL);
}

/* -- 17b-4: third RegisterEventHandler (HDCP_STATUS) fails ------------ */
TEST_F(DsMgrTest, DsmgrStart_RegEvtHdcpFails_ReturnsError)
{
    stubIarmPassInit(iarmMock);
    EXPECT_CALL(iarmMock, IARM_Bus_RegisterEventHandler(_, _, _))
        .WillOnce(Return(IARM_RESULT_SUCCESS))      /* SYSMGR */
        .WillOnce(Return(IARM_RESULT_SUCCESS))       /* HDMI_HOTPLUG */
        .WillOnce(Return(IARM_RESULT_IPCCORE_FAIL)); /* HDCP_STATUS */

    EXPECT_EQ(DSMgr_Start(), IARM_RESULT_IPCCORE_FAIL);
}

/* -- 17b-5: RegisterCall (SysModeChange) fails ------------------------ */
TEST_F(DsMgrTest, DsmgrStart_RegCallSysModeFails_ReturnsError)
{
    stubIarmPassInit(iarmMock);
    ON_CALL(iarmMock, IARM_Bus_RegisterEventHandler(_, _, _))
        .WillByDefault(Return(IARM_RESULT_SUCCESS));
    EXPECT_CALL(iarmMock, IARM_Bus_RegisterCall(_, _))
        .WillOnce(Return(IARM_RESULT_IPCCORE_FAIL));

    EXPECT_EQ(DSMgr_Start(), IARM_RESULT_IPCCORE_FAIL);
}

/* -- 17b-6: pthread_cond_init fails ----------------------------------- */
TEST_F(DsMgrTest, DsmgrStart_CondInitFails_ReturnsError)
{
    stubIarmPassInit(iarmMock);
    stubIarmPassRegister(iarmMock);
    g_stub_pthread_cond_init_ret = EINVAL;  /* any non-zero → failure */

    EXPECT_EQ(DSMgr_Start(), IARM_RESULT_IPCCORE_FAIL);
}

/* -- 17b-7: pthread_create fails -------------------------------------- */
TEST_F(DsMgrTest, DsmgrStart_ThreadCreateFails_ReturnsError)
{
    stubIarmPassInit(iarmMock);
    stubIarmPassRegister(iarmMock);
    g_stub_pthread_create_ret = EAGAIN;  /* any non-zero → failure */

    EXPECT_EQ(DSMgr_Start(), IARM_RESULT_IPCCORE_FAIL);
}

/* -- 17b-8: ddcDelay file not found (fopen returns NULL) -------------- */
TEST_F(DsMgrTest, DsmgrStart_DdcDelayMissing_ContinuesOk)
{
    stubIarmPassInit(iarmMock);
    stubIarmPassRegister(iarmMock);

    /* fopen("/opt/ddcDelay") → NULL via ON_CALL default (no EXPECT_CALL
     * so that the later fopen("/etc/device.properties") inside
     * isEUPlatform() is also resolved by the same default without
     * triggering an "unexpected call" failure). */

    /* IARM_Bus_Call for GetSystemStates → succeed, TuneReady = 0 */
    ON_CALL(iarmMock, IARM_Bus_Call(_, _, _, _))
        .WillByDefault(Return(IARM_RESULT_SUCCESS));

    EXPECT_EQ(DSMgr_Start(), IARM_RESULT_SUCCESS);
}

/* -- 17b-9: ddcDelay file present, fscanf succeeds -------------------- */
/* Uses a real tmpfile so the real fscanf can read "42" from it.  The
 * fclose mock swallows the close call (returns 0), so we manually
 * release the tmpfile via __real_fclose after DSMgr_Start returns.      */
TEST_F(DsMgrTest, DsmgrStart_DdcDelayRead_UpdatesCount)
{
    stubIarmPassInit(iarmMock);
    stubIarmPassRegister(iarmMock);

    /* Build a real temp file containing "42" so fscanf("%d") succeeds. */
    FILE *tmpfp = tmpfile();
    ASSERT_NE(tmpfp, nullptr);
    fputs("42", tmpfp);
    rewind(tmpfp);

    ON_CALL(wrapsMock, fopen(StrEq("/opt/ddcDelay"), _))
        .WillByDefault(Return(tmpfp));

    ON_CALL(iarmMock, IARM_Bus_Call(_, _, _, _))
        .WillByDefault(Return(IARM_RESULT_SUCCESS));

    EXPECT_EQ(DSMgr_Start(), IARM_RESULT_SUCCESS);
    EXPECT_EQ(iResnCount, 42);

    /* fclose was mocked (no-op); release the real FILE* ourselves. */
    __real_fclose(tmpfp);
}

/* -- 17b-10: ddcDelay file present, fscanf fails ---------------------- */
/* Empty tmpfile → fscanf("%d") returns EOF (not 1) → error branch.     */
TEST_F(DsMgrTest, DsmgrStart_DdcDelayFscanfFails_ContinuesOk)
{
    stubIarmPassInit(iarmMock);
    stubIarmPassRegister(iarmMock);

    /* Empty file: fscanf will return EOF because there is nothing to read. */
    FILE *tmpfp = tmpfile();
    ASSERT_NE(tmpfp, nullptr);

    ON_CALL(wrapsMock, fopen(StrEq("/opt/ddcDelay"), _))
        .WillByDefault(Return(tmpfp));

    ON_CALL(iarmMock, IARM_Bus_Call(_, _, _, _))
        .WillByDefault(Return(IARM_RESULT_SUCCESS));

    EXPECT_EQ(DSMgr_Start(), IARM_RESULT_SUCCESS);

    /* fclose was mocked (no-op); release the real FILE* ourselves. */
    __real_fclose(tmpfp);
}

/* -- 17b-11: IARM_Bus_Call (GetSystemStates) fails -------------------- */
TEST_F(DsMgrTest, DsmgrStart_GetSysStatesFails_ReturnsError)
{
    stubIarmPassInit(iarmMock);
    stubIarmPassRegister(iarmMock);

    /* No EXPECT_CALL on fopen — ON_CALL default returns nullptr for any
     * path (/opt/ddcDelay, etc.).  DSMgr_Start returns early here so
     * isEUPlatform() is never reached. */

    EXPECT_CALL(iarmMock, IARM_Bus_Call(_, _, _, _))
        .WillOnce(Return(IARM_RESULT_IPCCORE_FAIL));

    EXPECT_EQ(DSMgr_Start(), IARM_RESULT_IPCCORE_FAIL);
}

/* -- 17b-12: TuneReadyStatus.state == 1 → iTuneReady set ------------- */
TEST_F(DsMgrTest, DsmgrStart_TuneReady_SetsFlag)
{
    stubIarmPassInit(iarmMock);
    stubIarmPassRegister(iarmMock);

    /* No EXPECT_CALL on fopen — ON_CALL default returns nullptr. */

    /* Program IARM_Bus_Call to set TuneReadyStatus.state = 1 */
    EXPECT_CALL(iarmMock, IARM_Bus_Call(_, _, _, _))
        .WillOnce(::testing::Invoke(
            [](const char *, const char *, void *arg, size_t) -> IARM_Result_t {
                auto *p = static_cast<IARM_Bus_SYSMgr_GetSystemStates_Param_t *>(arg);
                memset(p, 0, sizeof(*p));
                p->TuneReadyStatus.state = 1;
                return IARM_RESULT_SUCCESS;
            }));

    EXPECT_EQ(DSMgr_Start(), IARM_RESULT_SUCCESS);
    EXPECT_EQ(iTuneReady, 1);
}

/* -- 17b-13: TuneReadyStatus.state != 1 → iTuneReady stays 0 --------- */
TEST_F(DsMgrTest, DsmgrStart_TuneNotReady_FlagUnset)
{
    stubIarmPassInit(iarmMock);
    stubIarmPassRegister(iarmMock);

    /* No EXPECT_CALL on fopen — ON_CALL default returns nullptr. */

    EXPECT_CALL(iarmMock, IARM_Bus_Call(_, _, _, _))
        .WillOnce(::testing::Invoke(
            [](const char *, const char *, void *arg, size_t) -> IARM_Result_t {
                auto *p = static_cast<IARM_Bus_SYSMgr_GetSystemStates_Param_t *>(arg);
                memset(p, 0, sizeof(*p));
                p->TuneReadyStatus.state = 0;
                return IARM_RESULT_SUCCESS;
            }));

    EXPECT_EQ(DSMgr_Start(), IARM_RESULT_SUCCESS);
    EXPECT_EQ(iTuneReady, 0);
}

/* -- 17b-14: g_main_loop_new returns NULL → else-branch -------------- */
TEST_F(DsMgrTest, DsmgrStart_GloopNull_SkipsTimeout)
{
    stubIarmPassInit(iarmMock);
    stubIarmPassRegister(iarmMock);

    /* No EXPECT_CALL on fopen — ON_CALL default returns nullptr. */
    ON_CALL(iarmMock, IARM_Bus_Call(_, _, _, _))
        .WillByDefault(Return(IARM_RESULT_SUCCESS));

    g_stub_gloop_null = true;

    EXPECT_EQ(DSMgr_Start(), IARM_RESULT_SUCCESS);
    EXPECT_EQ(dsMgr_Gloop, nullptr);
}

/* -- 17b-15: full success path (g_main_loop_new non-NULL) ------------- */
TEST_F(DsMgrTest, DsmgrStart_FullSuccess_SetsGloop)
{
    stubIarmPassInit(iarmMock);
    stubIarmPassRegister(iarmMock);

    /* No EXPECT_CALL on fopen — ON_CALL default returns nullptr for both
     * /opt/ddcDelay and /etc/device.properties (non-EU default). */
    ON_CALL(iarmMock, IARM_Bus_Call(_, _, _, _))
        .WillByDefault(Return(IARM_RESULT_SUCCESS));

    g_stub_gloop_null = false;  /* returns 0xBEEF sentinel */

    EXPECT_EQ(DSMgr_Start(), IARM_RESULT_SUCCESS);
    EXPECT_NE(dsMgr_Gloop, nullptr);
}

/* =======================================================================
 * Section 17c – _SetVideoPortResolution(): HDMI non-NULL branch coverage
 *
 * Each test calls _SetVideoPortResolution() directly.  ON_CALL on
 * dsGetVideoPort / dsIsDisplayConnected controls which branch is taken.
 * usleep is wrapped to a no-op so tests run without a 100 ms delay.
 * ===================================================================== */

/* Helper: sentinel address for a safe non-NULL intptr_t handle value. */
static char g_port_sentinel_buf[4];
#define STUB_HANDLE(offset) \
    (reinterpret_cast<intptr_t>(g_port_sentinel_buf + (offset)))

/* -- 17c-1: HDMI handle present + display connected → HDMI resolution -- */
TEST_F(DsMgrTest, SetVPR_HdmiConnected_SetsHdmiResolution)
{
    iInitResnFlag = 1;
    ON_CALL(dsHalMock, dsGetVideoPort(_))
        .WillByDefault(Invoke([](dsVideoPortGetHandleParam_t *p) {
            if (p->type == dsVIDEOPORT_TYPE_HDMI) p->handle = STUB_HANDLE(0);
            return IARM_RESULT_SUCCESS;
        }));
    ON_CALL(dsHalMock, dsIsDisplayConnected(_))
        .WillByDefault(Invoke([](dsVideoPortIsDisplayConnectedParam_t *p) {
            p->connected = true;
            return IARM_RESULT_SUCCESS;
        }));
    /* dsGetDisplay (NiceMock) returns 0 for _displayHandle by default →
     * _SetResolution skips the EDID block, falls back to default res,
     * calls _dsSetResolution stub safely.                              */
    EXPECT_EQ(_SetVideoPortResolution(), 0);
}

/* -- 17c-2: HDMI present + not connected + component handle present ---- */
TEST_F(DsMgrTest, SetVPR_HdmiNotConnected_CompPresent_SetsComponent)
{
    ON_CALL(dsHalMock, dsGetVideoPort(_))
        .WillByDefault(Invoke([](dsVideoPortGetHandleParam_t *p) {
            if (p->type == dsVIDEOPORT_TYPE_HDMI)      p->handle = STUB_HANDLE(0);
            else if (p->type == dsVIDEOPORT_TYPE_COMPONENT) p->handle = STUB_HANDLE(1);
            return IARM_RESULT_SUCCESS;
        }));
    EXPECT_EQ(_SetVideoPortResolution(), 0);
}

/* -- 17c-3: HDMI present + not connected + no comp + BB handle present - */
TEST_F(DsMgrTest, SetVPR_HdmiNotConnected_NullComp_BBPresent_SetsBB)
{
    ON_CALL(dsHalMock, dsGetVideoPort(_))
        .WillByDefault(Invoke([](dsVideoPortGetHandleParam_t *p) {
            if (p->type == dsVIDEOPORT_TYPE_HDMI) p->handle = STUB_HANDLE(0);
            else if (p->type == dsVIDEOPORT_TYPE_BB)   p->handle = STUB_HANDLE(2);
            return IARM_RESULT_SUCCESS;
        }));
    EXPECT_EQ(_SetVideoPortResolution(), 0);
}

/* -- 17c-4: HDMI present + not connected + no comp/BB + RF present ----- */
TEST_F(DsMgrTest, SetVPR_HdmiNotConnected_NullBB_RFPresent_SetsRF)
{
    ON_CALL(dsHalMock, dsGetVideoPort(_))
        .WillByDefault(Invoke([](dsVideoPortGetHandleParam_t *p) {
            if (p->type == dsVIDEOPORT_TYPE_HDMI) p->handle = STUB_HANDLE(0);
            else if (p->type == dsVIDEOPORT_TYPE_RF)   p->handle = STUB_HANDLE(3);
            return IARM_RESULT_SUCCESS;
        }));
    EXPECT_EQ(_SetVideoPortResolution(), 0);
}

/* -- 17c-5: HDMI present + not connected + all other handles NULL ------ */
TEST_F(DsMgrTest, SetVPR_HdmiNotConnected_AllNull_LogsAndReturns)
{
    ON_CALL(dsHalMock, dsGetVideoPort(_))
        .WillByDefault(Invoke([](dsVideoPortGetHandleParam_t *p) {
            if (p->type == dsVIDEOPORT_TYPE_HDMI) p->handle = STUB_HANDLE(0);
            return IARM_RESULT_SUCCESS;
        }));
    EXPECT_EQ(_SetVideoPortResolution(), 0);
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

        /* Ensure wrapped pthread calls fall through to real implementations. */
        g_stub_pthread_cond_init_ret = 0;
        g_stub_pthread_create_ret    = 0;

        pthread_mutex_init(&tdsMutexLock, nullptr);
        pthread_cond_init(&tdsMutexCond, nullptr);
        isEAS       = IARM_BUS_SYS_MODE_NORMAL;
        iTuneReady  = 0;
        dsMgr_Gloop = nullptr;
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

/* =======================================================================
 * Section 19a – isResolutionSupported(): EDID × platform resolution lookup
 *
 * isResolutionSupported(edidData, numResolutions, pNumResolutions,
 *                       Resn, &index) scans edidData->suppResolutionList
 * [0..numResolutions-1] for Resn, then verifies that entry also exists
 * in the platform list returned by _dsGetVideoPortResolutions().
 * pNumResolutions is a legacy parameter that is no longer used internally.
 * Returns true + index on match.
 * ===================================================================== */

/* 19a-1: Resn found in EDID list AND in platform resolution list → true +
 * index 0.  The platform list comes from _dsGetVideoPortResolutions(). */
TEST_F(DsMgrTest, IsResolutionSupported_FoundInBothLists_ReturnsTrueWithIndex)
{
    /* Inject a one-entry platform resolution list containing "720p". */
    static dsVideoPortResolution_t platformRes720p;
    memset(&platformRes720p, 0, sizeof(platformRes720p));
    strncpy(platformRes720p.name, "720p", sizeof(platformRes720p.name) - 1);

    ON_CALL(dsHalMock, dsGetVideoPortResolutions(_, _))
        .WillByDefault(Invoke([](int *outSize, dsVideoPortResolution_t **outRes) {
            *outSize = 1;
            *outRes  = &platformRes720p;
            return dsERR_NONE;
        }));

    dsDisplayEDID_t edid;
    memset(&edid, 0, sizeof(edid));
    strncpy(edid.suppResolutionList[0].name, "720p", 31);

    int index = -1;
    EXPECT_TRUE(isResolutionSupported(&edid, 1, 0, "720p", &index));
    EXPECT_EQ(index, 0);
}

/* 19a-2: numResolutions == 0 → empty EDID list → false. */
TEST_F(DsMgrTest, IsResolutionSupported_EmptyEdidList_ReturnsFalse)
{
    dsDisplayEDID_t edid;
    memset(&edid, 0, sizeof(edid));

    int index = -1;
    EXPECT_FALSE(isResolutionSupported(&edid, 0, 0, "720p", &index));
}

/* 19a-3: Resn found in EDID list but NOT in platform resolution list → false. */
TEST_F(DsMgrTest, IsResolutionSupported_NotInKResolutions_ReturnsFalse)
{
    dsDisplayEDID_t edid;
    memset(&edid, 0, sizeof(edid));
    strncpy(edid.suppResolutionList[0].name, "unknown_res", 31);

    int index = -1;
    EXPECT_FALSE(isResolutionSupported(&edid, 1, 0, "unknown_res", &index));
}

/* 19a-4: Resn not found in EDID list at all → false. */
TEST_F(DsMgrTest, IsResolutionSupported_ResnNotInEdidList_ReturnsFalse)
{
    dsDisplayEDID_t edid;
    memset(&edid, 0, sizeof(edid));
    strncpy(edid.suppResolutionList[0].name, "1080p60", 31);

    int index = -1;
    EXPECT_FALSE(isResolutionSupported(&edid, 1, 0, "720p", &index));
}

/* =======================================================================
 * Section 19b – _SetResolution(): branch coverage
 *
 * g_stub_edid_* globals let tests control what _dsGetEDID fills into
 * the EDID struct.  dsGetDisplay() is provided by DsHalMock.
 * dumpEdidOnChecksumDiff(NULL) — called unconditionally inside
 * _SetResolution — also calls dsGetDisplay; hence tests that need a
 * non-zero _displayHandle use WillRepeatedly for dsGetDisplay so both
 * calls (in dumpEdid and in the HDMI branch) get the same stub value.
 * ===================================================================== */

/* 19b-1: NULL handle → malloc'd edidData freed, early return, no crash. */
TEST_F(DsMgrTest, SetResolution_NullHandle_ReturnsEarly)
{
    intptr_t handle = 0;  /* NULL */
    EXPECT_EQ(_SetResolution(&handle, dsVIDEOPORT_TYPE_COMPONENT), 0);
}

/* 19b-2: COMPONENT port + persisted resolution name doesn't match any
 * platform resolution entry (stub returns ""); falls back to the default
 * resolution index from _dsGetDefaultResolutionIndex → _dsSetResolution. */
TEST_F(DsMgrTest, SetResolution_ComponentNoMatch_UsesFallback)
{
    static char sentinel[4];
    intptr_t handle = reinterpret_cast<intptr_t>(sentinel);
    /* _dsGetResolution stub: returns empty name "" → no match in platform
     * resolution list → IsValidResolution=false → default resolution used.
     * _dsSetResolution stub absorbs the call safely.                     */
    EXPECT_EQ(_SetResolution(&handle, dsVIDEOPORT_TYPE_COMPONENT), 0);
}

/* 19b-3: BB port → identical COMPONENT/BB/RF code path; no crash. */
TEST_F(DsMgrTest, SetResolution_BbPort_UsesFallback)
{
    static char sentinel[4];
    intptr_t handle = reinterpret_cast<intptr_t>(sentinel);
    EXPECT_EQ(_SetResolution(&handle, dsVIDEOPORT_TYPE_BB), 0);
}

/* 19b-4: RF port → identical path; no crash. */
TEST_F(DsMgrTest, SetResolution_RfPort_UsesFallback)
{
    static char sentinel[4];
    intptr_t handle = reinterpret_cast<intptr_t>(sentinel);
    EXPECT_EQ(_SetResolution(&handle, dsVIDEOPORT_TYPE_RF), 0);
}

/* 19b-5: HDMI + dsGetDisplay returns 0 → _displayHandle=0 →
 * skips EDID block → IsValidResolution=false → kDefaultResIndex. */
TEST_F(DsMgrTest, SetResolution_HdmiDisplayHandleZero_UsesFallback)
{
    static char sentinel[4];
    intptr_t handle = reinterpret_cast<intptr_t>(sentinel);
    /* NiceMock default: dsGetDisplay doesn't set out-param → stays 0. */
    EXPECT_EQ(_SetResolution(&handle, dsVIDEOPORT_TYPE_HDMI), 0);
}

/* 19b-6: HDMI + non-zero displayHandle + numResolutions=0 → early return
 * via "(0 == numResolutions) || (!hdmiDeviceType)" guard.             */
TEST_F(DsMgrTest, SetResolution_HdmiZeroNumResolutions_EarlyReturn)
{
    static char sentinel[4];
    intptr_t handle = reinterpret_cast<intptr_t>(sentinel);
    /* Both calls to dsGetDisplay (in dumpEdidOnChecksumDiff and in the
     * HDMI branch) must return the same non-zero handle.               */
    EXPECT_CALL(dsHalMock, dsGetDisplay(dsVIDEOPORT_TYPE_HDMI, 0, _))
        .WillRepeatedly(::testing::DoAll(
            ::testing::SetArgPointee<2>(static_cast<intptr_t>(0x1000)),
            Return(dsERR_NONE)));
    /* NiceMock default: dsGetEDID leaves struct zeroed →
     * numOfSupportedResolution = 0 → early return.                    */
    EXPECT_EQ(_SetResolution(&handle, dsVIDEOPORT_TYPE_HDMI), 0);
}

/* 19b-7: HDMI + non-zero displayHandle + numResolutions>0 +
 * hdmiDeviceType=false → DVI device → early return.                  */
TEST_F(DsMgrTest, SetResolution_HdmiDviDevice_EarlyReturn)
{
    static char sentinel[4];
    intptr_t handle = reinterpret_cast<intptr_t>(sentinel);
    EXPECT_CALL(dsHalMock, dsGetDisplay(dsVIDEOPORT_TYPE_HDMI, 0, _))
        .WillRepeatedly(::testing::DoAll(
            ::testing::SetArgPointee<2>(static_cast<intptr_t>(0x1000)),
            Return(dsERR_NONE)));
    ON_CALL(dsHalMock, dsGetEDID(_))
        .WillByDefault(Invoke([](dsDisplayGetEDIDParam_t *p) {
            p->edid.numOfSupportedResolution = 1;
            p->edid.hdmiDeviceType           = false;
            strncpy(p->edid.suppResolutionList[0].name, "720p",
                    sizeof(p->edid.suppResolutionList[0].name) - 1);
            return IARM_RESULT_SUCCESS;
        }));

    EXPECT_EQ(_SetResolution(&handle, dsVIDEOPORT_TYPE_HDMI), 0);
}

/* 19b-8: HDMI + non-zero displayHandle + numResolutions=1 +
 * hdmiDeviceType=true + EDID has "720p".
 * Persisted resolution "" has no match; default "720p" found in EDID;
 * IsValidResolution=true → _dsSetResolution called.                   */
TEST_F(DsMgrTest, SetResolution_HdmiValidEdid_DefaultResolutionMatch)
{
    static char sentinel[4];
    intptr_t handle = reinterpret_cast<intptr_t>(sentinel);
    EXPECT_CALL(dsHalMock, dsGetDisplay(dsVIDEOPORT_TYPE_HDMI, 0, _))
        .WillRepeatedly(::testing::DoAll(
            ::testing::SetArgPointee<2>(static_cast<intptr_t>(0x1000)),
            Return(dsERR_NONE)));
    ON_CALL(dsHalMock, dsGetEDID(_))
        .WillByDefault(Invoke([](dsDisplayGetEDIDParam_t *p) {
            p->edid.numOfSupportedResolution = 1;
            p->edid.hdmiDeviceType           = true;
            strncpy(p->edid.suppResolutionList[0].name, "720p",
                    sizeof(p->edid.suppResolutionList[0].name) - 1);
            return IARM_RESULT_SUCCESS;
        }));

    EXPECT_EQ(_SetResolution(&handle, dsVIDEOPORT_TYPE_HDMI), 0);
}

/* =======================================================================
 * Section 19c – _DSMgrResnThreadFunc(): branch coverage
 *
 * __real_pthread_create bypasses __wrap_pthread_create so a real thread
 * is spawned.  A wrapper function installs a cleanup handler that
 * unlocks tdsMutexLock on cancellation (pthread_cond_wait re-locks the
 * mutex before invoking cleanup, so this prevents mutex corruption on
 * TearDown).  nanosleep() is used instead of usleep() (which is wrapped
 * to a no-op) to provide real delays for thread synchronisation.
 * ===================================================================== */

namespace {
    static void tdsMutexUnlockCleanup(void *arg)
    {
        pthread_mutex_unlock(static_cast<pthread_mutex_t *>(arg));
    }

    /* Wrapper: installs a cleanup handler so mutex is unlocked if the
     * thread is cancelled while inside pthread_cond_wait.              */
    static void *resnThreadWrapper(void *)
    {
        pthread_cleanup_push(tdsMutexUnlockCleanup, &tdsMutexLock);
        _DSMgrResnThreadFunc(nullptr);
        pthread_cleanup_pop(0);
        return nullptr;
    }

    static void realSleepMs(long ms)
    {
        struct timespec ts;
        ts.tv_sec  = ms / 1000;
        ts.tv_nsec = (ms % 1000) * 1000000L;
        nanosleep(&ts, nullptr);
    }
}

/* 19c-1: CONNECTED + iTuneReady=1 + bHDCPAuthenticated=true
 * → _SetVideoPortResolution() called; hotplug_event_src not set.      */
TEST_F(DsMgrTest, ResnThread_ConnectedTuneHDCP_SetsResolution)
{
    iTuneReady         = 1;
    bHDCPAuthenticated = true;
    hotplug_event_src  = 0;

    pthread_t t;
    __real_pthread_create(&t, nullptr, resnThreadWrapper, nullptr);
    realSleepMs(30);   /* let thread enter pthread_cond_wait */

    pthread_mutex_lock(&tdsMutexLock);
    edisplayEventStatus = dsDISPLAY_EVENT_CONNECTED;
    pthread_cond_signal(&tdsMutexCond);
    pthread_mutex_unlock(&tdsMutexLock);
    realSleepMs(50);   /* let thread process and re-enter cond_wait */

    pthread_cancel(t);
    pthread_join(t, nullptr);

    /* Thread called _SetVideoPortResolution (no crash, all handles=0) +
     * _setAudioMode (EAS=NORMAL, port stubs ok).  hotplug_event_src was
     * cleared (it was 0) and not set on connected path.               */
    EXPECT_EQ(hotplug_event_src, static_cast<guint>(0));
}

/* 19c-2: DISCONNECTED + iTuneReady=1 + isComponentPortPresent=true
 * → bHDCPAuthenticated cleared + hotplug_event_src set to 1 by
 * __wrap_g_timeout_add_seconds.                                        */
TEST_F(DsMgrTest, ResnThread_DisconnectedTuneReady_SetsHotplugTimer)
{
    iTuneReady         = 1;
    bHDCPAuthenticated = true;
    hotplug_event_src  = 0;

    pthread_t t;
    __real_pthread_create(&t, nullptr, resnThreadWrapper, nullptr);
    realSleepMs(30);

    pthread_mutex_lock(&tdsMutexLock);
    edisplayEventStatus = dsDISPLAY_EVENT_DISCONNECTED;
    pthread_cond_signal(&tdsMutexCond);
    pthread_mutex_unlock(&tdsMutexLock);
    realSleepMs(50);

    pthread_cancel(t);
    pthread_join(t, nullptr);

    EXPECT_FALSE(bHDCPAuthenticated);
    /* isComponentPortPresent() stub → true → g_timeout_add_seconds
     * wrapped → returns 1 → hotplug_event_src = 1.                    */
    EXPECT_EQ(hotplug_event_src, static_cast<guint>(1));
}

/* 19c-3: DISCONNECTED + iTuneReady=0 → both conditions false → no action;
 * bHDCPAuthenticated and hotplug_event_src unchanged.                  */
TEST_F(DsMgrTest, ResnThread_DisconnectedTuneNotReady_NoAction)
{
    iTuneReady         = 0;
    bHDCPAuthenticated = true;
    hotplug_event_src  = 0;

    pthread_t t;
    __real_pthread_create(&t, nullptr, resnThreadWrapper, nullptr);
    realSleepMs(30);

    pthread_mutex_lock(&tdsMutexLock);
    edisplayEventStatus = dsDISPLAY_EVENT_DISCONNECTED;
    pthread_cond_signal(&tdsMutexCond);
    pthread_mutex_unlock(&tdsMutexLock);
    realSleepMs(50);

    pthread_cancel(t);
    pthread_join(t, nullptr);

    EXPECT_TRUE(bHDCPAuthenticated);   /* unchanged */
    EXPECT_EQ(hotplug_event_src, static_cast<guint>(0));
}

/* =======================================================================
 * Section 19d – _setAudioMode() / _setEASAudioMode(): full port loop
 *
 * kSupportedPortTypes = { dsAUDIOPORT_TYPE_HDMI_ARC, dsAUDIOPORT_TYPE_SPDIF }
 * Neither entry matches dsAUDIOPORT_TYPE_HDMI so the HDMI-specific block
 * inside _setAudioMode is dead for this platform stub; only the base path
 * (_dsGetAudioPort + _dsGetStereoMode + _dsSetStereoMode per port) runs.
 * ===================================================================== */

/* 19d-1: _setAudioMode with EAS=NORMAL → full port loop; no crash. */
TEST_F(DsMgrTest, SetAudioMode_NormalEas_FullPortLoop)
{
    isEAS = IARM_BUS_SYS_MODE_NORMAL;
    /* Iterates HDMI_ARC + SPDIF ports; all DS HAL stubs are no-ops. */
    _setAudioMode();
}

/* 19d-2: _setAudioMode with HDMI video handle set and display connected;
 * kSupportedPortTypes doesn't include HDMI so HDMI logic is not reached,
 * but the test exercises the video-port state with non-default globals. */
TEST_F(DsMgrTest, SetAudioMode_HdmiHandleConnected_NormalEas)
{
    isEAS = IARM_BUS_SYS_MODE_NORMAL;
    ON_CALL(dsHalMock, dsGetVideoPort(_))
        .WillByDefault(Invoke([](dsVideoPortGetHandleParam_t *p) {
            if (p->type == dsVIDEOPORT_TYPE_HDMI) p->handle = STUB_HANDLE(0);
            return IARM_RESULT_SUCCESS;
        }));
    ON_CALL(dsHalMock, dsIsDisplayConnected(_))
        .WillByDefault(Invoke([](dsVideoPortIsDisplayConnectedParam_t *p) {
            p->connected = true;
            return IARM_RESULT_SUCCESS;
        }));
    _setAudioMode();
}

/* 19d-3: _setEASAudioMode EAS path — PASSTHRU mode → set to STEREO. */
TEST_F(DsMgrTest, SetEasAudioMode_PassthruMode_FallsBackToStereo)
{
    isEAS = IARM_BUS_SYS_MODE_EAS;
    ON_CALL(dsHalMock, dsGetStereoMode(_))
        .WillByDefault(Invoke([](dsAudioSetStereoModeParam_t *p) {
            p->mode = dsAUDIO_STEREO_PASSTHRU;
            return IARM_RESULT_SUCCESS;
        }));
    /* _dsGetStereoMode mock fills PASSTHRU → code overrides to STEREO →
     * _dsSetStereoMode stub absorbs the call; no crash expected.       */
    _setEASAudioMode();
}

/* 19d-4: _setEASAudioMode EAS path — non-PASSTHRU mode → unchanged. */
TEST_F(DsMgrTest, SetEasAudioMode_SurroundMode_Unchanged)
{
    isEAS = IARM_BUS_SYS_MODE_EAS;
    ON_CALL(dsHalMock, dsGetStereoMode(_))
        .WillByDefault(Invoke([](dsAudioSetStereoModeParam_t *p) {
            p->mode = dsAUDIO_STEREO_SURROUND;
            return IARM_RESULT_SUCCESS;
        }));
    _setEASAudioMode();
}

/* =======================================================================
 * Section 19e – _SetResolution(): full branch coverage
 *
 * Root cause of prior zero-coverage: NiceMock<DsHalMock> default for
 * dsGetVideoPortResolutions leaves outSize=0 and outRes=nullptr, which
 * triggers the guard at line 763-766 (resolutionsSize<=0 || pResolutions==NULL)
 * on every call, making all downstream code unreachable.
 *
 * Every test below either:
 *   (a) explicitly tests one of the early-exit guards, or
 *   (b) uses setupValidPlatformResolutions() to supply a non-empty platform
 *       resolution table so the code advances past the guards.
 *
 * Platform resolution table used by (b) tests:
 *   index 0 → "480p"  (also used as NTSC fallback)
 *   index 1 → "720p"  (default)
 *   index 2 → "1080p"
 *   index 3 → "1080p50" (framerate-qualified; needed for EU fps tests)
 *
 * fallBackResolutionList is populated by calling setupPlatformConfig() with
 * wrapsMock set to return nullptr for fopen (IsEUPlatform=false), giving:
 *   ["2160p","1080p","1080i","720p","480p",""] (576p excluded for non-EU).
 * Tests that need IsEUPlatform=true set the global after the call.
 * ===================================================================== */

namespace {

/* Shared platform-resolution table; sized to hold up to 4 entries.     */
static dsVideoPortResolution_t g_testPlatformRes[4];

/* Populate g_testPlatformRes and install ON_CALLs for the two mandatory
 * mock methods.  defaultIdx selects which entry is the platform default.*/
static void setupValidPlatformResolutions(
        ::testing::NiceMock<DsHalMock> &mock,
        int defaultIdx = 1,
        int numRes     = 3)
{
    memset(g_testPlatformRes, 0, sizeof(g_testPlatformRes));
    strncpy(g_testPlatformRes[0].name, "480p",   sizeof(g_testPlatformRes[0].name)-1);
    strncpy(g_testPlatformRes[1].name, "720p",   sizeof(g_testPlatformRes[1].name)-1);
    strncpy(g_testPlatformRes[2].name, "1080p",  sizeof(g_testPlatformRes[2].name)-1);
    strncpy(g_testPlatformRes[3].name, "1080p50",sizeof(g_testPlatformRes[3].name)-1);

    ON_CALL(mock, dsGetVideoPortResolutions(_, _))
        .WillByDefault(Invoke([numRes](int *sz, dsVideoPortResolution_t **res) {
            *sz  = numRes;
            *res = g_testPlatformRes;
            return dsERR_NONE;
        }));
    ON_CALL(mock, dsGetDefaultResolutionIndex(_))
        .WillByDefault(Invoke([defaultIdx](int *idx) {
            *idx = defaultIdx;
            return dsERR_NONE;
        }));
}

} /* anonymous namespace */

/* ----------------------------------------------------------------------- *
 * 19e-1: _dsGetVideoPortResolutions returns an error code →              *
 *        early return at the very first guard (line 758-760).            *
 * ----------------------------------------------------------------------- */
TEST_F(DsMgrTest, SetResolution_GetResolutionsFails_EarlyReturn)
{
    ON_CALL(dsHalMock, dsGetVideoPortResolutions(_, _))
        .WillByDefault(Return(dsERR_GENERAL));
    static char sentinel[4];
    intptr_t handle = reinterpret_cast<intptr_t>(sentinel);
    EXPECT_EQ(_SetResolution(&handle, dsVIDEOPORT_TYPE_COMPONENT), 0);
}

/* ----------------------------------------------------------------------- *
 * 19e-2: _dsGetVideoPortResolutions succeeds but sets outSize=0 →        *
 *        guard at line 763-766 (resolutionsSize<=0 || pResolutions==NULL) *
 *        fires and returns early.                                         *
 * ----------------------------------------------------------------------- */
TEST_F(DsMgrTest, SetResolution_ZeroResolutionsSize_EarlyReturn)
{
    ON_CALL(dsHalMock, dsGetVideoPortResolutions(_, _))
        .WillByDefault(Invoke([](int *sz, dsVideoPortResolution_t **res) {
            *sz  = 0;
            *res = nullptr;
            return dsERR_NONE;
        }));
    static char sentinel[4];
    intptr_t handle = reinterpret_cast<intptr_t>(sentinel);
    EXPECT_EQ(_SetResolution(&handle, dsVIDEOPORT_TYPE_COMPONENT), 0);
}

/* ----------------------------------------------------------------------- *
 * 19e-3: _dsGetDefaultResolutionIndex returns an error →                 *
 *        early return at line 770-772.                                    *
 * ----------------------------------------------------------------------- */
TEST_F(DsMgrTest, SetResolution_GetDefaultIndexFails_EarlyReturn)
{
    /* Provide valid resolutions so the code can reach the next guard.  */
    setupValidPlatformResolutions(dsHalMock);
    ON_CALL(dsHalMock, dsGetDefaultResolutionIndex(_))
        .WillByDefault(Return(dsERR_GENERAL));
    static char sentinel[4];
    intptr_t handle = reinterpret_cast<intptr_t>(sentinel);
    EXPECT_EQ(_SetResolution(&handle, dsVIDEOPORT_TYPE_COMPONENT), 0);
}

/* ----------------------------------------------------------------------- *
 * 19e-4: _dsGetDefaultResolutionIndex returns an index >= resolutionsSize *
 *        → guard at line 775-778 fires and returns early.                *
 * ----------------------------------------------------------------------- */
TEST_F(DsMgrTest, SetResolution_DefaultIndexOutOfRange_EarlyReturn)
{
    setupValidPlatformResolutions(dsHalMock);
    ON_CALL(dsHalMock, dsGetDefaultResolutionIndex(_))
        .WillByDefault(Invoke([](int *idx) {
            *idx = 99;  /* > resolutionsSize (3) */
            return dsERR_NONE;
        }));
    static char sentinel[4];
    intptr_t handle = reinterpret_cast<intptr_t>(sentinel);
    EXPECT_EQ(_SetResolution(&handle, dsVIDEOPORT_TYPE_COMPONENT), 0);
}

/* ----------------------------------------------------------------------- *
 * 19e-5: NULL handle, but resolution data is valid → edidData allocated  *
 *        then freed at the NULL-handle guard (line 797-801).             *
 *        (Previous 19b-1 never reached this guard; it hit line 764.)    *
 * ----------------------------------------------------------------------- */
TEST_F(DsMgrTest, SetResolution_NullHandleAfterValidResolutions_EarlyReturn)
{
    setupValidPlatformResolutions(dsHalMock);
    intptr_t handle = 0;  /* NULL */
    EXPECT_EQ(_SetResolution(&handle, dsVIDEOPORT_TYPE_COMPONENT), 0);
}

/* ----------------------------------------------------------------------- *
 * 19e-6: bootup_flag_enabled=false + dumpEdidOnChecksumDiff returns false *
 *        (dsGetDisplay returns 0 → no checksum diff) → toPersist=false   *
 *        branch (line 811) + platform-resolution log (line 818).         *
 * ----------------------------------------------------------------------- */
TEST_F(DsMgrTest, SetResolution_BootupDisabled_PersistFalse)
{
    setupValidPlatformResolutions(dsHalMock);
    bootup_flag_enabled = false;
    /* dsGetDisplay NiceMock default: returns handle=0 → no EDID diff.  */
    static char sentinel[4];
    intptr_t handle = reinterpret_cast<intptr_t>(sentinel);
    EXPECT_EQ(_SetResolution(&handle, dsVIDEOPORT_TYPE_COMPONENT), 0);
}

/* ----------------------------------------------------------------------- *
 * 19e-7: bootup_flag_enabled=true → toPersist=true branch (line 809) +  *
 *        persisted-resolution log (line 816).                            *
 * ----------------------------------------------------------------------- */
TEST_F(DsMgrTest, SetResolution_BootupEnabled_PersistTrue)
{
    setupValidPlatformResolutions(dsHalMock);
    bootup_flag_enabled = true;
    static char sentinel[4];
    intptr_t handle = reinterpret_cast<intptr_t>(sentinel);
    EXPECT_EQ(_SetResolution(&handle, dsVIDEOPORT_TYPE_COMPONENT), 0);
}

/* ----------------------------------------------------------------------- *
 * 19e-8: COMPONENT port + persisted name "720p" matches g_testPlatformRes *
 *        → IsValidResolution=true via the COMPONENT for-loop (line 1000-  *
 *        1007) → _dsSetResolution called on the success path.            *
 * ----------------------------------------------------------------------- */
TEST_F(DsMgrTest, SetResolution_ComponentPersistMatchesPlatform_SetsResolution)
{
    setupValidPlatformResolutions(dsHalMock);
    ON_CALL(dsHalMock, dsGetResolution(_))
        .WillByDefault(Invoke([](dsVideoPortGetResolutionParam_t *p) {
            strncpy(p->resolution.name, "720p",
                    sizeof(p->resolution.name)-1);
            return IARM_RESULT_SUCCESS;
        }));
    static char sentinel[4];
    intptr_t handle = reinterpret_cast<intptr_t>(sentinel);
    EXPECT_EQ(_SetResolution(&handle, dsVIDEOPORT_TYPE_COMPONENT), 0);
}

/* ----------------------------------------------------------------------- *
 * 19e-9: BB port + persisted name "480p" matches first platform entry →  *
 *        IsValidResolution=true via the COMPONENT/BB/RF for-loop.        *
 * ----------------------------------------------------------------------- */
TEST_F(DsMgrTest, SetResolution_BbPortPersistMatchesPlatform_SetsResolution)
{
    setupValidPlatformResolutions(dsHalMock);
    ON_CALL(dsHalMock, dsGetResolution(_))
        .WillByDefault(Invoke([](dsVideoPortGetResolutionParam_t *p) {
            strncpy(p->resolution.name, "480p",
                    sizeof(p->resolution.name)-1);
            return IARM_RESULT_SUCCESS;
        }));
    static char sentinel[4];
    intptr_t handle = reinterpret_cast<intptr_t>(sentinel);
    EXPECT_EQ(_SetResolution(&handle, dsVIDEOPORT_TYPE_BB), 0);
}

/* ----------------------------------------------------------------------- *
 * 19e-10: RF port + persisted name "1080p" matches third platform entry → *
 *         IsValidResolution=true via the COMPONENT/BB/RF for-loop.       *
 * ----------------------------------------------------------------------- */
TEST_F(DsMgrTest, SetResolution_RfPortPersistMatchesPlatform_SetsResolution)
{
    setupValidPlatformResolutions(dsHalMock);
    ON_CALL(dsHalMock, dsGetResolution(_))
        .WillByDefault(Invoke([](dsVideoPortGetResolutionParam_t *p) {
            strncpy(p->resolution.name, "1080p",
                    sizeof(p->resolution.name)-1);
            return IARM_RESULT_SUCCESS;
        }));
    static char sentinel[4];
    intptr_t handle = reinterpret_cast<intptr_t>(sentinel);
    EXPECT_EQ(_SetResolution(&handle, dsVIDEOPORT_TYPE_RF), 0);
}

/* ----------------------------------------------------------------------- *
 * 19e-11: COMPONENT port + persisted name not in platform table →        *
 *         IsValidResolution=false → fallback to default (line 1016-1018) *
 *         → _dsSetResolution called with default entry.                  *
 * ----------------------------------------------------------------------- */
TEST_F(DsMgrTest, SetResolution_ComponentPersistNoMatch_FallsBackToDefault)
{
    setupValidPlatformResolutions(dsHalMock);
    /* NiceMock default for dsGetResolution leaves name="" → no match.  */
    static char sentinel[4];
    intptr_t handle = reinterpret_cast<intptr_t>(sentinel);
    EXPECT_EQ(_SetResolution(&handle, dsVIDEOPORT_TYPE_COMPONENT), 0);
}

/* ----------------------------------------------------------------------- *
 * 19e-12: HDMI + valid resolution data + dsGetDisplay returns handle=0  *
 *         (NiceMock default) → EDID block is skipped entirely →          *
 *         IsValidResolution=false → _dsSetResolution with default entry. *
 *         (Previous 19b-5 never reached line 821; it hit line 764.)      *
 * ----------------------------------------------------------------------- */
TEST_F(DsMgrTest, SetResolution_HdmiDisplayHandleZeroValidResns_UsesFallback)
{
    setupValidPlatformResolutions(dsHalMock);
    /* dsGetDisplay NiceMock default: leaves handle=0.                   */
    static char sentinel[4];
    intptr_t handle = reinterpret_cast<intptr_t>(sentinel);
    EXPECT_EQ(_SetResolution(&handle, dsVIDEOPORT_TYPE_HDMI), 0);
}

/* ----------------------------------------------------------------------- *
 * 19e-13: HDMI + non-zero display handle + numOfSupportedResolution=0 →  *
 *         early return at the "(0==numResolutions)||(!hdmiDeviceType)"    *
 *         guard (lines 852-859). Edidparam and edidData are freed.       *
 *         (Previous 19b-6 never reached line 823; it hit line 764.)      *
 * ----------------------------------------------------------------------- */
TEST_F(DsMgrTest, SetResolution_HdmiZeroNumResolutionsValidResns_EarlyReturn)
{
    setupValidPlatformResolutions(dsHalMock);
    EXPECT_CALL(dsHalMock, dsGetDisplay(dsVIDEOPORT_TYPE_HDMI, 0, _))
        .WillRepeatedly(::testing::DoAll(
            ::testing::SetArgPointee<2>(static_cast<intptr_t>(0x1000)),
            Return(dsERR_NONE)));
    /* NiceMock default for dsGetEDID leaves numOfSupportedResolution=0. */
    static char sentinel[4];
    intptr_t handle = reinterpret_cast<intptr_t>(sentinel);
    EXPECT_EQ(_SetResolution(&handle, dsVIDEOPORT_TYPE_HDMI), 0);
}

/* ----------------------------------------------------------------------- *
 * 19e-14: HDMI + non-zero display handle + numResolutions>0 +            *
 *         hdmiDeviceType=false (DVI device) →                            *
 *         early return at the "!hdmiDeviceType" condition.               *
 *         (Previous 19b-7 never reached line 823; it hit line 764.)      *
 * ----------------------------------------------------------------------- */
TEST_F(DsMgrTest, SetResolution_HdmiDviDeviceValidResns_EarlyReturn)
{
    setupValidPlatformResolutions(dsHalMock);
    EXPECT_CALL(dsHalMock, dsGetDisplay(dsVIDEOPORT_TYPE_HDMI, 0, _))
        .WillRepeatedly(::testing::DoAll(
            ::testing::SetArgPointee<2>(static_cast<intptr_t>(0x1000)),
            Return(dsERR_NONE)));
    ON_CALL(dsHalMock, dsGetEDID(_))
        .WillByDefault(Invoke([](dsDisplayGetEDIDParam_t *p) {
            p->edid.numOfSupportedResolution = 1;
            p->edid.hdmiDeviceType           = false;  /* DVI */
            strncpy(p->edid.suppResolutionList[0].name, "720p",
                    sizeof(p->edid.suppResolutionList[0].name)-1);
            return IARM_RESULT_SUCCESS;
        }));
    static char sentinel[4];
    intptr_t handle = reinterpret_cast<intptr_t>(sentinel);
    EXPECT_EQ(_SetResolution(&handle, dsVIDEOPORT_TYPE_HDMI), 0);
}

/* ----------------------------------------------------------------------- *
 * 19e-15: HDMI + persisted "720p" matches EDID entry "720p" →           *
 *         IsValidResolution=true via the direct name-match loop          *
 *         (lines 866-875); Setparam.forceCompatible set to true;         *
 *         _dsSetResolution called.                                        *
 * ----------------------------------------------------------------------- */
TEST_F(DsMgrTest, SetResolution_HdmiPersistedMatchesEdid_SetsResolution)
{
    setupValidPlatformResolutions(dsHalMock);
    EXPECT_CALL(dsHalMock, dsGetDisplay(dsVIDEOPORT_TYPE_HDMI, 0, _))
        .WillRepeatedly(::testing::DoAll(
            ::testing::SetArgPointee<2>(static_cast<intptr_t>(0x1000)),
            Return(dsERR_NONE)));
    ON_CALL(dsHalMock, dsGetEDID(_))
        .WillByDefault(Invoke([](dsDisplayGetEDIDParam_t *p) {
            p->edid.numOfSupportedResolution = 2;
            p->edid.hdmiDeviceType           = true;
            strncpy(p->edid.suppResolutionList[0].name, "1080p",
                    sizeof(p->edid.suppResolutionList[0].name)-1);
            strncpy(p->edid.suppResolutionList[1].name, "720p",
                    sizeof(p->edid.suppResolutionList[1].name)-1);
            return IARM_RESULT_SUCCESS;
        }));
    ON_CALL(dsHalMock, dsGetResolution(_))
        .WillByDefault(Invoke([](dsVideoPortGetResolutionParam_t *p) {
            strncpy(p->resolution.name, "720p",
                    sizeof(p->resolution.name)-1);
            return IARM_RESULT_SUCCESS;
        }));
    static char sentinel[4];
    intptr_t handle = reinterpret_cast<intptr_t>(sentinel);
    EXPECT_EQ(_SetResolution(&handle, dsVIDEOPORT_TYPE_HDMI), 0);
}

/* ----------------------------------------------------------------------- *
 * 19e-16: HDMI + persisted "2160p50" not in EDID (direct match fails) +  *
 *         IsEUPlatform=false → EU block skipped; fallback resolution      *
 *         loop: base "2160p" found in fallBackResolutionList; next entry  *
 *         "1080p" → getFallBackResolution("1080p",...,0) → "1080p60";    *
 *         EDID has "1080p60" AND g_testPlatformRes has "1080p" (exact    *
 *         match inside isResolutionSupported fails here); but "1080p60"   *
 *         is in EDID AND added to platform table → IsValidResolution=true *
 *         via fallback fps path (lines 899-935).                          *
 *                                                                         *
 * g_testPlatformRes is extended to 4 entries to include "1080p60" so     *
 * isResolutionSupported can find a cross-match between EDID and platform. *
 * ----------------------------------------------------------------------- */
TEST_F(DsMgrTest, SetResolution_HdmiFallbackResolutionFound)
{
    /* Extend platform table to include the fps-qualified entry.          */
    memset(g_testPlatformRes, 0, sizeof(g_testPlatformRes));
    strncpy(g_testPlatformRes[0].name, "480p",  sizeof(g_testPlatformRes[0].name)-1);
    strncpy(g_testPlatformRes[1].name, "720p",  sizeof(g_testPlatformRes[1].name)-1);
    strncpy(g_testPlatformRes[2].name, "1080p", sizeof(g_testPlatformRes[2].name)-1);
    strncpy(g_testPlatformRes[3].name, "1080p60",sizeof(g_testPlatformRes[3].name)-1);
    ON_CALL(dsHalMock, dsGetVideoPortResolutions(_, _))
        .WillByDefault(Invoke([](int *sz, dsVideoPortResolution_t **res) {
            *sz  = 4;
            *res = g_testPlatformRes;
            return dsERR_NONE;
        }));
    ON_CALL(dsHalMock, dsGetDefaultResolutionIndex(_))
        .WillByDefault(Invoke([](int *idx) {
            *idx = 1;  /* "720p" */
            return dsERR_NONE;
        }));

    /* Build fallBackResolutionList: ["2160p","1080p","1080i","720p","480p",""] */
    setupPlatformConfig();  /* uses wrapsMock fopen→nullptr → non-EU list */

    IsEUPlatform = false;
    EXPECT_CALL(dsHalMock, dsGetDisplay(dsVIDEOPORT_TYPE_HDMI, 0, _))
        .WillRepeatedly(::testing::DoAll(
            ::testing::SetArgPointee<2>(static_cast<intptr_t>(0x1000)),
            Return(dsERR_NONE)));
    ON_CALL(dsHalMock, dsGetEDID(_))
        .WillByDefault(Invoke([](dsDisplayGetEDIDParam_t *p) {
            p->edid.numOfSupportedResolution = 1;
            p->edid.hdmiDeviceType           = true;
            /* getFallBackResolution("1080p",...,0) → "1080p60"          */
            strncpy(p->edid.suppResolutionList[0].name, "1080p60",
                    sizeof(p->edid.suppResolutionList[0].name)-1);
            return IARM_RESULT_SUCCESS;
        }));
    ON_CALL(dsHalMock, dsGetResolution(_))
        .WillByDefault(Invoke([](dsVideoPortGetResolutionParam_t *p) {
            /* "2160p50" → parseResolution → base "2160p" is index 0 in
             * fallBackResolutionList; next entry "1080p" generates
             * fallback "1080p60" which is in both EDID and platform.    */
            strncpy(p->resolution.name, "2160p50",
                    sizeof(p->resolution.name)-1);
            return IARM_RESULT_SUCCESS;
        }));
    static char sentinel[4];
    intptr_t handle = reinterpret_cast<intptr_t>(sentinel);
    EXPECT_EQ(_SetResolution(&handle, dsVIDEOPORT_TYPE_HDMI), 0);
}

/* ----------------------------------------------------------------------- *
 * 19e-17: HDMI + persisted "576p" not in EDID, not in fallback list,     *
 *         not via EU secondary; default platform entry "720p" IS in EDID *
 *         → IsValidResolution=true via the default-resolution check      *
 *         (lines 945-957).                                                *
 * ----------------------------------------------------------------------- */
TEST_F(DsMgrTest, SetResolution_HdmiDefaultPlatformResMatchesEdid)
{
    setupValidPlatformResolutions(dsHalMock, /*defaultIdx=*/1);
    setupPlatformConfig();  /* populate fallBackResolutionList (non-EU) */
    IsEUPlatform = false;
    EXPECT_CALL(dsHalMock, dsGetDisplay(dsVIDEOPORT_TYPE_HDMI, 0, _))
        .WillRepeatedly(::testing::DoAll(
            ::testing::SetArgPointee<2>(static_cast<intptr_t>(0x1000)),
            Return(dsERR_NONE)));
    ON_CALL(dsHalMock, dsGetEDID(_))
        .WillByDefault(Invoke([](dsDisplayGetEDIDParam_t *p) {
            p->edid.numOfSupportedResolution = 1;
            p->edid.hdmiDeviceType           = true;
            /* "720p" == defaultResn (g_testPlatformRes[1]) → matched at
             * the default-platform-resolution check.                    */
            strncpy(p->edid.suppResolutionList[0].name, "720p",
                    sizeof(p->edid.suppResolutionList[0].name)-1);
            return IARM_RESULT_SUCCESS;
        }));
    ON_CALL(dsHalMock, dsGetResolution(_))
        .WillByDefault(Invoke([](dsVideoPortGetResolutionParam_t *p) {
            /* "576p" not in EDID, not in fallback list for non-EU.      */
            strncpy(p->resolution.name, "576p",
                    sizeof(p->resolution.name)-1);
            return IARM_RESULT_SUCCESS;
        }));
    static char sentinel[4];
    intptr_t handle = reinterpret_cast<intptr_t>(sentinel);
    EXPECT_EQ(_SetResolution(&handle, dsVIDEOPORT_TYPE_HDMI), 0);
}

/* ----------------------------------------------------------------------- *
 * 19e-18: HDMI + persisted "576p" + default "480p" NOT in EDID + EDID   *
 *         has "480p" → 480p fallback path (lines 962-972).               *
 *         default set to index 2 ("1080p") so that "1080p" is not in     *
 *         EDID, forcing the code into the 480p check.                    *
 * ----------------------------------------------------------------------- */
TEST_F(DsMgrTest, SetResolution_HdmiNoDefaultInEdid_480pFallback)
{
    setupValidPlatformResolutions(dsHalMock, /*defaultIdx=*/2);
    setupPlatformConfig();
    IsEUPlatform = false;
    EXPECT_CALL(dsHalMock, dsGetDisplay(dsVIDEOPORT_TYPE_HDMI, 0, _))
        .WillRepeatedly(::testing::DoAll(
            ::testing::SetArgPointee<2>(static_cast<intptr_t>(0x1000)),
            Return(dsERR_NONE)));
    ON_CALL(dsHalMock, dsGetEDID(_))
        .WillByDefault(Invoke([](dsDisplayGetEDIDParam_t *p) {
            p->edid.numOfSupportedResolution = 1;
            p->edid.hdmiDeviceType           = true;
            /* Only "480p" in EDID; default "1080p" is absent, so the
             * default-resolution check fails and the 480p check fires.  */
            strncpy(p->edid.suppResolutionList[0].name, "480p",
                    sizeof(p->edid.suppResolutionList[0].name)-1);
            return IARM_RESULT_SUCCESS;
        }));
    ON_CALL(dsHalMock, dsGetResolution(_))
        .WillByDefault(Invoke([](dsVideoPortGetResolutionParam_t *p) {
            strncpy(p->resolution.name, "576p",
                    sizeof(p->resolution.name)-1);
            return IARM_RESULT_SUCCESS;
        }));
    static char sentinel[4];
    intptr_t handle = reinterpret_cast<intptr_t>(sentinel);
    EXPECT_EQ(_SetResolution(&handle, dsVIDEOPORT_TYPE_HDMI), 0);
}

/* ----------------------------------------------------------------------- *
 * 19e-19: HDMI + persisted "576p" + default "480p" NOT in EDID + no 480p *
 *         in EDID either → last-resort "boot with TV supported" path      *
 *         (lines 977-990): EDID "1080p" found in g_testPlatformRes →     *
 *         IsValidResolution=true.                                         *
 * ----------------------------------------------------------------------- */
TEST_F(DsMgrTest, SetResolution_HdmiLastResortBootWithTvSupported)
{
    /* Use default=index 0 ("480p") so default-resolution check uses
     * "480p" which is absent from the EDID.                             */
    setupValidPlatformResolutions(dsHalMock, /*defaultIdx=*/0);
    setupPlatformConfig();
    IsEUPlatform = false;
    EXPECT_CALL(dsHalMock, dsGetDisplay(dsVIDEOPORT_TYPE_HDMI, 0, _))
        .WillRepeatedly(::testing::DoAll(
            ::testing::SetArgPointee<2>(static_cast<intptr_t>(0x1000)),
            Return(dsERR_NONE)));
    ON_CALL(dsHalMock, dsGetEDID(_))
        .WillByDefault(Invoke([](dsDisplayGetEDIDParam_t *p) {
            p->edid.numOfSupportedResolution = 1;
            p->edid.hdmiDeviceType           = true;
            /* "1080p" not matched by direct/EU/fallback/default/480p
             * checks, but IS in g_testPlatformRes → last-resort match.  */
            strncpy(p->edid.suppResolutionList[0].name, "1080p",
                    sizeof(p->edid.suppResolutionList[0].name)-1);
            return IARM_RESULT_SUCCESS;
        }));
    ON_CALL(dsHalMock, dsGetResolution(_))
        .WillByDefault(Invoke([](dsVideoPortGetResolutionParam_t *p) {
            /* "576p" not in EDID, not in non-EU fallback list.          */
            strncpy(p->resolution.name, "576p",
                    sizeof(p->resolution.name)-1);
            return IARM_RESULT_SUCCESS;
        }));
    static char sentinel[4];
    intptr_t handle = reinterpret_cast<intptr_t>(sentinel);
    EXPECT_EQ(_SetResolution(&handle, dsVIDEOPORT_TYPE_HDMI), 0);
}

/* ----------------------------------------------------------------------- *
 * 19e-20: EU platform + HDMI + persisted "1080p50" not directly in EDID  *
 *         → EU secondary path (lines 883-894):                           *
 *         getSecondaryResolution("1080p50") = "1080p60";                 *
 *         EDID has "1080p60" AND platform has "1080p60" →                *
 *         IsValidResolution=true via EU secondary resolution.             *
 * ----------------------------------------------------------------------- */
TEST_F(DsMgrTest, SetResolution_HdmiEuSecondaryResolutionMatch)
{
    /* Add "1080p60" to platform table.                                   */
    memset(g_testPlatformRes, 0, sizeof(g_testPlatformRes));
    strncpy(g_testPlatformRes[0].name, "480p",   sizeof(g_testPlatformRes[0].name)-1);
    strncpy(g_testPlatformRes[1].name, "720p",   sizeof(g_testPlatformRes[1].name)-1);
    strncpy(g_testPlatformRes[2].name, "1080p",  sizeof(g_testPlatformRes[2].name)-1);
    strncpy(g_testPlatformRes[3].name, "1080p60",sizeof(g_testPlatformRes[3].name)-1);
    ON_CALL(dsHalMock, dsGetVideoPortResolutions(_, _))
        .WillByDefault(Invoke([](int *sz, dsVideoPortResolution_t **res) {
            *sz  = 4;
            *res = g_testPlatformRes;
            return dsERR_NONE;
        }));
    ON_CALL(dsHalMock, dsGetDefaultResolutionIndex(_))
        .WillByDefault(Invoke([](int *idx) {
            *idx = 1;
            return dsERR_NONE;
        }));

    IsEUPlatform = true;
    EXPECT_CALL(dsHalMock, dsGetDisplay(dsVIDEOPORT_TYPE_HDMI, 0, _))
        .WillRepeatedly(::testing::DoAll(
            ::testing::SetArgPointee<2>(static_cast<intptr_t>(0x1000)),
            Return(dsERR_NONE)));
    ON_CALL(dsHalMock, dsGetEDID(_))
        .WillByDefault(Invoke([](dsDisplayGetEDIDParam_t *p) {
            p->edid.numOfSupportedResolution = 2;
            p->edid.hdmiDeviceType           = true;
            strncpy(p->edid.suppResolutionList[0].name, "720p",
                    sizeof(p->edid.suppResolutionList[0].name)-1);
            strncpy(p->edid.suppResolutionList[1].name, "1080p60",
                    sizeof(p->edid.suppResolutionList[1].name)-1);
            return IARM_RESULT_SUCCESS;
        }));
    ON_CALL(dsHalMock, dsGetResolution(_))
        .WillByDefault(Invoke([](dsVideoPortGetResolutionParam_t *p) {
            /* "1080p50" not directly in EDID; secondary = "1080p60".    */
            strncpy(p->resolution.name, "1080p50",
                    sizeof(p->resolution.name)-1);
            return IARM_RESULT_SUCCESS;
        }));
    static char sentinel[4];
    intptr_t handle = reinterpret_cast<intptr_t>(sentinel);
    EXPECT_EQ(_SetResolution(&handle, dsVIDEOPORT_TYPE_HDMI), 0);
}

/* ----------------------------------------------------------------------- *
 * 19e-21: EU platform + HDMI + persisted "2160p50" + EDID has "1080p50"  *
 *         → EU secondary ("2160p60") not in EDID; EU fallback fps path   *
 *         (lines 914-919): getFallBackResolution("1080p","...",1)→        *
 *         "1080p50"; EDID has "1080p50" AND platform has "1080p50" →     *
 *         IsValidResolution=true via EU fallback fps.                     *
 * ----------------------------------------------------------------------- */
TEST_F(DsMgrTest, SetResolution_HdmiEuFallbackFpsMatch)
{
    /* Include "1080p50" in platform table so isResolutionSupported passes.*/
    memset(g_testPlatformRes, 0, sizeof(g_testPlatformRes));
    strncpy(g_testPlatformRes[0].name, "480p",   sizeof(g_testPlatformRes[0].name)-1);
    strncpy(g_testPlatformRes[1].name, "720p",   sizeof(g_testPlatformRes[1].name)-1);
    strncpy(g_testPlatformRes[2].name, "1080p",  sizeof(g_testPlatformRes[2].name)-1);
    strncpy(g_testPlatformRes[3].name, "1080p50",sizeof(g_testPlatformRes[3].name)-1);
    ON_CALL(dsHalMock, dsGetVideoPortResolutions(_, _))
        .WillByDefault(Invoke([](int *sz, dsVideoPortResolution_t **res) {
            *sz  = 4;
            *res = g_testPlatformRes;
            return dsERR_NONE;
        }));
    ON_CALL(dsHalMock, dsGetDefaultResolutionIndex(_))
        .WillByDefault(Invoke([](int *idx) {
            *idx = 1;
            return dsERR_NONE;
        }));

    setupPlatformConfig();   /* builds fallBackResolutionList (non-EU) */
    IsEUPlatform = true;     /* override the global after setup         */

    EXPECT_CALL(dsHalMock, dsGetDisplay(dsVIDEOPORT_TYPE_HDMI, 0, _))
        .WillRepeatedly(::testing::DoAll(
            ::testing::SetArgPointee<2>(static_cast<intptr_t>(0x1000)),
            Return(dsERR_NONE)));
    ON_CALL(dsHalMock, dsGetEDID(_))
        .WillByDefault(Invoke([](dsDisplayGetEDIDParam_t *p) {
            p->edid.numOfSupportedResolution = 1;
            p->edid.hdmiDeviceType           = true;
            /* "1080p50" satisfies: EU fps fallback for "1080p" entry   *
             * in fallBackResolutionList (index 1).                      */
            strncpy(p->edid.suppResolutionList[0].name, "1080p50",
                    sizeof(p->edid.suppResolutionList[0].name)-1);
            return IARM_RESULT_SUCCESS;
        }));
    ON_CALL(dsHalMock, dsGetResolution(_))
        .WillByDefault(Invoke([](dsVideoPortGetResolutionParam_t *p) {
            /* "2160p50": direct match fails; secondary "2160p60" not in
             * EDID; base="2160p" is at index 0 in fallBackResolutionList;
             * loop starts at i=1: "1080p"→EU fps→"1080p50"→match!       */
            strncpy(p->resolution.name, "2160p50",
                    sizeof(p->resolution.name)-1);
            return IARM_RESULT_SUCCESS;
        }));
    static char sentinel[4];
    intptr_t handle = reinterpret_cast<intptr_t>(sentinel);
    EXPECT_EQ(_SetResolution(&handle, dsVIDEOPORT_TYPE_HDMI), 0);
}
