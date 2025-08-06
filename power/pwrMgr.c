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
* @defgroup power
* @{
**/


#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C"
{
#endif
#ifdef __cplusplus
}
#endif

#ifdef OFFLINE_MAINT_REBOOT
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <sys/ioctl.h>
#include <net/if.h>
#endif

#include <sys/stat.h>
#include "iarmUtil.h"
#include "sysMgr.h"
#include "libIBus.h"
#include "plat_power.h"
#include "pwrMgrInternal.h"
#include <pthread.h>
#include "libIBusDaemon.h"
#include "manager.hpp"
#include "host.hpp"
#include "list.hpp"
#include "sleepMode.hpp"
#include "pwrlogger.h"
#include "frontPanelIndicator.hpp"
#include "resetModes.h"
#include "rfcapi.h"
#include "deepSleepMgr.h"
#include "exception.hpp"
#include "dsRpc.h"
#include "dsMgr.h"

#include <secure_wrapper.h>
#include <thread>

/* For glib APIs*/
#include <glib.h>

#include <assert.h>
#include <stdint.h>

/* Check for Y2K38_SAFETY - Portable static assertion */
#if defined(__cplusplus)
#define STATIC_ASSERT(COND, MSG) static_assert(COND, #MSG)
STATIC_ASSERT(sizeof(double) == 8, double_must_be_8_bytes);
STATIC_ASSERT(sizeof(time_t) >= 8, time_t_must_be_at_least_8_bytes);
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define STATIC_ASSERT(COND, MSG) _Static_assert(COND, #MSG)
STATIC_ASSERT(sizeof(double) == 8, double_must_be_8_bytes);
STATIC_ASSERT(sizeof(time_t) >= 8, time_t_must_be_at_least_8_bytes);
#else
#define STATIC_ASSERT(COND, MSG) typedef char static_assertion_##MSG[(COND)?1:-1]
STATIC_ASSERT(sizeof(double) == 8, double_must_be_8_bytes);
STATIC_ASSERT(sizeof(time_t) >= 8, time_t_must_be_at_least_8_bytes);
#endif

#define PADDING_SIZE 32
#define _UIMGR_SETTINGS_MAGIC 0xFEBEEFAC
/*LED settings*/
typedef struct _PWRMgr_LED_Settings_t {
    unsigned int brightness;
    unsigned int color;
} PWRMgr_LED_Settings_t;

typedef struct _PWRMgr_Settings_t {
    uint32_t magic;
    uint32_t version;
    uint32_t length;
    volatile PWRMgr_PowerState_t powerState;
    PWRMgr_LED_Settings_t ledSettings;
#ifdef ENABLE_DEEP_SLEEP
    uint32_t deep_sleep_timeout;
#endif
    bool nwStandbyMode;
    char padding[PADDING_SIZE];
} PWRMgr_Settings_t;

typedef enum _UIDev_PowerState_t {
    UIDEV_POWERSTATE_OFF,
    UIDEV_POWERSTATE_STANDBY,
    UIDEV_POWERSTATE_ON,
    UIDEV_POWERSTATE_UNKNOWN
} UIDev_PowerState_t;

typedef struct _UIMgr_Settings_t {
    uint32_t magic;
    uint32_t version;
    uint32_t length;
    UIDev_PowerState_t powerState;
    char padding[PADDING_SIZE];
} UIMgr_Settings_t ;

static PWRMgr_Settings_t m_settings = {0};
static const char *m_settingsFile = NULL;

#define MAX_NUM_VIDEO_PORTS 5
typedef struct {
    char port[PWRMGR_MAX_VIDEO_PORT_NAME_LENGTH];
    bool isEnabled;
} PWRMgr_Standby_Video_State_t;
static PWRMgr_Standby_Video_State_t g_standby_video_port_setting[MAX_NUM_VIDEO_PORTS];

static pthread_mutex_t  wareHouseOpsMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_t asyncPowerWarehouseOpsThreadId = NULL;


static void _sleepModeChangeHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len);
static void _systemStateChangeHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len);
IARM_Result_t _SetPowerState(void *arg);
static IARM_Result_t _GetPowerState(void *arg);
static IARM_Result_t _WareHouseReset(void *arg);
static IARM_Result_t _WareHouseClear(void *arg);
static IARM_Result_t _ColdFactoryReset(void *);
static IARM_Result_t _FactoryReset(void *);
static IARM_Result_t _UserFactoryReset(void *);
static IARM_Result_t _GetPowerStateBeforeReboot(void *arg);
static IARM_Result_t _HandleReboot(void *arg);
static IARM_Result_t _SetWakeupSrcConfig(void *arg);
static IARM_Result_t _GetWakeupSrcConfig(void *arg);
void _handleDeepsleepTimeoutWakeup();
void  handleDeepsleepTimeoutWakeup ();
static void*  deepsleepTimeoutWakeupThread (void * arg);
static int _InitSettings(const char *settingsFile);
static int _WriteSettings(const char *settingsFile);
static void _DumpSettings(const PWRMgr_Settings_t *pSettings);
static PWRMgr_PowerState_t _ConvertUIDevToIARMBusPowerState(UIDev_PowerState_t powerState);
static int ecm_connectivity_lost = 0;
dsSleepMode_t  m_sleepMode = dsHOST_SLEEP_MODE_LIGHT;
time_t xre_timer; // Hack to fix DELIA-11393
static bool deepSleepWakeup = false;
static uint32_t pwrMode=0;
extern void IARM_Bus_PWRMGR_RegisterSleepTimerAPIs(void *);

/*EAS handling */
static IARM_Result_t _SysModeChange(void *arg);
static IARM_Bus_Daemon_SysMode_t isEASInProgress = IARM_BUS_SYS_MODE_NORMAL;

/*pwrMgr Glib variables */
GMainLoop *pwrMgr_Gloop = NULL;
static gboolean heartbeatMsg(gpointer data);
std::string powerStateBeforeReboot_gc;
static PWRMgr_PowerState_t g_last_known_power_state = PWRMGR_POWERSTATE_ON;

#ifdef ENABLE_DEEP_SLEEP
/* PwrMgr Static Functions for Deep SLeep feature */
extern IARM_Result_t GetPwrMgrDeepSleepStatus(int *status);
extern IARM_Result_t PwrMgrDeepSleepWakeup(IARM_Bus_CommonAPI_PowerPreChange_Param_t *arg);
extern void IARM_Bus_PWRMGR_RegisterDeepSleepAPIs();
extern void HandleDeepSleepStateChange(void *data);
extern void PwrMgrDeepSleepTimeout();
#
/* IARM RPC Handler to Set Deep Sleep Wakeup Timer */
static IARM_Result_t _SetDeepSleepTimeOut(void *arg);

/* Gloop Wakeup Timer Handler */
static gboolean deep_sleep_wakeup_fn(gpointer data);

/* Calculate Wakeup Time */
static uint32_t getWakeupTime();

/* Gloop Handler to invoke deep sleep , if the box boots in deep sleep and timer expires.*/
static gboolean invoke_deep_sleep_on_bootup_timeout(gpointer data);

/* Variables for Deep Sleep */
static uint32_t deep_sleep_wakeup_timeout_sec = 28800; //8*60*60 - 8 hours
static uint8_t IsWakeupTimerSet = 0;
static guint wakeup_event_src = 0;
static guint dsleep_bootup_event_src = 0;
static time_t timeAtDeepSleep = 0;
#endif  // END OF #ifdef ENABLE_DEEP_SLEEP

static bool nwStandbyMode_gs = false;
#ifdef OFFLINE_MAINT_REBOOT
gint64 standby_time = 0;
static bool rfcUpdated = false;
static int standby_reboot_threshold = 86400*3;
static int force_reboot_threshold = 172800*3;

#define MAX_RFC_LEN 15

#define UPTIME_ABOVE_STANDBY_REBOOT(time) (time >= standby_reboot_threshold)
#define UPTIME_ABOVE_FORCE_REBOOT(time)   (time >= force_reboot_threshold)
#define REBOOT_GRACE_INTERVAL(u, s)       ((u - s) >= 900)

#define STANDBY_REBOOT_ENABLE "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.StandbyReboot.Enable"
#define STANDBY_REBOOT        "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.StandbyReboot.StandbyAutoReboot"
#define FORCE_REBOOT          "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.StandbyReboot.ForceAutoReboot"
#endif

static IARM_Result_t _SetNetworkStandbyMode(void *arg);
static IARM_Result_t _GetNetworkStandbyMode(void *arg);

static void setPowerStateBeforeReboot (PWRMgr_PowerState_t powerState) {
    switch (powerState) {
    case PWRMGR_POWERSTATE_OFF:
        powerStateBeforeReboot_gc = std::string ("OFF");
        break;
    case PWRMGR_POWERSTATE_STANDBY:
        powerStateBeforeReboot_gc = std::string ("STANDBY");
        break;
    case PWRMGR_POWERSTATE_ON:
        powerStateBeforeReboot_gc = std::string ("ON");
        break;
    case PWRMGR_POWERSTATE_STANDBY_LIGHT_SLEEP:
        powerStateBeforeReboot_gc = std::string ("LIGHT_SLEEP");
        break;
    case PWRMGR_POWERSTATE_STANDBY_DEEP_SLEEP:
        powerStateBeforeReboot_gc = std::string ("DEEP_SLEEP");
        break;
    default :
        powerStateBeforeReboot_gc = std::string ("UNKNOWN");
        break;
    }
    printf ("[%s]:[%d] powerStateBeforeReboot: %s \n", __FUNCTION__, __LINE__, powerStateBeforeReboot_gc.c_str());
}

