/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_AUDIO_DEVICE_AUDIO_DEVICE_H
#define WEBRTC_AUDIO_DEVICE_AUDIO_DEVICE_H

#include "module.h"
#include "audio_device_defines.h"

namespace webrtc {

class AudioDeviceModule : public Module
{
public:
    enum ErrorCode
    {
        kAdmErrNone = 0,
        kAdmErrArgument = 1
    };

    enum AudioLayer
    {
        kPlatformDefaultAudio = 0,
        kWindowsWaveAudio = 1,
        kWindowsCoreAudio = 2,
        kLinuxAlsaAudio = 3,
        kLinuxPulseAudio = 4,
        kDummyAudio = 5
    };

    enum WindowsDeviceType
    {
        kDefaultCommunicationDevice = -1,
        kDefaultDevice = -2
    };

    enum BufferType
    {
        kFixedBufferSize  = 0,
        kAdaptiveBufferSize = 1
    };

    enum ChannelType
    {
        kChannelLeft = 0,
        kChannelRight = 1,
        kChannelBoth = 2
    };

public:
    // Factory methods (resource allocation/deallocation)
    static AudioDeviceModule* Create(
        const int32_t id,
        const AudioLayer audioLayer = kPlatformDefaultAudio);
    static void Destroy(AudioDeviceModule* module);

    // Retrieve the currently utilized audio layer
    virtual int32_t ActiveAudioLayer(AudioLayer* audioLayer) const = 0;

    // Module methods
    static int32_t GetVersion(char* version,
                              uint32_t& remainingBufferInBytes,
                              uint32_t& position);
    virtual int32_t ChangeUniqueId(const int32_t id) = 0;

    // Error handling
    virtual ErrorCode LastError() const = 0;
    virtual int32_t RegisterEventObserver(
        AudioDeviceObserver* eventCallback) = 0;

    // Full-duplex transportation of PCM audio
    virtual int32_t RegisterAudioCallback(AudioTransport* audioCallback) = 0;

    // Main initialization and termination
    virtual int32_t Init() = 0;
    virtual int32_t Terminate() = 0;
    virtual bool Initialized() const = 0;

    // Device enumeration
    virtual int16_t PlayoutDevices() = 0;
    virtual int16_t RecordingDevices() = 0;
    virtual int32_t PlayoutDeviceName(uint16_t index,
                                      char name[kAdmMaxDeviceNameSize],
                                      char guid[kAdmMaxGuidSize]) = 0;
    virtual int32_t RecordingDeviceName(uint16_t index,
                                        char name[kAdmMaxDeviceNameSize],
                                        char guid[kAdmMaxGuidSize]) = 0;

    // Device selection
    virtual int32_t SetPlayoutDevice(uint16_t index) = 0;
    virtual int32_t SetPlayoutDevice(WindowsDeviceType device) = 0;
    virtual int32_t SetRecordingDevice(uint16_t index) = 0;
    virtual int32_t SetRecordingDevice(WindowsDeviceType device) = 0;

    // Audio transport initialization
    virtual int32_t PlayoutIsAvailable(bool* available) = 0;
    virtual int32_t InitPlayout() = 0;
    virtual bool PlayoutIsInitialized() const = 0;
    virtual int32_t RecordingIsAvailable(bool* available) = 0;
    virtual int32_t InitRecording() = 0;
    virtual bool RecordingIsInitialized() const = 0;

    // Audio transport control
    virtual int32_t StartPlayout() = 0;
    virtual int32_t StopPlayout() = 0;
    virtual bool Playing() const = 0;
    virtual int32_t StartRecording() = 0;
    virtual int32_t StopRecording() = 0;
    virtual bool Recording() const = 0;

    // Microphone Automatic Gain Control (AGC)
    virtual int32_t SetAGC(bool enable) = 0;
    virtual bool AGC() const = 0;

    // Volume control based on the Windows Wave API (Windows only)
    virtual int32_t SetWaveOutVolume(uint16_t volumeLeft,
                                     uint16_t volumeRight) = 0;
    virtual int32_t WaveOutVolume(uint16_t* volumeLeft,
                                  uint16_t* volumeRight) const = 0;

    // Audio mixer initialization
    virtual int32_t SpeakerIsAvailable(bool* available) = 0;
    virtual int32_t InitSpeaker() = 0;
    virtual bool SpeakerIsInitialized() const = 0;
    virtual int32_t MicrophoneIsAvailable(bool* available) = 0;
    virtual int32_t InitMicrophone() = 0;
    virtual bool MicrophoneIsInitialized() const = 0;

