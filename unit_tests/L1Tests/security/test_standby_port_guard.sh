#!/bin/sh
set -eu

source_file="${srcdir:-.}/../../dsmgr/dsMgrPwrEventListener.c"

if [ ! -r "$source_file" ]; then
    echo "DSMgr power listener source is not readable: $source_file" >&2
    exit 1
fi

count=$(grep -Fc "memchr(param->port, '\\0', sizeof(param->port))" "$source_file")
if [ "$count" -ne 2 ]; then
    echo "Both standby-video RPC handlers must reject unterminated port fields" >&2
    exit 1
fi