IARM_Result_t PWRMgr_Start(int argc, char *argv[])
{
    IARM_Result_t iarmStatus;
    char *settingsFile = NULL;
    time(&xre_timer); // Hack to fix DELIA-11393

    if (argc == 2) settingsFile = argv[1];

    setvbuf(stdout, NULL, _IOLBF, 0);
    LOG("Entering [%s] - [%s] - disabling io redirect buf\r\n", __FUNCTION__,IARM_BUS_PWRMGR_NAME);


    PLAT_INIT();
#ifdef ENABLE_DEEP_SLEEP
    PLAT_DS_INIT();
#endif
    iarmStatus = IARM_Bus_Init(IARM_BUS_PWRMGR_NAME);
    if (iarmStatus != IARM_RESULT_SUCCESS) {
        LOG("IARM_Bus_Init failed with status %d\r\n", iarmStatus);
        return iarmStatus;
    }
    iarmStatus = IARM_Bus_Connect();
    if (iarmStatus != IARM_RESULT_SUCCESS) {
        LOG("IARM_Bus_Connect failed with status %d\r\n", iarmStatus);
        return iarmStatus;
    }
#if 0
    /* LOG("Initing PwrMgr Settings START\r\n");*/
    try {
        device::Manager::Initialize();
    }
    catch (...) {
        LOG("Exception Caught during [device::Manager::Initialize]\r\n");
    }
#endif
    _InitSettings(settingsFile);
    iarmStatus = IARM_Bus_RegisterEvent(IARM_BUS_PWRMGR_EVENT_MAX);
    if (iarmStatus != IARM_RESULT_SUCCESS) {
        LOG("IARM_Bus_RegisterEvent failed with status %d\r\n", iarmStatus);
        return iarmStatus;
    }
    iarmStatus = IARM_Bus_RegisterCall(IARM_BUS_PWRMGR_API_Reboot, _HandleReboot);
    if (iarmStatus != IARM_RESULT_SUCCESS) {
        LOG("IARM_Bus_RegisterCall failed with status %d\r\n", iarmStatus);
        return iarmStatus;
    }
    iarmStatus = IARM_Bus_RegisterCall(IARM_BUS_PWRMGR_API_SetPowerState, _SetPowerState);
    if (iarmStatus != IARM_RESULT_SUCCESS) {
        LOG("IARM_Bus_RegisterCall failed with status %d\r\n", iarmStatus);
        return iarmStatus;
    }
    iarmStatus = IARM_Bus_RegisterCall(IARM_BUS_PWRMGR_API_GetPowerState, _GetPowerState);
    if (iarmStatus != IARM_RESULT_SUCCESS) {
        LOG("IARM_Bus_RegisterCall failed with status %d\r\n", iarmStatus);
        return iarmStatus;
    }
    iarmStatus = IARM_Bus_RegisterCall(IARM_BUS_PWRMGR_API_WareHouseReset, _WareHouseReset);
    if (iarmStatus != IARM_RESULT_SUCCESS) {
        LOG("IARM_Bus_RegisterCall failed with status %d\r\n", iarmStatus);
        return iarmStatus;
    }
    iarmStatus = IARM_Bus_RegisterCall(IARM_BUS_PWRMGR_API_WareHouseClear, _WareHouseClear);
    if (iarmStatus != IARM_RESULT_SUCCESS) {
        LOG("IARM_Bus_RegisterCall failed with status %d\r\n", iarmStatus);
        return iarmStatus;
    }

    iarmStatus = IARM_Bus_RegisterCall(IARM_BUS_PWRMGR_API_ColdFactoryReset, _ColdFactoryReset);
    if (iarmStatus != IARM_RESULT_SUCCESS) {
        LOG("IARM_Bus_RegisterCall failed with status %d\r\n", iarmStatus);
        return iarmStatus;
    }
    iarmStatus = IARM_Bus_RegisterCall(IARM_BUS_PWRMGR_API_FactoryReset, _FactoryReset);
    if (iarmStatus != IARM_RESULT_SUCCESS) {
        LOG("IARM_Bus_RegisterCall failed with status %d\r\n", iarmStatus);
        return iarmStatus;
    }
    iarmStatus = IARM_Bus_RegisterCall(IARM_BUS_PWRMGR_API_UserFactoryReset, _UserFactoryReset);
    if (iarmStatus != IARM_RESULT_SUCCESS) {
        LOG("IARM_Bus_RegisterCall failed with status %d\r\n", iarmStatus);
        return iarmStatus;
    }
#ifdef ENABLE_DEEP_SLEEP
    iarmStatus = IARM_Bus_RegisterCall(IARM_BUS_PWRMGR_API_SetDeepSleepTimeOut, _SetDeepSleepTimeOut);
    if (iarmStatus != IARM_RESULT_SUCCESS) {
        LOG("IARM_Bus_RegisterCall failed with status %d\r\n", iarmStatus);
        return iarmStatus;
    }
#endif
    iarmStatus = IARM_Bus_RegisterCall(IARM_BUS_PWRMGR_API_SetNetworkStandbyMode, _SetNetworkStandbyMode);
    if (iarmStatus != IARM_RESULT_SUCCESS) {
        LOG("IARM_Bus_RegisterCall failed with status %d\r\n", iarmStatus);
        return iarmStatus;
    }
    iarmStatus = IARM_Bus_RegisterCall(IARM_BUS_PWRMGR_API_GetNetworkStandbyMode, _GetNetworkStandbyMode);
    if (iarmStatus != IARM_RESULT_SUCCESS) {
        LOG("IARM_Bus_RegisterCall failed with status %d\r\n", iarmStatus);
        return iarmStatus;
    }
    iarmStatus = IARM_Bus_RegisterCall(IARM_BUS_PWRMGR_API_SetWakeupSrcConfig, _SetWakeupSrcConfig);
    if (iarmStatus != IARM_RESULT_SUCCESS) {
        LOG("IARM_Bus_RegisterCall failed with status %d\r\n", iarmStatus);
        return iarmStatus;
    }
    iarmStatus = IARM_Bus_RegisterCall(IARM_BUS_PWRMGR_API_GetWakeupSrcConfig, _GetWakeupSrcConfig);
    if (iarmStatus != IARM_RESULT_SUCCESS) {
        LOG("IARM_Bus_RegisterCall failed with status %d\r\n", iarmStatus);
        return iarmStatus;
    }
    iarmStatus = IARM_Bus_RegisterEventHandler(IARM_BUS_SYSMGR_NAME, IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE, _systemStateChangeHandler);
    if (iarmStatus != IARM_RESULT_SUCCESS) {
        LOG("IARM_Bus_RegisterEventHandler failed with status %d\r\n", iarmStatus);
        return iarmStatus;
    }
    iarmStatus = IARM_Bus_RegisterEventHandler(IARM_BUS_DSMGR_NAME,IARM_BUS_DSMGR_EVENT_SLEEP_MODE_CHANGED,_sleepModeChangeHandler);
    if (iarmStatus != IARM_RESULT_SUCCESS) {
        LOG("IARM_Bus_RegisterEventHandler failed with status %d\r\n", iarmStatus);
        return iarmStatus;
    }
    /*Register EAS handler so that we can ensure audio settings for EAS */
    iarmStatus = IARM_Bus_RegisterCall(IARM_BUS_COMMON_API_SysModeChange,_SysModeChange);
    if (iarmStatus != IARM_RESULT_SUCCESS) {
        LOG("IARM_Bus_RegisterCall failed with status %d\r\n", iarmStatus);
        return iarmStatus;
    }
    iarmStatus = IARM_Bus_RegisterCall(IARM_BUS_PWRMGR_API_GetPowerStateBeforeReboot, _GetPowerStateBeforeReboot);
    if (iarmStatus != IARM_RESULT_SUCCESS) {
        LOG("IARM_Bus_RegisterCall failed with status %d\r\n", iarmStatus);
        return iarmStatus;
    }
#ifdef ENABLE_THERMAL_PROTECTION
    initializeThermalProtection();
#endif //ENABLE_THERMAL_PROTECTION

#ifdef ENABLE_DEEP_SLEEP
    IARM_Bus_PWRMGR_RegisterDeepSleepAPIs();
#endif

    /* Create Main loop for Power Manager */
    pwrMgr_Gloop = g_main_loop_new ( NULL, FALSE );
    if(pwrMgr_Gloop != NULL) {
        g_timeout_add_seconds (300, heartbeatMsg, pwrMgr_Gloop);
    }
    else {
        LOG("Fails to Create a main Loop for [%s] \r\n",IARM_BUS_PWRMGR_NAME);
    }

    IARM_Bus_PWRMGR_RegisterSleepTimerAPIs(pwrMgr_Gloop);
    char *rdk_deep_sleep_wakeup = getenv("RDK_DEEPSLEEP_WAKEUP_ON_POWER_BUTTON");
    deepSleepWakeup = (rdk_deep_sleep_wakeup && atoi(rdk_deep_sleep_wakeup));

    try
    {
            m_sleepMode = static_cast<dsSleepMode_t>(device::Host::getInstance().getPreferredSleepMode().getId());
            printf ("initial value m_sleepMode:%d \n", m_sleepMode);
    }
    catch(...)
    {
        LOG("PwrMgr: Exception coughht while processing getPreferredSleepMode\r\n");
    }
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t PWRMgr_Loop()
{

    /* Power Mgr loop */
    if(pwrMgr_Gloop)
    {
        g_main_loop_run (pwrMgr_Gloop);
        g_main_loop_unref(pwrMgr_Gloop);
    }
    return IARM_RESULT_SUCCESS;
}

#ifdef OFFLINE_MAINT_REBOOT
bool isStandbyRebootEnabled()
{
    RFC_ParamData_t rfcParam;
    char* key = STANDBY_REBOOT_ENABLE;
    if (WDMP_SUCCESS == getRFCParameter("PwrMgr", key, &rfcParam))
    {
        return (strncasecmp(rfcParam.value, "true", 4) == 0);
    }

    return false;
}

int getStandbyRebootValue(char* key)
{
    RFC_ParamData_t param;
    char rfcVal[MAX_RFC_LEN+1] = {0};
    int len = 0;

    if (WDMP_SUCCESS == getRFCParameter("PwrMgr", key, &param))
    {
        len = strlen(param.value);
        if (len > MAX_RFC_LEN)
        {
            len = MAX_RFC_LEN;
        }

        if ( (param.value[0] == '"') && (param.value[len] == '"'))
        {
            strncpy (rfcVal, &param.value[1], len - 1);
            rfcVal[len] = '\0';
        }
        else
        {
            strncpy (rfcVal, param.value, MAX_RFC_LEN-1);
            rfcVal[len] = '\0';
        }
        return atoi(rfcVal);
    }

    return -1;
}

bool isOfflineMode()
{
#if 0
    struct stat buf;
    return ((stat("/tmp/addressaquired_ipv4", &buf) != 0)
            && (stat("/tmp/addressaquired_ipv6", &buf) != 0));
#endif

    struct ifaddrs *ifAddr, *ifAddrIt;
    bool offline = true;

    getifaddrs(&ifAddr);
    for (ifAddrIt = ifAddr; ifAddrIt != NULL; ifAddrIt = ifAddrIt->ifa_next)
    {
        if (NULL != ifAddrIt->ifa_addr
                && (!strcmp(ifAddrIt->ifa_name, "eth0:0") || !strcmp(ifAddrIt->ifa_name, "wlan0:0")))
        {
            LOG("ifa_name=%s sa_family=%s ifa_flags=0x%X\n",
                ifAddrIt->ifa_name,
                ifAddrIt->ifa_addr->sa_family == AF_INET? "AF_INET" : ifAddrIt->ifa_addr->sa_family == AF_INET6? "AF_INET6" : "None",
                (ifAddrIt->ifa_flags & IFF_RUNNING) );
        }

        if (NULL != ifAddrIt->ifa_addr
                && (ifAddrIt->ifa_addr->sa_family == AF_INET || ifAddrIt->ifa_addr->sa_family == AF_INET6)
                && (!strcmp(ifAddrIt->ifa_name, "eth0:0") || !strcmp(ifAddrIt->ifa_name, "wlan0:0"))
                && (ifAddrIt->ifa_flags & IFF_RUNNING))
        {
            offline = false;
        }
    }
    freeifaddrs(ifAddr);

    return offline;
}

bool isInStandby()
{
    PWRMgr_Settings_t *pSettings = &m_settings;
    PWRMgr_PowerState_t curState = pSettings->powerState;
    LOG("%s PowerState = %d\n", __func__, curState);
    return  (curState == PWRMGR_POWERSTATE_STANDBY
             || curState == PWRMGR_POWERSTATE_STANDBY_LIGHT_SLEEP);
}

#endif

/* coverity[ignore : Y2K38_SAFETY] see the assert check at the top which prevents overflow. */
static gboolean heartbeatMsg(gpointer data)
{
    LOG("I-ARM POWER Mgr: HeartBeat ping.\r\n");

#ifdef OFFLINE_MAINT_REBOOT
    if (!rfcUpdated)
    {
        LOG("StandbyReboot.Enable = %s\n", isStandbyRebootEnabled() ? "true" : "false");

        standby_reboot_threshold = getStandbyRebootValue(STANDBY_REBOOT);
        if (standby_reboot_threshold == -1)
        {
            standby_reboot_threshold = 86400*3;
        }
        LOG("StandbyReboot.StandbyAutoReboot = %d\n", standby_reboot_threshold);

        force_reboot_threshold = getStandbyRebootValue(FORCE_REBOOT);
        if (force_reboot_threshold == -1)
        {
            force_reboot_threshold = 172800*3;
        }
        LOG("StandbyReboot.ForceAutoReboot = %d\n", force_reboot_threshold);
        rfcUpdated = true;
    }

    if (isStandbyRebootEnabled())
    {
        gint64 uptime = g_get_monotonic_time()/G_USEC_PER_SEC;
        if (UPTIME_ABOVE_STANDBY_REBOOT(uptime))
        {
            if (REBOOT_GRACE_INTERVAL(uptime, standby_time) && isInStandby())
            {
                LOG("Going to reboot after %lld\n", uptime);
                sleep(10);
                system("sh /rebootNow.sh -s PwrMgr -o 'Standby Maintenance reboot'");
            }

            if (UPTIME_ABOVE_FORCE_REBOOT(uptime))
            {
                LOG("Going to reboot after %lld\n", uptime);
                sleep(10);
                system("sh /rebootNow.sh -s PwrMgr -o 'Forced Maintenance reboot'");
            }
        }
    }
#endif

    return TRUE;
}


IARM_Result_t PWRMgr_Stop(void)
{
    IARM_Result_t iarmStatus;
    if(pwrMgr_Gloop)
    {
        g_main_loop_quit(pwrMgr_Gloop);
    }

    try {
        device::Manager::DeInitialize();
    }
    catch (...) {
        LOG("Exception Caught during [device::Manager::Initialize]\r\n");
    }

    iarmStatus = IARM_Bus_Disconnect();
    if (IARM_RESULT_SUCCESS != iarmStatus) {
        LOG("IARM_Bus_Disconnect failed with status %d\r\n", iarmStatus);
        return iarmStatus;
    }
    iarmStatus = IARM_Bus_Term();
    if (IARM_RESULT_SUCCESS != iarmStatus) {
        LOG("IARM_Bus_Term failed with status %d\r\n", iarmStatus);
        return iarmStatus;
    }
    PLAT_TERM();
#ifdef ENABLE_DEEP_SLEEP
    PLAT_DS_TERM();
#endif
    return IARM_RESULT_SUCCESS;
}

void _sleepModeChangeHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len)
{
    if (strcmp(owner,IARM_BUS_DSMGR_NAME) == 0)
    {
        switch (eventId) {
        case IARM_BUS_DSMGR_EVENT_SLEEP_MODE_CHANGED:
        {
            IARM_Bus_DSMgr_EventData_t *eventData = (IARM_Bus_DSMgr_EventData_t *)data;
            m_sleepMode = eventData->data.sleepModeInfo.sleepMode;
            printf("%s  m_sleepMode :%d \n",__FUNCTION__,m_sleepMode);
        }
        break;

        default:
            break;
        }
    }
}

