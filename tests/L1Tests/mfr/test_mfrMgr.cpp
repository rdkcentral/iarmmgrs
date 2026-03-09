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
 * @file test_mfrMgr.cpp
 * @brief L1 unit tests for mfr/mfrMgr.c.
 *
 * Components covered:
 *   - MFRLib_Stop()
 *   - writeImageCb()           (IARM broadcast on upgrade status)
 *   - writeImage_()            (back-to-back guard)
 *   - verifyImage_()           (back-to-back guard)
 *   - getSerializedData_()     (HAL path; mocked via --wrap,mfrGetSerializedData)
 *   - MFRLib_Start()           (mfr_init + IARM init/connect/register sequence)
 *   - All static RPC handlers  (return IARM_RESULT_INVALID_STATE without RDK_MFRLIB_NAME)
 *
 * Include-order rationale
 * -----------------------
 * mfrMgr.h is included BEFORE Iarm.h.  mfrMgr.h pulls in mfrTypes.h
 * (from mfr/include/) which sets the _MFR_TYPES_H include-guard.  When
 * Iarm.h subsequently tries to include the lighter testframework copy of
 * mfrTypes.h, the guard fires and the file is skipped.  This ensures that
 * the full HAL types (mfrUpgradeProgress_t, mfrSerializedData_t, etc.)
 * come from the iarmmgrs headers.
 *
 * HAL wrappers
 * ------------
 * --wrap,mfr_init            : controls MFRLib_Start behaviour
 * --wrap,mfrGetSerializedData: controls getSerializedData_ behaviour
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <cstring>
#include <cstdio>

/* Stub mfrMgr.h (tests/L1Tests/stubs/mfrMgr.h) is picked up first via -I
 * order.  It sets _MFR_MGR_H_ and provides macros + non-conflicting types.
 * Conflicting types (ThermalState, SetBLPattern, etc.) are left to Iarm.h. */
#include "mfrMgr.h"

/* IARM mock framework.  Comes after mfrMgr.h so the mfrTypes.h guard is
 * already set when Iarm.h tries to include it. */
#include "Iarm.h"
#include "IarmBusMock.h"

/* -----------------------------------------------------------------------
 * HAL stub control variables (read by the __wrap_ functions below)
 * ----------------------------------------------------------------------- */
static mfrError_t g_mfr_init_result            = mfrERR_NONE;
static mfrError_t g_mfr_gsd_result             = mfrERR_NONE;
static char       g_mfr_gsd_buf[MAX_SERIALIZED_BUF];
static int        g_mfr_gsd_bufLen              = 0;

/* __wrap_ functions intercept the real HAL symbols at link time. */
extern "C" {

mfrError_t __wrap_mfr_init(void)
{
    return g_mfr_init_result;
}

mfrError_t __wrap_mfrGetSerializedData(mfrSerializedType_t /*type*/,
                                       mfrSerializedData_t *data)
{
    if (g_mfr_gsd_result == mfrERR_NONE && data != nullptr) {
        data->buf     = g_mfr_gsd_buf;
        data->bufLen  = (size_t)g_mfr_gsd_bufLen;
        data->freeBuf = nullptr; /* no dynamic allocation in the stub */
    }
    return g_mfr_gsd_result;
}

} /* extern "C" */

/* -----------------------------------------------------------------------
 * dlopen / dlsym / dlclose wrap controls
 *
 * With RDK_MFRLIB_NAME defined (see Makefile.am) each RPC handler enters
 * its #else branch and attempts:  dlopen → dlsym → call func.
 * These wraps intercept those calls so tests can exercise every branch:
 *
 *   g_dlopen_fail = true  (default in SetUp)  → dlopen returns NULL
 *                                               → "Opening … failed" path
 *   g_dlopen_fail = false, g_dlsym_fail = true → dlopen ok, dlsym NULL
 *                                               → "not defined" path
 *   g_dlopen_fail = false, g_dlsym_fail = false→ both ok, stub func called
 *
 * A per-test stub result is controlled via g_stub_hal_result.
 * ----------------------------------------------------------------------- */
#include <map>
#include <string>

static void * const g_fake_dllib = reinterpret_cast<void *>(
                                       static_cast<uintptr_t>(0xDEADBEEF));
static bool       g_dlopen_fail      = true;   /* safe default: dlopen fails */
static bool       g_dlsym_fail       = false;
static mfrError_t g_stub_hal_result  = mfrERR_NONE;

/* ---- Stub HAL implementations ----------------------------------------
 * Each function has the exact signature that mfrMgr.c casts dlsym's result
 * to before calling.  Using the right prototype avoids calling-convention UB.
 * ---------------------------------------------------------------------- */
