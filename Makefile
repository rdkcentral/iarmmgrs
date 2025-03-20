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
# List of Libraries
install_dir := ../install/bin
install_lib_dir := ../install/lib

# List of Executable
exe_power           := power/
exe_pwrstate        := pwrstate/
exe_sysmgr          := sysmgr/
exe_mfr             := mfr/
exe_ds              := dsmgr/
exe_tr69Bus         := tr69Bus/
exe_test            := test
exe_mfr_test        := mfr/test_mfr
exe_utils	    := utils
ifneq ($(PLATFORM_SOC),L2HalMock)
exe_platform_ir     := ../soc/${PLATFORM_SOC}/ir
exe_platform_power  := ../soc/${PLATFORM_SOC}/power
exe_platform_pwrstate  := ../soc/${PLATFORM_SOC}/pwrstate
exe_platform_fp     := ../soc/${PLATFORM_SOC}/fp
endif

ifneq ($(MFR_MGR_SUPPORT),nomfrmgr)
ifeq ($(PLATFORM_SOC),L2HalMock)
executable := $(exe_utils) $(exe_ds) $(exe_power) $(exe_sysmgr)
else
executable := $(exe_platform_ir) $(exe_platform_power) $(exe_platform_fp) $(exe_power) $(exe_platform_pwrstate) $(exe_pwrstate) $(exe_sysmgr) $(exe_tr69Bus) $(exe_test) $(exe_mfr) $(exe_ds)
endif
else	
executable := $(exe_platform_ir) $(exe_platform_power) $(exe_platform_fp) $(exe_power) $(exe_platform_pwrstate) $(exe_pwrstate) $(exe_sysmgr) $(exe_tr69Bus) $(exe_test) $(exe_ds)
endif	


.PHONY: clean all $(executable) install

all: clean $(executable) install 

$(executable):
	$(MAKE) -C $@

install:
	echo "Creating directory.."
	mkdir -p $(install_dir)
	mkdir -p $(install_lib_dir)
	echo "Copying files now.."	
	cp $(exe_sysmgr)/*Main $(install_dir)
	cp $(exe_power)/*Main $(install_dir)
	cp $(exe_pwrstate)/pwrstate_notifier $(install_dir)
	cp $(exe_ds)/*Main $(install_dir)
	cp $(exe_tr69Bus)/*Main $(install_dir)
	cp $(exe_ds)/*Main $(install_dir)

clean:
	rm -rf $(install_dir)
	rm -rf $(install_lib_dir)
	

