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
#include <string.h>
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

/**
 * @brief Fatal signal handler for SIGABRT (6) and SIGSEGV (11).
 *
 * Installed from main() before DSMgr_Start() so it is the first handler
 * registered — it runs at the earliest possible point in the process
 * lifetime and is not affected by any IARM/GLib initialisation order.
 *
 * Chaining design — handlers execute in the correct order:
 *
 *   1. Invoke the previously saved Breakpad handler inline in the dsmgr
 *      process so it runs in the crashed thread's context — correct stack
 *      and register state for an accurate minidump.
 *
 *   2. After Breakpad returns (or if no prior handler was installed),
 *      call execve("/bin/sh /rebootNow.sh -s dsMgrMain") directly in the
 *      dsmgr process context — no fork.  execve() replaces the process
 *      image in-place; the reboot script inherits dsmgr's PID, cgroup
 *      and systemd slice.
 *
 *   3. If execve fails (e.g. /bin/sh missing): log and _exit(127).
 *
 * Uses only async-signal-safe functions: write, execve, _exit, sigaction.
 */
static struct sigaction _dsMgr_old_sigabrt;
static struct sigaction _dsMgr_old_sigsegv;

static void dsmgr_signalhandler_thread(int signum, siginfo_t *info, void *ctx)
{
    const char msg[] = "[DSMgr] Fatal signal caught: invoking Breakpad handler then reboot\n";
    write(STDERR_FILENO, msg, sizeof(msg) - 1);

    /* Step 1 — Invoke Breakpad's saved handler inline in dsmgr's context.
     * Running in the crashing thread preserves the stack and registers
     * needed for an accurate minidump. */
    struct sigaction *old = (signum == SIGABRT) ? &_dsMgr_old_sigabrt : &_dsMgr_old_sigsegv;
    if ((old->sa_flags & SA_SIGINFO) && old->sa_sigaction != NULL)
    {
        old->sa_sigaction(signum, info, ctx);
    }
    else if (old->sa_handler != SIG_DFL && old->sa_handler != SIG_IGN)
    {
        old->sa_handler(signum);
    }

    /* Step 2 — Breakpad returned (or was not installed).
     * Execute /rebootNow.sh directly in dsmgr's process context via execve.
     * execve replaces the current process image with no fork — the reboot
     * script inherits dsmgr's PID, credentials, cgroup and systemd slice. */
    const char reboot_msg[] = "[DSMgr] Breakpad done. Exiting DSMgr process, calling reboot script\n";
    write(STDERR_FILENO, reboot_msg, sizeof(reboot_msg) - 1);
    char *const argv[] = { (char *)"sh", (char *)"/rebootNow.sh", (char *)"-s", (char *)"dsMgrMain", NULL };
    execve("/bin/sh", argv, NULL);

    /* Step 3 — execve failed (e.g. /bin/sh missing): log and exit.
     * _exit is async-signal-safe and avoids running atexit/stdio flush. */
    const char done_msg[] = "[DSMgr] execve of rebootNow.sh failed, exiting\n";
    write(STDERR_FILENO, done_msg, sizeof(done_msg) - 1);
    _exit(127);
}

static void _dsMgrInstallSignalHandlers(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = dsmgr_signalhandler_thread;
    sigemptyset(&sa.sa_mask);
    /* SA_SIGINFO: receive siginfo_t + ucontext so they can be forwarded
     * accurately to Breakpad's handler.  No SA_RESETHAND — SIG_DFL is
     * restored manually after Breakpad returns (Step 2 execve path). */
    sa.sa_flags = SA_SIGINFO;

    if (sigaction(SIGABRT, &sa, &_dsMgr_old_sigabrt) != 0) {
        INT_ERROR("[DSMgr] Failed to install SIGABRT (6) handler\r\n");
    } else {
        INT_INFO("[DSMgr] SIGABRT (6) handler installed (Breakpad chain + reboot)\r\n");
    }

    if (sigaction(SIGSEGV, &sa, &_dsMgr_old_sigsegv) != 0) {
        INT_ERROR("[DSMgr] Failed to install SIGSEGV (11) handler\r\n");
    } else {
        INT_INFO("[DSMgr] SIGSEGV (11) handler installed (Breakpad chain + reboot)\r\n");
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
    /* Install SIGABRT/SIGSEGV catchers before anything else so the handler
     * is active for the entire lifetime of the dsmgr process. */
    _dsMgrInstallSignalHandlers();

    if (IARM_RESULT_SUCCESS != DSMgr_Start()) {
        INT_ERROR("DSMgr_Start() failed\n");
        return -1;
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
