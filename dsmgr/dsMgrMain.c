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
* @defgroup dsmgr
* @{
**/


#include "dsMgrInternal.h"
#include "dsserverregisterlog.h"

#ifdef __cplusplus 
extern "C" {
#endif
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include "libIBus.h"
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#ifdef __cplusplus 
}
#endif

#ifdef ENABLE_BREAKPAD
#include "client/linux/handler/exception_handler.h"
static google_breakpad::ExceptionHandler* g_breakpad_handler = NULL;
#endif

#ifdef ENABLE_SD_NOTIFY
#include <systemd/sd-daemon.h>
#endif

#ifdef DSMGR_LOGGER_ENABLED

int b_rdk_logger_enabled = 0;

#endif

#ifdef ENABLE_BREAKPAD
/* Breakpad minidump callback - called after minidump is written */
static bool breakpad_dump_callback(const google_breakpad::MinidumpDescriptor& descriptor,
                                   void* context,
                                   bool succeeded)
{
  /* Use only async-signal-safe functions */
  const char msg[] = "[DSMGR_CRASH] Minidump written to: ";
  write(STDERR_FILENO, msg, sizeof(msg) - 1);
  write(STDERR_FILENO, descriptor.path(), strlen(descriptor.path()));
  write(STDERR_FILENO, "\n", 1);
  
  if (succeeded) {
    const char success_msg[] = "[DSMGR_CRASH] Minidump generation: SUCCESS\n";
    write(STDERR_FILENO, success_msg, sizeof(success_msg) - 1);
  } else {
    const char fail_msg[] = "[DSMGR_CRASH] Minidump generation: FAILED\n";
    write(STDERR_FILENO, fail_msg, sizeof(fail_msg) - 1);
  }
  
  /* Trigger system reboot via rebootNow.sh */
  const char reboot_msg[] = "[DSMGR_CRASH] Triggering system reboot...\n";
  write(STDERR_FILENO, reboot_msg, sizeof(reboot_msg) - 1);
  
  pid_t pid = fork();
  if (pid == 0) {
    /* Child process: execute the reboot script */
    char *const argv[] = { (char *)"sh", (char *)"/rebootNow.sh", 
                          (char *)"-s", (char *)"dsMgrMain", NULL };
    execve("/bin/sh", argv, NULL);
    _exit(127);
  }
  
  /* Return true to allow default crash behavior (core dump if enabled) */
  /* Return false to suppress default handling */
  return succeeded;
}
#endif

#ifdef RDK_LOGGER_ENABLED

void logCallback(const char *buff)
{
    INT_DEBUG("%s",buff);
}
void dslogCallback(int priority,const char *buff)
{
    if(priority == 0)
    {
        INT_INFO("%s",buff);
    }
    else if(priority == 1)
    {
        INT_WARN("%s",buff);
    }
    else if(priority == 2)
    {
        INT_ERROR("%s",buff);
    }
    else if(priority == 3)
    {
        INT_DEBUG("%s",buff);
    }
} 
  
#endif

static void dsmgr_signalhandler_thread(int signum)
{
  (void)signum; /* signal number unused to keep handler simple and async-signal-safe */

  /* Use only async-signal-safe functions in the signal handler */
  const char msg[] = "Exiting DSMgr process, calling reboot script\n";
  write(STDERR_FILENO, msg, sizeof(msg) - 1);

  pid_t pid = fork();
  if (pid == 0)
  {
    /* Child process: execute the reboot script via /bin/sh */
    char *const argv[] = { (char *)"sh", (char *)"/rebootNow.sh", (char *)"-s", (char *)"dsMgrMain", NULL };
    const char start_msg[] = "Start the rebootNow.sh script\n";
    write(STDERR_FILENO, start_msg, sizeof(start_msg) - 1);
    execve("/bin/sh", argv, NULL);
    const char done_msg[] = "Completed the rebootNow.sh script\n";
    write(STDERR_FILENO, done_msg, sizeof(done_msg) - 1);
    /* If execve fails, exit the child immediately */
    _exit(127);
  }
}

int main(int argc, char *argv[])
{
    const char* debugConfigFile = NULL;
    int itr=0;

        while (itr < argc)
        {
                if(strcmp(argv[itr],"--debugconfig")==0)
                {
                        itr++;
                        if (itr < argc)
                        {
                                debugConfigFile = argv[itr];
                        }
                        else
                        {
                                break;
                        }
                }
                itr++;
        }

#if (defined(RDK_LOGGER_ENABLED) || defined(DSMGR_LOGGER_ENABLED))

    const char* PdebugConfigFile = NULL;
    const char* DSMGR_DEBUG_ACTUAL_PATH    = "/etc/debug.ini";
    const char* DSMGR_DEBUG_OVERRIDE_PATH  = "/opt/debug.ini";

        /* Init the logger */
    if (access(DSMGR_DEBUG_OVERRIDE_PATH, F_OK) != -1 ) {
        PdebugConfigFile = DSMGR_DEBUG_OVERRIDE_PATH;
    }
    else {
        PdebugConfigFile = DSMGR_DEBUG_ACTUAL_PATH;
    }

    if (rdk_logger_init(PdebugConfigFile) == 0) {
       b_rdk_logger_enabled = 1;
    }

//    IARM_Bus_RegisterForLog(logCallback);
//    DSServer_RegisterForLog(dslogCallback);

//    TODO: This could be enabled only after ensuring that all platforms have DSHal_RegisterForLog implemented 
//    in devicesettings/hal 
//
//    DSHal_RegisterForLog(dslogCallback);

#endif
    if (IARM_RESULT_SUCCESS != DSMgr_Start()) {
        INT_ERROR("DSMgr_Start() failed\n");
        return -1;
    }
    
#ifdef ENABLE_BREAKPAD
    /* Initialize Breakpad minidump handler */
    const char* minidump_path = "/opt/minidumps";
    const char* secure_minidump_path = "/opt/secure/minidumps";
    const char* selected_path = minidump_path;
    
    /* Check if secure path exists, otherwise use non-secure */
    struct stat st;
    if (stat(secure_minidump_path, &st) == 0 && S_ISDIR(st.st_mode)) {
        selected_path = secure_minidump_path;
        printf("[DSMGR_INIT] Using secure minidump path: %s\n", selected_path);
    } else {
        /* Create non-secure minidump directory if it doesn't exist */
        mkdir(minidump_path, 0755);
        printf("[DSMGR_INIT] Using minidump path: %s\n", selected_path);
    }
    
    google_breakpad::MinidumpDescriptor descriptor(selected_path);
    g_breakpad_handler = new google_breakpad::ExceptionHandler(
        descriptor,
        NULL,                          /* filter callback */
        breakpad_dump_callback,        /* minidump callback */
        NULL,                          /* callback context */
        true,                          /* install handler */
        -1                             /* server fd (not used) */
    );
    
    if (g_breakpad_handler) {
        printf("[DSMGR_INIT] Breakpad exception handler initialized successfully\n");
        printf("[DSMGR_INIT] Minidumps will be written to: %s\n", selected_path);
        INT_INFO("[DSMGR_INIT] Breakpad minidump handler enabled\n");
    } else {
        printf("[DSMGR_INIT] WARNING: Failed to initialize Breakpad handler\n");
        INT_ERROR("[DSMGR_INIT] Breakpad initialization failed\n");
    }
#else
    printf("[DSMGR_INIT] Breakpad not enabled - no minidump generation\n");
#endif
    
    printf("DSMgr Register signal handler\n");

    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sa.sa_handler = dsmgr_signalhandler_thread;

    sigaction(SIGABRT, &sa, NULL);
    sigaction(SIGSEGV, &sa, NULL);
    usleep(10000); // Sleep for 10 milliseconds to allow the d-bus to initialize
    #ifdef ENABLE_SD_NOTIFY
           sd_notifyf(0, "READY=1\n"
           "STATUS=DsMgr is Successfully Initialized\n"
              "MAINPID=%lu", (unsigned long) getpid());
    #endif

#ifdef PID_FILE_PATH
#define xstr(s) str(s)
#define str(s) #s
    // write pidfile because sd_notify() does not work inside container
    IARM_Bus_WritePIDFile(xstr(PID_FILE_PATH) "/dsmgr.pid");
#endif
    DSMgr_Loop();
    DSMgr_Stop();
    
#ifdef ENABLE_BREAKPAD
    /* Cleanup Breakpad handler on normal exit */
    if (g_breakpad_handler) {
        delete g_breakpad_handler;
        g_breakpad_handler = NULL;
    }
#endif
    
    return 0;
}




/** @} */
/** @} */