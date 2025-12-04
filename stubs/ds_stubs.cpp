#include "host.hpp"
#include "iarm/IarmImpl.hpp"
#include "videoOutputPortType.hpp"
#include "audioOutputPortType.hpp"
#include "manager.hpp"
#include "dsRpc.h"
#include "sleepMode.hpp"

using namespace std;

namespace device{
    Host::Host() {

    }

    Host::~Host() {

    }

    IarmImpl::~IarmImpl() {}

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

    VideoOutputPort::~VideoOutputPort() {}

    const VideoOutputPortType &VideoOutputPort::getType() const {
        static VideoOutputPortType t(0);
        return t;
    }

    VideoOutputPort::Display::Display(VideoOutputPort &vPort) :
                                                            _productCode(0), _serialNumber(0),
                                                            _manufacturerYear(0), _manufacturerWeek(0),
                                                            _hdmiDeviceType(true), _isSurroundCapable(false),
                                                            _isDeviceRepeater(false), _aspectRatio(0),
                                                            _physicalAddressA(1), _physicalAddressB(0),
                                                            _physicalAddressC(0), _physicalAddressD(0),
                                                            _handle(-1)
    {

    }

    VideoOutputPort::Display::~Display() {}

    AudioOutputPort::AudioOutputPort(const int type, const int index, const int id) {
    }

    AudioOutputPort::~AudioOutputPort() {
    }

    List<AudioOutputPort> Host::getAudioOutputPorts() {
        return device::List<device::AudioOutputPort>();
    }

    AudioOutputPort& Host::getAudioOutputPort(const std::string& name) {
        return getAudioOutputPorts().at(0);
    }

    AudioOutputPort& Host::getAudioOutputPort(int id) {
        return getAudioOutputPorts().at(0);
    }
    
    AudioOutputPort & AudioOutputPort::getInstance(int id) {
        return Host::getInstance().getAudioOutputPorts().at(0);
    }

    const AudioOutputPortType &AudioOutputPort::getType() const {
        static AudioOutputPortType t(0);
        return t;
    }

    VideoOutputPortType::VideoOutputPortType(const int id) {
        (void)id;
        _dtcpSupported = false;
        _hdcpSupported = false;
        _dynamic = false;
        _restrictedResolution = 0;
    }

    VideoOutputPortType::~VideoOutputPortType() {}

    AudioOutputPortType::AudioOutputPortType(int id) {
        (void)id;
    }

    AudioOutputPortType::~AudioOutputPortType() {}

    SleepMode Host::getPreferredSleepMode() {
        return SleepMode::getInstance(dsHOST_SLEEP_MODE_LIGHT);
    }

    void VideoOutputPort::enable() {
    }

    void VideoOutputPort::disable() {
    }

    void AudioOutputPort::enable() {
    }

    void AudioOutputPort::disable() {
    }

    bool AudioOutputPort::getEnablePersist () const {
        return true;
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