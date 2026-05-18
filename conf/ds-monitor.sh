#!/bin/sh

LOG_FILE="/opt/logs/uimgr_log.txt"
POLL_INTERVAL=5
MAX_WAIT=180

log() {
    echo "[ds-monitor] $*" >> "$LOG_FILE"
}

# Send SIGABRT to dsMgrMain (if still running), wait for breakpad, then reboot.
abort_and_reboot() {
    PID=$(systemctl show dsmgr.service -p MainPID --value)
    if [ -n "$PID" ] && [ "$PID" != "0" ]; then
        log "Sending SIGABRT to dsMgrMain PID=$PID"
        kill -SIGABRT "$PID"
        # Wait for breakpad to capture and upload minidump before rebooting.
        sleep 5
    else
        log "dsMgrMain not running — calling rebootNow.sh directly"
    fi
    log "Triggering: /rebootNow.sh -c dsMgrMain"
    /rebootNow.sh -c dsMgrMain
}

elapsed=0

while true
do
    if [ -f "/tmp/dsmgr_monitor_stop" ]; then
        rm -f /tmp/dsmgr_monitor_stop
        log "dsmgr started successfully — stop flag set, exiting monitor"
        exit 0
    fi

    RESULT=$(systemctl show dsmgr.service -p Result --value)

    if [ "$RESULT" = "timeout" ]; then
        log "dsmgr Result=timeout detected after ${elapsed}s"
        abort_and_reboot
        exit 0
    fi

    if [ "$elapsed" -ge "$MAX_WAIT" ]; then
        log "Monitor: ${elapsed}s elapsed — dsmgr still not started, sending SIGABRT"
        abort_and_reboot
        exit 0
    fi

    sleep "$POLL_INTERVAL"
    elapsed=$((elapsed + POLL_INTERVAL))
done
