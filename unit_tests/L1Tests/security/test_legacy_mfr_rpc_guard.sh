#!/bin/sh
set -eu

source_file="${srcdir:-.}/../../mfr/common/rpc/srv/mfrSrv.c"
manager_file="${srcdir:-.}/../../mfr/mfrMgr.c"

for file in "$source_file" "$manager_file"; do
    if [ ! -r "$file" ]; then
        echo "MFR RPC source is not readable: $file" >&2
        exit 1
    fi
done

if ! grep -Fq 'mfrStringIsTerminated(param->crypto, sizeof(param->crypto))' "$source_file"; then
    echo "Crypto field termination is not validated" >&2
    exit 1
fi

for field in name path imageName version; do
    if ! grep -Fq "mfrStringIsTerminated(pParam->$field" "$source_file"; then
        echo "Legacy $field termination is not validated" >&2
        exit 1
    fi
done

for field in name path callerModuleName cbData; do
    if ! grep -Fq "mfrStringIsTerminated(param->$field, sizeof(param->$field))" "$manager_file"; then
        echo "Modern image $field termination is not validated" >&2
        exit 1
    fi
done

for field in cSSID cPassword; do
    if ! grep -Fq "mfrStringIsTerminated(param->wifiCredentials.$field" "$manager_file"; then
        echo "Wi-Fi $field termination is not validated" >&2
        exit 1
    fi
done

if ! grep -Fq 'mfrStringIsTerminated(param->path, sizeof(param->path))' "$manager_file"; then
    echo "Splash-screen path termination is not validated" >&2
    exit 1
fi

if [ "$(grep -Fc 'isValidWriteImageParam(pParam)' "$manager_file")" -lt 3 ]; then
    echo "Write, verify, and mirror image RPCs must reject invalid string fields" >&2
    exit 1
fi