extern "C" {

static mfrError_t stub_void(void)
    { return g_stub_hal_result; }

static mfrError_t stub_writeimage(const char * /*name*/,
                                   const char * /*path*/,
                                   mfrImageType_t /*type*/,
                                   mfrUpgradeStatusNotify_t /*notify*/)
    { return g_stub_hal_result; }

static mfrError_t stub_mirror(mfrUpgradeStatusNotify_t /*notify*/)
    { return g_stub_hal_result; }

static mfrError_t stub_pattern(mfrBlPattern_t /*p*/)
    { return g_stub_hal_result; }

static mfrError_t stub_securetime(IARM_Bus_MFRLib_SecureTime_Param * /*p*/)
    { return g_stub_hal_result; }

static mfrError_t stub_fsr(uint16_t *p)
    { if (p) *p = 1u; return g_stub_hal_result; }

static mfrError_t stub_splash(const char * /*path*/)
    { return g_stub_hal_result; }

static mfrError_t stub_setconfig(unsigned int /*bl*/)
    { return g_stub_hal_result; }

static mfrError_t stub_getconfig(unsigned int *bl)
    { if (bl) *bl = 0u; return g_stub_hal_result; }

} /* extern "C" stubs */

/* Symbol-name → stub function-pointer table */
static const std::map<std::string, void *> g_sym_map {
    { "mfrSetSerializedData",    reinterpret_cast<void *>(stub_void)        },
    { "mfrDeletePDRI",           reinterpret_cast<void *>(stub_void)        },
    { "mfrScrubAllBanks",        reinterpret_cast<void *>(stub_void)        },
    { "mfrClearBlSplashScreen",  reinterpret_cast<void *>(stub_void)        },
    { "mfrWriteImage",           reinterpret_cast<void *>(stub_writeimage)  },
    { "mfrVerifyImage",          reinterpret_cast<void *>(stub_writeimage)  },
    { "mfrMirrorImage",          reinterpret_cast<void *>(stub_mirror)      },
    { "mfrSetBootloaderPattern", reinterpret_cast<void *>(stub_pattern)     },
    { "mfrGetSecureTime",        reinterpret_cast<void *>(stub_securetime)  },
    { "mfrSetSecureTime",        reinterpret_cast<void *>(stub_securetime)  },
    { "mfrSetFSRflag",           reinterpret_cast<void *>(stub_fsr)         },
    { "mfrGetFSRflag",           reinterpret_cast<void *>(stub_fsr)         },
    { "mfrSetBlSplashScreen",    reinterpret_cast<void *>(stub_splash)      },
    { "mfr_setConfigData",       reinterpret_cast<void *>(stub_setconfig)   },
    { "mfr_getConfigData",       reinterpret_cast<void *>(stub_getconfig)   },
};

extern "C" {

void * __wrap_dlopen(const char * /*filename*/, int /*flags*/)
{
    return g_dlopen_fail ? nullptr : g_fake_dllib;
}

void * __wrap_dlsym(void *handle, const char *name)
{
    if (handle == g_fake_dllib) {
        if (g_dlsym_fail) return nullptr;
        auto it = g_sym_map.find(name);
        return (it != g_sym_map.end()) ? it->second : nullptr;
    }
    /* No real handles are ever opened in the test build; if somehow reached,
     * declare __real_dlsym so the linker resolves it but return null safely. */
    extern void * __real_dlsym(void *, const char *);
    return __real_dlsym(handle, name);
}

int __wrap_dlclose(void * /*handle*/) { return 0; }

} /* extern "C" wraps */

/* -----------------------------------------------------------------------
 * Source under test
 * ----------------------------------------------------------------------- */
extern "C" {
#include "mfrMgr.c"
}

using ::testing::_;
using ::testing::Return;
using ::testing::StrEq;

/* -----------------------------------------------------------------------
 * Fixture
 * ----------------------------------------------------------------------- */
class MfrMgrTest : public ::testing::Test
{
protected:
    ::testing::NiceMock<IarmBusImplMock> iarmMock;

    void SetUp() override
    {
        IarmBus::setImpl(&iarmMock);

        /* Reset file-static state in mfrMgr.c that affects test outcomes */
        memset(&lastStatus,   0, sizeof(lastStatus));   /* progress = NOT_STARTED */
        memset(&notifyStruct, 0, sizeof(notifyStruct));
        profileType  = PROFILE_INVALID;
        is_connected = 0;

        /* Reset HAL stub controls */
        g_mfr_init_result = mfrERR_NONE;
        g_mfr_gsd_result  = mfrERR_NONE;
        memset(g_mfr_gsd_buf, 0, sizeof(g_mfr_gsd_buf));
        g_mfr_gsd_bufLen  = 0;

        /* Reset dlopen/dlsym wrap controls.
         * g_dlopen_fail=true  means all RPC handlers' dlopen calls return NULL
         * so the existing _DlopenFails_ tests need no special setup, and no
         * real .so is ever touched during the test run. */
        g_dlopen_fail     = true;
        g_dlsym_fail      = false;
        g_stub_hal_result = mfrERR_NONE;
    }

