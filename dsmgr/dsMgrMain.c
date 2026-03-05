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
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <limits.h>
#ifdef __cplusplus 
}
#endif
#ifdef ENABLE_SD_NOTIFY
#include <systemd/sd-daemon.h>
#endif

#ifdef DSMGR_LOGGER_ENABLED

int b_rdk_logger_enabled = 0;

#endif

/* Store original signal handlers for proper crash handling */
static struct sigaction _originalSegmentationHandler;
static struct sigaction _originalAbortHandler;
static const char* _postMortemPath = "/opt/logs/dsmgr_crashes/";

/* Dump crash metadata - similar to Thunder's DumpMetadata() */
static void dsmgr_DumpCrashMetadata(int signum, const char* reason)
{
    char filename[256];
    char timestamp[64];
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    FILE* dumpFile = NULL;
    
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", tm_info);
    snprintf(filename, sizeof(filename), "%sdsmgr_crash_%s_sig%d.log", 
             _postMortemPath, timestamp, signum);
    
    /* Create crash dump directory if it doesn't exist */
    mkdir(_postMortemPath, 0755);
    
    dumpFile = fopen(filename, "w");
    if (dumpFile != NULL) {
        fprintf(dumpFile, "===============================================\n");
        fprintf(dumpFile, "DSMgr Crash Report\n");
        fprintf(dumpFile, "===============================================\n");
        fprintf(dumpFile, "Timestamp: %s\n", timestamp);
        fprintf(dumpFile, "Process ID: %d\n", getpid());
        fprintf(dumpFile, "Signal: %d (%s)\n", signum, 
                (signum == SIGSEGV) ? "SIGSEGV - Segmentation Fault" :
                (signum == SIGABRT) ? "SIGABRT - Abort Signal" : "Unknown");
        fprintf(dumpFile, "Reason: %s\n", reason);
        fprintf(dumpFile, "===============================================\n\n");
        
        /* Dump process information */
        fprintf(dumpFile, "Process Information:\n");
        fprintf(dumpFile, "  Working Directory: ");
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            fprintf(dumpFile, "%s\n", cwd);
        } else {
            fprintf(dumpFile, "Unable to retrieve\n");
        }
        
        /* Dump memory maps for debugging */
        fprintf(dumpFile, "\n  Memory Maps: /proc/%d/maps\n", getpid());
        char maps_cmd[128];
        snprintf(maps_cmd, sizeof(maps_cmd), "cat /proc/%d/maps", getpid());
        FILE* maps = popen(maps_cmd, "r");
        if (maps) {
            char line[256];
            while (fgets(line, sizeof(line), maps)) {
                fprintf(dumpFile, "    %s", line);
            }
            pclose(maps);
        }
        
        fprintf(dumpFile, "\n===============================================\n");
        fprintf(dumpFile, "End of Crash Report\n");
        fprintf(dumpFile, "Core dump location: /opt/logs/core.%d\n", getpid());
        fprintf(dumpFile, "===============================================\n");
        
        fclose(dumpFile);
        
        /* Log to syslog/stderr */
        const char log_msg[] = "[DSMGR_CRASH] Metadata dumped to: ";
        write(STDERR_FILENO, log_msg, sizeof(log_msg) - 1);
        write(STDERR_FILENO, filename, strlen(filename));
        write(STDERR_FILENO, "\n", 1);
    } else {
        const char err_msg[] = "[DSMGR_CRASH] ERROR: Failed to create crash dump file\n";
        write(STDERR_FILENO, err_msg, sizeof(err_msg) - 1);
    }
}

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
    char sigbuf[128];
    int len;
    
    /* Log signal reception */
    const char entry_msg[] = "[DSMGR_CRASH] Signal handler triggered: ";
    write(STDERR_FILENO, entry_msg, sizeof(entry_msg) - 1);
    
    /* Convert signal number to string safely */
    len = snprintf(sigbuf, sizeof(sigbuf), "signum=%d (%s) in process [%d]\n", 
                   signum,
                   (signum == SIGSEGV) ? "SIGSEGV" :
                   (signum == SIGABRT) ? "SIGABRT" :
                   (signum == SIGTERM) ? "SIGTERM" :
                   (signum == SIGQUIT) ? "SIGQUIT" :
                   (signum == SIGUSR1) ? "SIGUSR1" : "UNKNOWN",
                   getpid());
    write(STDERR_FILENO, sigbuf, len);
    
    if ((signum == SIGTERM) || (signum == SIGQUIT)) {
        /* Graceful shutdown requested */
        const char shutdown_msg[] = "[DSMGR_CRASH] DSMgr shutting down due to SIGTERM/SIGQUIT. Regular shutdown.\n";
        write(STDERR_FILENO, shutdown_msg, sizeof(shutdown_msg) - 1);
        
        /* Clean shutdown - call stop functions */
        DSMgr_Stop();
        exit(0);
    }
    else if ((signum == SIGSEGV) || (signum == SIGABRT)) {
        /* CRASH DETECTED - Thunder-style handling */
        const char crash_msg[] = "[DSMGR_CRASH] ============================================\n"
                                  "[DSMGR_CRASH] FATAL CRASH DETECTED - Collecting debug data\n"
                                  "[DSMGR_CRASH] ============================================\n";
        write(STDERR_FILENO, crash_msg, sizeof(crash_msg) - 1);
        
        /* Step 1: Restore original handlers to prevent recursive crash */
        sigaction(SIGSEGV, &_originalSegmentationHandler, NULL);
        sigaction(SIGABRT, &_originalAbortHandler, NULL);
        
        const char restore_msg[] = "[DSMGR_CRASH] Original signal handlers restored\n";
        write(STDERR_FILENO, restore_msg, sizeof(restore_msg) - 1);
        
        /* Step 2: Enable core dump generation */
        struct rlimit core_limit;
        core_limit.rlim_cur = RLIM_INFINITY;
        core_limit.rlim_max = RLIM_INFINITY;
        if (setrlimit(RLIMIT_CORE, &core_limit) == 0) {
            const char core_msg[] = "[DSMGR_CRASH] Core dump enabled (unlimited size)\n";
            write(STDERR_FILENO, core_msg, sizeof(core_msg) - 1);
        }
        
        /* Step 3: Dump metadata (debug information) */
        const char dump_msg[] = "[DSMGR_CRASH] Dumping crash metadata...\n";
        write(STDERR_FILENO, dump_msg, sizeof(dump_msg) - 1);
        
        dsmgr_DumpCrashMetadata(signum, "Segmentation fault or abort signal");
        
        /* Step 4: Trigger reboot in background (non-blocking) */
        pid_t pid = fork();
        if (pid == 0) {
            /* Child process: execute the reboot script */
            const char reboot_msg[] = "[DSMGR_CRASH] Child process: Triggering system reboot...\n";
            write(STDERR_FILENO, reboot_msg, sizeof(reboot_msg) - 1);
            
            char *const argv[] = { (char *)"sh", (char *)"/rebootNow.sh", 
                                   (char *)"-s", (char *)"dsMgrMain_Crash", NULL };
            execve("/bin/sh", argv, NULL);
            _exit(127); /* If exec fails */
        }
        
        /* Step 5: Generate core dump by re-raising signal */
        const char reraise_msg[] = "[DSMGR_CRASH] Re-raising signal to generate core dump...\n"
                                    "[DSMGR_CRASH] All relevant data has been dumped\n"
                                    "[DSMGR_CRASH] ============================================\n";
        write(STDERR_FILENO, reraise_msg, sizeof(reraise_msg) - 1);
        
        /* Flush all buffers before crash */
        sync();
        
        /* Re-raise the signal - this will generate the core dump */
        raise(signum);
        
        /* Should not reach here */
        _exit(128 + signum);
    }
    else if (signum == SIGUSR1) {
        /* Manual metadata dump request (for debugging live system) */
        const char usr1_msg[] = "[DSMGR_DEBUG] SIGUSR1 received - Dumping metadata (no crash)\n";
        write(STDERR_FILENO, usr1_msg, sizeof(usr1_msg) - 1);
        
        dsmgr_DumpCrashMetadata(signum, "Manual dump via SIGUSR1");
        
        const char done_msg[] = "[DSMGR_DEBUG] Metadata dump complete. Process continues.\n";
        write(STDERR_FILENO, done_msg, sizeof(done_msg) - 1);
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
    printf("[DSMGR_INIT] Registering crash handlers (Breakpad-style)...\n");
    INT_INFO("[DSMGR_INIT] Setting up Thunder-style crash handling with metadata dump\n");

    /* Setup signal handler structure */
    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; /* Not SA_RESTART for crash signals */
    sa.sa_handler = dsmgr_signalhandler_thread;

    /* Register graceful shutdown handlers */
    if (sigaction(SIGTERM, &sa, NULL) == 0) {
        printf("[DSMGR_INIT] SIGTERM handler registered (graceful shutdown)\n");
    }
    if (sigaction(SIGQUIT, &sa, NULL) == 0) {
        printf("[DSMGR_INIT] SIGQUIT handler registered (graceful shutdown)\n");
    }
    
    /* Register crash handlers - SAVE ORIGINAL HANDLERS (Thunder pattern) */
    if (sigaction(SIGSEGV, &sa, &_originalSegmentationHandler) == 0) {
        printf("[DSMGR_INIT] SIGSEGV handler registered (crash detection + core dump)\n");
        INT_INFO("[DSMGR_INIT] SIGSEGV: Will dump metadata and generate core dump\n");
    } else {
        printf("[DSMGR_INIT] ERROR: Failed to register SIGSEGV handler\n");
        INT_ERROR("[DSMGR_INIT] CRITICAL: SIGSEGV handler registration failed\n");
    }
    
    if (sigaction(SIGABRT, &sa, &_originalAbortHandler) == 0) {
        printf("[DSMGR_INIT] SIGABRT handler registered (crash detection + core dump)\n");
        INT_INFO("[DSMGR_INIT] SIGABRT: Will dump metadata and generate core dump\n");
    } else {
        printf("[DSMGR_INIT] ERROR: Failed to register SIGABRT handler\n");
        INT_ERROR("[DSMGR_INIT] CRITICAL: SIGABRT handler registration failed\n");
    }
    
    /* Register SIGUSR1 for manual metadata dump (debugging aid) */
    if (sigaction(SIGUSR1, &sa, NULL) == 0) {
        printf("[DSMGR_INIT] SIGUSR1 handler registered (manual metadata dump)\n");
        printf("[DSMGR_INIT] Use 'kill -USR1 %d' to dump metadata without crash\n", getpid());
    }
    
    /* Configure core dump settings */
    struct rlimit core_limit;
    getrlimit(RLIMIT_CORE, &core_limit);
    printf("[DSMGR_INIT] Current core dump limit: %lu bytes\n", (unsigned long)core_limit.rlim_cur);
    
    printf("[DSMGR_INIT] Crash metadata will be saved to: %s\n", _postMortemPath);
    printf("[DSMGR_INIT] Breakpad-style crash handling: ENABLED\n");
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