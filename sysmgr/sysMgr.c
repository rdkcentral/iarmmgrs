/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2016 RDK Management
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
* @defgroup iarmmgrs
* @{
* @defgroup sysmgr
* @{
**/


#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "safec_lib.h"
#include "libIBus.h"
#include "iarmUtil.h"
#include "sysMgr.h"
#include "mfrMgr.h"
#include "sysMgrInternal.h"
#include <assert.h>
#ifdef ENABLE_SD_NOTIFY
#include <systemd/sd-daemon.h>
#endif

#define UNLOCK_AND_RETURN_IF_ERROR(call) \
    retStatus = (call); \
    if (IARM_RESULT_SUCCESS != retStatus) { \
        pthread_mutex_unlock(&tMutexLock); \
        LOG("IARM call failed, error: %d\n", retStatus); \
        return retStatus; \
    }

static pthread_mutex_t tMutexLock = PTHREAD_MUTEX_INITIALIZER;
static IARM_Bus_SYSMgr_GetSystemStates_Param_t systemStates;
static void _sysEventHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len);
static IARM_Result_t _GetSystemStates(void *arg);
static volatile int initialized = 0;


static const char *ntp_filename = "/tmp/stt_received";
/*Support for HDCP Profile */
static const char *profile_1_filename ="/opt/.hdcp_profile_1";
static int CheckHdcpProfile(void);
static void GetSerialNumber(void);

static IARM_Result_t _SetHDCPProfile(void *arg);
static IARM_Result_t _GetHDCPProfile(void *arg);

/* Functions added to get and set Key Code Logging status*/
static int keyLogStatus = 1;
static int getKeyCodeLoggingPref(void);
static void setKeyCodeLoggingPref(int logStatus);
static IARM_Result_t _GetKeyCodeLoggingPref(void *arg);
static IARM_Result_t _SetKeyCodeLoggingPref(void *arg);

IARM_Result_t SYSMgr_Start()
{
	IARM_Result_t retStatus = IARM_RESULT_INVALID_STATE;
	LOG("Entering [%s] - [%s] - disabling io redirect buf\r\n", __FUNCTION__, IARM_BUS_SYSMGR_NAME);
	setvbuf(stdout, NULL, _IOLBF, 0);

    pthread_mutex_lock(&tMutexLock);
    if (!initialized) {
        LOG("I-ARM Sys Mgr: %d\r\n", __LINE__);
        UNLOCK_AND_RETURN_IF_ERROR(IARM_Bus_Init(IARM_BUS_SYSMGR_NAME));
        UNLOCK_AND_RETURN_IF_ERROR(IARM_Bus_Connect());
        UNLOCK_AND_RETURN_IF_ERROR(IARM_Bus_RegisterEvent(IARM_BUS_SYSMGR_EVENT_MAX));
        UNLOCK_AND_RETURN_IF_ERROR(IARM_Bus_RegisterCall(IARM_BUS_SYSMGR_API_GetSystemStates, _GetSystemStates));
        UNLOCK_AND_RETURN_IF_ERROR(IARM_Bus_RegisterEventHandler(IARM_BUS_SYSMGR_NAME, IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE, _sysEventHandler));
        initialized = 1;
	#ifdef ENABLE_SD_NOTIFY
           sd_notifyf(0, "READY=1\n"
              "STATUS=sysMgr is Successfully Initialized\n"
              "MAINPID=%lu",
              (unsigned long) getpid());
        #endif

#ifdef PID_FILE_PATH
#define xstr(s) str(s)
#define str(s) #s
        // write pidfile because sd_notify() does not work inside container
        IARM_Bus_WritePIDFile(xstr(PID_FILE_PATH) "/sysmgr.pid");
#endif
        //  LOG("I-ARM Sys Mgr: %d\r\n", __LINE__);

	/*HDCP Profile required*/
	UNLOCK_AND_RETURN_IF_ERROR(IARM_Bus_RegisterCall(IARM_BUS_SYSMGR_API_SetHDCPProfile,_SetHDCPProfile));
	UNLOCK_AND_RETURN_IF_ERROR(IARM_Bus_RegisterCall(IARM_BUS_SYSMGR_API_GetHDCPProfile,_GetHDCPProfile));


        UNLOCK_AND_RETURN_IF_ERROR(IARM_Bus_RegisterCall(IARM_BUS_SYSMGR_API_GetKeyCodeLoggingPref,_GetKeyCodeLoggingPref));
	UNLOCK_AND_RETURN_IF_ERROR(IARM_Bus_RegisterCall(IARM_BUS_SYSMGR_API_SetKeyCodeLoggingPref,_SetKeyCodeLoggingPref));
        keyLogStatus = 1;

        systemStates.channel_map = {0};
        systemStates.disconnect_mgr_state = {0};
        systemStates.TuneReadyStatus = {0};
        systemStates.exit_ok_key_sequence = {0};
        systemStates.cmac = {0};
        systemStates.card_moto_entitlements = {0};
        systemStates.card_moto_hrv_rx = {0};
        systemStates.dac_init_timestamp = {0};
        systemStates.card_cisco_status = {0};
        systemStates.video_presenting = {0};
        systemStates.hdmi_out = {0};
        systemStates.hdcp_enabled = {0};
        systemStates.hdmi_edid_read = {0};
        systemStates.firmware_download = {0};
        systemStates.firmware_update_state = {0};
        systemStates.red_recov_state = {0};
        systemStates.time_source = {0};
        systemStates.time_zone_available = {0};
        systemStates.ca_system = {0};
        systemStates.estb_ip = {0};
        systemStates.ecm_ip = {0};
        systemStates.lan_ip = {0};
        systemStates.moca = {0};
        systemStates.docsis = {0};
        systemStates.dsg_broadcast_tunnel = {0};
        systemStates.dsg_ca_tunnel = {0};
        systemStates.cable_card = {0};
        systemStates.cable_card_download = {0};
        systemStates.cvr_subsystem = {0};
        systemStates.download = {0};
        systemStates.vod_ad = {0};
        systemStates.card_serial_no ={0};
        systemStates.ecm_mac ={0};
        systemStates.dac_id = {0};
        systemStates.plant_id = {0};
        systemStates.stb_serial_no={0};
        systemStates.bootup = {0};
        systemStates.hdcp_enabled.state = 1; /*Default HDCP state is enabled.*/
        systemStates.dst_offset = {0};
        systemStates.ip_mode = {0};
        systemStates.qam_ready_status = {0};
        LOG("I-ARM Sys Mgr: %d\r\n", __LINE__);
    }
    else {
      LOG("VENU: I-ARM Sys Mgr Error case: %d\r\n", __LINE__);
      retStatus = IARM_RESULT_INVALID_STATE;
    }
    pthread_mutex_unlock(&tMutexLock);
    return retStatus;
}

IARM_Result_t SYSMgr_Loop()
{
    while(1)
    {
        LOG("I-ARM Sys Mgr: HeartBeat ping.\r\n");
        sleep(2000);
    }
    return IARM_RESULT_SUCCESS;
}


IARM_Result_t SYSMgr_Stop(void)
{
    IARM_Result_t retStatus = IARM_RESULT_INVALID_STATE;
    pthread_mutex_lock(&tMutexLock);
    if (initialized) {
        UNLOCK_AND_RETURN_IF_ERROR(IARM_Bus_Disconnect());
        UNLOCK_AND_RETURN_IF_ERROR(IARM_Bus_Term());
        initialized = 0;
        pthread_mutex_unlock(&tMutexLock);
        pthread_mutex_destroy(&tMutexLock);
    }
    else {
        pthread_mutex_unlock(&tMutexLock);
        retStatus = IARM_RESULT_INVALID_STATE;
    }
    return retStatus;
}

/**
 * @brief This functions sets/updates the HDCP Profile
 *
 * @param callCtx: Context to the caller function
 * @param methodID: Method to be invoke
 * @param arg: Specifies the timeout to be set
 * @param serial: Handshake code to share with
 *
 * @return None 
 */
static int CheckHdcpProfile(void)
{
    struct stat st;
    int ret = lstat(profile_1_filename, &st);

    if (ret >= 0) {
        return 1;
    }
    else {
        return 0;
    }
}

static IARM_Result_t _SetHDCPProfile(void *arg)
{
    IARM_Result_t retStatus = IARM_RESULT_INVALID_STATE;
    IARM_BUS_SYSMGR_HDCPProfileInfo_Param_t *param = (IARM_BUS_SYSMGR_HDCPProfileInfo_Param_t *)arg;
    int new_profile = param->HdcpProfile;

    if ((CheckHdcpProfile() != new_profile) && (new_profile == 0 || new_profile == 1)) {
        int fd = -1;
        if (new_profile == 0) {
            unlink(profile_1_filename);
        }
        else {
            fd = open(profile_1_filename, O_WRONLY|O_CREAT, 0666);
            if (fd >= 0) {
                close(fd);
            }
        }

        {
            __TIMESTAMP(); printf ("The hdcp profile is set to %d\n", new_profile);
			IARM_Bus_SYSMgr_EventData_t eventData;
			eventData.data.hdcpProfileData.hdcpProfile = new_profile;
			__TIMESTAMP(); printf("<<<<<<< Send HDCP Profile UPdate Event is %d>>>>>>>>",eventData.data.hdcpProfileData.hdcpProfile);
			retStatus = IARM_Bus_BroadcastEvent(IARM_BUS_SYSMGR_NAME, (IARM_EventId_t)IARM_BUS_SYSMGR_EVENT_HDCP_PROFILE_UPDATE,(void *)&eventData,sizeof(eventData));
		}
    }
    return retStatus;
}

/**
 * @brief This functions returns the HDCP Profile
 *
 * @param Get current Profile state
 *
 * @return None 
 */
static IARM_Result_t _GetHDCPProfile(void *arg)
{
    IARM_BUS_SYSMGR_HDCPProfileInfo_Param_t *param = (IARM_BUS_SYSMGR_HDCPProfileInfo_Param_t *)arg;
    param->HdcpProfile = CheckHdcpProfile();
    __TIMESTAMP(); printf ("The hdcp profile is %d\n", param->HdcpProfile);
	return IARM_RESULT_SUCCESS;
}



/**
 * @brief This functions returns the SYstem STates
 *
 * @param Data to return
 *
 * @return None 
 */


static IARM_Result_t _GetSystemStates(void *arg)
{
#ifdef USE_MFR_FOR_SERIAL
    GetSerialNumber();
#endif
    pthread_mutex_lock(&tMutexLock);
    static int current_channel_map_state = 0;
    IARM_Bus_SYSMgr_GetSystemStates_Param_t *param = (IARM_Bus_SYSMgr_GetSystemStates_Param_t *)arg;

    if(current_channel_map_state != systemStates.channel_map.state) {
      __TIMESTAMP();LOG("_GetSystemStates return ChannelMapState = %d\r\n", systemStates.channel_map.state);
      current_channel_map_state = systemStates.channel_map.state;
    }

    #ifdef MEDIA_CLIENT
    	systemStates.channel_map.state=2;
    	systemStates.TuneReadyStatus.state=1;
	if(systemStates.time_source.state==0) {
 	   if( access(ntp_filename, F_OK ) != -1 ) {
      	      systemStates.time_source.state=1;
	   }
	}
    #endif
    *param = systemStates;
    pthread_mutex_unlock(&tMutexLock);
    return IARM_RESULT_SUCCESS;
}

static void _sysEventHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len)
{
    errno_t rc = -1;	
	/* Only handle state events */
    if (eventId != IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE) return;

	/*Handle only Sys Manager Events */
	if (strcmp(owner, IARM_BUS_SYSMGR_NAME)  == 0) 
	{
	
		//__TIMESTAMP();LOG("_sysEventHandler invoked ww\r\n");
		pthread_mutex_lock(&tMutexLock);
		IARM_Bus_SYSMgr_EventData_t *sysEventData = (IARM_Bus_SYSMgr_EventData_t*)data;

		IARM_Bus_SYSMgr_SystemState_t stateId = sysEventData->data.systemStates.stateId;
		int state = sysEventData->data.systemStates.state;
		int error = sysEventData->data.systemStates.error;
		char* payload=sysEventData->data.systemStates.payload;
		__TIMESTAMP();LOG("_sysEventHandler invoked for stateid %d of state %d and error %d\r\n", stateId, state,error);
		switch(stateId) {
			case IARM_BUS_SYSMGR_SYSSTATE_CHANNELMAP:
				systemStates.channel_map.state = state;
				systemStates.channel_map.error = error;
				/* memcpy can be replaced with strcpy once the RI is replaced with RMF */
					rc = memcpy_s( systemStates.channel_map.payload,sizeof(systemStates.channel_map.payload), payload, sizeof( systemStates.channel_map.payload ) );
					if(rc!=EOK)
					{
						ERR_CHK(rc);
					}
					memcpy( systemStates.channel_map.payload, payload, sizeof( systemStates.channel_map.payload ) );
				systemStates.channel_map.payload[ sizeof( systemStates.channel_map.payload ) - 1] = 0;
				LOG( "Got  IARM_BUS_SYSMGR_SYSSTATE_CHANNELMAP ID = %s", systemStates.channel_map.payload );				
				break;
			case IARM_BUS_SYSMGR_SYSSTATE_DISCONNECTMGR:
				systemStates.disconnect_mgr_state.state = state;
				systemStates.disconnect_mgr_state.error = error;
				LOG( "Got  IARM_BUS_SYSMGR_SYSSTATE_DISCONNECTMGR state = %d\n", state );				
				break;
			case IARM_BUS_SYSMGR_SYSSTATE_TUNEREADY:
				systemStates.TuneReadyStatus.state = state;
				systemStates.TuneReadyStatus.error = error;
				break;
			case   IARM_BUS_SYSMGR_SYSSTATE_EXIT_OK :
				systemStates.exit_ok_key_sequence.state = state;
				systemStates.exit_ok_key_sequence.error = error;
				break;
			case   IARM_BUS_SYSMGR_SYSSTATE_CMAC :
				systemStates.cmac.state = state;
				systemStates.cmac.error = error;
				break;
			case   IARM_BUS_SYSMGR_SYSSTATE_MOTO_ENTITLEMENT :
				systemStates.card_moto_entitlements.state = state;
				systemStates.card_moto_entitlements.error = error;
				break;
			case   IARM_BUS_SYSMGR_SYSSTATE_MOTO_HRV_RX :
				systemStates.card_moto_hrv_rx.state = state;
				systemStates.card_moto_hrv_rx.error = error;
				break;
			case IARM_BUS_SYSMGR_SYSSTATE_DAC_INIT_TIMESTAMP :
				systemStates.dac_init_timestamp.state = state;
				systemStates.dac_init_timestamp.error = error;
				strncpy(systemStates.dac_init_timestamp.payload,payload,strlen(payload));
				systemStates.dac_init_timestamp.payload[strlen(payload)]='\0';
				printf("systemStates.dac_init_timestamp.payload=%s\n",systemStates.dac_init_timestamp.payload);
				break;
			case   IARM_BUS_SYSMGR_SYSSTATE_CARD_CISCO_STATUS :
				systemStates.card_cisco_status.state = state;
				systemStates.card_cisco_status.error = error;
				break;
			case   IARM_BUS_SYSMGR_SYSSTATE_VIDEO_PRESENTING :
				systemStates.video_presenting.state = state;
				systemStates.video_presenting.error = error;
				break;
			case   IARM_BUS_SYSMGR_SYSSTATE_HDMI_OUT :
				systemStates.hdmi_out.state = state;
				systemStates.hdmi_out.error = error;
				break;
			case   IARM_BUS_SYSMGR_SYSSTATE_HDCP_ENABLED :
				systemStates.hdcp_enabled.state = state;
				systemStates.hdcp_enabled.error = error;
				break;
			case   IARM_BUS_SYSMGR_SYSSTATE_HDMI_EDID_READ :
				systemStates.hdmi_edid_read.state = state;
				systemStates.hdmi_edid_read.error = error;
				break;
			case   IARM_BUS_SYSMGR_SYSSTATE_FIRMWARE_DWNLD :
				systemStates.firmware_download.state = state;
				systemStates.firmware_download.error = error;
				break;
			case   IARM_BUS_SYSMGR_SYSSTATE_FIRMWARE_UPDATE_STATE :
				systemStates.firmware_update_state.state = state;
				systemStates.firmware_update_state.error = error;
				break;
			case   IARM_BUS_SYSMGR_SYSSTATE_RED_RECOV_UPDATE_STATE :
				systemStates.red_recov_state.state = state;
				systemStates.red_recov_state.error = error;
				break;
			case   IARM_BUS_SYSMGR_SYSSTATE_TIME_SOURCE :
				systemStates.time_source.state = state;
				systemStates.time_source.error = error;
				break;
				/*
					payload contains any of the string below for various time zones
					time zone = the string if no dst /if dst

					HST ="HST11" / "HST11HDT,M3.2.0,M11.1.0"
					AKST="AKST" / "AKST09AKDT,M3.2.0,M11.1.0"
					PST = "PST08" / "PST08PDT,M3.2.0,M11.1.0"
					MST = "MST07" / "MST07MDT,M3.2.0,M11.1.0"
					CST = "CST06" / "CST06CDT,M3.2.0,M11.1.0"
					EST = "EST05" / "EST05EDT,M3.2.0,M11.1.0"
				*/
			case   IARM_BUS_SYSMGR_SYSSTATE_TIME_ZONE :
				systemStates.time_zone_available.state = state;
				systemStates.time_zone_available.error = error;
				/* memcpy can be replaced with strcpy once the RI is replaced with RMF */
					rc = memcpy_s( systemStates.time_zone_available.payload,sizeof(systemStates.time_zone_available.payload), payload, sizeof( systemStates.time_zone_available.payload ) );
					if(rc!=EOK)
					{
						ERR_CHK(rc);
					}
				systemStates.time_zone_available.payload[ sizeof( systemStates.time_zone_available.payload ) - 1] = 0;
				LOG( "Got IARM_BUS_SYSMGR_SYSSTATE_TIME_ZONE = %s\n", systemStates.time_zone_available.payload );
				break;
			case   IARM_BUS_SYSMGR_SYSSTATE_CA_SYSTEM :
				systemStates.ca_system.state = state;
				systemStates.ca_system.error = error;
				break;
			case   IARM_BUS_SYSMGR_SYSSTATE_ESTB_IP :
				systemStates.estb_ip.state = state;
				systemStates.estb_ip.error = error;
				break;
			case   IARM_BUS_SYSMGR_SYSSTATE_ECM_IP :
				systemStates.ecm_ip.state = state;
				systemStates.ecm_ip.error = error;
				break;
			case   IARM_BUS_SYSMGR_SYSSTATE_LAN_IP :
				systemStates.lan_ip.state = state;
				systemStates.lan_ip.error = error;
				break;
			case   IARM_BUS_SYSMGR_SYSSTATE_MOCA :
				systemStates.moca.state = state;
				systemStates.moca.error = error;
				break;
			case   IARM_BUS_SYSMGR_SYSSTATE_DOCSIS :
				systemStates.docsis.state = state;
				systemStates.docsis.error = error;
				break;
			case   IARM_BUS_SYSMGR_SYSSTATE_DSG_BROADCAST_CHANNEL :
				systemStates.dsg_broadcast_tunnel.state = state;
				systemStates.dsg_broadcast_tunnel.error = error;
				break;
			case   IARM_BUS_SYSMGR_SYSSTATE_DSG_CA_TUNNEL :
				systemStates.dsg_ca_tunnel.state = state;
				systemStates.dsg_ca_tunnel.error = error;
				break;
			case   IARM_BUS_SYSMGR_SYSSTATE_CABLE_CARD :
				systemStates.cable_card.state = state;
				systemStates.cable_card.error = error;
				break;
			case   IARM_BUS_SYSMGR_SYSSTATE_CABLE_CARD_DWNLD :
				systemStates.cable_card_download.state = state;
				systemStates.cable_card_download.error = error;
				break;
			case   IARM_BUS_SYSMGR_SYSSTATE_CVR_SUBSYSTEM :
				systemStates.cvr_subsystem.state = state;
				systemStates.cvr_subsystem.error = error;
				break;
			case   IARM_BUS_SYSMGR_SYSSTATE_DOWNLOAD :
				systemStates.download.state = state;
				systemStates.download.error = error;
				break;
			case   IARM_BUS_SYSMGR_SYSSTATE_VOD_AD :
				systemStates.vod_ad.state = state;
				systemStates.vod_ad.error = error;
				/* memcpy can be replaced with strcpy once the RI is replaced with RMF */			
					rc = memcpy_s( systemStates.vod_ad.payload,sizeof(systemStates.vod_ad.payload), payload, sizeof( systemStates.vod_ad.payload ) );
					if(rc!=EOK)
					{
						ERR_CHK(rc);
					}
				systemStates.vod_ad.payload[ sizeof( systemStates.vod_ad.payload ) -1 ] =0;
				LOG( "Got IARM_BUS_SYSMGR_SYSSTATE_VOD_AD = %s\n", systemStates.vod_ad.payload );
				break;
			case IARM_BUS_SYSMGR_SYSSTATE_CABLE_CARD_SERIAL_NO:
				systemStates.card_serial_no.error =error;
				strncpy(systemStates.card_serial_no.payload,payload,strlen(payload));
				systemStates.card_serial_no.payload[strlen(payload)]='\0';
				printf("systemStates.card.serial.no.payload=%s\n",systemStates.card_serial_no.payload);
				break;
			case IARM_BUS_SYSMGR_SYSSTATE_ECM_MAC:
				systemStates.ecm_mac.error =error;
				strncpy(systemStates.ecm_mac.payload,payload,strlen(payload));
				systemStates.ecm_mac.payload[strlen(payload)]='\0';
				printf("systemStates.ecm.mac.payload=%s\n",systemStates.ecm_mac.payload);
				break;
			case   IARM_BUS_SYSMGR_SYSSTATE_DAC_ID :
				systemStates.dac_id.state = state;
				systemStates.dac_id.error = error;
				assert ( ( sizeof(systemStates.dac_id.payload) -1 ) > strlen(payload) );
					rc = strcpy_s( systemStates.dac_id.payload,sizeof(systemStates.dac_id.payload), payload );
					if(rc!=EOK)
					{
						ERR_CHK(rc);
					}
				LOG( "Got IARM_BUS_SYSMGR_SYSSTATE_DAC_ID = %s\n", systemStates.dac_id.payload );
				break;
			case   IARM_BUS_SYSMGR_SYSSTATE_PLANT_ID :
				systemStates.plant_id.state = state;
				systemStates.plant_id.error = error;
				assert ( ( sizeof(systemStates.plant_id.payload) -1 ) > strlen(payload) );
					rc = strcpy_s( systemStates.plant_id.payload,sizeof(systemStates.plant_id.payload), payload );
					if(rc!=EOK)
					{
						ERR_CHK(rc);
					}
				LOG( "Got IARM_BUS_SYSMGR_SYSSTATE_PLANT_ID = %s\n", systemStates.plant_id.payload );
				break;
			case IARM_BUS_SYSMGR_SYSSTATE_STB_SERIAL_NO:
			  systemStates.stb_serial_no.error =error;
			  strncpy(systemStates.stb_serial_no.payload,payload,strlen(payload));
			  systemStates.stb_serial_no.payload[strlen(payload)]='\0';
			  printf("systemStates.stb.serial.payload=%s\n",systemStates.stb_serial_no.payload);
			  break;
			case   IARM_BUS_SYSMGR_SYSSTATE_BOOTUP :				
				systemStates.bootup.state = state;
				systemStates.bootup.error = error;
				break;		    
            case   IARM_BUS_SYSMGR_SYSSTATE_DST_OFFSET :
				systemStates.dst_offset.state = state;
				strncpy( systemStates.dst_offset.payload,payload,strlen(payload));
                systemStates.dst_offset.payload[ strlen(payload) ] = '\0';
				systemStates.dst_offset.error = error;
				break;
          
            case   IARM_BUS_SYSMGR_SYSSTATE_RF_CONNECTED :
				systemStates.rf_connected.state = state;
				systemStates.rf_connected.error = error;
				LOG( "Got IARM_BUS_SYSMGR_SYSSTATE_RF_CONNECTED,State = %d, Error = %d\n", systemStates.rf_connected.state,systemStates.rf_connected.error );
				break;
            case   IARM_BUS_SYSMGR_SYSSTATE_PARTNERID_CHANGE:
				systemStates.partnerid_changed.state = state;
				systemStates.partnerid_changed.error = error;
				LOG( "Got IARM_BUS_SYSMGR_SYSSTATE_PARTNERID_CHANGE ,State = %d, Error = %d\n", systemStates.partnerid_changed.state,systemStates.partnerid_changed.error );
				break;
			case   IARM_BUS_SYSMGR_SYSSTATE_IP_MODE:
				systemStates.ip_mode.state=state;
				systemStates.ip_mode.error =error;
				strncpy(systemStates.ip_mode.payload,payload,strlen(payload));
				systemStates.ip_mode.payload[strlen(payload)]='\0';
				printf("Got IARM_BUS_SYSMGR_SYSSTATE_IP_MODE systemStates.ip_mode.payload=%s\n",systemStates.ip_mode.payload);
				break;				
			case   IARM_BUS_SYSMGR_SYSSTATE_QAM_READY:
				systemStates.qam_ready_status.state = state;
				systemStates.qam_ready_status.error = error;
				LOG( "Got IARM_BUS_SYSMGR_SYSSTATE_QAM_READY,State = %d, Error = %d\n", systemStates.qam_ready_status.state,systemStates.qam_ready_status.error );
				break;				
			default:
				break;
		}
		pthread_mutex_unlock(&tMutexLock);
	}
}

