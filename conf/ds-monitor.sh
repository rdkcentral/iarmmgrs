#!/bin/sh

LOG_FILE="/opt/logs/uimgr_log.txt"

log() {
    echo "[ds-monitor] $*" >> "$LOG_FILE"
}

while true
do
    RESULT=$(systemctl show dsmgr.service -p Result --value)

    if [ "$RESULT" = "timeout" ]; then
        log "dsmgr Result=timeout detected"

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

    sleep 2
done
