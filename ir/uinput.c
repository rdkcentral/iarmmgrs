/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2018 RDK Management
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
/* 
 * Reference implementation to dispatch IR events into /dev/uinput
 * May switch to use libevdev in the future.
 */

#include <linux/uinput.h>
#include <linux/input.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "irMgrInternal.h"
#include "IrInputRemoteKeyCodes.h"

#define UINPUT_SETUP_ID(uidev) \
do {\
    memset(&uidev, 0, sizeof(uidev));\
    snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "uinput-remote");\
    uidev.id.bustype = BUS_USB;\
    uidev.id.vendor  = 0x1234;\
    uidev.id.product = 0xfedc;\
    uidev.id.version = 1;\
} while(0)

#define UINPUT_SETUP_ID_SKY_KEYSIM(uidev) \
do {\
    memset(&uidev, 0, sizeof(uidev));\
    snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "key-simulator");\
    uidev.id.bustype = BUS_USB;\
    uidev.id.vendor  = 0xbeef;\
    uidev.id.product = 0xfedc;\
    uidev.id.version = 1;\
} while(0)


#ifdef __cplusplus
extern "C" { 
#endif
static int devFd = -1;
static uint32_t getKeyCode(uint32_t keycode, uint32_t *uCode, uint32_t *uModi)
{
    unsigned char i;

    printf("%s %d uinput received Key code %d 0x%x \r\n", __FUNCTION__, __LINE__, keycode, keycode);
    for (i=0; i < (sizeof(kcodesMap_IARM2Linux)/sizeof(kcodesMap_IARM2Linux[0])); i++)
    {   
        if (kcodesMap_IARM2Linux[i].iCode == keycode)
        {   
            *uCode = kcodesMap_IARM2Linux[i].uCode;
            *uModi = kcodesMap_IARM2Linux[i].uModi;
            return kcodesMap_IARM2Linux[i].uCode;
        }   
    }   

    printf("UNrecognized Key code %d \r\n", keycode);
    return KED_UNDEFINEDKEY;
}


static uint32_t getKeyValue(uint32_t keyType)
{
    switch (keyType) 
    {
        case KET_KEYDOWN:
            return 1;
        case KET_KEYUP:
            return 0;
        case KET_KEYREPEAT:
            return 2;
        default:
            printf("UNrecognized Key type %d \r\n", keyType);
    }

    return 0;
}

static void udispatcher_internal(int code, int value)
{
    int ret = -1;
    struct input_event ev;
    memset(&ev, 0, sizeof(ev));
    gettimeofday(&ev.time, NULL);
    ev.type = EV_KEY;
    ev.code = code;
    ev.value = value;
    ret = write(devFd, &ev, sizeof(ev));
    if (ret == sizeof(ev)) {
        /* reuse same timestamp */
        ev.type = EV_SYN;
        ev.code = SYN_REPORT;
        ev.value = 0;
        ret = write(devFd, &ev, sizeof(ev));
    }
    else {
        perror("uinput write key failed\r\n");
        UINPUT_term();
    }
}

static void udispatcher (int keyCode, int keyType, int source)
{
    printf("%s %d uinput received Key code= %d 0x%x  keyType= %d 0x%x \r\n", __FUNCTION__, __LINE__, keyCode, keyCode, keyType, keyType);
    if (devFd >= 0) {
        static const char * type2str[] = {"KEY_UP", "KEY_DOWN", "KEY_REPEAT"};
        uint32_t uCode = _KEY_INVALID;
        uint32_t uModi = _KEY_INVALID;
        uint32_t value = 0;

        getKeyCode(keyCode, &uCode, &uModi);
        value = getKeyValue(keyType);
        printf("IR-Keyboard Regular Key: IR=%x key=%x Modifier=%x val=%x [%s]\r\n", keyCode, uCode, uModi, value, type2str[value]);
        /*
         *  Send Modifier KEY_DOWN and KEY_UP event
         *  along with keycode's DOWN and UP event.
         *
         *  Do not send Modifier on REPEAT event.
         */

        if ((keyType == KET_KEYDOWN)) {
            if (uModi != _KEY_INVALID) {
                udispatcher_internal(uModi, value);
            }
        }
        udispatcher_internal((int)uCode, (int)value);
        if ((keyType == KET_KEYUP)) {
            if (uModi != _KEY_INVALID) {
                udispatcher_internal(uModi, value);
            }
        }
    }
}


int UINPUT_init(bool useSimulatorVendorId)
{
#ifndef UINPUT_VERSION
#define UINPUT_VERSION (0)
#endif

    int fd = -1;
    int ret = -1;
    fd = open("/dev/uinput", O_WRONLY|O_SYNC);
    if (fd >= 0) {
        printf("Linux uinput version [%d] is built-in with kernel\r\n", UINPUT_VERSION);
        /* Fist setup input capabilities*/
        {
            //Add event types
            ret = ioctl(fd, UI_SET_EVBIT, EV_KEY);
            ret = ioctl(fd, UI_SET_EVBIT, EV_SYN);
        }

        if (ret == 0)
        {
            //Add keycodes 
            for (size_t i = 0; i < sizeof(kcodesMap_IARM2Linux)/sizeof(kcodesMap_IARM2Linux[0]); i++) {
                ret = ioctl(fd, UI_SET_KEYBIT, kcodesMap_IARM2Linux[i].uCode);
                if (kcodesMap_IARM2Linux[i].uModi != _KEY_INVALID) {
                    ret = ioctl(fd, UI_SET_KEYBIT, kcodesMap_IARM2Linux[i].uModi);
                }
            }
        }
        /* Then setup input id*/
#if defined UINPUT_VERSION && (UINPUT_VERSION >= 5)
        if (ret == 0)
        {
            struct uinput_setup usetup;
            if (useSimulatorVendorId) {
                UINPUT_SETUP_ID_SKY_KEYSIM(usetup);
            }else {
                UINPUT_SETUP_ID(usetup);
            }
            ret = ioctl(fd, UI_DEV_SETUP, &usetup);
        }
#else
        if (ret == 0)
        {
            /* Legacy uinput appraoch */
            struct uinput_user_dev uidev;
            UINPUT_SETUP_ID(uidev);
            ret = write(fd, &uidev, sizeof(uidev));
            printf("write uinput_user_dev return %d vs %d\r\n", ret, sizeof(uidev));
            ret = ((ret == sizeof(uidev)) ? 0 :  -1);
        }
#endif
        /* Now setup complete, create the device*/
        if (ret == 0)
        {
            ret = ioctl(fd, UI_DEV_CREATE, 0);
        }

        if (ret != 0)
        {
            perror("uinput setup failed\r\n");
            if (fd >= 0) {
                ret = ioctl(fd, UI_DEV_DESTROY, 0);
                close(fd);
            }
            fd = -1;
        }
        else {
        }
    }
    else {
        printf("Linux uinput is not built-in with kernel\r\n");
    }

    devFd = fd;
    return 0;
}

uinput_dispatcher_t UINPUT_GetDispatcher(void)
{
    if (devFd >= 0) return udispatcher;
    else return NULL;
}

int UINPUT_term()
{
    if (devFd >= 0) 
    {
        ioctl(devFd, UI_DEV_DESTROY);
        close(devFd);
    }

    devFd = -1;
    return 0;
}
#ifdef __cplusplus
}
#endif