static void _systemStateChangeHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len)
{
    IARM_Bus_SYSMgr_EventData_t *sysEventData = (IARM_Bus_SYSMgr_EventData_t*)data;
    int state;
    int error;
    FILE *fp;
    unsigned long secs   = 0;
    IARM_Bus_SYSMgr_SystemState_t stateId = sysEventData->data.systemStates.stateId;
    state = sysEventData->data.systemStates.state;
    error = sysEventData->data.systemStates.error;

    if (strcmp(owner, IARM_BUS_SYSMGR_NAME)  == 0)
    {
        switch(stateId)
        {
        case IARM_BUS_SYSMGR_SYSSTATE_DSG_CA_TUNNEL:
        case IARM_BUS_SYSMGR_SYSSTATE_ECM_IP:
            if(1 == error)
            {
                fp = fopen("/proc/uptime", "r");
                if (NULL != fp)
                {
                    setvbuf(fp, (char *) NULL, _IONBF, 0);
                    fseek(fp, 0, SEEK_SET);
                    if(0 > fscanf(fp, "%ld", &secs))
                    {
                        LOG("Error: fscanf on uptime failed \r\n");
                        /*Proc is not accessible(may never happen) taken as ecm connection lost */
                        ecm_connectivity_lost = 1;
                    }
                    fclose(fp);
                }
                else
                {
                    LOG("Error: fopen on uptime failed \r\n");
                    /*Proc is not up(may never happen) taken as ecm connection lost */
                    ecm_connectivity_lost = 1;
                }
                /* Refer DELIA-6512 */
                if(secs > 300)
                {
                    if(0 == ecm_connectivity_lost)
                    {
                        if (stateId == IARM_BUS_SYSMGR_SYSSTATE_ECM_IP)
                        {
                            LOG("[PwrMgr] ECM connectivity Lost on ECM_IP Event..\r\n");
                            ecm_connectivity_lost = 1;
                        }
                        else if ((stateId == IARM_BUS_SYSMGR_SYSSTATE_DSG_CA_TUNNEL) && (0 == state))
                        {
                            LOG("[PwrMgr] ECM connectivity Lost on DSG_CA_TUNNEL Event ..\r\n");
                            ecm_connectivity_lost = 1;
                        }
                    }
                }
            }
            else if(0 == error)
            {
                /* Refer DELIA-6512 */
                if(1 == ecm_connectivity_lost)
                {
                    if (stateId == IARM_BUS_SYSMGR_SYSSTATE_ECM_IP)
                    {
                        LOG("[PwrMgr] ECM connectivity recovered on ECM_IP Event..\r\n");
                        ecm_connectivity_lost = 0;
                    }
                    else if ((stateId == IARM_BUS_SYSMGR_SYSSTATE_DSG_CA_TUNNEL) && (2 == state))
                    {
                        LOG("[PwrMgr] ECM connectivity recovered on DSG_CA_TUNNEL Event..\r\n");
                        ecm_connectivity_lost = 0;
                    }
                }
            }
            break;

        default:
            break;
        }
    }
}