/**
 * @brief This function executes a shell script and returns its value. 
 *
 * @param script_name [in]  Null terminated path name to the script.
 *
 * @param return_value [in] value returned by the system command executing the script.
  *
 * @return None 
 */

void GetSerialNumber(void)
{
    IARM_Bus_MFRLib_GetSerializedData_Param_t param;
    IARM_Result_t iarm_ret = IARM_RESULT_IPCCORE_FAIL;

    pthread_mutex_lock(&tMutexLock);
    if(strlen(systemStates.stb_serial_no.payload) > 0)
    {
        pthread_mutex_unlock(&tMutexLock);
        return;
    } else {
           LOG("STB Serial Number is N.A, release LOCK for IARM_BUS_MFRLIB_API_GetSerializedData response.\n");
    }
    pthread_mutex_unlock(&tMutexLock);

    param.type = mfrSERIALIZED_TYPE_SERIALNUMBER;
    param.bufLen = 0; 
    iarm_ret = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME, IARM_BUS_MFRLIB_API_GetSerializedData, &param, sizeof(param));
    param.buffer[param.bufLen] = '\0';
    pthread_mutex_lock(&tMutexLock);
    if(iarm_ret == IARM_RESULT_SUCCESS)
    {
        memset(systemStates.stb_serial_no.payload, 0, sizeof(systemStates.stb_serial_no.payload));
        strncpy(systemStates.stb_serial_no.payload, param.buffer, sizeof(systemStates.stb_serial_no.payload)-1); /*Limiting the size as that of destination*/
        systemStates.stb_serial_no.error = 0;
    }
    else
    {
        systemStates.stb_serial_no.error = 1;
    }
    pthread_mutex_unlock(&tMutexLock);
}

