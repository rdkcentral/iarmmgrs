#!/bin/bash

WORKDIR=`pwd`
export ROOT=/usr
export INSTALL_DIR=${ROOT}/local
mkdir -p $INSTALL_DIR

export CC=gcc
export CXX=g++
export AR=ar
export LD=ld
export NM=nm
export RANLIB=ranlib
export STRIP=strip

apt-get update && apt-get install -y libsoup-3.0 libcjson-dev libdbus-1-dev

#Build rfc
cd $ROOT
rm -rf rfc
git clone https://github.com/rdkcentral/rfc.git
cd rfc
autoreconf -i
./configure --enable-rfctool=yes --enable-tr181set=yes
cd rfcapi
make librfcapi_la_CPPFLAGS="-I/usr/include/cjson"
make install
export RFC_PATH=$ROOT/rfc

cd $ROOT
rm -rf iarmbus
git clone https://github.com/rdkcentral/iarmbus.git
export IARMBUS_PATH=$ROOT/iarmbus
export IARM_PATH=$IARMBUS_PATH

cd $ROOT
rm -rf iarmmgrs
git clone https://github.com/rdkcentral/iarmmgrs.git
export IARMMGRS_PATH=$ROOT/iarmmgrs

cd $ROOT
rm -rf rdk_logger
git clone https://github.com/rdkcentral/rdk_logger.git
export RDKLOGGER_PATH=$ROOT/rdk_logger
cd rdk_logger

#build log4c
wget --no-check-certificate https://sourceforge.net/projects/log4c/files/log4c/1.2.4/log4c-1.2.4.tar.gz/download -O log4c-1.2.4.tar.gz
tar -xvf log4c-1.2.4.tar.gz
cd log4c-1.2.4
./configure
make clean && make && make install

cd ${RDKLOGGER_PATH}
export PKG_CONFIG_PATH=${INSTALL_DIR}/rdk_logger/log4c-1.2.4:$PKG_CONFIG_PATH
autoreconf -i
./configure
make clean && make && make install

cd $ROOT
rm -rf telemetry
git clone https://github.com/rdkcentral/telemetry.git
export TELEMETRY_PATH=$ROOT/telemetry

cd $ROOT
rm -rf devicesettings
git clone https://github.com/rdkcentral/devicesettings.git
export DS_PATH=$ROOT/devicesettings

cd $ROOT
rm -rf rdk-halif-device_settings
git clone https://github.com/rdkcentral/rdk-halif-device_settings.git
export DS_IF_PATH=$ROOT/rdk-halif-device_settings

cd $ROOT
rm -rf rdk-halif-power_manager
git clone https://github.com/rdkcentral/rdk-halif-power_manager.git
export POWER_IF_PATH=$ROOT/rdk-halif-power_manager

cd $ROOT
rm -rf rdk-halif-deepsleep_manager
git clone https://github.com/rdkcentral/rdk-halif-deepsleep_manager.git
export DEEPSLEEP_IF_PATH=$ROOT/rdk-halif-deepsleep_manager

cd $ROOT
rm -rf rdkvhal-devicesettings-raspberrypi4
git clone https://github.com/rdkcentral/rdkvhal-devicesettings-raspberrypi4.git
export DS_HAL_PATH=$ROOT/rdkvhal-devicesettings-raspberrypi4


