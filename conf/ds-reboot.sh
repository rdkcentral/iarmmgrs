#!/bin/sh
##########################################################################
# If not stated otherwise in this file or this component's LICENSE
# file the following copyright and licenses apply:
#
# Copyright 2016 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
##########################################################################

# ds-reboot.sh
#
# Called by systemd via ExecStopPost= in dsmgr.service after dsMgrMain
# exits for ANY reason (clean stop, SIGABRT, SIGSEGV, or any other crash).
#
# Systemd sets $SERVICE_RESULT when invoking ExecStopPost:
#   "success"   — clean exit (return 0 or SIGTERM from systemctl stop)
#   "signal"    — killed by unhandled signal (SIGSEGV, SIGABRT, etc.)
#   "exit-code" — non-zero exit (e.g. DSMgr_Start() returned failure)
#   "timeout"   — watchdog / start timeout expired
#   "core-dump" — process produced a core dump
#
# Only trigger a device reboot when the exit was abnormal.  A clean
# systemctl stop (SERVICE_RESULT=success) does NOT reboot.

EXIT_CODE="${EXIT_CODE:-0}"
EXIT_STATUS="${EXIT_STATUS:-}"

# $SERVICE_RESULT is only injected by systemd >= v232.  On older systemd (this
# platform runs v230) it is empty, so query the result from systemd directly.
# NOTE: --value flag was added in v230; use Result=xxx parse as primary to be safe.
if [ -z "${SERVICE_RESULT}" ] || [ "${SERVICE_RESULT}" = "unknown" ]; then
    _raw=$(systemctl show dsmgr --property=Result 2>/dev/null)
    # _raw is "Result=success" / "Result=timeout" etc.
    SERVICE_RESULT=$(echo "${_raw}" | sed 's/^Result=//')
    # If sed left it unchanged (no match) or empty, flag as unknown
    [ "${SERVICE_RESULT}" = "${_raw}" ] && SERVICE_RESULT=""
    SERVICE_RESULT="${SERVICE_RESULT:-unknown}"
fi

echo "[ds-reboot] dsMgrMain exited: SERVICE_RESULT=${SERVICE_RESULT}" \
     "EXIT_CODE=${EXIT_CODE} EXIT_STATUS=${EXIT_STATUS}" >&2

case "${SERVICE_RESULT}" in
    success)
        # Clean stop — systemctl stop dsmgr or DSMgr_Stop() returned 0.
        # No reboot needed.
        echo "[ds-reboot] Clean exit — no reboot triggered." >&2
        exit 0
        ;;
    unknown)
        # systemctl show could not determine the result (very old systemd or
        # dbus not available).  Last resort: use EXIT_CODE + EXIT_STATUS.
        #
        # Start-timeout case: systemd kills dsMgrMain with SIGTERM, so
        #   EXIT_CODE=0  but  EXIT_STATUS=TERM (or 15).
        # Clean systemctl stop: also SIGTERM, indistinguishable here.
        # We therefore check the sentinel created by ExecStartPost:
        #   sentinel present  → process had fully started → clean stop → no reboot
        #   sentinel absent   → never reached ready       → start-timeout → reboot
        SENTINEL="/tmp/dsmgr.ready"
        if [ "${EXIT_CODE}" = "0" ]; then
            if [ -f "${SENTINEL}" ]; then
                echo "[ds-reboot] Result unknown; EXIT_CODE=0; sentinel present" \
                     "— treating as clean exit, no reboot." >&2
                rm -f "${SENTINEL}"
                exit 0
            else
                echo "[ds-reboot] Result unknown; EXIT_CODE=0; sentinel absent" \
                     "— dsMgrMain never fully started (start-timeout or early failure)" \
                     "— triggering reboot." >&2
            fi
        else
            echo "[ds-reboot] Result unknown; EXIT_CODE=${EXIT_CODE}" \
                 "— treating as abnormal exit, triggering reboot." >&2
        fi
        rm -f "${SENTINEL}"
        ;;
    signal|core-dump)
        # Crashed by an unhandled signal (SIGSEGV=11, SIGABRT=6, etc.)
        echo "[ds-reboot] Crash detected (${SERVICE_RESULT} / ${EXIT_STATUS})" \
             "— triggering reboot via rebootNow.sh" >&2
        ;;
    exit-code|timeout|*)
        # Non-zero exit or watchdog timeout — treat as abnormal.
        echo "[ds-reboot] Abnormal exit (${SERVICE_RESULT} / code=${EXIT_CODE})" \
             "— triggering reboot via rebootNow.sh" >&2
        ;;
esac

# Invoke the platform reboot script.
# -s <component>  identifies the rebooting component in the reboot log.
if [ -x /rebootNow.sh ]; then
    exec /rebootNow.sh -s dsMgrMain
else
    echo "[ds-reboot] ERROR: /rebootNow.sh not found or not executable" >&2
    # Fall back to a hard reboot if the script is missing.
    /sbin/reboot
fi