IARM_Result_t _SetPowerState(void *arg)
{
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    IARM_Bus_PWRMgr_SetPowerState_Param_t *param = (IARM_Bus_PWRMgr_SetPowerState_Param_t *)arg;
    PWRMgr_Settings_t *pSettings = &m_settings;
    IARM_Bus_CommonAPI_PowerPreChange_Param_t powerPreChangeParam;
    PWRMgr_PowerState_t newState = (PWRMgr_PowerState_t)param->newState;
    PWRMgr_PowerState_t curState = pSettings->powerState;    /* Notify application of Power Changes */
    static const char *powerstateString[5] = {"OFF","STANDBY","ON", "LIGHTSLEEP", "DEEPSLEEP"};

    if (access("/tmp/ignoredeepsleep", F_OK) == 0) {
        if(newState == PWRMGR_POWERSTATE_STANDBY_DEEP_SLEEP) {
            LOG(" Ignoring DEEPSLEEP state due to tmp override /tmp/ignoredeepsleep \r\n");
            return IARM_RESULT_INVALID_STATE;
        }
    }

    LOG("[%s:%d] Entering...\r\n",__FUNCTION__,__LINE__);

    if(curState != newState) {

#ifdef ENABLE_DEEP_SLEEP
        /* * When Changing from Deep sleep wakeup
         * Notify Deep sleep manager first followed by
         * Power pre change call.
         */
        //Changing power state ,Remove event source
        if(dsleep_bootup_event_src)
        {
            g_source_remove(dsleep_bootup_event_src);
            dsleep_bootup_event_src = 0;
            __TIMESTAMP();
            LOG("Removed Deep sleep boot up event Time source %d  \r\n",dsleep_bootup_event_src);
        }
        if(  (PWRMGR_POWERSTATE_STANDBY_DEEP_SLEEP == curState)
                && (PWRMGR_POWERSTATE_STANDBY_DEEP_SLEEP != newState))
        {
            IARM_Result_t rpcRet = IARM_RESULT_SUCCESS;
            int dsStatus= 0;
            rpcRet = GetPwrMgrDeepSleepStatus(&dsStatus);
            if((DeepSleepStatus_InProgress == dsStatus) || (IARM_RESULT_SUCCESS != rpcRet))
            {
                LOG("%s  deepsleep in  progress  ignoreing the request dsStatus %d rpcRet :%d\r\n",__FUNCTION__,dsStatus,rpcRet);
                return retCode;
            }
            IARM_Bus_CommonAPI_PowerPreChange_Param_t deepSleepWakeupParam;
            __TIMESTAMP();
            LOG("Waking up from Deep Sleep.. \r\n");
            deepSleepWakeupParam.curState = (IARM_Bus_PWRMgr_PowerState_t)curState;
            deepSleepWakeupParam.newState = (IARM_Bus_PWRMgr_PowerState_t)newState;

            /* Notify Deep sleep manager on Power Mode change */
            PwrMgrDeepSleepWakeup(&deepSleepWakeupParam);
        }
#endif

        __TIMESTAMP();
        LOG("Power Mode Change from %s to %s start\n",powerstateString[curState],powerstateString[newState]);

        powerPreChangeParam.newState = (IARM_Bus_PWRMgr_PowerState_t)param->newState;
        powerPreChangeParam.curState = (IARM_Bus_PWRMgr_PowerState_t)pSettings->powerState;
        IARM_BusDaemon_PowerPrechange(powerPreChangeParam);
        __TIMESTAMP();
        LOG("Power Mode Change from %s to %s end\n",powerstateString[curState],powerstateString[newState]);
#ifdef OFFLINE_MAINT_REBOOT
        if(newState != PWRMGR_POWERSTATE_ON) {

            standby_time = g_get_monotonic_time()/G_USEC_PER_SEC;
            LOG("Power state changed at %lld\n", standby_time);
        }
#endif

        pSettings->powerState = newState;
        _WriteSettings(m_settingsFile);
        /* Independent of Deep sleep */
        PLAT_API_SetPowerState(newState);
        /*  * Power Change Event
         * Used by Deep sleep and HDMI CEC.
         */
        LOG("[PwrMgr] Post Power Mode Change Event \r\n");
        {
            IARM_Bus_PWRMgr_EventData_t *pwrModeEventData = NULL;
            pwrModeEventData = (IARM_Bus_PWRMgr_EventData_t *)malloc(sizeof(IARM_Bus_PWRMgr_EventData_t));

            if(pwrModeEventData == NULL)
            {
                LOG("[%s:%d] Failed due to memory allocation. \r\n",__FUNCTION__,__LINE__);
                return IARM_RESULT_OOM;
            }

            memset(pwrModeEventData, 0, sizeof(IARM_Bus_PWRMgr_EventData_t));
            pwrModeEventData->data.state.curState = (IARM_Bus_PowerState_t)curState;
            pwrModeEventData->data.state.newState = (IARM_Bus_PowerState_t)newState;

#ifdef ENABLE_DEEP_SLEEP
            if(IsWakeupTimerSet)
            {
                /* Use the wakeup timer set by XRE */
                pwrModeEventData->data.state.deep_sleep_timeout = deep_sleep_wakeup_timeout_sec;
            }
            else
            {
                /* Set the  wakeup time till 2AM */
                deep_sleep_wakeup_timeout_sec = getWakeupTime();
                pwrModeEventData->data.state.deep_sleep_timeout = deep_sleep_wakeup_timeout_sec;
            }

            pwrModeEventData->data.state.nwStandbyMode = nwStandbyMode_gs;

            /* Start a Deep sleep Wakeup Time source
             * Reboot the box after elapse of user configured / Calculated  Timeout.
             */
            if(PWRMGR_POWERSTATE_STANDBY_DEEP_SLEEP == newState)
            {

                /*
                   - In Deep Sleep, the timer resets and the does not reboot after wakeup time out
                   - As Gloop calls the handler only after Soc wakeup from dep sleep and after expiry of deep_sleep_wakeup_timeout_sec
                   - So if Wakeup timeout is 10 minutes , box reboots after 20 minutes,
                   - To handle this, the handler is called every 30 sec and check for expiry of deep sleep wakeup timeout.
                   */
                time(&timeAtDeepSleep);
#ifndef USE_WAKEUP_TIMER_EVT
                wakeup_event_src = g_timeout_add_seconds ((guint)1, deep_sleep_wakeup_fn, pwrMgr_Gloop);
#else
                __TIMESTAMP();
                LOG("deep_sleep_wakeup_fn is not running\r\n");
#endif
                __TIMESTAMP();
                LOG("Networkstandbymode for Source %d is: %s \r\n",wakeup_event_src, (nwStandbyMode_gs?("Enabled"):("Disabled")));
                __TIMESTAMP();
                LOG("Added Deep Sleep Wakeup Time Source %d for %d Sec \r\n",wakeup_event_src,deep_sleep_wakeup_timeout_sec);

            }
            else if(wakeup_event_src)
            {
                //We got some key event, Remove event source
                __TIMESTAMP();
                LOG("Removed Deep sleep Wakeup Time source %d for %d Sec.. \r\n",wakeup_event_src,deep_sleep_wakeup_timeout_sec);
                g_source_remove(wakeup_event_src);
                wakeup_event_src = 0;
                timeAtDeepSleep = 0;
            }
#endif
            IARM_Bus_BroadcastEvent( IARM_BUS_PWRMGR_NAME,
                                     IARM_BUS_PWRMGR_EVENT_MODECHANGED,
                                     (void *)pwrModeEventData,
                                     sizeof(IARM_Bus_PWRMgr_EventData_t));
            /*
             * Call DeepSleep event update.
             */
#ifdef ENABLE_DEEP_SLEEP
            if(PWRMGR_POWERSTATE_STANDBY_DEEP_SLEEP == newState)
            {
                HandleDeepSleepStateChange((void *)pwrModeEventData);
            }
            else {
                if(pwrModeEventData)
                {
                    free(pwrModeEventData);
                    pwrModeEventData = NULL;
                }
            }
#else
            if(pwrModeEventData)
            {
                free(pwrModeEventData);
                pwrModeEventData = NULL;
            }
#endif
        }

    }
    else
    {
        LOG("Warning:PowerState is same as requested\r\n");
    }
    LOG("[%s:%d] Exiting...\n",__FUNCTION__,__LINE__);
    return retCode;
}


static IARM_Result_t _GetPowerState(void *arg)
{
    PWRMgr_Settings_t *pSettings = &m_settings;
    IARM_Bus_PWRMgr_GetPowerState_Param_t *param = (IARM_Bus_PWRMgr_GetPowerState_Param_t *)arg;
    param->curState = (IARM_Bus_PowerState_t)pSettings->powerState;
    param->prevState = (IARM_Bus_PowerState_t)g_last_known_power_state;
//LOG("_GetPowerState return %d\r\n", pSettings->powerState);
    return IARM_RESULT_SUCCESS;
}

static IARM_Result_t _GetPowerStateBeforeReboot(void *arg)
{
    IARM_Bus_PWRMgr_GetPowerStateBeforeReboot_Param_t *param = (IARM_Bus_PWRMgr_GetPowerStateBeforeReboot_Param_t *)arg;
    memset (param->powerStateBeforeReboot, '\0', MAX_PWR_STATE_BEF_REBOOR_STR_LEN);
    strncpy (param->powerStateBeforeReboot, powerStateBeforeReboot_gc.c_str(), MAX_PWR_STATE_BEF_REBOOR_STR_LEN-1);
    return IARM_RESULT_SUCCESS;
}

inline static void check_payload(const char ** input, const char * default_arg)
{
    if((NULL == *input) || (0 == (*input)[0]))
    {
        *input = default_arg;
    }
    return;
}

void performReboot(const char * requestor, const char * reboot_reason_custom, const char * reboot_reason_other)
{
    LOG("performReboot: Rebooting box now. Requestor: %s. Reboot reason: %s\n", requestor, reboot_reason_custom);
    const char * default_arg = "unknown";

    check_payload(&requestor, default_arg);
    check_payload(&reboot_reason_custom, default_arg);
    check_payload(&reboot_reason_other, default_arg);

    IARM_Bus_PWRMgr_RebootParam_t eventData;
    //Note: assumes caller has checked arg reboot_reason_custom and is safe to use.
    strncpy(eventData.reboot_reason_custom, reboot_reason_custom, sizeof(eventData.reboot_reason_custom));
    strncpy(eventData.reboot_reason_other, reboot_reason_other, sizeof(eventData.reboot_reason_other));
    strncpy(eventData.requestor, requestor, sizeof(eventData.requestor));
    eventData.reboot_reason_custom[sizeof(eventData.reboot_reason_custom) - 1] = '\0';
    eventData.reboot_reason_other[sizeof(eventData.reboot_reason_other) - 1] = '\0';
    eventData.requestor[sizeof(eventData.requestor) - 1] = '\0';
    IARM_Bus_BroadcastEvent( IARM_BUS_PWRMGR_NAME, IARM_BUS_PWRMGR_EVENT_REBOOTING, (void *)&eventData, sizeof(eventData));

    /*
     * performReboot() can be called from the context of an RPC, and the sleep() call below can trigger RPC timeouts. So the time-consuming operations
     * must be handled asynchronously to let this function return promptly. Make a copy of the necessary members so that they can be accessed
     * safely from the new thread.
     * Note: not using strndup() here as lengths of the incoming string are already sanitized.
    */
    char * requestor_cpy = strdup(requestor);
    char * reboot_reason_custom_cpy = strdup(reboot_reason_custom);
    char * reboot_reason_other_cpy = strdup(reboot_reason_other);

    std::thread async_reboot_thread([requestor_cpy, reboot_reason_custom_cpy, reboot_reason_other_cpy] () {
        v_secure_system("echo 0 > /opt/.rebootFlag");
        sleep(5);
        if(0 == access("/rebootNow.sh", F_OK))
        {
            v_secure_system("/rebootNow.sh -s '%s' -r '%s' -o '%s'", requestor_cpy, reboot_reason_custom_cpy, reboot_reason_other_cpy);
        }
        else
        {
            v_secure_system("/lib/rdk/rebootNow.sh -s '%s' -r '%s' -o '%s'", requestor_cpy, reboot_reason_custom_cpy, reboot_reason_other_cpy);
        }
        free(requestor_cpy);
        free(reboot_reason_custom_cpy);
        free(reboot_reason_other_cpy);
    });
    async_reboot_thread.detach();
}