    void TearDown() override
    {
        ::testing::Mock::VerifyAndClearExpectations(&iarmMock);
        IarmBus::setImpl(nullptr);
    }
};

/* ===================================================================== *
 * MFRLib_Stop
 * ===================================================================== */

TEST_F(MfrMgrTest, Stop_NotConnected_ReturnsSuccess)
{
    is_connected = 0;
    EXPECT_EQ(IARM_RESULT_SUCCESS, MFRLib_Stop());
}

TEST_F(MfrMgrTest, Stop_Connected_DisconnectAndTermSucceed)
{
    is_connected = 1;
    EXPECT_CALL(iarmMock, IARM_Bus_Disconnect()).WillOnce(Return(IARM_RESULT_SUCCESS));
    EXPECT_CALL(iarmMock, IARM_Bus_Term()).WillOnce(Return(IARM_RESULT_SUCCESS));
    EXPECT_EQ(IARM_RESULT_SUCCESS, MFRLib_Stop());
}

TEST_F(MfrMgrTest, Stop_Connected_DisconnectFails_ReturnsFailure)
{
    is_connected = 1;
    EXPECT_CALL(iarmMock, IARM_Bus_Disconnect()).WillOnce(Return(IARM_RESULT_OOM));
    EXPECT_EQ(IARM_RESULT_OOM, MFRLib_Stop());
}

/* ===================================================================== *
 * writeImageCb
 * ===================================================================== */

TEST_F(MfrMgrTest, WriteImageCb_BroadcastsEventAndUpdatesProgress)
{
    notifyStruct.cbData = (char *)"test_module";

    mfrUpgradeStatus_t status;
    memset(&status, 0, sizeof(status));
    status.progress   = mfrUPGRADE_PROGRESS_STARTED;
    status.error      = mfrERR_NONE;
    status.percentage = 5000; /* internal unit: percentage * 100 */

    EXPECT_CALL(iarmMock, IARM_Bus_BroadcastEvent(
        StrEq(IARM_BUS_MFRLIB_NAME),
        IARM_BUS_MFRMGR_EVENT_STATUS_UPDATE,
        _, _))
        .WillOnce(Return(IARM_RESULT_SUCCESS));

    writeImageCb(&status);

    EXPECT_EQ(mfrUPGRADE_PROGRESS_STARTED, lastStatus.progress);
}

TEST_F(MfrMgrTest, WriteImageCb_BroadcastFails_StillUpdatesLastStatus)
{
    notifyStruct.cbData = (char *)"any_module";

    mfrUpgradeStatus_t status;
    memset(&status, 0, sizeof(status));
    status.progress = mfrUPGRADE_PROGRESS_COMPLETED;

    ON_CALL(iarmMock, IARM_Bus_BroadcastEvent(_, _, _, _))
        .WillByDefault(Return(IARM_RESULT_IPCCORE_FAIL));

    writeImageCb(&status);

    /* lastStatus must be updated even when the broadcast fails */
    EXPECT_EQ(mfrUPGRADE_PROGRESS_COMPLETED, lastStatus.progress);
}

/* ===================================================================== *
 * writeImage_ / verifyImage_ back-to-back guard
 *
 * When lastStatus.progress is VERIFYING or FLASHING the functions must
 * return IARM_RESULT_INVALID_STATE immediately (before any dlopen).
 * ===================================================================== */

TEST_F(MfrMgrTest, WriteImage_RejectsWhenVerifying)
{
    lastStatus.progress = mfrUPGRADE_PROGRESS_VERIFYING;
    IARM_Bus_MFRLib_WriteImage_Param_t param{};
    EXPECT_EQ(IARM_RESULT_INVALID_STATE, writeImage_(&param));
}

TEST_F(MfrMgrTest, WriteImage_RejectsWhenFlashing)
{
    lastStatus.progress = mfrUPGRADE_PROGRESS_FLASHING;
    IARM_Bus_MFRLib_WriteImage_Param_t param{};
    EXPECT_EQ(IARM_RESULT_INVALID_STATE, writeImage_(&param));
}

TEST_F(MfrMgrTest, VerifyImage_RejectsWhenVerifying)
{
    lastStatus.progress = mfrUPGRADE_PROGRESS_VERIFYING;
    IARM_Bus_MFRLib_WriteImage_Param_t param{};
    EXPECT_EQ(IARM_RESULT_INVALID_STATE, verifyImage_(&param));
}

TEST_F(MfrMgrTest, VerifyImage_RejectsWhenFlashing)
{
    lastStatus.progress = mfrUPGRADE_PROGRESS_FLASHING;
    IARM_Bus_MFRLib_WriteImage_Param_t param{};
    EXPECT_EQ(IARM_RESULT_INVALID_STATE, verifyImage_(&param));
}

