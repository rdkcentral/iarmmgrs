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

if grep -Eq 'find_func\(RDK_MFRCRYPTOLIB_NAME,[[:space:]]*param->crypto\)' "$source_file"; then
    echo "Caller-controlled crypto symbols must not reach dynamic resolution" >&2
    exit 1
fi

for symbol in mfrCrypto_Encrypt mfrCrypto_Decrypt; do
    if ! grep -Fq "find_func(RDK_MFRCRYPTOLIB_NAME, \"$symbol\")" "$source_file"; then
        echo "Missing fixed allow-listed crypto symbol: $symbol" >&2
        exit 1
    fi
done

if ! grep -Fq 'param->bufLen > sizeof(param->buffer)' "$source_file"; then
    echo "Serialized-data length is not bounded to the RPC buffer" >&2
    exit 1
fi

if ! grep -Fq 'strnlen(param->crypto, sizeof(param->crypto))' "$source_file"; then
    echo "Crypto field termination is not validated" >&2
    exit 1
fi

for field in name path; do
    if ! grep -Fq "memchr(param->$field, '\\0', sizeof(param->$field))" "$manager_file"; then
        echo "Write-image $field termination is not validated" >&2
        exit 1
    fi
done

if [ "$(grep -Fc 'isValidWriteImageParam(pParam)' "$manager_file")" -lt 2 ]; then
    echo "Write and verify image RPCs must reject invalid string fields" >&2
    exit 1
fi
