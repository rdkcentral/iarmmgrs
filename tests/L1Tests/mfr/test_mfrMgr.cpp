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

/* mfr/include headers first — set _MFR_TYPES_H and _MFR_MGR_H_ guards
 * so the testframework's lighter copies of these files are skipped. */
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
    status.error      = 0;
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
 * Functions guarded by #ifndef RDK_MFRLIB_NAME
 *
 * Without that compile-time define (never set in the test build), every
 * static RPC handler returns IARM_RESULT_INVALID_STATE immediately,
 * before attempting any dlopen.  These tests verify that path.
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
