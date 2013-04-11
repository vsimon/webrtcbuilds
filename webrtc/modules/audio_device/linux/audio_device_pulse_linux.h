/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_AUDIO_DEVICE_AUDIO_DEVICE_PULSE_LINUX_H
#define WEBRTC_AUDIO_DEVICE_AUDIO_DEVICE_PULSE_LINUX_H

#include "audio_device_generic.h"
#include "audio_mixer_manager_pulse_linux.h"
#include "critical_section_wrapper.h"

#include <pulse/pulseaudio.h>

// Set this define to make the code behave like in GTalk/libjingle
//#define WEBRTC_PA_GTALK

// We define this flag if it's missing from our headers, because we want to be
// able to compile against old headers but still use PA_STREAM_ADJUST_LATENCY
// if run against a recent version of the library.
#ifndef PA_STREAM_ADJUST_LATENCY
#define PA_STREAM_ADJUST_LATENCY 0x2000U
#endif
#ifndef PA_STREAM_START_MUTED
#define PA_STREAM_START_MUTED 0x1000U
#endif

// Set this constant to 0 to disable latency reading
const uint32_t WEBRTC_PA_REPORT_LATENCY = 1;

// Constants from implementation by Tristan Schmelcher [tschmelcher@google.com]

// First PulseAudio protocol version that supports PA_STREAM_ADJUST_LATENCY.
const uint32_t WEBRTC_PA_ADJUST_LATENCY_PROTOCOL_VERSION = 13;

// Some timing constants for optimal operation. See
// https://tango.0pointer.de/pipermail/pulseaudio-discuss/2008-January/001170.html
// for a good explanation of some of the factors that go into this.

// Playback.

// For playback, there is a round-trip delay to fill the server-side playback
// buffer, so setting too low of a latency is a buffer underflow risk. We will
// automatically increase the latency if a buffer underflow does occur, but we
// also enforce a sane minimum at start-up time. Anything lower would be
// virtually guaranteed to underflow at least once, so there's no point in
// allowing lower latencies.
const uint32_t WEBRTC_PA_PLAYBACK_LATENCY_MINIMUM_MSECS = 20;

// Every time a playback stream underflows, we will reconfigure it with target
// latency that is greater by this amount.
const uint32_t WEBRTC_PA_PLAYBACK_LATENCY_INCREMENT_MSECS = 20;

// We also need to configure a suitable request size. Too small and we'd burn
// CPU from the overhead of transfering small amounts of data at once. Too large
// and the amount of data remaining in the buffer right before refilling it
// would be a buffer underflow risk. We set it to half of the buffer size.
const uint32_t WEBRTC_PA_PLAYBACK_REQUEST_FACTOR = 2;

// Capture.

// For capture, low latency is not a buffer overflow risk, but it makes us burn
// CPU from the overhead of transfering small amounts of data at once, so we set
// a recommended value that we use for the kLowLatency constant (but if the user
// explicitly requests something lower then we will honour it).
// 1ms takes about 6-7% CPU. 5ms takes about 5%. 10ms takes about 4.x%.
const uint32_t WEBRTC_PA_LOW_CAPTURE_LATENCY_MSECS = 10;

// There is a round-trip delay to ack the data to the server, so the
// server-side buffer needs extra space to prevent buffer overflow. 20ms is
// sufficient, but there is no penalty to making it bigger, so we make it huge.
// (750ms is libpulse's default value for the _total_ buffer size in the
// kNoLatencyRequirements case.)
const uint32_t WEBRTC_PA_CAPTURE_BUFFER_EXTRA_MSECS = 750;

const uint32_t WEBRTC_PA_MSECS_PER_SEC = 1000;

// Init _configuredLatencyRec/Play to this value to disable latency requirements
const int32_t WEBRTC_PA_NO_LATENCY_REQUIREMENTS = -1;

// Set this const to 1 to account for peeked and used data in latency calculation
const uint32_t WEBRTC_PA_CAPTURE_BUFFER_LATENCY_ADJUSTMENT = 0;

namespace webrtc
{
class EventWrapper;
class ThreadWrapper;

class AudioDeviceLinuxPulse: public AudioDeviceGeneric
{
public:
    AudioDeviceLinuxPulse(const int32_t id);
    ~AudioDeviceLinuxPulse();

    static bool PulseAudioIsSupported();

    // Retrieve the currently utilized audio layer
    virtual int32_t
        ActiveAudioLayer(AudioDeviceModule::AudioLayer& audioLayer) const;

    // Main initializaton and termination
    virtual int32_t Init();
    virtual int32_t Terminate();
    virtual bool Initialized() const;

    // Device enumeration
    virtual int16_t PlayoutDevices();
    virtual int16_t RecordingDevices();
    virtual int32_t PlayoutDeviceName(
        uint16_t index,
        char name[kAdmMaxDeviceNameSize],
        char guid[kAdmMaxGuidSize]);
    virtual int32_t RecordingDeviceName(
        uint16_t index,
        char name[kAdmMaxDeviceNameSize],
        char guid[kAdmMaxGuidSize]);

