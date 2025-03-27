/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2021 RDK Management
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

#ifndef _PRODUCT_TRAITS_
#define _PRODUCT_TRAITS_
#include <string>
#include <mutex>
#include "mfrTypes.h"
#include "libIBus.h"
#include "libIBusDaemon.h"
#include "power_controller.h"


namespace dsMgrProductTraits
{
typedef enum
{
    /*******   Important note! **********
     *  Do NOT disturb or rearrange existing enums defined in productProfileId_t. Always add new profiles just above PROFILE_MAX or havoc will ensue.
     *  The enums mapped here are passed as POWERMGR_PRODUCT_PROFILE_ID=<insert number> with the build configuration, which is how the various platforms
     *  choose user experience profiles applicable to them.
     */
    DEFAULT_STB_PROFILE = 0,
    DEFAULT_TV_PROFILE,
    DEFAULT_STB_PROFILE_EUROPE,
    DEFAULT_TV_PROFILE_EUROPE,
    PROFILE_MAX
} productProfileId_t;

typedef enum
{
    DEVICE_TYPE_STB = 0,
    DEVICE_TYPE_TV,
    DEVICE_TYPE_MAX
} deviceType_t;

typedef enum
{
    POWER_MODE_ON = 0,
    POWER_MODE_LIGHT_SLEEP,
    POWER_MODE_LAST_KNOWN,
    POWER_MODE_UNSPECIFIED,
    POWER_MODE_MAX
} powerModeTrait_t;

enum class reboot_type_t { HARD, SOFT, UNAVAILABLE};

/*
    ux_controller or 'user experience controller' is in charge of maintaining and applying the user experience attributes owned by power manager.
    That means attributes like this:
     * Preferred power mode when the device reboots.
     * power LED configuration when device is in standby and ON modes.
     * Support for silent reboot (suppressing flash screen, power LED etc) when executing a maintenance reboot.
     * and more.

    User experiences will vary among the various product classes. The various derivates of ux_controller class will represent each such specialiation.
*/
class ux_controller
{
protected:
    unsigned int id;
    std::string name;
    deviceType_t deviceType;
    bool invalidateAsyncBootloaderPattern;
    bool firstPowerTransitionComplete;
    mutable std::mutex mutex;

    bool enableMultiColourLedSupport;

    bool ledEnabledInStandby;
    int ledColorInStandby;

    bool ledEnabledInOnState;
    int ledColorInOnState;

    powerModeTrait_t preferedPowerModeOnReboot;
    bool enableSilentRebootSupport;

    static ux_controller * singleton;

    void initialize_safe_defaults();
    void sync_power_led_with_power_state(PowerController_PowerState_t state) const;
    inline void sync_display_ports_with_power_state(PowerController_PowerState_t state) const;
    bool _set_bootloader_pattern(mfrBlPattern_t pattern) const;
    void _set_bootloader_pattern_async(mfrBlPattern_t pattern) const;
    bool set_bootloader_pattern(mfrBlPattern_t pattern);
    bool set_bootloader_pattern_fault_tolerant(mfrBlPattern_t pattern);

public:
    static bool initialize_ux_controller(unsigned int profile_id); //Not thread-safe
    static ux_controller * get_instance(); //Not thread-safe
    ux_controller(unsigned int in_id, const std::string &in_name, deviceType_t in_device_type);
    virtual ~ux_controller() {}

    virtual bool applyPowerStateChangeConfig(PowerController_PowerState_t new_state, PowerController_PowerState_t prev_state) {
        return false;
    };
    virtual bool applyPreRebootConfig(PowerController_PowerState_t current_state) const {
        return false;
    }
    virtual bool applyPreMaintenanceRebootConfig(PowerController_PowerState_t current_state) {
        return false;
    }
    virtual bool applyPostRebootConfig(PowerController_PowerState_t new_state, PowerController_PowerState_t prev_state) {
        return false;
    }
    virtual PowerController_PowerState_t getPreferredPostRebootPowerState(PowerController_PowerState_t prev_state) const {
        return prev_state;
    }
    virtual void sync_display_ports_with_reboot_reason(reboot_type_t type) {};


};

class ux_controller_tv_eu : public ux_controller
{
public:
    ux_controller_tv_eu(unsigned int in_id, const std::string &in_name);
    virtual bool applyPowerStateChangeConfig(PowerController_PowerState_t new_state, PowerController_PowerState_t prev_state) override;
    virtual bool applyPreRebootConfig(PowerController_PowerState_t current_state) const override;
    virtual bool applyPreMaintenanceRebootConfig(PowerController_PowerState_t current_state) override;
    virtual bool applyPostRebootConfig(PowerController_PowerState_t new_state, PowerController_PowerState_t prev_state) override;
    virtual PowerController_PowerState_t getPreferredPostRebootPowerState(PowerController_PowerState_t prev_state) const override;
};

class ux_controller_stb_eu : public ux_controller
{
public:
    ux_controller_stb_eu(unsigned int in_id, const std::string &in_name);
    virtual bool applyPowerStateChangeConfig(PowerController_PowerState_t new_state, PowerController_PowerState_t prev_state) override;
    virtual bool applyPreRebootConfig(PowerController_PowerState_t current_state) const override;
    virtual bool applyPreMaintenanceRebootConfig(PowerController_PowerState_t current_state) override;
    virtual bool applyPostRebootConfig(PowerController_PowerState_t new_state, PowerController_PowerState_t prev_state) override;
    virtual PowerController_PowerState_t getPreferredPostRebootPowerState(PowerController_PowerState_t prev_state) const override;
};

class ux_controller_tv : public ux_controller
{
public:
    ux_controller_tv(unsigned int in_id, const std::string &in_name);
    virtual bool applyPowerStateChangeConfig(PowerController_PowerState_t new_state, PowerController_PowerState_t prev_state) override;
    virtual bool applyPreRebootConfig(PowerController_PowerState_t current_state) const override;
    virtual bool applyPreMaintenanceRebootConfig(PowerController_PowerState_t current_state) override;
    virtual bool applyPostRebootConfig(PowerController_PowerState_t new_state, PowerController_PowerState_t prev_state) override;
    virtual PowerController_PowerState_t getPreferredPostRebootPowerState(PowerController_PowerState_t prev_state) const override;
    virtual void sync_display_ports_with_reboot_reason(reboot_type_t type) override;
};

class ux_controller_stb : public ux_controller
{
public:
    ux_controller_stb(unsigned int in_id, const std::string &in_name);
    virtual bool applyPowerStateChangeConfig(PowerController_PowerState_t new_state, PowerController_PowerState_t prev_state) override;
    virtual bool applyPreRebootConfig(PowerController_PowerState_t current_state) const override;
    virtual bool applyPreMaintenanceRebootConfig(PowerController_PowerState_t current_state) override;
    virtual bool applyPostRebootConfig(PowerController_PowerState_t new_state, PowerController_PowerState_t prev_state) override;
    virtual PowerController_PowerState_t getPreferredPostRebootPowerState(PowerController_PowerState_t prev_state) const override;
};
}
#endif