static IARM_Result_t _HandleReboot(void *arg)
{
    IARM_Result_t result = IARM_RESULT_SUCCESS;
    IARM_Bus_PWRMgr_RebootParam_t *param = (IARM_Bus_PWRMgr_RebootParam_t *)arg;
    param->reboot_reason_custom[sizeof(param->reboot_reason_custom) - 1] = '\0'; //Just to be on the safe side.
    param->reboot_reason_custom[sizeof(param->reboot_reason_other) - 1] = '\0';
    param->requestor[sizeof(param->requestor) - 1] = '\0';
    dsMgrRebootConfigParam_t rebootParam;
    strncpy(rebootParam.reboot_reason_custom,param->reboot_reason_custom,sizeof(param->reboot_reason_custom));
    rebootParam.reboot_reason_custom[sizeof(rebootParam.reboot_reason_custom) - 1] = '\0';
    rebootParam.powerState = m_settings.powerState;
    result = IARM_Bus_Call(IARM_BUS_DSMGR_NAME, IARM_BUS_DSMGR_API_SetRebootConfig, &rebootParam, sizeof(rebootParam));
    if (IARM_RESULT_SUCCESS != result)
    {
        LOG("Set reboot param failed");
    }
    performReboot(param->requestor, param->reboot_reason_custom, param->reboot_reason_other);
    return IARM_RESULT_SUCCESS;
}

static IARM_Result_t _SetWakeupSrcConfig(void *arg)
{
    IARM_Result_t retCode = IARM_RESULT_IPCCORE_FAIL;
    int32_t result = -1;

    if(NULL != arg)
    {
        IARM_Bus_PWRMgr_WakeupSrcConfig_Param_t *param = (IARM_Bus_PWRMgr_WakeupSrcConfig_Param_t *)arg;
        pwrMode = param->pwrMode;
        LOG("[PwrMgr] PowerMode  %d : config: %d src %d \r\n",param->pwrMode, param->config, param->srcType );
        for(uint32_t src=PWRMGR_WAKEUPSRC_VOICE; src < PWRMGR_WAKEUPSRC_MAX; src++ )
        {
            if(param->srcType & (1<<src) )
            {
                LOG("[PwrMgr] Setting WakeupSrcConfiguration for src type %d to %d\n", src,(param->config & (1<<src)));
                result = PLAT_API_SetWakeupSrc((PWRMGR_WakeupSrcType_t)src,(bool) (param->config & (1<<src)));
                if(result<0)
                {
                    LOG("[PwrMgr] Setting WakeupSrcConfiguration for src type %d failed\n", src);
                }
                else if(result>0)
                {
                    LOG("[PwrMgr] Setting WakeupSrcConfiguration for src type %d Not Supported\n", src);
                }
                else
                {
                    LOG("[PwrMgr] Setting WakeupSrcConfiguration for src type %d succefuly Done\n", src);
                }
            }
            retCode = IARM_RESULT_SUCCESS;
        }
    }
    else
    {
        retCode = IARM_RESULT_INVALID_PARAM;
    }
    return retCode;
}
static IARM_Result_t _GetWakeupSrcConfig(void *arg)
{
    IARM_Result_t retCode = IARM_RESULT_IPCCORE_FAIL;
    int32_t result = -1;
    bool enabled = false;

    if (NULL != arg)
    {
        IARM_Bus_PWRMgr_WakeupSrcConfig_Param_t *param = (IARM_Bus_PWRMgr_WakeupSrcConfig_Param_t *)arg;
        param->config = 0x0;
        for (uint32_t src=PWRMGR_WAKEUPSRC_VOICE; src < PWRMGR_WAKEUPSRC_MAX; src++)
        {
            LOG("[PwrMgr] Getting WakeupSrcConfiguration for src type %d\n",src);
            result = PLAT_API_GetWakeupSrc((PWRMGR_WakeupSrcType_t)src,&enabled);
            if(result == 0)
            {
                param->srcType |= (1<<src);
                if(enabled)
                {
                    param->config |= (1 << src);
                }
                else
                {
                    param->config &= ~(1 << src);
                }
            }
            else if(result > 0)
            {
                param->srcType &= (~(1 << src));
                LOG("[PwrMgr] Getting WakeupSrcConfiguration for src type %d not supported\n",src);
            }
            else
            {
                LOG("[PwrMgr] Getting WakeupSrcConfiguration for src type %d failed\n",src);
            }
            retCode = IARM_RESULT_SUCCESS;
        }
        param->pwrMode = pwrMode;
        LOG("[PwrMgr] PowerMode  %d : config: %d src %d \r\n",param->pwrMode, param->config, param->srcType );
    }
    else
    {
        retCode = IARM_RESULT_INVALID_PARAM;
    }
    return retCode;
}


static void* _AsyncPowerWareHouseOperation(void *pWareHouseOpnArg)
{
    IARM_Bus_PWRMgr_WareHouseOps_t* pWareHouseOpn = (IARM_Bus_PWRMgr_WareHouseOps_t*) pWareHouseOpnArg;

    pthread_mutex_lock(&wareHouseOpsMutex);
    if (NULL == pWareHouseOpn) {
        LOG("_AsyncPowerWareHouseOperation pWareHouseOpnArg is NULL\r\n");
        pthread_mutex_unlock(&wareHouseOpsMutex);
        asyncPowerWarehouseOpsThreadId = NULL;
        pthread_exit(NULL);
    }

    IARM_BUS_PWRMgr_WareHouseOpn_EventData_t wareHouseOpnEventData;
    wareHouseOpnEventData.wareHouseOpn = (*pWareHouseOpn);
    wareHouseOpnEventData.status = IARM_BUS_PWRMGR_WAREHOUSE_INPROGRESS;
    LOG("_AsyncPowerWareHouseOperation pWareHouseOpnArg is %d pWareHouseOpn is %d\r\n", *((int*)pWareHouseOpnArg), *pWareHouseOpn);

    if (IARM_BUS_PWRMGR_WAREHOUSE_RESET == (*pWareHouseOpn)) {
        processWHResetNoReboot();
        wareHouseOpnEventData.status = IARM_BUS_PWRMGR_WAREHOUSE_COMPLETED;
    }
    else if (IARM_BUS_PWRMGR_WAREHOUSE_CLEAR == (*pWareHouseOpn)) {
        processWHClearNoReboot();
        wareHouseOpnEventData.status = IARM_BUS_PWRMGR_WAREHOUSE_COMPLETED;
    }
    else {
        /* goto sleep */
        LOG("_AsyncPowerWareHouseOperation unexpected pWareHouseOpnArg %d\r\n", (*pWareHouseOpn));
        wareHouseOpnEventData.status = IARM_BUS_PWRMGR_WAREHOUSE_FAILED;
    }

    LOG("_AsyncPowerWareHouseOperation broadcasted IARM_BUS_PWRMGR_EVENT_WAREHOUSEOPS_STATUSCHANGED event\r\n");
    IARM_Bus_BroadcastEvent(IARM_BUS_PWRMGR_NAME, IARM_BUS_PWRMGR_EVENT_WAREHOUSEOPS_STATUSCHANGED, (void *)&wareHouseOpnEventData, sizeof(wareHouseOpnEventData));

    free (pWareHouseOpn);
    pWareHouseOpn=NULL;
    pthread_mutex_unlock(&wareHouseOpsMutex);
    asyncPowerWarehouseOpsThreadId = NULL;
    pthread_exit(NULL);
}

static int _SetPowerWareHouseOperation(IARM_Bus_PWRMgr_WareHouseOps_t eWareHouseOpn)
{

    IARM_Result_t retCode = IARM_RESULT_SUCCESS;

    if (asyncPowerWarehouseOpsThreadId == NULL)
    {
        IARM_Bus_PWRMgr_WareHouseOps_t *pWareHouseOpn = (IARM_Bus_PWRMgr_WareHouseOps_t *) malloc (sizeof(IARM_Bus_PWRMgr_WareHouseOps_t));
        *pWareHouseOpn = eWareHouseOpn;
        LOG("_SetPowerWareHouseOperation eWareHouseOpn is %d pWareHouseOpn is %d\r\n", eWareHouseOpn, *pWareHouseOpn);
        int err = pthread_create(&asyncPowerWarehouseOpsThreadId, NULL, _AsyncPowerWareHouseOperation, (void*)pWareHouseOpn);
        if(err != 0) {
            LOG("_AsyncPowerWareHouseOperation thread create failed \r\n");
        } else {
            err = pthread_detach(asyncPowerWarehouseOpsThreadId);
            if(err != 0) {
                LOG("_AsyncPowerWareHouseOperation thread detach failed \r\n");
            }
            else
                LOG("_AsyncPowerWareHouseOperation thread detach success \r\n");
        }
    }
    else {
        retCode = IARM_RESULT_INVALID_STATE;
        LOG("_SetPowerWareHouseOperation already in progress %d. Pls call it once existing reset finished.\r\n", retCode);
    }
    return retCode;
}

static IARM_Result_t _WareHouseReset(void *arg)
{
    IARM_Bus_PWRMgr_WareHouseReset_Param_t *param = (IARM_Bus_PWRMgr_WareHouseReset_Param_t *)arg;
    int ret = param->suppressReboot ? _SetPowerWareHouseOperation (IARM_BUS_PWRMGR_WAREHOUSE_RESET) : processWHReset();
    LOG("_WareHouseReset returned : %d\r\n", ret);
    fflush(stdout);
    if (ret == 0)
        return IARM_RESULT_SUCCESS;
    else
        return IARM_RESULT_IPCCORE_FAIL;
}

static IARM_Result_t _WareHouseClear(void *arg)
{
    IARM_Bus_PWRMgr_WareHouseReset_Param_t *param = (IARM_Bus_PWRMgr_WareHouseReset_Param_t *)arg;
    int ret = param->suppressReboot ? _SetPowerWareHouseOperation (IARM_BUS_PWRMGR_WAREHOUSE_CLEAR) : processWHClear();
    LOG("_WareHouseClear returned : %d\r\n", ret);
    fflush(stdout);
    if (ret == 0)
        return IARM_RESULT_SUCCESS;
    else
        return IARM_RESULT_IPCCORE_FAIL;
}