    // Device selection
    virtual int32_t SetPlayoutDevice(uint16_t index);
    virtual int32_t SetPlayoutDevice(
        AudioDeviceModule::WindowsDeviceType device);
    virtual int32_t SetRecordingDevice(uint16_t index);
    virtual int32_t SetRecordingDevice(
        AudioDeviceModule::WindowsDeviceType device);

    // Audio transport initialization
    virtual int32_t PlayoutIsAvailable(bool& available);
    virtual int32_t InitPlayout();
    virtual bool PlayoutIsInitialized() const;
    virtual int32_t RecordingIsAvailable(bool& available);
    virtual int32_t InitRecording();
    virtual bool RecordingIsInitialized() const;

    // Audio transport control
    virtual int32_t StartPlayout();
    virtual int32_t StopPlayout();
    virtual bool Playing() const;
    virtual int32_t StartRecording();
    virtual int32_t StopRecording();
    virtual bool Recording() const;

    // Microphone Automatic Gain Control (AGC)
    virtual int32_t SetAGC(bool enable);
    virtual bool AGC() const;

    // Volume control based on the Windows Wave API (Windows only)
    virtual int32_t SetWaveOutVolume(uint16_t volumeLeft, uint16_t volumeRight);
    virtual int32_t WaveOutVolume(uint16_t& volumeLeft,
                                  uint16_t& volumeRight) const;

    // Audio mixer initialization
    virtual int32_t SpeakerIsAvailable(bool& available);
    virtual int32_t InitSpeaker();
    virtual bool SpeakerIsInitialized() const;
    virtual int32_t MicrophoneIsAvailable(bool& available);
    virtual int32_t InitMicrophone();
    virtual bool MicrophoneIsInitialized() const;

    // Speaker volume controls
    virtual int32_t SpeakerVolumeIsAvailable(bool& available);
    virtual int32_t SetSpeakerVolume(uint32_t volume);
    virtual int32_t SpeakerVolume(uint32_t& volume) const;
    virtual int32_t MaxSpeakerVolume(uint32_t& maxVolume) const;
    virtual int32_t MinSpeakerVolume(uint32_t& minVolume) const;
    virtual int32_t SpeakerVolumeStepSize(uint16_t& stepSize) const;

    // Microphone volume controls
    virtual int32_t MicrophoneVolumeIsAvailable(bool& available);
    virtual int32_t SetMicrophoneVolume(uint32_t volume);
    virtual int32_t MicrophoneVolume(uint32_t& volume) const;
    virtual int32_t MaxMicrophoneVolume(uint32_t& maxVolume) const;
    virtual int32_t MinMicrophoneVolume(uint32_t& minVolume) const;
    virtual int32_t MicrophoneVolumeStepSize(
        uint16_t& stepSize) const;

    // Speaker mute control
    virtual int32_t SpeakerMuteIsAvailable(bool& available);
    virtual int32_t SetSpeakerMute(bool enable);
    virtual int32_t SpeakerMute(bool& enabled) const;

    // Microphone mute control
    virtual int32_t MicrophoneMuteIsAvailable(bool& available);
    virtual int32_t SetMicrophoneMute(bool enable);
    virtual int32_t MicrophoneMute(bool& enabled) const;

    // Microphone boost control
    virtual int32_t MicrophoneBoostIsAvailable(bool& available);
    virtual int32_t SetMicrophoneBoost(bool enable);
    virtual int32_t MicrophoneBoost(bool& enabled) const;

    // Stereo support
    virtual int32_t StereoPlayoutIsAvailable(bool& available);
    virtual int32_t SetStereoPlayout(bool enable);
    virtual int32_t StereoPlayout(bool& enabled) const;
    virtual int32_t StereoRecordingIsAvailable(bool& available);
    virtual int32_t SetStereoRecording(bool enable);
    virtual int32_t StereoRecording(bool& enabled) const;

    // Delay information and control
    virtual int32_t
        SetPlayoutBuffer(const AudioDeviceModule::BufferType type,
                         uint16_t sizeMS);
    virtual int32_t PlayoutBuffer(AudioDeviceModule::BufferType& type,
                                  uint16_t& sizeMS) const;
    virtual int32_t PlayoutDelay(uint16_t& delayMS) const;
    virtual int32_t RecordingDelay(uint16_t& delayMS) const;

    // CPU load
    virtual int32_t CPULoad(uint16_t& load) const;

public:
    virtual bool PlayoutWarning() const;
    virtual bool PlayoutError() const;
    virtual bool RecordingWarning() const;
    virtual bool RecordingError() const;
    virtual void ClearPlayoutWarning();
    virtual void ClearPlayoutError();
    virtual void ClearRecordingWarning();
    virtual void ClearRecordingError();

public:
    virtual void AttachAudioBuffer(AudioDeviceBuffer* audioBuffer);

private:
    void Lock()
    {
        _critSect.Enter();
    }
    ;
    void UnLock()
    {
        _critSect.Leave();
    }
    ;
    void WaitForOperationCompletion(pa_operation* paOperation) const;
    void WaitForSuccess(pa_operation* paOperation) const;

private:
    static void PaContextStateCallback(pa_context *c, void *pThis);
    static void PaSinkInfoCallback(pa_context *c, const pa_sink_info *i,
                                   int eol, void *pThis);
    static void PaSourceInfoCallback(pa_context *c, const pa_source_info *i,
                                     int eol, void *pThis);
    static void PaServerInfoCallback(pa_context *c, const pa_server_info *i,
                                     void *pThis);
    static void PaStreamStateCallback(pa_stream *p, void *pThis);
    void PaContextStateCallbackHandler(pa_context *c);
    void PaSinkInfoCallbackHandler(const pa_sink_info *i, int eol);
    void PaSourceInfoCallbackHandler(const pa_source_info *i, int eol);
    void PaServerInfoCallbackHandler(const pa_server_info *i);
    void PaStreamStateCallbackHandler(pa_stream *p);

