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
 * @file tests/L1Tests/stubs/dsMgr.h
 *
 * Stub that shadows devicesettings/rpc/include/dsMgr.h for the L1 test build.
 *
 * Purpose
 * -------
 * The testframework Iarm.h already defines _DSMgr_EventId_t and
 * IARM_BUS_DSMGR_NAME.  If the real dsMgr.h were included after Iarm.h,
 * the compiler would report a "multiple definition of enum _DSMgr_EventId_t"
 * error.  This stub uses the same include guard (RPDSMGR_H_) as the real
 * file so the preprocessor skips it on the second encounter, and provides
 * only the declarations/types that dsMgr.c needs beyond what Iarm.h already
 * supplies.
 *
 * What is provided here
 * ---------------------
 *   - dsTypes.h   : HAL interface types (dsHdmiInPort_t, dsVideoPortResolution_t, …)
 *   - dsRpc.h     : RPC parameter structs (dsVideoPortGetHandleParam_t, …)
 *                   and dsSleepMode_t
 *   - dsMgr_init / dsMgr_term declarations
 *   - dsAudioPortState_t (guarded with HAVE_DSAUDIOPORT_STATE_TYPE)
 *   - IARM_Bus_DSMgr_EventData_t union
 *
 * What is intentionally omitted
 * ------------------------------
 *   - _DSMgr_EventId_t / IARM_Bus_DSMgr_EventId_t  (provided by Iarm.h)
 *   - IARM_BUS_DSMGR_NAME                           (provided by Iarm.h)
 */

#ifndef RPDSMGR_H_
#define RPDSMGR_H_

#include "dsTypes.h"    /* HAL types — dsHdmiInPort_t, dsVideoPortResolution_t, … */

/* dsRpc.h redefines IARM_BUS_DSMGR_API_SetStandbyVideoState and
 * IARM_BUS_DSMGR_API_GetStandbyVideoState with different string literals
 * than those in the testframework Iarm.h mock.  Undefine them first so
 * dsRpc.h can set the canonical production values that dsMgr.c expects. */
#ifdef IARM_BUS_DSMGR_API_SetStandbyVideoState
#undef IARM_BUS_DSMGR_API_SetStandbyVideoState
#endif
#ifdef IARM_BUS_DSMGR_API_GetStandbyVideoState
#undef IARM_BUS_DSMGR_API_GetStandbyVideoState
#endif 

#include "dsRpc.h"      /* RPC param structs + dsSleepMode_t                       */

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Lifecycle functions (defined as stubs in test_dsMgr.cpp) ----------- */
IARM_Result_t dsMgr_init(void);
IARM_Result_t dsMgr_term(void);

/* ---- Audio-port state (used inside IARM_Bus_DSMgr_EventData_t) ---------- */
#ifndef HAVE_DSAUDIOPORT_STATE_TYPE
#define HAVE_DSAUDIOPORT_STATE_TYPE
typedef enum _dsAudioPortState {
    dsAUDIOPORT_STATE_UNINITIALIZED,
    dsAUDIOPORT_STATE_INITIALIZED,
    dsAUDIOPORT_STATE_MAX
} dsAudioPortState_t;
#endif

/* ---- DS Manager event data union ----------------------------------------
 * Same layout as devicesettings/rpc/include/dsMgr.h.
 * _DSMgr_EventId_t and IARM_BUS_DSMGR_NAME are deliberately absent here;
 * Iarm.h (included before this stub) already provides them.
 * ----------------------------------------------------------------------- */