static IARM_Result_t _ColdFactoryReset(void *)
{
    int ret = processColdFactoryReset();
    LOG("_ColdFactoryReset returned : %d\r\n", ret);
    fflush(stdout);
    if (ret == 0)
        return IARM_RESULT_SUCCESS;
    else
        return IARM_RESULT_IPCCORE_FAIL;
}

static IARM_Result_t _FactoryReset(void *)
{
    int ret = processFactoryReset();
    LOG("_FactoryReset returned : %d\r\n", ret);
    fflush(stdout);
    if (ret == 0)
        return IARM_RESULT_SUCCESS;
    else
        return IARM_RESULT_IPCCORE_FAIL;
}

static IARM_Result_t _UserFactoryReset(void *)
{
    int ret = processUserFactoryReset();
    LOG("_UserFactoryReset returned : %d\r\n", ret);
    fflush(stdout);
    if (ret == 0)
        return IARM_RESULT_SUCCESS;
    else
        return IARM_RESULT_IPCCORE_FAIL;
}

static int _InitSettings(const char *settingsFile)
{
    if (settingsFile == NULL) settingsFile = "/opt/uimgr_settings.bin";

    m_settingsFile = settingsFile;
    LOG("Initializing settings at file %s\r\n", settingsFile);

    int ret = open(settingsFile, O_CREAT|O_RDWR, S_IRWXU|S_IRUSR);
    int fd = ret;
    struct stat buf;

    if (fd >= 0) {

        PWRMgr_Settings_t *pSettings = &m_settings;
        int read_size = sizeof(uint32_t) * 3;
        lseek(fd, 0, SEEK_SET);
        ret = read(fd, pSettings,read_size);
        if((ret == read_size))
        {
            switch(pSettings->version)
            {
            case 0:
            {
                UIMgr_Settings_t uiMgrSettings;
                lseek(fd, 0, SEEK_SET);
                pSettings->length = sizeof(UIMgr_Settings_t) - PADDING_SIZE;
                read_size = pSettings->length;
                ret = read(fd, &uiMgrSettings,read_size);
                if(ret == read_size)
                {

                    pSettings->magic = _UIMGR_SETTINGS_MAGIC;
                    pSettings->version = 1;
                    pSettings->length = sizeof(PWRMgr_Settings_t) - PADDING_SIZE;
                    pSettings->powerState =
                        _ConvertUIDevToIARMBusPowerState(uiMgrSettings.powerState);
                    g_last_known_power_state = pSettings->powerState;
#ifdef ENABLE_DEEP_SLEEP
                    pSettings->deep_sleep_timeout = deep_sleep_wakeup_timeout_sec;
#endif
                    pSettings->nwStandbyMode = nwStandbyMode_gs;
                    lseek(fd, 0, SEEK_SET);
                    write(fd, pSettings, pSettings->length);

                }
                else
                {
                    ret = 0;  //error case, not able to read full length
                }
            }

            break;
            case 1:
            {
                if((pSettings->length == (sizeof(PWRMgr_Settings_t) - PADDING_SIZE )))
                {
                    LOG("[PwrMgr] Length of Persistence matches with Current Data Size \r\n");
                    lseek(fd, 0, SEEK_SET);
                    read_size = pSettings->length;
                    ret = read(fd, pSettings,read_size);
                    if(ret != read_size)
                    {
                        ret = 0;  //error case, not able to read full length
                        LOG("[PwrMgr] error case, not able to read full length \r\n");
                    }
                    else
                    {
                        g_last_known_power_state = pSettings->powerState;
#ifdef ENABLE_DEEP_SLEEP
                        deep_sleep_wakeup_timeout_sec = pSettings->deep_sleep_timeout;
                        __TIMESTAMP();
                        LOG("Persisted deep_sleep_delay = %d Secs \r\n",deep_sleep_wakeup_timeout_sec);
#endif
                        nwStandbyMode_gs = pSettings->nwStandbyMode;
                        PLAT_API_SetWakeupSrc(PWRMGR_WAKEUPSRC_LAN,nwStandbyMode_gs);
                        PLAT_API_SetWakeupSrc(PWRMGR_WAKEUPSRC_WIFI,nwStandbyMode_gs);
                        __TIMESTAMP();
                        LOG("Persisted network standby mode is: %s \r\n", nwStandbyMode_gs?("Enabled"):("Disabled"));
#ifdef PLATCO_BOOTTO_STANDBY
                        if(stat("/tmp/pwrmgr_restarted",&buf) != 0)
                        {
                            pSettings->powerState = PWRMGR_POWERSTATE_STANDBY;
                            __TIMESTAMP();
                            LOG("Setting default powerstate to standby\n\r");
                        }
#endif
                    }
                }
                else if (((pSettings->length < (sizeof(PWRMgr_Settings_t) - PADDING_SIZE ))))
                {
                    /* New Code reading the old version persistent file  information */
                    LOG("[PwrMgr] Length of Persistence is less than  Current Data Size \r\n");
                    lseek(fd, 0, SEEK_SET);
                    read_size = pSettings->length;
                    ret = read(fd, pSettings,read_size);
                    if(ret != read_size)
                    {
                        LOG("[PwrMgr] Read Failed for Data Length %d \r\n",ret);
                        ret = 0;  //error case, not able to read full length
                    }
                    else
                    {
                        /*TBD - The struct should be initialized first so that we dont need to add
                                 manually the new fields.
                         */
                        g_last_known_power_state = pSettings->powerState;
                        lseek(fd, 0, SEEK_SET);
#ifdef ENABLE_DEEP_SLEEP
                        pSettings->deep_sleep_timeout = deep_sleep_wakeup_timeout_sec;
#endif
                        pSettings->nwStandbyMode = nwStandbyMode_gs;
#ifdef PLATCO_BOOTTO_STANDBY
                        if(stat("/tmp/pwrmgr_restarted",&buf) != 0) {
                            pSettings->powerState = PWRMGR_POWERSTATE_STANDBY;
                        }
#endif
                        pSettings->length = (sizeof(PWRMgr_Settings_t) - PADDING_SIZE );
                        LOG("[PwrMgr] Write PwrMgr Settings File With Current Data Length %d \r\n",pSettings->length);
                        ret = write(fd, pSettings, pSettings->length);
                        if(ret != pSettings->length)
                        {
                            LOG("[PwrMgr] Write Failed For  New Data Length %d \r\n",ret);
                            ret = 0;  //error case, not able to read full length
                        }
                    }
                }
                else if (((pSettings->length > (sizeof(PWRMgr_Settings_t) - PADDING_SIZE ))))
                {
                    /* Old Code reading the migrated new  version persistent file  information */
                    LOG("[PwrMgr] Length of Persistence is more than  Current Data Size. \r\n");

                    lseek(fd, 0, SEEK_SET);
                    read_size = (sizeof(PWRMgr_Settings_t) - PADDING_SIZE );
                    ret = read(fd,pSettings,read_size);
                    if(ret != read_size)
                    {
                        LOG("[PwrMgr] Read Failed for Data Length %d \r\n",ret);
                        ret = 0;  //error case, not able to read full length
                    }
                    else
                    {
                        /*Update the length and truncate the file */
                        g_last_known_power_state = pSettings->powerState;
                        lseek(fd, 0, SEEK_SET);
                        pSettings->length = (sizeof(PWRMgr_Settings_t) - PADDING_SIZE );
                        LOG("[PwrMgr] Write and Truncate  PwrMgr Settings File With Current  Data Length %d ........\r\n",pSettings->length);
                        ret = write(fd, pSettings, pSettings->length);
                        if(ret != pSettings->length)
                        {
                            LOG("[PwrMgr] Write Failed For  New Data Length %d \r\n",ret);
                            ret = 0;  //error case, not able to read full length
                        }
                        else
                        {
                            /* Truncate the File information */
                            int fret = 0;
                            lseek(fd, 0, SEEK_SET);
                            fret = ftruncate(fd,pSettings->length);
                            if(fret != 0)
                            {
                                LOG("[PwrMgr] Truncate Failed For  New Data Length %d \r\n",fret);
                                ret = 0;  //error case, not able to read full length
                            }
                        }

                    }

                }
                else
                {
                    ret = 0;  //Version 1 but not with current size and data...
                }
            }
            break;
            default:
                ret = 0; //Consider it as an invalid file.
            }
        }
        else
        {
            /**/
#ifdef PLATCO_BOOTTO_STANDBY
            if(stat("/tmp/pwrmgr_restarted",&buf) != 0) {
                powerStateBeforeReboot_gc = std::string("UNKNOWN");
                printf ("[%s]:[%d] powerStateBeforeReboot: %s\n", __FUNCTION__, __LINE__, powerStateBeforeReboot_gc.c_str());
            }
#endif
            ret = 0;
        }
        if (ret == 0) {

            lseek(fd, 0, SEEK_SET);
            LOG("Initial Creation of UIMGR Settings\r\n");
            pSettings->magic = _UIMGR_SETTINGS_MAGIC;
            pSettings->version = 1;
            pSettings->length = sizeof(*pSettings) - PADDING_SIZE;
            pSettings->powerState = PWRMGR_POWERSTATE_ON;
#ifdef ENABLE_DEEP_SLEEP
            pSettings->deep_sleep_timeout = deep_sleep_wakeup_timeout_sec;
#endif
            pSettings->nwStandbyMode = nwStandbyMode_gs;
#ifdef PLATCO_BOOTTO_STANDBY
            if(stat("/tmp/pwrmgr_restarted",&buf) != 0)
                pSettings->powerState = PWRMGR_POWERSTATE_STANDBY;
#endif
            ret = write(fd, pSettings, pSettings->length);
            if (ret < 0) {
            }
        }

#ifdef ENABLE_DEEP_SLEEP
        /* If Persistent power mode is Deep Sleep
           start a thread to put box to deep sleep after specified time.
        */
        if(pSettings->powerState == PWRMGR_POWERSTATE_STANDBY_DEEP_SLEEP)
        {
            __TIMESTAMP();
            LOG("Box Reboots with Deep Sleep mode.. Start a Event Time SOurce  .. \r\n");
            guint dsleep_bootup_timeout = 3600; //In sec
            dsleep_bootup_event_src = g_timeout_add_seconds (dsleep_bootup_timeout,invoke_deep_sleep_on_bootup_timeout,pwrMgr_Gloop);
            __TIMESTAMP();
            LOG("Added Time source %d to put the box to deep sleep after %d Sec.. \r\n",dsleep_bootup_event_src,dsleep_bootup_timeout);
        }
#endif

        setPowerStateBeforeReboot (g_last_known_power_state);
        __TIMESTAMP();LOG("Setting PowerStateBeforeReboot %d \r\n", g_last_known_power_state);

        /* Sync with platform if it is supported */
        {
            PWRMgr_PowerState_t state;
            ret = PLAT_API_GetPowerState(&state);
            if (ret == 0) {
                if (pSettings->powerState == state) {
                    LOG("PowerState is already sync'd with hardware to %d\r\n", state);
                }
                else {
                    LOG(" \n PowerState before sync hardware state %d with UIMGR to %d\r\n", state, pSettings->powerState);                      
                    ret = PLAT_API_SetPowerState((PWRMgr_PowerState_t)pSettings->powerState);
                    PLAT_API_GetPowerState((PWRMgr_PowerState_t*)&state);
                    LOG(" \n PowerState after sync hardware state %d with UIMGR to %d\r\n", state, pSettings->powerState);

                    if (state != pSettings->powerState) {
                        LOG("CRITICAL ERROR: PowerState sync failed \r\n");
                        pSettings->powerState = state;
                    }
                }
                if (fd >= 0) {
                    lseek(fd, 0, SEEK_SET);
                    ret = write(fd, pSettings, pSettings->length);
                    if (ret < 0) {
                    }
                }
            }
            else {
                /* Use settings stored in uimgr file */
            }
        }

        IARM_Bus_PWRMgr_EventData_t _eventData;
#ifdef PLATCO_BOOTTO_STANDBY
        if(stat("/tmp/pwrmgr_restarted",&buf) != 0) {
            _eventData.data.state.curState = IARM_BUS_PWRMGR_POWERSTATE_OFF;
            _eventData.data.state.newState = IARM_BUS_PWRMGR_POWERSTATE_STANDBY;
        }
        else
#endif
        {
            _eventData.data.state.curState = (IARM_Bus_PowerState_t)g_last_known_power_state;
            _eventData.data.state.newState = (IARM_Bus_PowerState_t)pSettings->powerState;
        }
        LOG("%s: Init setting powermode change from %d to %d \r\n", __FUNCTION__,_eventData.data.state.curState, \
                                                                                   _eventData.data.state.newState);
        IARM_Bus_BroadcastEvent( IARM_BUS_PWRMGR_NAME, IARM_BUS_PWRMGR_EVENT_MODECHANGED, (void *)&_eventData, sizeof(_eventData));

        if (ret > 0 && ret < (int)sizeof(*pSettings)) {
            _DumpSettings(pSettings);
            /* @TODO: assert pSettings->length == FileLength */
        }

        if (ret > (int)sizeof(*pSettings)) {
            LOG("Error: Should not have read that much ! \r\n");
            /* @TODO: Error Handling */
            /* Init Settings to default, truncate settings to 0 */
        }

        fsync(fd);
        close(fd);
    }

    if (ret < 0) {
        LOG("Error: Failed to operate on uimgr_settings.bin, reason=[%s]\r\n", strerror(errno));
    }

    return ret;
}


