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
#ifdef __cplusplus 
}
#endif
#ifdef ENABLE_SD_NOTIFY
#include <systemd/sd-daemon.h>
#endif

#ifdef DSMGR_LOGGER_ENABLED

int b_rdk_logger_enabled = 0;

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
  /* Use only async-signal-safe functions in the signal handler */
  const char entry_msg[] = "[DSMGR_DEBUG] Signal handler triggered, signum=";
  write(STDERR_FILENO, entry_msg, sizeof(entry_msg) - 1);
  
  /* Print signal number (convert to string safely) */
  char signum_str[12];
  int len = 0;
  int temp = signum;
  if (temp == 0) {
    signum_str[len++] = '0';
  } else {
    int divisor = 1;
    while (temp / divisor >= 10) divisor *= 10;
    while (divisor > 0) {
      signum_str[len++] = '0' + (temp / divisor);
      temp %= divisor;
      divisor /= 10;
    }
  }
  signum_str[len++] = '\n';
  write(STDERR_FILENO, signum_str, len);

  const char msg[] = "[DSMGR_DEBUG] Exiting DSMgr process, calling reboot script\n";
  write(STDERR_FILENO, msg, sizeof(msg) - 1);

  const char fork_msg[] = "[DSMGR_DEBUG] About to fork for reboot script\n";
  write(STDERR_FILENO, fork_msg, sizeof(fork_msg) - 1);

  pid_t pid = fork();
  if (pid == 0)
  {
    /* Child process: execute the reboot script via /bin/sh */
    char *const argv[] = { (char *)"sh", (char *)"/rebootNow.sh", (char *)"-s", (char *)"dsMgrMain", NULL };
    const char start_msg[] = "[DSMGR_DEBUG] Child process: Start the rebootNow.sh script\n";
    write(STDERR_FILENO, start_msg, sizeof(start_msg) - 1);
    execve("/bin/sh", argv, NULL);
    const char done_msg[] = "[DSMGR_DEBUG] Child process: Completed the rebootNow.sh script\n";
    write(STDERR_FILENO, done_msg, sizeof(done_msg) - 1);
    /* If execve fails, exit the child immediately */
    _exit(127);
  }
  else if (pid > 0)
  {
    const char parent_msg[] = "[DSMGR_DEBUG] Parent process: Fork successful, child PID created\n";
    write(STDERR_FILENO, parent_msg, sizeof(parent_msg) - 1);
  }
  else
  {
    const char fork_fail_msg[] = "[DSMGR_DEBUG] ERROR: Fork failed\n";
    write(STDERR_FILENO, fork_fail_msg, sizeof(fork_fail_msg) - 1);
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
    printf("[DSMGR_DEBUG] DSMgr Register signal handler\n");
    INT_INFO("[DSMGR_DEBUG] Setting up signal handlers for SIGABRT and SIGSEGV\n");

    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sa.sa_handler = dsmgr_signalhandler_thread;

    if (sigaction(SIGABRT, &sa, NULL) == 0) {
        printf("[DSMGR_DEBUG] SIGABRT handler registered successfully\n");
        INT_INFO("[DSMGR_DEBUG] SIGABRT handler registered successfully\n");
    } else {
        printf("[DSMGR_DEBUG] ERROR: Failed to register SIGABRT handler\n");
        INT_ERROR("[DSMGR_DEBUG] ERROR: Failed to register SIGABRT handler\n");
    }
    
    if (sigaction(SIGSEGV, &sa, NULL) == 0) {
        printf("[DSMGR_DEBUG] SIGSEGV handler registered successfully\n");
        INT_INFO("[DSMGR_DEBUG] SIGSEGV handler registered successfully\n");
    } else {
        printf("[DSMGR_DEBUG] ERROR: Failed to register SIGSEGV handler\n");
        INT_ERROR("[DSMGR_DEBUG] ERROR: Failed to register SIGSEGV handler\n");
    }
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
    return 0;
}




/** @} */
/** @} */