/* ===================================================================== *
 * RPC handlers — dlopen-fails path
 *
 * RDK_MFRLIB_NAME is now defined (see Makefile.am AM_CPPFLAGS), so every
 * handler enters its #else branch and calls dlopen(RDK_MFRLIB_NAME, ...).
 * SetUp() leaves g_dlopen_fail=true so __wrap_dlopen returns NULL, which
 * exercises the "Opening RDK_MFRLIB_NAME failed" early-return inside each
 * handler.  The expected result is still IARM_RESULT_INVALID_STATE, so
 * every test below passes unchanged.
 * ===================================================================== */

TEST_F(MfrMgrTest, WriteImage_NoLibName_ReturnsInvalidState)
{
    /* progress = NOT_STARTED so the back-to-back guard does not fire;
     * execution reaches the #ifndef path and returns INVALID_STATE. */
    IARM_Bus_MFRLib_WriteImage_Param_t param{};
    EXPECT_EQ(IARM_RESULT_INVALID_STATE, writeImage_(&param));
}

TEST_F(MfrMgrTest, VerifyImage_NoLibName_ReturnsInvalidState)
{
    IARM_Bus_MFRLib_WriteImage_Param_t param{};
    EXPECT_EQ(IARM_RESULT_INVALID_STATE, verifyImage_(&param));
}

TEST_F(MfrMgrTest, SetSerializedData_NoLibName_ReturnsInvalidState)
{
    EXPECT_EQ(IARM_RESULT_INVALID_STATE, setSerializedData_(nullptr));
}

TEST_F(MfrMgrTest, DeletePDRI_NoLibName_ReturnsInvalidState)
{
    EXPECT_EQ(IARM_RESULT_INVALID_STATE, deletePDRI_(nullptr));
}

TEST_F(MfrMgrTest, ScrubAllBanks_NoLibName_ReturnsInvalidState)
{
    EXPECT_EQ(IARM_RESULT_INVALID_STATE, scrubAllBanks_(nullptr));
}

TEST_F(MfrMgrTest, SetBootloaderPattern_NoLibName_ReturnsInvalidState)
{
    EXPECT_EQ(IARM_RESULT_INVALID_STATE, setBootloaderPattern_(nullptr));
}

TEST_F(MfrMgrTest, MirrorImage_NoLibName_ReturnsInvalidState)
{
    EXPECT_EQ(IARM_RESULT_INVALID_STATE, mirrorImage(nullptr));
}

TEST_F(MfrMgrTest, SetBlSplashScreen_NoLibName_ReturnsInvalidState)
{
    EXPECT_EQ(IARM_RESULT_INVALID_STATE, mfrSetBlSplashScreen_(nullptr));
}

TEST_F(MfrMgrTest, ClearBlSplashScreen_NoLibName_ReturnsInvalidState)
{
    EXPECT_EQ(IARM_RESULT_INVALID_STATE, mfrClearBlSplashScreen_(nullptr));
}

TEST_F(MfrMgrTest, GetSecureTime_NoLibName_ReturnsInvalidState)
{
    EXPECT_EQ(IARM_RESULT_INVALID_STATE, getSecureTime_(nullptr));
}

TEST_F(MfrMgrTest, SetSecureTime_NoLibName_ReturnsInvalidState)
{
    EXPECT_EQ(IARM_RESULT_INVALID_STATE, setSecureTime_(nullptr));
}

TEST_F(MfrMgrTest, SetFsrFlag_NoLibName_ReturnsInvalidState)
{
    EXPECT_EQ(IARM_RESULT_INVALID_STATE, setFSRflag_(nullptr));
}

TEST_F(MfrMgrTest, GetFsrFlag_NoLibName_ReturnsInvalidState)
{
    EXPECT_EQ(IARM_RESULT_INVALID_STATE, getFSRflag_(nullptr));
}

TEST_F(MfrMgrTest, SetConfigData_NoLibName_ReturnsInvalidState)
{
    EXPECT_EQ(IARM_RESULT_INVALID_STATE, setConfigData_(nullptr));
}

TEST_F(MfrMgrTest, GetConfigData_NoLibName_ReturnsInvalidState)
{
    EXPECT_EQ(IARM_RESULT_INVALID_STATE, getConfigData_(nullptr));
}

/* ===================================================================== *
 * getSerializedData_
 *
 * mfrGetSerializedData is intercepted via --wrap,mfrGetSerializedData.
 * profileType is pre-set to PROFILE_TV to skip the searchRdkProfile()
 * call (which would require fopen/fgets mocking).
 * ===================================================================== */

