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

if ! sed -n '/^static IARM_Result_t mfrSetBlSplashScreen_(void \*arg)/,/^static IARM_Result_t mfrClearBlSplashScreen_(void \*arg)/p' "$manager_file" |
    grep -Fq 'isTerminatedString(param->path, sizeof(param->path))'; then
    echo "Manager splash-screen path termination is not validated" >&2
    exit 1
fi

if ! grep -Fq 'pthread_cond_timedwait(&writeImageStatusCond' "$server_file"; then
    echo "WriteImage callback wait has no bounded timeout" >&2
    exit 1
fi

if ! grep -Fq 'MFR_WRITE_IMAGE_CALLBACK_TIMEOUT_SEC' "$server_file"; then
    echo "WriteImage callback timeout is not configurable" >&2
    exit 1
fi

if ! grep -Fq 'if (!beginWriteImageOperation(' "$server_file"; then
    echo "WriteImage operation reservation is missing" >&2
    exit 1
fi

if ! grep -Fq 'if (writeImageStatusIsTerminal(status.progress))' "$server_file" ||
   ! grep -Fq 'finishWriteImageOperation();' "$server_file"; then
    echo "Terminal callback does not release the operation reservation" >&2
    exit 1
fi

callback_block=$(sed -n '/^static void writeImageCb/,/^IARM_Result_t _mfrWriteImage/p' "$server_file")
if ! printf '%s\n' "$callback_block" | grep -Fq 'writeImageCompletionPending = true'; then
    echo "Terminal callback does not preserve completion state" >&2
    exit 1
fi
if printf '%s\n' "$callback_block" | grep -Fq 'finishWriteImageOperation();'; then
    echo "Terminal callback releases the reservation before result consumption" >&2
    exit 1
fi

if ! sed -n '/^IARM_Result_t _mfrWriteImage(void \*arg)/,/^IARM_Result_t /p' "$server_file" |
    grep -Fq 'pthread_mutex_lock(&mfrLock)'; then
    echo "WriteImage vendor invocation is not serialized with other MFR calls" >&2
    exit 1
fi

for function in _mfrDeletePDRI _mfrScrubAllBanks _mfrReboot; do
    if ! sed -n "/^IARM_Result_t $function(void \*arg)/,/^IARM_Result_t /p" "$server_file" |
        grep -Fq 'if (!beginWriteImageOperation('; then
        echo "$function is not serialized with image operations" >&2
        exit 1
    fi
    if ! sed -n "/^IARM_Result_t $function(void \*arg)/,/^IARM_Result_t /p" "$server_file" |
        grep -Fq 'pthread_mutex_lock(&mfrLock)'; then
        echo "$function vendor invocation is not serialized with other MFR calls" >&2
        exit 1
    fi
done

for field in cSSID cPassword; do
    if ! grep -Fq "isTerminatedString(param->wifiCredentials.$field, sizeof(param->wifiCredentials.$field))" "$manager_file"; then
        echo "Wi-Fi credential $field termination is not validated" >&2
        exit 1
    fi
done
