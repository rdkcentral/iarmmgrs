#include "host.hpp"
#include "manager.hpp"
#include "dsRpc.h"
#include "sleepMode.hpp"

using namespace std;

namespace device{
	Host::Host() {

	}

	Host::~Host() {

	} 

	Host& Host::getInstance() {
		static Host instance; // instance is in thread-safe now.
		return instance;
	}

	List<VideoOutputPort> Host::getVideoOutputPorts() {
		return device::List<device::VideoOutputPort>();
	}

	VideoOutputPort& Host::getVideoOutputPort(const std::string& name) {
		return getVideoOutputPorts().at(0);
	}
	
	VideoOutputPort & VideoOutputPort::getInstance(int id) {
		return Host::getInstance().getVideoOutputPorts().at(0);
	}

	SleepMode Host::getPreferredSleepMode() {
		return SleepMode::getInstance(dsHOST_SLEEP_MODE_LIGHT);
	}

	void VideoOutputPort::enable() {
	}

	void VideoOutputPort::disable() {
	}

	void Manager::load() {
	}

	void Manager::DeInitialize() {
	}

	SleepMode::SleepMode(int id) {
	}

	SleepMode::~SleepMode() {
	}

	SleepMode & SleepMode::getInstance(int id) {
		static SleepMode instance(id);
		return instance;
	}

	List<SleepMode> SleepMode::getSleepModes() {
		List<SleepMode> sleepModes;
		return sleepModes;
	}
}