    // Speaker volume controls
    virtual int32_t SpeakerVolumeIsAvailable(bool* available) = 0;
    virtual int32_t SetSpeakerVolume(uint32_t volume) = 0;
    virtual int32_t SpeakerVolume(uint32_t* volume) const = 0;
    virtual int32_t MaxSpeakerVolume(uint32_t* maxVolume) const = 0;
    virtual int32_t MinSpeakerVolume(uint32_t* minVolume) const = 0;
    virtual int32_t SpeakerVolumeStepSize(uint16_t* stepSize) const = 0;

    // Microphone volume controls
    virtual int32_t MicrophoneVolumeIsAvailable(bool* available) = 0;
    virtual int32_t SetMicrophoneVolume(uint32_t volume) = 0;
    virtual int32_t MicrophoneVolume(uint32_t* volume) const = 0;
    virtual int32_t MaxMicrophoneVolume(uint32_t* maxVolume) const = 0;
    virtual int32_t MinMicrophoneVolume(uint32_t* minVolume) const = 0;
    virtual int32_t MicrophoneVolumeStepSize(uint16_t* stepSize) const = 0;

    // Speaker mute control
    virtual int32_t SpeakerMuteIsAvailable(bool* available) = 0;
    virtual int32_t SetSpeakerMute(bool enable) = 0;
    virtual int32_t SpeakerMute(bool* enabled) const = 0;

    // Microphone mute control
    virtual int32_t MicrophoneMuteIsAvailable(bool* available) = 0;
    virtual int32_t SetMicrophoneMute(bool enable) = 0;
    virtual int32_t MicrophoneMute(bool* enabled) const = 0;

    // Microphone boost control
    virtual int32_t MicrophoneBoostIsAvailable(bool* available) = 0;
    virtual int32_t SetMicrophoneBoost(bool enable) = 0;
    virtual int32_t MicrophoneBoost(bool* enabled) const = 0;

    // Stereo support
    virtual int32_t StereoPlayoutIsAvailable(bool* available) const = 0;
    virtual int32_t SetStereoPlayout(bool enable) = 0;
    virtual int32_t StereoPlayout(bool* enabled) const = 0;
    virtual int32_t StereoRecordingIsAvailable(bool* available) const = 0;
    virtual int32_t SetStereoRecording(bool enable) = 0;
    virtual int32_t StereoRecording(bool* enabled) const = 0;
    virtual int32_t SetRecordingChannel(const ChannelType channel) = 0;
    virtual int32_t RecordingChannel(ChannelType* channel) const = 0;

    // Delay information and control
    virtual int32_t SetPlayoutBuffer(const BufferType type,
                                     uint16_t sizeMS = 0) = 0;
    virtual int32_t PlayoutBuffer(BufferType* type,
                                  uint16_t* sizeMS) const = 0;
    virtual int32_t PlayoutDelay(uint16_t* delayMS) const = 0;
    virtual int32_t RecordingDelay(uint16_t* delayMS) const = 0;

    // CPU load
    virtual int32_t CPULoad(uint16_t* load) const = 0;

    // Recording of raw PCM data
    virtual int32_t StartRawOutputFileRecording(
        const char pcmFileNameUTF8[kAdmMaxFileNameSize]) = 0;
    virtual int32_t StopRawOutputFileRecording() = 0;
    virtual int32_t StartRawInputFileRecording(
        const char pcmFileNameUTF8[kAdmMaxFileNameSize]) = 0;
    virtual int32_t StopRawInputFileRecording() = 0;

    // Native sample rate controls (samples/sec)
    virtual int32_t SetRecordingSampleRate(const uint32_t samplesPerSec) = 0;
    virtual int32_t RecordingSampleRate(uint32_t* samplesPerSec) const = 0;
    virtual int32_t SetPlayoutSampleRate(const uint32_t samplesPerSec) = 0;
    virtual int32_t PlayoutSampleRate(uint32_t* samplesPerSec) const = 0;

    // Mobile device specific functions
    virtual int32_t ResetAudioDevice() = 0;
    virtual int32_t SetLoudspeakerStatus(bool enable) = 0;
    virtual int32_t GetLoudspeakerStatus(bool* enabled) const = 0;

    // Android specific function
    static int32_t SetAndroidObjects(void* javaVM, void* env, void* context);

protected:
    virtual ~AudioDeviceModule() {};
};

}  // namespace webrtc

#endif  // WEBRTC_AUDIO_DEVICE_AUDIO_DEVICE_H
