/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_AUDIO_DEVICE_AUDIO_DEVICE_BUFFER_H
#define WEBRTC_AUDIO_DEVICE_AUDIO_DEVICE_BUFFER_H

#include "typedefs.h"
#include "../../../../common_audio/resampler/main/interface/resampler.h"
#include "file_wrapper.h"
#include "audio_device.h"
#include "list_wrapper.h"

namespace webrtc {
class CriticalSectionWrapper;

const WebRtc_UWord32 kPulsePeriodMs = 1000;

class AudioDeviceObserver;
class MediaFile;

class AudioDeviceBuffer
{
public:
    void SetId(WebRtc_UWord32 id);
    WebRtc_Word32 RegisterAudioCallback(AudioTransport* audioCallback);

    WebRtc_Word32 InitPlayout();
    WebRtc_Word32 InitRecording();

    WebRtc_Word32 SetRecordingSampleRate(WebRtc_UWord32 fsHz);
    WebRtc_Word32 SetPlayoutSampleRate(WebRtc_UWord32 fsHz);
    WebRtc_Word32 RecordingSampleRate() const;
    WebRtc_Word32 PlayoutSampleRate() const;

    WebRtc_Word32 SetRecordingChannels(WebRtc_UWord8 channels);
    WebRtc_Word32 SetPlayoutChannels(WebRtc_UWord8 channels);
    WebRtc_UWord8 RecordingChannels() const;
    WebRtc_UWord8 PlayoutChannels() const;
    WebRtc_Word32 SetRecordingChannel(const AudioDeviceModule::ChannelType channel);
    WebRtc_Word32 RecordingChannel(AudioDeviceModule::ChannelType& channel) const;

    WebRtc_Word32 SetRecordedBuffer(const WebRtc_Word8* audioBuffer, WebRtc_UWord32 nSamples);
    WebRtc_Word32 SetCurrentMicLevel(WebRtc_UWord32 level);
    WebRtc_Word32 SetVQEData(WebRtc_UWord32 playDelayMS, WebRtc_UWord32 recDelayMS, WebRtc_Word32 clockDrift);
    WebRtc_Word32 DeliverRecordedData();
    WebRtc_UWord32 NewMicLevel() const;

    WebRtc_Word32 RequestPlayoutData(WebRtc_UWord32 nSamples);
    WebRtc_Word32 GetPlayoutData(WebRtc_Word8* audioBuffer);

    WebRtc_Word32 StartInputFileRecording(const WebRtc_Word8 fileName[kAdmMaxFileNameSize]);
    WebRtc_Word32 StopInputFileRecording();
    WebRtc_Word32 StartOutputFileRecording(const WebRtc_Word8 fileName[kAdmMaxFileNameSize]);
    WebRtc_Word32 StopOutputFileRecording();

    AudioDeviceBuffer();
    ~AudioDeviceBuffer();

private:
    void _EmptyList();

private:
    WebRtc_Word32                   _id;
    CriticalSectionWrapper&         _critSect;
    CriticalSectionWrapper&         _critSectCb;

    AudioTransport*                 _ptrCbAudioTransport;

    WebRtc_UWord32                  _recSampleRate;
    WebRtc_UWord32                  _playSampleRate;

    WebRtc_UWord8                   _recChannels;
    WebRtc_UWord8                   _playChannels;

    // selected recording channel (left/right/both)
    AudioDeviceModule::ChannelType _recChannel;

    // 2 or 4 depending on mono or stereo
    WebRtc_UWord8                   _recBytesPerSample;
    WebRtc_UWord8                   _playBytesPerSample;

    // 10ms in stereo @ 48kHz
    WebRtc_Word8                    _recBuffer[1920];

    // one sample <=> 2 or 4 bytes
    WebRtc_UWord32                  _recSamples;
    WebRtc_UWord32                  _recSize;           // in bytes

    // 10ms in stereo @ 48kHz
    WebRtc_Word8                    _playBuffer[1920];

    // one sample <=> 2 or 4 bytes
    WebRtc_UWord32                  _playSamples;
    WebRtc_UWord32                  _playSize;          // in bytes

    FileWrapper&                    _recFile;
    FileWrapper&                    _playFile;

    WebRtc_UWord32                  _currentMicLevel;
    WebRtc_UWord32                  _newMicLevel;

    WebRtc_UWord32                  _playDelayMS;
    WebRtc_UWord32                  _recDelayMS;

    WebRtc_Word32                   _clockDrift;

    bool                            _measureDelay;
    ListWrapper                     _pulseList;
    WebRtc_UWord32                  _lastPulseTime;
};

}  // namespace webrtc

#endif  // WEBRTC_AUDIO_DEVICE_AUDIO_DEVICE_BUFFER_H
