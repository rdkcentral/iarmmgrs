#!/bin/sh
set -eu

header="${srcdir:-.}/../../../stubs/safec_lib.h"

if grep -Eq '^[[:space:]]*#define[[:space:]]+SAFEC_DUMMY_API' "$header"; then
    echo "SAFEC_DUMMY_API must not be enabled by the production header" >&2
    exit 1
fi

if ! grep -Fq '#error "SAFEC_DUMMY_API is not permitted in production builds"' "$header"; then
    echo "SAFEC_DUMMY_API compile-time guard is missing" >&2
    exit 1
fi

if printf '#include "%s"\n' "$header" | "${CPP:-cpp}" -DSAFEC_DUMMY_API - >/dev/null 2>&1; then
    echo "A SAFEC_DUMMY_API build unexpectedly passed preprocessing" >&2
    exit 1
fi

if ! grep -Fq '#include "safe_str_lib.h"' "$header" || ! grep -Fq '#include "safe_mem_lib.h"' "$header"; then
    echo "Production header does not require the real Safe C API" >&2
    exit 1
fi