TEST_F(MfrMgrTest, GetSerializedData_HalSucceeds_CopiesBufferAndReturnsSuccess)
{
    profileType = PROFILE_TV;

    IARM_Bus_MFRLib_GetSerializedData_Param_t param{};
    param.type = mfrSERIALIZED_TYPE_SERIALNUMBER;

    strncpy(g_mfr_gsd_buf, "SERIAL001", sizeof(g_mfr_gsd_buf) - 1);
    g_mfr_gsd_bufLen = 9;
    g_mfr_gsd_result = mfrERR_NONE;

    EXPECT_EQ(IARM_RESULT_SUCCESS, getSerializedData_(&param));
    EXPECT_EQ(9, param.bufLen);
    EXPECT_STREQ("SERIAL001", param.buffer);
}

TEST_F(MfrMgrTest, GetSerializedData_HalFails_ReturnsIpcCoreFail)
{
    profileType = PROFILE_TV;

    IARM_Bus_MFRLib_GetSerializedData_Param_t param{};
    param.type       = mfrSERIALIZED_TYPE_MODELNAME;
    g_mfr_gsd_result = mfrERR_GENERAL;

    EXPECT_EQ(IARM_RESULT_IPCCORE_FAIL, getSerializedData_(&param));
}

TEST_F(MfrMgrTest, GetSerializedData_StbProfile_ProvisionedModelNameUsesSkymodeQuery)
{
    /* When profileType == PROFILE_STB and the requested type is
     * mfrSERIALIZED_TYPE_PROVISIONED_MODELNAME, getSerializedData_
     * substitutes the HAL query with mfrSERIALIZED_TYPE_SKYMODELNAME.
     * The __wrap function succeeds for any type, so we just verify the
     * overall success and the data that comes back. */
    profileType = PROFILE_STB;

    IARM_Bus_MFRLib_GetSerializedData_Param_t param{};
    param.type = mfrSERIALIZED_TYPE_PROVISIONED_MODELNAME;

    strncpy(g_mfr_gsd_buf, "SkyModel", sizeof(g_mfr_gsd_buf) - 1);
    g_mfr_gsd_bufLen = 8;
    g_mfr_gsd_result = mfrERR_NONE;

    EXPECT_EQ(IARM_RESULT_SUCCESS, getSerializedData_(&param));
    EXPECT_STREQ("SkyModel", param.buffer);
}

/* ===================================================================== *
 * MFRLib_Start
 * ===================================================================== */

TEST_F(MfrMgrTest, Start_MfrInitFails_ReturnsInvalidState)
{
    g_mfr_init_result = mfrERR_GENERAL;
    EXPECT_EQ(IARM_RESULT_INVALID_STATE, MFRLib_Start());
}

TEST_F(MfrMgrTest, Start_IarmInitFails_ReturnsError)
{
    g_mfr_init_result = mfrERR_NONE;
    EXPECT_CALL(iarmMock, IARM_Bus_Init(StrEq(IARM_BUS_MFRLIB_NAME)))
        .WillOnce(Return(IARM_RESULT_OOM));
    IARM_Result_t result = MFRLib_Start();
    EXPECT_NE(IARM_RESULT_SUCCESS, result);
}

TEST_F(MfrMgrTest, Start_AllSucceed_ReturnsSuccessAndSetsConnected)
{
    is_connected      = 0;
    g_mfr_init_result = mfrERR_NONE;
    /* NiceMock returns IARM_RESULT_SUCCESS for all IARM calls */
    EXPECT_EQ(IARM_RESULT_SUCCESS, MFRLib_Start());
    EXPECT_EQ(1, is_connected);
}
/* ===================================================================== *
 * RPC handlers — dlsym-fails path
 *
 * g_dlopen_fail = false  → __wrap_dlopen returns g_fake_dllib
 * g_dlsym_fail  = true   → __wrap_dlsym returns nullptr
 *
 * The handler finds dlsym() returned NULL, calls dlclose(), and returns
 * IARM_RESULT_INVALID_STATE.  The static func pointer stays 0 so a
 * subsequent func-success test can still load it.
 *
 * IMPORTANT: GTest runs tests in declaration order within a class.  These
 * dlsym-fails tests are declared after the corresponding dlopen-fails tests
 * above, so the static func pointer is still 0 when they execute.
 * ===================================================================== */

TEST_F(MfrMgrTest, SetSerializedData_DlsymFails_ReturnsInvalidState)
{
    g_dlopen_fail = false;
    g_dlsym_fail  = true;
    EXPECT_EQ(IARM_RESULT_INVALID_STATE, setSerializedData_(nullptr));
}

TEST_F(MfrMgrTest, DeletePDRI_DlsymFails_ReturnsInvalidState)
{
    g_dlopen_fail = false;
    g_dlsym_fail  = true;
    EXPECT_EQ(IARM_RESULT_INVALID_STATE, deletePDRI_(nullptr));
}

