#!/bin/bash
##########################################################################
# If not stated otherwise in this file or this component's LICENSE
# file the following copyright and licenses apply:
#
# Copyright 2016 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
##########################################################################
#

#######################################
#
# Build Framework standard script for
#
# IARMMgrs component

# use -e to fail on any shell issue
# -e is the requirement from Build Framework
set -e


# default PATHs - use `man readlink` for more info
# the path to combined build
export RDK_PROJECT_ROOT_PATH=${RDK_PROJECT_ROOT_PATH-`readlink -m ..`}/
export COMBINED_ROOT=$RDK_PROJECT_ROOT_PATH

# path to build script (this script)
export RDK_SCRIPTS_PATH=${RDK_SCRIPTS_PATH-`readlink -m $0 | xargs dirname`}/

# path to components sources and target
export RDK_SOURCE_PATH=${RDK_SOURCE_PATH-`readlink -m .`}/
export RDK_TARGET_PATH=${RDK_TARGET_PATH-$RDK_SOURCE_PATH}

# fsroot and toolchain (valid for all devices)
export RDK_FSROOT_PATH=${RDK_FSROOT_PATH-`readlink -m $RDK_PROJECT_ROOT_PATH/sdk/fsroot/ramdisk`}/
export RDK_TOOLCHAIN_PATH=${RDK_TOOLCHAIN_PATH-`readlink -m $RDK_PROJECT_ROOT_PATH/sdk/toolchain/staging_dir`}


# default component name
export RDK_COMPONENT_NAME=${RDK_COMPONENT_NAME-`basename $RDK_SOURCE_PATH`}


# parse arguments
INITIAL_ARGS=$@

function usage()
{
    set +x
    echo "Usage: `basename $0` [-h|--help] [-v|--verbose] [action]"
    echo "    -h    --help                  : this help"
    echo "    -v    --verbose               : verbose output"
    echo "    -p    --platform  =PLATFORM   : specify platform for IARMMgrs"
    echo
    echo "Supported actions:"
    echo "      configure, clean, build (DEFAULT), rebuild, install"
}

# options may be followed by one colon to indicate they have a required argument
if ! GETOPT=$(getopt -n "build.sh" -o hvp: -l help,verbose,platform: -- "$@")
then
    usage
    exit 1
fi

eval set -- "$GETOPT"

while true; do
  case "$1" in
    -h | --help ) usage; exit 0 ;;
    -v | --verbose ) set -x ;;
    -p | --platform ) CC_PLATFORM="$2" ; shift ;;
    -- ) shift; break;;
    * ) break;;
  esac
  shift
done

ARGS=$@


# component-specific vars
CC_PATH=$RDK_SOURCE_PATH
export FSROOT=${RDK_FSROOT_PATH}
export TOOLCHAIN_DIR=${RDK_TOOLCHAIN_PATH}
export WORK_DIR=$RDK_PROJECT_ROOT_PATH/work${RDK_PLATFORM_DEVICE^^}
export BUILDS_DIR=$RDK_PROJECT_ROOT_PATH


# functional modules

function configure()
{
   true #use this function to perform any pre-build configuration
}

function clean()
{
    true #use this function to provide instructions to clean workspace
}

