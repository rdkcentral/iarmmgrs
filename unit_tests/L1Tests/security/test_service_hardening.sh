#!/bin/sh
set -eu

root="${srcdir:-.}/../.."
for unit in mfrmgr sysmgr dsmgr diskmgr deviceupdatemgr; do
    file="$root/conf/$unit.service"
    [ -r "$file" ] || { echo "Missing service unit: $file" >&2; exit 1; }
    for setting in NoNewPrivileges=true ProtectSystem=full ProtectHome=true RestrictSUIDSGID=true LockPersonality=true; do
        grep -Fxq "$setting" "$file" || { echo "$unit.service lacks $setting" >&2; exit 1; }
    done
done