TEST_F(MfrMgrTest, ScrubAllBanks_DlsymFails_ReturnsInvalidState)
{
    g_dlopen_fail = false;
    g_dlsym_fail  = true;
    EXPECT_EQ(IARM_RESULT_INVALID_STATE, scrubAllBanks_(nullptr));
}

TEST_F(MfrMgrTest, WriteImage_DlsymFails_ReturnsInvalidState)
{
    g_dlopen_fail = false;
    g_dlsym_fail  = true;
    IARM_Bus_MFRLib_WriteImage_Param_t param{};
    EXPECT_EQ(IARM_RESULT_INVALID_STATE, writeImage_(&param));
}

TEST_F(MfrMgrTest, VerifyImage_DlsymFails_ReturnsInvalidState)
{
    g_dlopen_fail = false;
    g_dlsym_fail  = true;
    IARM_Bus_MFRLib_WriteImage_Param_t param{};
    EXPECT_EQ(IARM_RESULT_INVALID_STATE, verifyImage_(&param));
}

TEST_F(MfrMgrTest, MirrorImage_DlsymFails_ReturnsInvalidState)
{
    g_dlopen_fail = false;
    g_dlsym_fail  = true;
    EXPECT_EQ(IARM_RESULT_INVALID_STATE, mirrorImage(nullptr));
}

TEST_F(MfrMgrTest, SetBLSplashScreen_DlsymFails_ReturnsInvalidState)
{
    g_dlopen_fail = false;
    g_dlsym_fail  = true;
    EXPECT_EQ(IARM_RESULT_INVALID_STATE, mfrSetBlSplashScreen_(nullptr));
}

TEST_F(MfrMgrTest, ClearBLSplashScreen_DlsymFails_ReturnsInvalidState)
{
    g_dlopen_fail = false;
    g_dlsym_fail  = true;
    EXPECT_EQ(IARM_RESULT_INVALID_STATE, mfrClearBlSplashScreen_(nullptr));
}

TEST_F(MfrMgrTest, GetSecureTime_DlsymFails_ReturnsInvalidState)
{
    g_dlopen_fail = false;
    g_dlsym_fail  = true;
    EXPECT_EQ(IARM_RESULT_INVALID_STATE, getSecureTime_(nullptr));
}

TEST_F(MfrMgrTest, SetSecureTime_DlsymFails_ReturnsInvalidState)
{
    g_dlopen_fail = false;
    g_dlsym_fail  = true;
    EXPECT_EQ(IARM_RESULT_INVALID_STATE, setSecureTime_(nullptr));
}

TEST_F(MfrMgrTest, SetFsrFlag_DlsymFails_ReturnsInvalidState)
{
    g_dlopen_fail = false;
    g_dlsym_fail  = true;
    EXPECT_EQ(IARM_RESULT_INVALID_STATE, setFSRflag_(nullptr));
}

TEST_F(MfrMgrTest, GetFsrFlag_DlsymFails_ReturnsInvalidState)
{
    g_dlopen_fail = false;
    g_dlsym_fail  = true;
    EXPECT_EQ(IARM_RESULT_INVALID_STATE, getFSRflag_(nullptr));
}

TEST_F(MfrMgrTest, SetConfigData_DlsymFails_ReturnsInvalidState)
{
    g_dlopen_fail = false;
    g_dlsym_fail  = true;
    IARM_Bus_MFRLib_Platformblockdata_Param_t param{};
    EXPECT_EQ(IARM_RESULT_INVALID_STATE, setConfigData_(&param));
}

TEST_F(MfrMgrTest, GetConfigData_DlsymFails_ReturnsInvalidState)
{
    g_dlopen_fail = false;
    g_dlsym_fail  = true;
    IARM_Bus_MFRLib_Platformblockdata_Param_t param{};
    EXPECT_EQ(IARM_RESULT_INVALID_STATE, getConfigData_(&param));
}

/* ===================================================================== *
 * RPC handlers — func loaded and called (dlopen + dlsym succeed)
 *
 * g_dlopen_fail = false, g_dlsym_fail = false (SetUp reset).
 * __wrap_dlsym looks up the symbol name in g_sym_map and returns the
 * matching stub function pointer.  The handler caches it in its static
 * func variable and calls it; the stub returns g_stub_hal_result.
 *
 * These tests declare AFTER the dlsym-fails tests above so the static
 * func pointers are still 0 when the tests first load the stubs.
 * ===================================================================== */

TEST_F(MfrMgrTest, SetSerializedData_FuncLoaded_ReturnsSuccess)
{
    g_dlopen_fail = false;
    EXPECT_EQ(IARM_RESULT_SUCCESS, setSerializedData_(nullptr));
}

