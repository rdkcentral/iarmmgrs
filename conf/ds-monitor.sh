#!/bin/sh

LOG_FILE="/opt/logs/uimgr_log.txt"
POLL_INTERVAL=5
MAX_WAIT=240

log() {
    echo "[ds-monitor] $*" >> "$LOG_FILE"
}

elapsed=0

while true
do
    RESULT=$(systemctl show dsmgr.service -p Result --value)

    if [ "$RESULT" = "timeout" ]; then
        log "dsmgr Result=timeout detected after ${elapsed}s"

        PID=$(pidof dsMgrMain)
        if [ -n "$PID" ]; then
            log "Sending SIGABRT to dsMgrMain PID=$PID"
            kill -SIGABRT "$PID"
        else
            log "dsMgrMain not running — calling rebootNow.sh directly"
        fi

        log "Triggering: /rebootNow.sh -c dsMgrMain"
        /rebootNow.sh -c dsMgrMain
        exit 0
    fi

    if [ "$elapsed" -ge "$MAX_WAIT" ]; then
        log "Monitor: ${elapsed}s elapsed — dsmgr still not started, sending SIGABRT"
        PID=$(pidof dsMgrMain)
        if [ -n "$PID" ]; then
            log "Sending SIGABRT to dsMgrMain PID=$PID"
            kill -SIGABRT "$PID"
            sleep 5
            if pidof dsMgrMain > /dev/null 2>&1; then
                log "dsMgrMain still alive after SIGABRT — calling rebootNow.sh"
                /rebootNow.sh -c dsMgrMain
            fi
        else
            log "dsMgrMain not running — ds-reboot.sh will handle reboot"
        fi
        exit 0
    fi

    sleep "$POLL_INTERVAL"
    elapsed=$((elapsed + POLL_INTERVAL))
done