static int _WriteSettings(const char *settingsFile)
{
    PWRMgr_Settings_t *pSettings = &m_settings;
    int fd = open(settingsFile, O_CREAT|O_WRONLY, 0644);
    int ret = fd;

    if (fd >= 0) {
        lseek(fd, 0, SEEK_SET);
        PWRMgr_Settings_t *pSettings = &m_settings;
        ret = write(fd, pSettings, pSettings->length);
        fsync(fd);
        close(fd);
    }

    if (ret < 0) {
        LOG("Error: Failed to write on [%s], reason=[%s]\r\n", settingsFile, strerror(errno));
    }
    else {
        LOG("Settings updated successfully\r\n");
        _DumpSettings(pSettings);
    }

    return ret;
}

static void _DumpSettings(const PWRMgr_Settings_t *pSettings)
{
    LOG("PWRMGR-Settings-Mag: %X\r\n", pSettings->magic);
    LOG("PWRMGR-Settings-Ver: %d\r\n", pSettings->version);
    LOG("PWRMGR-Settings-Len: %d\r\n", pSettings->length );
    LOG("PWRMGR-Settings-PWR: %d\r\n", pSettings->powerState);
    LOG("PWRMGR-Settings-Brightness: %d\r\n", pSettings->ledSettings.brightness);
#ifdef ENABLE_DEEP_SLEEP
    LOG("PWRMGR-Settings-Deep Sleep Timeout: %d\r\n", pSettings->deep_sleep_timeout);
#endif
}

static PWRMgr_PowerState_t _ConvertUIDevToIARMBusPowerState(UIDev_PowerState_t powerState)
{
    PWRMgr_PowerState_t ret = PWRMGR_POWERSTATE_ON;
    switch(powerState)
    {
    case UIDEV_POWERSTATE_OFF:
        ret = PWRMGR_POWERSTATE_OFF;
        break;
    case UIDEV_POWERSTATE_STANDBY:
        ret = PWRMGR_POWERSTATE_ON;
        break;
    }
    return ret;
}

/**
 * @fn static IARM_Result_t _SysModeChange(void *arg){
 * @brief This function is a event handler which returns current system
 *  mode using IARM. It returns mode as  "NORMAL", "WAREHOUSE","EAS" or "UNKNOWN".
 *
 * @param[in] void pointer to void, containing IARM_Bus_CommonAPI_SysModeChange_Param_t data.
 *
 * @return variable of IARM_Result_t type.
 * @retval IARM_RESULT_SUCCESS On function completion.
 */
static IARM_Result_t _SysModeChange(void *arg)
{
    IARM_Bus_CommonAPI_SysModeChange_Param_t *param = (IARM_Bus_CommonAPI_SysModeChange_Param_t *)arg;

    __TIMESTAMP();
    printf("[PwrMgr] Recvd Sysmode Change::New mode --> %d,Old mode --> %d",param->newMode,param->oldMode);

    if (param->newMode == IARM_BUS_SYS_MODE_EAS) {
        isEASInProgress = IARM_BUS_SYS_MODE_EAS;
    }
    else if (param->newMode == IARM_BUS_SYS_MODE_NORMAL) {
        isEASInProgress = IARM_BUS_SYS_MODE_NORMAL;
    }
    return IARM_RESULT_SUCCESS;
}

#ifdef ENABLE_DEEP_SLEEP
static IARM_Result_t _SetDeepSleepTimeOut(void *arg)
{
    IARM_Bus_PWRMgr_SetDeepSleepTimeOut_Param_t *param = (IARM_Bus_PWRMgr_SetDeepSleepTimeOut_Param_t *)arg;

    if(param != NULL)
    {
        __TIMESTAMP();
        LOG("Deep sleep timeout set to : %d\r\n", param->timeout);
        deep_sleep_wakeup_timeout_sec = param->timeout;
        m_settings.deep_sleep_timeout = param->timeout;
        IsWakeupTimerSet = 1;
        _WriteSettings(m_settingsFile);
        return IARM_RESULT_SUCCESS;
    }
    return IARM_RESULT_IPCCORE_FAIL;
}

void handleDeepsleepTimeoutWakeup ()
{
    __TIMESTAMP();
    LOG("Entering %s \r\n", __FUNCTION__);
    IARM_BUS_PWRMgr_DeepSleepTimeout_EventData_t param;
    param.timeout = deep_sleep_wakeup_timeout_sec;
    IARM_Bus_BroadcastEvent( IARM_BUS_PWRMGR_NAME,
                             IARM_BUS_PWRMGR_EVENT_DEEPSLEEP_TIMEOUT, (void *)&param, sizeof(param));
    PwrMgrDeepSleepTimeout();
#if !defined (_DISABLE_SCHD_REBOOT_AT_DEEPSLEEP)
    __TIMESTAMP();
    LOG("Reboot the box due to Deep Sleep Timer Expiry : %d \r\n", param.timeout);
#else
    /*Scheduled maintanace reboot is disabled. Instead state will change to LIGHT_SLEEP*/
    IARM_Bus_PWRMgr_SetPowerState_Param_t paramSetPwr;
    PWRMgr_Settings_t *pSettings = &m_settings;
    __TIMESTAMP();
    LOG("deep_sleep_wakeup_fn: Set Device to light sleep on Deep Sleep timer expiry..\r\n");
    pSettings->powerState = PWRMGR_POWERSTATE_STANDBY_DEEP_SLEEP;
    paramSetPwr.newState = IARM_BUS_PWRMGR_POWERSTATE_STANDBY_LIGHT_SLEEP;
    _SetPowerState((void *)&paramSetPwr);
#endif /*End of _DISABLE_SCHD_REBOOT_AT_DEEPSLEEP*/
}
static void* deepsleepTimeoutWakeupThread (void * arg)
{
    __TIMESTAMP();
    LOG("Entering %s \r\n", __FUNCTION__);
    handleDeepsleepTimeoutWakeup();
    pthread_exit(NULL);
}

void  _handleDeepsleepTimeoutWakeup ()
{
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    //Deepsleep wakeup will take time and a low freq event, hence using detach thread approch here.
    pthread_t asyncDeepsleepTimeoutThreadId = 0;
    int err = pthread_create(&asyncDeepsleepTimeoutThreadId, NULL, deepsleepTimeoutWakeupThread, NULL);
    if(err != 0) {
        __TIMESTAMP();
        LOG("handleDeepsleepTimeoutWakeup thread create failed \r\n");
    } else {
        err = pthread_detach(asyncDeepsleepTimeoutThreadId);
        if(err != 0) {
            __TIMESTAMP();
            LOG("handleDeepsleepTimeoutWakeup thread detach failed \r\n");
        }
    }
}

/*  Wakeup the box after wakeup timeout for maintenance activities
    Reboot the box and entered to  light sleep.
*/
static gboolean deep_sleep_wakeup_fn(gpointer data)
{
    uint32_t timeout = (uint32_t) difftime(time(NULL),timeAtDeepSleep);

    __TIMESTAMP();
    LOG("Sec Elapsed Since Deep Sleep : %d \r\n",timeout);

    IARM_Result_t rpcRet = IARM_RESULT_SUCCESS;
    int dsStatus= 0;
    rpcRet = GetPwrMgrDeepSleepStatus(&dsStatus);
    if((DeepSleepStatus_InProgress != dsStatus) && (timeout >= deep_sleep_wakeup_timeout_sec))
    {
        //Calling synchronously here
        handleDeepsleepTimeoutWakeup();
        return FALSE;
    }
    return TRUE;
}