TEST_F(MfrMgrTest, DeletePDRI_FuncLoaded_ReturnsSuccess)
{
    g_dlopen_fail = false;
    EXPECT_EQ(IARM_RESULT_SUCCESS, deletePDRI_(nullptr));
}

TEST_F(MfrMgrTest, ScrubAllBanks_FuncLoaded_ReturnsSuccess)
{
    g_dlopen_fail = false;
    EXPECT_EQ(IARM_RESULT_SUCCESS, scrubAllBanks_(nullptr));
}

TEST_F(MfrMgrTest, WriteImage_FuncLoaded_HalSucceeds_ReturnsSuccess)
{
    g_dlopen_fail = false;
    IARM_Bus_MFRLib_WriteImage_Param_t param{};
    strncpy(param.name, "test_image", sizeof(param.name) - 1);
    strncpy(param.path, "/tmp/test.bin", sizeof(param.path) - 1);
    param.type = mfrIMAGE_TYPE_CDL;
    EXPECT_EQ(IARM_RESULT_SUCCESS, writeImage_(&param));
}

TEST_F(MfrMgrTest, WriteImage_FuncLoaded_HalFails_ReturnsIpcCoreFail)
{
    /* func is already cached from the previous test; this call goes straight
     * to the cached stub but with a HAL error result. */
    g_stub_hal_result = mfrERR_GENERAL;
    IARM_Bus_MFRLib_WriteImage_Param_t param{};
    EXPECT_EQ(IARM_RESULT_IPCCORE_FAIL, writeImage_(&param));
}

TEST_F(MfrMgrTest, VerifyImage_FuncLoaded_HalSucceeds_ReturnsSuccess)
{
    g_dlopen_fail = false;
    IARM_Bus_MFRLib_WriteImage_Param_t param{};
    strncpy(param.name, "verify_image", sizeof(param.name) - 1);
    param.type = mfrIMAGE_TYPE_CDL;
    EXPECT_EQ(IARM_RESULT_SUCCESS, verifyImage_(&param));
}

TEST_F(MfrMgrTest, VerifyImage_FuncLoaded_HalFails_ReturnsIpcCoreFail)
{
    /* func already cached from previous test */
    g_stub_hal_result = mfrERR_GENERAL;
    IARM_Bus_MFRLib_WriteImage_Param_t param{};
    EXPECT_EQ(IARM_RESULT_IPCCORE_FAIL, verifyImage_(&param));
}

TEST_F(MfrMgrTest, MirrorImage_FuncLoaded_HalSucceeds_ReturnsSuccess)
{
    g_dlopen_fail = false;
    IARM_Bus_MFRLib_WriteImage_Param_t param{};
    EXPECT_EQ(IARM_RESULT_SUCCESS, mirrorImage(&param));
}

TEST_F(MfrMgrTest, MirrorImage_FuncLoaded_HalFails_ReturnsIpcCoreFail)
{
    /* func already cached */
    g_stub_hal_result = mfrERR_GENERAL;
    IARM_Bus_MFRLib_WriteImage_Param_t param{};
    EXPECT_EQ(IARM_RESULT_IPCCORE_FAIL, mirrorImage(&param));
}

TEST_F(MfrMgrTest, ClearBLSplashScreen_FuncLoaded_ReturnsSuccess)
{
    g_dlopen_fail = false;
    EXPECT_EQ(IARM_RESULT_SUCCESS, mfrClearBlSplashScreen_(nullptr));
}

TEST_F(MfrMgrTest, SetBLSplashScreen_FuncLoaded_ValidArg_ReturnsSuccess)
{
    g_dlopen_fail = false;
    IARM_Bus_MFRLib_SetBLSplashScreen_Param_t param{};
    strncpy(param.path, "/splash.png", sizeof(param.path) - 1);
    EXPECT_EQ(IARM_RESULT_SUCCESS, mfrSetBlSplashScreen_(&param));
}

TEST_F(MfrMgrTest, SetBLSplashScreen_FuncLoaded_NullArg_ReturnsInvalidParam)
{
    /* func is already cached; null arg exercises the NULL arg branch */
    EXPECT_EQ(IARM_RESULT_INVALID_PARAM, mfrSetBlSplashScreen_(nullptr));
}

TEST_F(MfrMgrTest, GetSecureTime_FuncLoaded_ValidArg_ReturnsSuccess)
{
    g_dlopen_fail = false;
    IARM_Bus_MFRLib_SecureTime_Param param{};
    EXPECT_EQ(IARM_RESULT_SUCCESS, getSecureTime_(&param));
}

TEST_F(MfrMgrTest, GetSecureTime_FuncLoaded_NullArg_ReturnsInvalidState)
{
    /* func already cached; null arg means if(func && NULL != arg) is false */
    EXPECT_EQ(IARM_RESULT_INVALID_STATE, getSecureTime_(nullptr));
}