typedef struct _DSMgr_EventData_t {
    union {
        struct _RESOLUTION_DATA {
            int width;
            int height;
        } resn;

        struct _DFC_DATA {
            int zoomsettings;
        } dfc;

        struct _AUDIOMODE_DATA {
            int type;
            int mode;
        } Audioport;

        struct _HDMI_HPD_DATA {
            int event;
        } hdmi_hpd;

        struct _HDMI_HDCP_DATA {
            int hdcpStatus;
        } hdmi_hdcp;

        struct _HDMI_RXSENSE_DATA {
            int status;
        } hdmi_rxsense;

        struct _HDMI_IN_CONNECT_DATA {
            dsHdmiInPort_t port;
            bool           isPortConnected;
        } hdmi_in_connect;

        struct _HDMI_IN_STATUS_DATA {
            dsHdmiInPort_t port;
            bool           isPresented;
        } hdmi_in_status;

        struct _HDMI_IN_SIG_STATUS_DATA {
            dsHdmiInPort_t         port;
            dsHdmiInSignalStatus_t status;
        } hdmi_in_sig_status;

        struct _HDMI_IN_VIDEO_MODE_DATA {
            dsHdmiInPort_t          port;
            dsVideoPortResolution_t resolution;
        } hdmi_in_video_mode;

        struct _COMPOSITE_IN_CONNECT_DATA {
            dsCompositeInPort_t port;
            bool                isPortConnected;
        } composite_in_connect;

        struct _COMPOSITE_IN_STATUS_DATA {
            dsCompositeInPort_t port;
            bool                isPresented;
        } composite_in_status;

        struct _COMPOSITE_IN_SIG_STATUS_DATA {
            dsCompositeInPort_t    port;
            dsCompInSignalStatus_t status;
        } composite_in_sig_status;

        struct _COMPOSITE_IN_VIDEO_MODE_DATA {
            dsCompositeInPort_t     port;
            dsVideoPortResolution_t resolution;
        } composite_in_video_mode;

        struct _FPD_TIME_FORMAT {
            dsFPDTimeFormat_t eTimeFormat;
        } FPDTimeFormat;

        struct _HDCP_PROTOCOL_DATA {
            dsHdcpProtocolVersion_t protocolVersion;
        } HDCPProtocolVersion;

        struct _SLEEP_MODE_DATA {
            dsSleepMode_t sleepMode;
        } sleepModeInfo;

        struct _AUDIO_LEVEL_DATA {
            int level;
        } AudioLevelInfo;

        struct _AUDIO_OUT_CONNECT_DATA {
            dsAudioPortType_t portType;
            unsigned int      uiPortNo;
            bool              isPortConnected;
        } audio_out_connect;

        struct _AUDIO_FORMAT_DATA {
            dsAudioFormat_t audioFormat;
        } AudioFormatInfo;

        struct _LANGUAGE_DATA {
            char audioLanguage[MAX_LANGUAGE_LEN];
        } AudioLanguageInfo;

        struct _FADER_CONTROL_DATA {
            int mixerbalance;
        } FaderControlInfo;

        struct _ASSOCIATED_AUDIO_MIXING_DATA {
            bool mixing;
        } AssociatedAudioMixingInfo;

        struct _VIDEO_FORMAT_DATA {
            dsHDRStandard_t videoFormat;
        } VideoFormatInfo;

        struct _AUDIO_PORTSTATE_DATA {
            dsAudioPortState_t audioPortState;
        } AudioPortStateInfo;

        struct _HDMI_IN_ALLM_MODE_DATA {
            dsHdmiInPort_t port;
            bool           allm_mode;
        } hdmi_in_allm_mode;

        struct _HDMI_IN_VRR_MODE_DATA {
            dsHdmiInPort_t port;
            dsVRRType_t    vrr_type;
        } hdmi_in_vrr_mode;

        struct _HDMI_IN_CONTENT_TYPE_DATA {
            dsHdmiInPort_t     port;
            dsAviContentType_t aviContentType;
        } hdmi_in_content_type;

        struct _HDMI_IN_AV_LATENCY {
            int audio_output_delay;
            int video_latency;
        } hdmi_in_av_latency;

        struct _ATMOS_CAPS_CHANGE {
            dsATMOSCapability_t caps;
            bool                status;
        } AtmosCapsChange;

        struct _DISPLAY_FRAMERATE_CHANGE {
            char framerate[20];
        } DisplayFrameRateChange;
    } data;
} IARM_Bus_DSMgr_EventData_t;

#ifdef __cplusplus
}
#endif

#endif /* RPDSMGR_H_ */
