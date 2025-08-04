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

#if defined(_TIME_BITS) && _TIME_BITS == 64
#define _GNU_SOURCE
#endif

#if defined(_FILE_OFFSET_BITS) && _FILE_OFFSET_BITS == 64
#define _LARGEFILE64_SOURCE
#endif

#include <linux/input.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

int main(int argc, char *argv[])
{
    /* open device */
    if (argc < 2) {
        printf("test_uinput: <device file path>\r\n");
        return 0;
    }

    const char *devFile = argv[1];


    int fd = open(devFile, O_RDONLY|O_SYNC);
    if (fd >= 0) {
        struct input_event ev;
        const int count = sizeof(ev);
        while(1) {
            memset(&ev, 0, count);
            int ret = read(fd, &ev, count);
            if (ret == count) {
#if defined(_TIME_BITS) && _TIME_BITS == 64
                printf("Getting input [%lld.%lld] - %d %d %d\r\n",
                                (long long)ev.time.tv_sec, (long long)ev.time.tv_usec,
                                ev.type,
                                ev.code,
                                ev.value);
#else
                printf("Getting input [%ld.%ld] - %d %d %d\r\n",
                                ev.time.tv_sec, ev.time.tv_usec,
                                ev.type,
                                ev.code,
                                ev.value);
#endif
                if (ev.type == EV_KEY) {
                    if (ev.value >= 0 && ev.value <=2) {
#if defined(_TIME_BITS) && _TIME_BITS == 64
                        printf("[%lld].[%lld] : Key [%d] [%s]\r\n",
                                (long long)ev.time.tv_sec, (long long)ev.time.tv_usec,
                                ev.code,
                                (ev.value == 1) ? "+++++PRESSED" : ((ev.value == 0) ? "=====Release" : "......."));
#else
                        printf("[%ld].[%ld] : Key [%d] [%s]\r\n",
                                ev.time.tv_sec, ev.time.tv_usec,
                                ev.code,
                                (ev.value == 1) ? "+++++PRESSED" : ((ev.value == 0) ? "=====Release" : "......."));
#endif
                    }
                }
                else if (ev.type == EV_SYN) {
#if defined(_TIME_BITS) && _TIME_BITS == 64
                    printf("[%lld].[%lld] : SYN [%s]\r\n",
                                (long long)ev.time.tv_sec, (long long)ev.time.tv_usec,
                                (ev.value == SYN_REPORT) ? "SYN_REPORT" : "SYN_OTHER");
#else
                    printf("[%ld].[%ld] : SYN [%s]\r\n",
                                ev.time.tv_sec, ev.time.tv_usec,
                                (ev.value == SYN_REPORT) ? "SYN_REPORT" : "SYN_OTHER");
#endif
                }
            }
            else {
                printf("read() input event returned %d failure [%s]\r\n", ret, strerror(errno));
                break;
            }
        }
    }
    else {
        printf("open file %s failed [%s]\r\n", devFile, strerror(errno));
    }

    return 0;
}