TEST_F(MfrMgrTest, SetSecureTime_FuncLoaded_ValidArg_ReturnsSuccess)
{
    g_dlopen_fail = false;
    IARM_Bus_MFRLib_SecureTime_Param param{};
    EXPECT_EQ(IARM_RESULT_SUCCESS, setSecureTime_(&param));
}

TEST_F(MfrMgrTest, SetSecureTime_FuncLoaded_NullArg_ReturnsInvalidState)
{
    /* func already cached */
    EXPECT_EQ(IARM_RESULT_INVALID_STATE, setSecureTime_(nullptr));
}

TEST_F(MfrMgrTest, SetFsrFlag_FuncLoaded_ValidArg_ReturnsSuccess)
{
    g_dlopen_fail = false;
    IARM_Bus_MFRLib_FsrFlag_Param_t param = 1;
    EXPECT_EQ(IARM_RESULT_SUCCESS, setFSRflag_(&param));
}

TEST_F(MfrMgrTest, SetFsrFlag_FuncLoaded_NullArg_ReturnsInvalidParam)
{
    /* func already cached */
    EXPECT_EQ(IARM_RESULT_INVALID_PARAM, setFSRflag_(nullptr));
}

TEST_F(MfrMgrTest, GetFsrFlag_FuncLoaded_ValidArg_ReturnsSuccess)
{
    /* stub_fsr sets *p=1 (not (uint16_t)-1) so the "success" condition
     * (err==mfrERR_NONE && param != -1) is met. */
    g_dlopen_fail = false;
    IARM_Bus_MFRLib_FsrFlag_Param_t param = 0;
    EXPECT_EQ(IARM_RESULT_SUCCESS, getFSRflag_(&param));
}

TEST_F(MfrMgrTest, GetFsrFlag_FuncLoaded_NullArg_ReturnsInvalidParam)
{
    /* func already cached */
    EXPECT_EQ(IARM_RESULT_INVALID_PARAM, getFSRflag_(nullptr));
}

TEST_F(MfrMgrTest, SetConfigData_FuncLoaded_ReturnsSuccess)
{
    g_dlopen_fail = false;
    IARM_Bus_MFRLib_Platformblockdata_Param_t param{};
    param.blocklist = 0u;
    EXPECT_EQ(IARM_RESULT_SUCCESS, setConfigData_(&param));
}

TEST_F(MfrMgrTest, SetConfigData_FuncLoaded_HalFails_ReturnsInvalidParam)
{
    /* func already cached */
    g_stub_hal_result = mfrERR_GENERAL;
    IARM_Bus_MFRLib_Platformblockdata_Param_t param{};
    EXPECT_EQ(IARM_RESULT_INVALID_PARAM, setConfigData_(&param));
}

TEST_F(MfrMgrTest, GetConfigData_FuncLoaded_ReturnsSuccess)
{
    g_dlopen_fail = false;
    IARM_Bus_MFRLib_Platformblockdata_Param_t param{};
    EXPECT_EQ(IARM_RESULT_SUCCESS, getConfigData_(&param));
}

TEST_F(MfrMgrTest, GetConfigData_FuncLoaded_HalFails_ReturnsInvalidParam)
{
    /* func already cached */
    g_stub_hal_result = mfrERR_GENERAL;
    IARM_Bus_MFRLib_Platformblockdata_Param_t param{};
    EXPECT_EQ(IARM_RESULT_INVALID_PARAM, getConfigData_(&param));
}

/* ===================================================================== *
 * setBootloaderPattern_ — special case: symbol_lookup_complete flag
 *
 * setBootloaderPattern_ has a static symbol_lookup_complete guard in
 * addition to the static func pointer.  Once dlopen is attempted the flag
 * is set to 1.  A second call with func==0 goes to the else{} branch and
 * returns INVALID_STATE without retrying dlopen.
 *
 * The dlopen-fails test above (SetBootloaderPattern_NoLibName_ReturnsInvalidState)
 * already set symbol_lookup_complete=1 with func still 0.
 * The test below exercises the secondary "lookup already done, still no func"
 * branch that runs on that second call.
 * ===================================================================== */

TEST_F(MfrMgrTest, SetBootloaderPattern_LookupAlreadyDone_ReturnsInvalidState)
{
    /* symbol_lookup_complete was set to 1 by the earlier dlopen-fails test;
     * func is still 0.  SetUp resets g_dlopen_fail only — not the statics
     * inside mfrMgr.c.  This call hits:
     *   if (func == 0) { if (0 == symbol_lookup_complete) { ... }
     *                    else { return IARM_RESULT_INVALID_STATE; } } */
    g_dlopen_fail = false; /* wouldn't matter — dlopen is not reached */
    EXPECT_EQ(IARM_RESULT_INVALID_STATE, setBootloaderPattern_(nullptr));
}