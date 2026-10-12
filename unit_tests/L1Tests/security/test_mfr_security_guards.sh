#!/bin/sh
set -eu

server_file="${srcdir:-.}/../../mfr/common/rpc/srv/mfrSrv.c"
manager_file="${srcdir:-.}/../../mfr/mfrMgr.c"

for file in "$server_file" "$manager_file"; do
    if [ ! -r "$file" ]; then
        echo "MFR source is not readable: $file" >&2
        exit 1
    fi
done

if grep -Eq 'find_func\(RDK_MFRCRYPTOLIB_NAME,[[:space:]]*param->crypto\)' "$server_file"; then
    echo "Caller-controlled crypto symbols must not reach dynamic resolution" >&2
    exit 1
fi

for symbol in mfrCrypto_Encrypt mfrCrypto_Decrypt; do
    if ! grep -Fq "find_func(RDK_MFRCRYPTOLIB_NAME, \"$symbol\")" "$server_file"; then
        echo "Missing fixed crypto symbol: $symbol" >&2
        exit 1
    fi
done

if ! grep -Fq 'param->bufLen > sizeof(param->buffer)' "$server_file"; then
    echo "Serialized-data length is not bounded" >&2
    exit 1
fi

if ! grep -Fq 'isTerminatedString(param->crypto, sizeof(param->crypto))' "$server_file"; then
    echo "Crypto field termination is not validated" >&2
    exit 1
fi

if [ "$(grep -Fc 'isValidWriteImageParam(pParam)' "$server_file")" -lt 1 ]; then
    echo "Legacy WriteImage input is not validated" >&2
    exit 1
fi

for field in imageName version; do
    if ! grep -Fq "isTerminatedString(pParam->$field, sizeof(pParam->$field))" "$server_file"; then
        echo "Legacy $field termination is not validated" >&2
        exit 1
    fi
done

for field in name path callerModuleName cbData; do
    if ! grep -Fq "isTerminatedString(param->$field, sizeof(param->$field))" "$manager_file"; then
        echo "Manager WriteImage $field termination is not validated" >&2
        exit 1
    fi
done

if [ "$(grep -Fc 'isValidWriteImageParam(pParam)' "$manager_file")" -lt 3 ]; then
    echo "Manager write, verify, and mirror inputs are not all validated" >&2
    exit 1
fi

if ! grep -Fq 'isTerminatedString(param->path, sizeof(param->path))' "$manager_file"; then
    echo "Manager splash-screen path termination is not validated" >&2
    exit 1
fi

for field in cSSID cPassword; do
    if ! grep -Fq "isTerminatedString(param->wifiCredentials.$field, sizeof(param->wifiCredentials.$field))" "$manager_file"; then
        echo "Wi-Fi credential $field termination is not validated" >&2
        exit 1
    fi
done