/*  Put the box to Deep Sleep after timeout (configured for now + 1 hour)
    if box had restarted in deep sleep mode
*/
static gboolean invoke_deep_sleep_on_bootup_timeout(gpointer data)
{
    IARM_Bus_PWRMgr_SetPowerState_Param_t param;
    PWRMgr_Settings_t *pSettings = &m_settings;

    __TIMESTAMP();
    LOG("deep_sleep_thread : Set Device to Deep Sleep on Bootip Timer Expiry.. \r\n");

    /* Change the current state to standby and new state to Deep Sleep */
    pSettings->powerState = PWRMGR_POWERSTATE_STANDBY;
    param.newState = IARM_BUS_PWRMGR_POWERSTATE_STANDBY_DEEP_SLEEP;
    _SetPowerState((void *)&param);
    return FALSE;
}


/*  Get TZ diff
    Added For wakeup Time calculation based on TZ
*/
// enum with Time Zone hours
typedef enum _tzValue {
    tzHST11=11,
    tzHST11HDT=10,
    tzAKST=9,
    tzAKST09AKDT=8,
    tzPST08=8,
    tzPST08PDT=8,
    tzMST07=7,
    tzMST07MDT=6,
    tzCST06=6,
    tzCST06CDT=5,
    tzEST05=5,
    tzEST05EDT=4
} tzValue;
// Map to associate the Time Zone strings with TZ hours
#include <map>
static std::map<std::string, tzValue> _maptzValues;
static void InitializeTimeZone();
static uint32_t getTZDiffInSec()
{
    uint32_t _TZDiffTime = 6*3600;
    IARM_Result_t iResult = IARM_RESULT_SUCCESS;
    tzValue value=tzCST06;

    /* Initialize the Time Zone */
    InitializeTimeZone();

    /* Get the Time Zone Pay Load from SysMgr */
    IARM_Bus_SYSMgr_GetSystemStates_Param_t param;
    iResult = IARM_Bus_Call(IARM_BUS_SYSMGR_NAME,IARM_BUS_SYSMGR_API_GetSystemStates,(void *)&param,sizeof(param));
    if(iResult == IARM_RESULT_SUCCESS)
    {
        if (param.time_zone_available.error)
        {
            __TIMESTAMP();
            LOG("Failed to get the Time Zone Information from SysMgr \r\n");
        }
        else if (param.time_zone_available.state == 2)
        {
            if (strlen(param.time_zone_available.payload) > 1)
            {
                __TIMESTAMP();
                LOG("TZ Payload - %s",param.time_zone_available.payload);
                value  = _maptzValues[param.time_zone_available.payload];
                _TZDiffTime = value * 3600;

                __TIMESTAMP();
                LOG("TZ value = %d\r\n",value);
                __TIMESTAMP();
                LOG("Time Zone in Sec = %d \r\n",_TZDiffTime);
            }
        }
    }
    return _TZDiffTime;
}

/*  Get TZ diff
    Have Record of All US TZ as of now.
*/
static void InitializeTimeZone()
{
    _maptzValues["HST11"] = tzHST11;
    _maptzValues["HST11HDT,M3.2.0,M11.1.0"] = tzHST11HDT;
    _maptzValues["AKST"] = tzAKST;
    _maptzValues["AKST09AKDT,M3.2.0,M11.1.0"] = tzAKST09AKDT;
    _maptzValues["PST08"] = tzPST08;
    _maptzValues["PST08PDT,M3.2.0,M11.1.0"] = tzPST08PDT;
    _maptzValues["MST07"] = tzMST07;
    _maptzValues["MST07MDT,M3.2.0,M11.1.0"] = tzMST07MDT;
    _maptzValues["CST06"] = tzCST06;
    _maptzValues["CST06CDT,M3.2.0,M11.1.0"] = tzCST06CDT;
    _maptzValues["EST05"] = tzEST05;
    _maptzValues["EST05EDT,M3.2.0,M11.1.0"] = tzEST05EDT;
//printf("_maptzValues contains %ld items \r\n",_maptzValues.size());
}

/*  Get Wakeup timeout.
    Wakeup the box to do Maintenance related activities.
*/
/* coverity[ignore : Y2K38_SAFETY] see the assert check at the top which prevents overflow. */
static uint32_t getWakeupTime()
{
    time_t now,wakeup;
    struct tm wakeupTime;
    uint32_t wakeupTimeInSec = 0,getTZDiffTime = 0;
    uint32_t wakeupTimeInMin = 5;
    FILE *fpWT = NULL;

    /* Read the wakeup Time in Seconds from /tmp override
       else calculate the Wakeup time til 2AM */
    fpWT = fopen("/tmp/deepSleepWakeupTimer","r");
    if (NULL != fpWT)
    {
        if(0 > fscanf(fpWT,"%d",&wakeupTimeInMin))
        {
            __TIMESTAMP();
            LOG("Error: fscanf on wakeupTimeInSec failed");
        }
        else
        {
            wakeupTimeInSec = wakeupTimeInMin * 60 ;
            fclose (fpWT);
            __TIMESTAMP();
            LOG(" /tmp/ override Deep Sleep Wakeup Time is %d \r\n",wakeupTimeInSec);
            return wakeupTimeInSec;
        }
        fclose (fpWT);
    }

    /* curr time */
    time (&now);

    /* wakeup time */
    time (&wakeup);
    wakeupTime = *localtime (&wakeup);

    if (wakeupTime.tm_hour >=0 && wakeupTime.tm_hour < 2)
    {
        /*Calculate the wakeup time till 2 AM..*/
        wakeupTime.tm_hour = 2;
        wakeupTime.tm_min = 0;
        wakeupTime.tm_sec = 0;
        wakeupTimeInSec = difftime(mktime(&wakeupTime),now);

    }
    else
    {
        /*Calculate the wakeup time till midnight + 2 hours for 2 AM..*/
        wakeupTime.tm_hour = 23;
        wakeupTime.tm_min = 59;
        wakeupTime.tm_sec = 60;
        wakeupTimeInSec = difftime(mktime(&wakeupTime),now);
        wakeupTimeInSec = wakeupTimeInSec + 7200; // 7200sec for 2 hours
    }

    /* Add randomness to calculated value i.e between 2AM - 3AM
        for 1 hour window
    */
    uint32_t randTimeInSec = 0;
    int urandom_fd = open("/dev/urandom", O_RDONLY);
    if (urandom_fd >= 0) {
        if (read(urandom_fd, &randTimeInSec, sizeof(randTimeInSec)) == sizeof(randTimeInSec)) {
            randTimeInSec = randTimeInSec % 3600; // 0 to 3599
        } else {
            randTimeInSec = 0; // fallback
        }
        close(urandom_fd);
    } else {
        randTimeInSec = 0; // fallback
    }
    // Prevent integer overflow when adding random offset
    if ((int64_t)wakeupTimeInSec > INT64_MAX - (int64_t)randTimeInSec) {
        wakeupTimeInSec = INT64_MAX;
    } else {
        wakeupTimeInSec += randTimeInSec;
    }
    //printf ("randTimeInSec is  : %d sec \r\n", randTimeInSec);
    __TIMESTAMP();
    LOG("Calculated Deep Sleep Wakeup Time Before TZ setting is %d Sec \r\n", wakeupTimeInSec);
    getTZDiffTime = getTZDiffInSec();
    // Prevent integer overflow when adding TZ diff
    if ((int64_t)wakeupTimeInSec > INT64_MAX - (int64_t)getTZDiffTime) {
        wakeupTimeInSec = INT64_MAX;
    } else {
        wakeupTimeInSec += getTZDiffTime;
    }
    __TIMESTAMP();
    LOG("Calculated Deep Sleep Wakeup Time After TZ setting is %d Sec \r\n", wakeupTimeInSec);

    return wakeupTimeInSec;
}

#endif // END OF #ifdef ENABLE_DEEP_SLEEP


static IARM_Result_t _SetNetworkStandbyMode(void *arg)
{
    IARM_Bus_PWRMgr_NetworkStandbyMode_Param_t *param = (IARM_Bus_PWRMgr_NetworkStandbyMode_Param_t *)arg;
    uint32_t uiTimeout = 3; /*Timeout in seconds*/
    if(param != NULL)
    {
        nwStandbyMode_gs = param->bStandbyMode;
        PLAT_API_SetWakeupSrc(PWRMGR_WAKEUPSRC_LAN, param->bStandbyMode);
        PLAT_API_SetWakeupSrc(PWRMGR_WAKEUPSRC_WIFI, param->bStandbyMode);
        LOG("Setting network standbyMode: %s \r\n", param->bStandbyMode?("Enabled"):("Disabled"));
        m_settings.nwStandbyMode = param->bStandbyMode;
        _WriteSettings(m_settingsFile);

        IARM_Bus_PWRMgr_EventData_t _eventData;
        _eventData.data.bNetworkStandbyMode = (IARM_Bus_PowerState_t)m_settings.nwStandbyMode;

        IARM_Bus_BroadcastEvent( IARM_BUS_PWRMGR_NAME, IARM_BUS_PWRMGR_EVENT_NETWORK_STANDBYMODECHANGED, (void *)&_eventData, sizeof(_eventData));
        return IARM_RESULT_SUCCESS;
    }
    return IARM_RESULT_IPCCORE_FAIL;
}

static IARM_Result_t _GetNetworkStandbyMode(void *arg)
{
    IARM_Bus_PWRMgr_NetworkStandbyMode_Param_t *param = (IARM_Bus_PWRMgr_NetworkStandbyMode_Param_t *)arg;

    if(param != NULL)
    {
        param->bStandbyMode = m_settings.nwStandbyMode;
        LOG("Network standbyMode is: %s \r\n", param->bStandbyMode?("Enabled"):("Disabled"));
        return IARM_RESULT_SUCCESS;
    }
    return IARM_RESULT_IPCCORE_FAIL;
}


/** @} */
/** @} */
