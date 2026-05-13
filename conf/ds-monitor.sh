#!/bin/sh

LOG_FILE="/opt/logs/uimgr_log.txt"

log() {
    echo "[ds-monitor] $*" >> "$LOG_FILE"
}

while true
do
    RESULT=$(systemctl show dsmgr.service -p Result --value)

    if [ "$RESULT" = "timeout" ]; then
        log "dsmgr Result=timeout detected — sending SIGFPE to dsMgrMain"

        PID=$(pidof dsMgrMain)
        if [ -n "$PID" ]; then
            log "Sending SIGFPE to dsMgrMain PID=$PID"
            kill -SIGFPE "$PID"
        else
            log "dsMgrMain not running — skipping SIGFPE, calling rebootNow.sh directly"
        fi

        log "Triggering reboot: /rebootNow.sh -c dsMgrMain"
        /rebootNow.sh -c dsMgrMain
        exit 0
    fi

    sleep 2
done
