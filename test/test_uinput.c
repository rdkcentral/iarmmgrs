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
#include <linux/input.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <sys/stat.h>
#include <signal.h>

/* Global variables for graceful shutdown */
static volatile int keep_running = 1;
static int global_fd = -1;

static void signal_handler(int sig) {
    printf("Received signal %d, shutting down gracefully...\n", sig);
    keep_running = 0;
    if (global_fd >= 0) {
        close(global_fd);
        global_fd = -1;
    }
}

/* Validate file path for security */
static int validate_device_path(const char *path) {
    struct stat statbuf;
    
    if (!path) {
        printf("Error: NULL file path\n");
        return 0;
    }
    
    /* Check path length to prevent buffer overflow */
    if (strlen(path) >= PATH_MAX) {
        printf("Error: File path too long\n");
        return 0;
    }
    
    /* Validate against dangerous path characters */
    if (strstr(path, "..") || strstr(path, "//") || 
        strchr(path, ';') || strchr(path, '|') || strchr(path, '&')) {
        printf("Error: Invalid characters in file path\n");
        return 0;
    }
    
    /* Check if file exists and is a character device (typical for input devices) */
    if (stat(path, &statbuf) == 0) {
        if (!S_ISCHR(statbuf.st_mode)) {
            printf("Warning: %s is not a character device\n", path);
        }
    } else {
        printf("Warning: Cannot stat file %s: %s\n", path, strerror(errno));
    }
    
    return 1;
}

int main(int argc, char *argv[])
{
    /* Install signal handlers for graceful shutdown */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* open device */
    if (argc < 2) {
        printf("test_uinput: <device file path>\r\n");
        return 0;
    }

    const char *devFile = argv[1];
    
    /* Validate file path for security before opening */
    if (!validate_device_path(devFile)) {
        printf("Error: Invalid or unsafe device file path: %s\n", devFile);
        return 1;
    }

    int fd = open(devFile, O_RDONLY|O_SYNC);
    global_fd = fd; /* Store for signal handler cleanup */
    
    if (fd >= 0) {
        struct input_event ev;
        const int count = sizeof(ev);
        
        /* Fixed infinite loop with proper exit condition */
        while(keep_running) {
            memset(&ev, 0, count);
            int ret = read(fd, &ev, count);
            
            /* Improved error handling for partial reads */
            if (ret == count) {
                /* Sanitize output data to prevent format string attacks */
                printf("Getting input [%ld.%ld] - %d %d %d\r\n",
                                (long)ev.time.tv_sec, (long)ev.time.tv_usec,
                                (int)ev.type,
                                (int)ev.code,
                                (int)ev.value);
                                
                if (ev.type == EV_KEY) {
                    if (ev.value >= 0 && ev.value <=2) {
                        printf("[%ld].[%ld] : Key [%d] [%s]\r\n",
                                (long)ev.time.tv_sec, (long)ev.time.tv_usec,
                                (int)ev.code,
                                (ev.value == 1) ? "+++++PRESSED" : ((ev.value == 0) ? "=====Release" : "......."));
                    }
                }
                else if (ev.type == EV_SYN) {
                    printf("[%ld].[%ld] : SYN [%s]\r\n",
                                (long)ev.time.tv_sec, (long)ev.time.tv_usec,
                                (ev.value == SYN_REPORT) ? "SYN_REPORT" : "SYN_OTHER");
                }
            }
            else if (ret == 0) {
                printf("End of file reached\n");
                break;
            }
            else if (ret < 0) {
                if (errno == EINTR) {
                    /* Interrupted by signal, check exit condition */
                    continue;
                }
                printf("read() input event returned %d failure [%s]\r\n", ret, strerror(errno));
                break;
            }
            else {
                /* Partial read - handle properly */
                printf("Partial read: got %d bytes, expected %d\n", ret, count);
                break;
            }
        }
        
        /* Ensure file descriptor is closed - fixes resource leak */
        close(fd);
        global_fd = -1;
        printf("File descriptor closed successfully\n");
    }
    else {
        printf("open file %s failed [%s]\r\n", devFile, strerror(errno));
        return 1;
    }

    return 0;
}