function build()
{
    IARMMGRS_PATH=${RDK_SCRIPTS_PATH}
    IARMMGRSGENERIC_PATH=${IARMMGRS_PATH}
    cd $IARMMGRSGENERIC_PATH

   
    export DS_PATH=$BUILDS_DIR/devicesettings
    export IARM_PATH=$BUILDS_DIR/iarmbus
    export IARM_MGRS=$BUILDS_DIR/iarmmgrs
    export LOGGER_PATH=$BUILDS_DIR/logger
    export CEC_PATH=$BUILDS_DIR/hdmicec
	export UTILS_PATH=$BUILDS_DIR/utils
	export FULL_VERSION_NAME_VALUE="\"$IMAGE_NAME\""
	export SDK_FSROOT=$COMBINED_ROOT/sdk/fsroot/ramdisk

	export TOOLCHAIN_DIR=$COMBINED_ROOT/sdk/toolchain/staging_dir
	export CROSS_TOOLCHAIN=$TOOLCHAIN_DIR
	export CROSS_COMPILE=$CROSS_TOOLCHAIN/bin/i686-cm-linux
	export CC=$CROSS_COMPILE-gcc
	export CXX=$CROSS_COMPILE-g++
	export OPENSOURCE_BASE=$COMBINED_ROOT/opensource
	export DFB_ROOT=$TOOLCHAIN_DIR
	export DFB_LIB=$TOOLCHAIN_DIR/lib
	export FSROOT=$COMBINED_ROOT/sdk/fsroot/ramdisk
	export MFR_FPD_PATH=$COMBINED_ROOT/mfrlibs
	export GLIB_INCLUDE_PATH=$CROSS_TOOLCHAIN/include/glib-2.0/
	export GLIB_LIBRARY_PATH=$CROSS_TOOLCHAIN/lib/
    	export GLIB_CONFIG_INCLUDE_PATH=$GLIB_LIBRARY_PATH/glib-2.0/
	export GLIBS='-lglib-2.0'
	export _ENABLE_WAKEUP_KEY=-D_ENABLE_WAKEUP_KEY
	export _INIT_RESN_SETTINGS=-D_INIT_RESN_SETTINGS
	export RF4CE_API="-DRF4CE_API -DUSE_UNIFIED_RF4CE_MGR_API_4"
	export RF4CE_PATH=$COMBINED_ROOT/rf4ce/generic/
	export CURL_INCLUDE_PATH=$OPENSOURCE_BASE/include/
	export CURL_LIBRARY_PATH=$OPENSOURCE_BASE/lib/
	export LDFLAGS="$LDFLAGS -Wl,-rpath-link=$RDK_FSROOT_PATH/usr/local/lib"
        export GLIB_HEADER_PATH=$CROSS_TOOLCHAIN/include/glib-2.0/
        export GLIB_CONFIG_PATH=$CROSS_TOOLCHAIN/lib/

   if [ -f $COMBINED_ROOT/rdklogger/build/lib/librdkloggers.so ]; then
	export RDK_LOGGER_ENABLED='y'
    fi


    make
}

function rebuild()
{
    clean
    build
}

function install()
{
    IARMMGRSINSTALL_PATH=${RDK_SCRIPTS_PATH}/../install

	RAMDISK_TARGET=${RDK_FSROOT_PATH}/lib
    mkdir -p $RAMDISK_TARGET


    cd $IARMMGRSINSTALL_PATH

	cp -v lib/libirInput.so ${RDK_FSROOT_PATH}lib
	cp -v lib/libPwrMgr.so ${RDK_FSROOT_PATH}lib

		cp -v bin/irMgrMain ${RDK_FSROOT_PATH}mnt/nfs/env
		cp -v bin/pwrMgrMain ${RDK_FSROOT_PATH}mnt/nfs/env    
		cp -v bin/sysMgrMain ${RDK_FSROOT_PATH}mnt/nfs/env
		cp -v bin/dsMgrMain  ${RDK_FSROOT_PATH}mnt/nfs/env
		cp -v bin/vrexPrefs.json ${RDK_FSROOT_PATH}mnt/nfs/env
		if [ -e bin/deviceUpdateMgrMain ]; then
			cp -v bin/deviceUpdateMgrMain ${RDK_FSROOT_PATH}mnt/nfs/env
			cp -v bin/deviceUpdateConfig.json ${RDK_FSROOT_PATH}mnt/nfs/env
		fi		
    
}


# run the logic

#these args are what left untouched after parse_args
HIT=false

for i in "$ARGS"; do
    case $i in
        configure)  HIT=true; configure ;;
        clean)      HIT=true; clean ;;
        build)      HIT=true; build ;;
        rebuild)    HIT=true; rebuild ;;
        install)    HIT=true; install ;;
        *)
            #skip unknown
        ;;
    esac
done

# if not HIT do build by default
if ! $HIT; then
  build
fi