    void EnableWriteCallback();
    void DisableWriteCallback();
    static void PaStreamWriteCallback(pa_stream *unused, size_t buffer_space,
                                      void *pThis);
    void PaStreamWriteCallbackHandler(size_t buffer_space);
    static void PaStreamUnderflowCallback(pa_stream *unused, void *pThis);
    void PaStreamUnderflowCallbackHandler();
    void EnableReadCallback();
    void DisableReadCallback();
    static void PaStreamReadCallback(pa_stream *unused1, size_t unused2,
                                     void *pThis);
    void PaStreamReadCallbackHandler();
    static void PaStreamOverflowCallback(pa_stream *unused, void *pThis);
    void PaStreamOverflowCallbackHandler();
    int32_t LatencyUsecs(pa_stream *stream);
    int32_t ReadRecordedData(const void* bufferData, size_t bufferSize);
    int32_t ProcessRecordedData(int8_t *bufferData,
                                uint32_t bufferSizeInSamples,
                                uint32_t recDelay);

    int32_t CheckPulseAudioVersion();
    int32_t InitSamplingFrequency();
    int32_t GetDefaultDeviceInfo(bool recDevice, char* name, uint16_t& index);
    int32_t InitPulseAudio();
    int32_t TerminatePulseAudio();

    void PaLock();
    void PaUnLock();

    static bool RecThreadFunc(void*);
    static bool PlayThreadFunc(void*);
    bool RecThreadProcess();
    bool PlayThreadProcess();

private:
    AudioDeviceBuffer* _ptrAudioBuffer;

    CriticalSectionWrapper& _critSect;
    EventWrapper& _timeEventRec;
    EventWrapper& _timeEventPlay;
    EventWrapper& _recStartEvent;
    EventWrapper& _playStartEvent;

    ThreadWrapper* _ptrThreadPlay;
    ThreadWrapper* _ptrThreadRec;
    uint32_t _recThreadID;
    uint32_t _playThreadID;
    int32_t _id;

    AudioMixerManagerLinuxPulse _mixerManager;

    uint16_t _inputDeviceIndex;
    uint16_t _outputDeviceIndex;
    bool _inputDeviceIsSpecified;
    bool _outputDeviceIsSpecified;

    uint32_t _samplingFreq;
    uint8_t _recChannels;
    uint8_t _playChannels;

    AudioDeviceModule::BufferType _playBufType;

private:
    bool _initialized;
    bool _recording;
    bool _playing;
    bool _recIsInitialized;
    bool _playIsInitialized;
    bool _startRec;
    bool _stopRec;
    bool _startPlay;
    bool _stopPlay;
    bool _AGC;
    bool update_speaker_volume_at_startup_;

private:
    uint16_t _playBufDelayFixed; // fixed playback delay

    uint32_t _sndCardPlayDelay;
    uint32_t _sndCardRecDelay;

    int32_t _writeErrors;
    uint16_t _playWarning;
    uint16_t _playError;
    uint16_t _recWarning;
    uint16_t _recError;

    uint16_t _deviceIndex;
    int16_t _numPlayDevices;
    int16_t _numRecDevices;
    char* _playDeviceName;
    char* _recDeviceName;
    char* _playDisplayDeviceName;
    char* _recDisplayDeviceName;
    char _paServerVersion[32];

    int8_t* _playBuffer;
    size_t _playbackBufferSize;
    size_t _playbackBufferUnused;
    size_t _tempBufferSpace;
    int8_t* _recBuffer;
    size_t _recordBufferSize;
    size_t _recordBufferUsed;
    const void* _tempSampleData;
    size_t _tempSampleDataSize;
    int32_t _configuredLatencyPlay;
    int32_t _configuredLatencyRec;

    // PulseAudio
    uint16_t _paDeviceIndex;
    bool _paStateChanged;

    pa_threaded_mainloop* _paMainloop;
    pa_mainloop_api* _paMainloopApi;
    pa_context* _paContext;

    pa_stream* _recStream;
    pa_stream* _playStream;
    uint32_t _recStreamFlags;
    uint32_t _playStreamFlags;
    pa_buffer_attr _playBufferAttr;
    pa_buffer_attr _recBufferAttr;
};

}

#endif  // MODULES_AUDIO_DEVICE_MAIN_SOURCE_LINUX_AUDIO_DEVICE_PULSE_LINUX_H_