static int getKeyCodeLoggingPref(void)
{
    int ret = 0;
    ret = keyLogStatus;
    return ret;
}
static void setKeyCodeLoggingPref(int logStatus)
{
    int prevKeyLogStatus = keyLogStatus;

    if(0 == logStatus)
    {
        keyLogStatus = 0;
    }
    else if(1 == logStatus)
    {
        keyLogStatus = 1;
    }
    else
    {
         /*do nothing*/
    }

    if(prevKeyLogStatus != keyLogStatus)
    {
         __TIMESTAMP(); printf ("The Key Code Logging Preference is set to %d\n", keyLogStatus);
	IARM_Bus_SYSMgr_EventData_t eventData;
	eventData.data.keyCodeLogData.logStatus = keyLogStatus;
        __TIMESTAMP(); printf("<<<<<<< Send KEYCODE LOGGING CHANGED Event with log status %d>>>>>>>>", eventData.data.keyCodeLogData.logStatus);
	IARM_Bus_BroadcastEvent(IARM_BUS_SYSMGR_NAME, (IARM_EventId_t)IARM_BUS_SYSMGR_EVENT_KEYCODE_LOGGING_CHANGED,(void *)&eventData,sizeof(eventData));
    }
    else
    {
       __TIMESTAMP(); printf ("Key Code Log status set to previous value itself!!\n");
    }

    return;
}

static IARM_Result_t _GetKeyCodeLoggingPref(void *arg)
{
    IARM_BUS_SYSMGR_KEYCodeLoggingInfo_Param_t *param = (IARM_BUS_SYSMGR_KEYCodeLoggingInfo_Param_t *)arg;

    param->logStatus = getKeyCodeLoggingPref();

    __TIMESTAMP(); printf ("_GetKeyCodeLoggingPref : Key Code Log status is %d\n", param->logStatus);
    return IARM_RESULT_SUCCESS;

}
static IARM_Result_t _SetKeyCodeLoggingPref(void *arg)
{
    IARM_BUS_SYSMGR_KEYCodeLoggingInfo_Param_t *param = (IARM_BUS_SYSMGR_KEYCodeLoggingInfo_Param_t *)arg;

    __TIMESTAMP(); printf ("_SetKeyCodeLoggingPref : Key Code Log status is %d\n", param->logStatus);
    setKeyCodeLoggingPref(param->logStatus);

    return IARM_RESULT_SUCCESS;
}



/** @} */
/** @} */
