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
# Called by ds-reboot-notifier@dsmgr.service which is triggered via
# OnFailure= in dsmgr.service.
#
# OnFailure= fires ONLY when systemd marks dsmgr as failed:
#   - signal    — killed by unhandled signal (SIGSEGV, SIGABRT, etc.)
#   - exit-code — non-zero exit (e.g. DSMgr_Start() returned failure)
#   - timeout   — start/watchdog timeout expired
#   - core-dump — process produced a core dump
#
# It does NOT fire on clean systemctl stop/restart (Result=success).
# No need to check SERVICE_RESULT or use flag files — if this script
# runs at all, the service has already failed.

# If this script is running, dsmgr has already failed (OnFailure= guarantee).
# No clean-stop check needed.
echo "[ds-reboot] dsMgrMain failed — triggered via OnFailure=ds-reboot-notifier" >&2

# Invoke the platform reboot script.
# -s <component>  identifies the rebooting component in the reboot log.

# ---------------------------------------------------------------------------
# Reboot storm protection — mirrors reboot-count-checker.sh from RDK-v.
#
# RDK-v logic (reboot-count-checker.sh / rebootCounterCheck dsmgr):
#   - Counter file: /opt/.dsmgr_restart_count  (persists across reboots)
#   - Increment counter on every abnormal exit
#   - count > 10  → log warning, suppress reboot (no more reboot loop)
#   - count ≤ 10  → check dependency failure, then call rebootNow.sh
#   - Reset counter: done on clean successful start via ExecStartPost in
#     dsmgr.service (removes the counter file)
#
# RDK-v also waits for coredump upload before rebooting.  On RDK-e that
# is handled asynchronously by breakpad, so the wait is intentionally skipped.
# ---------------------------------------------------------------------------
COUNTER_FILE="/opt/.dsmgr_restart_count"
LOG_FILE="/opt/logs/uimgr_log.txt"
MAX_REBOOTS=10

# Read and increment counter (matches RDK-v: expr $count + 1 logic)
if [ ! -f "${COUNTER_FILE}" ]; then
    count=1
else
    count=$(cat "${COUNTER_FILE}" 2>/dev/null)
    count=$(expr $count + 1)
fi
echo "${count}" > "${COUNTER_FILE}"

echo "[ds-reboot] dsMgrMain restart count: ${count}/${MAX_REBOOTS}" >&2
echo "[ds-reboot] dsMgrMain restart count: ${count}/${MAX_REBOOTS}" >> "${LOG_FILE}"

if [ "${count}" -gt "${MAX_REBOOTS}" ]; then
    # Mirrors: "-----Box has rebooted 10 times.. no more reboot----"
    echo "[ds-reboot] Box has rebooted ${MAX_REBOOTS} times — no more reboot." >&2
    echo "[ds-reboot] Box has rebooted ${MAX_REBOOTS} times — no more reboot." >> "${LOG_FILE}"
    exit 1
fi

# Mirrors: check "Dependency failed" then pick -s or -c flag for rebootNow.sh
if systemctl -l status dsmgr 2>/dev/null | grep -qi "Dependency failed"; then
    echo "[ds-reboot] Dependency failure detected." >&2
    REBOOT_ARGS="-s dsMgrMain -o due_to_service_dependency_failure"
else
    # -c indicates a crash reboot (mirrors RDK-v: /rebootNow.sh -c dsMgrMain)
    REBOOT_ARGS="-c dsMgrMain"
fi

echo "[ds-reboot] Triggering: /rebootNow.sh ${REBOOT_ARGS}" >&2

if [ -x /rebootNow.sh ]; then
    exec /rebootNow.sh ${REBOOT_ARGS}
else
    echo "[ds-reboot] ERROR: /rebootNow.sh not found or not executable" >&2
    # Fall back to a hard reboot if the script is missing.
    /sbin/reboot
fi
