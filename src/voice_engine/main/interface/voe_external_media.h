/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// In some cases it is desirable to use an audio source or sink which may
// not be available to the VoiceEngine, such as a DV camera. This sub-API
// contains functions that allow for the use of such external recording
// sources and playout sinks. It also describes how recorded data, or data
// to be played out, can be modified outside the VoiceEngine.
//
// Usage example, omitting error checking:
//
//  using namespace webrtc;
//  VoiceEngine* voe = VoiceEngine::Create();
//  VoEBase* base = VoEBase::GetInterface(voe);
//  VoEMediaProcess media = VoEMediaProcess::GetInterface(voe);
//  base->Init();
//  ...
//  media->SetExternalRecordingStatus(true);
//  ...
//  base->Terminate();
//  base->Release();
//  media->Release();
//  VoiceEngine::Delete(voe);
//
#ifndef WEBRTC_VOICE_ENGINE_VOE_EXTERNAL_MEDIA_H
#define WEBRTC_VOICE_ENGINE_VOE_EXTERNAL_MEDIA_H

#include "common_types.h"

namespace webrtc {

class VoiceEngine;

class WEBRTC_DLLEXPORT VoEMediaProcess
{
public:
    // The VoiceEngine user should override the Process() method in a
    // derived class. Process() will be called when audio is ready to
    // be processed. The audio can be accessed in several different modes
    // given by the |type| parameter. The function should modify the
    // original data and ensure that it is copied back to the |audio10ms|
    // array. The number of samples in the frame cannot be changed.
    // The sampling frequency will depend upon the codec used. 
    // If |isStereo| is true, audio10ms will contain 16-bit PCM data
    // samples in interleaved stereo format (L0,R0,L1,R1,�):
    virtual void Process(const int channel, const ProcessingTypes type,
                         WebRtc_Word16 audio10ms[], const int length,
                         const int samplingFreq, const bool isStereo) = 0;

protected:
    virtual ~VoEMediaProcess() {}
};

class WEBRTC_DLLEXPORT VoEExternalMedia
{
public:
    // Factory for the VoEExternalMedia sub-API. Increases an internal
    // reference counter if successful. Returns NULL if the API is not
    // supported or if construction fails.
    static VoEExternalMedia* GetInterface(VoiceEngine* voiceEngine);

    // Releases the VoEExternalMedia sub-API and decreases an internal
    // reference counter. Returns the new reference count. This value should
    // be zero for all sub-API:s before the VoiceEngine object can be safely
    // deleted.
    virtual int Release() = 0;

    // Installs a VoEMediaProcess derived instance and activates external
    // media for the specified |channel| and |type|.
    virtual int RegisterExternalMediaProcessing(
        int channel, ProcessingTypes type, VoEMediaProcess& processObject) = 0;

    // Removes the VoEMediaProcess derived instance and deactivates external
    // media for the specified |channel| and |type|.
    virtual int DeRegisterExternalMediaProcessing(
        int channel, ProcessingTypes type) = 0;

    // Toogles state of external recording.
    virtual int SetExternalRecordingStatus(bool enable) = 0;

    // Toogles state of external playout.
    virtual int SetExternalPlayoutStatus(bool enable) = 0;

    // This function accepts externally recorded audio. During transmission,
    // this method should be called at as regular an interval as possible
    // with frames of corresponding size.
    virtual int ExternalRecordingInsertData(
        const WebRtc_Word16 speechData10ms[], int lengthSamples,
        int samplingFreqHz, int current_delay_ms) = 0;

    // This function gets audio for an external playout sink.
    // During transmission, this function should be called every ~10 ms
    // to obtain a new 10 ms frame of audio. The length of the block will
    // be 160, 320, 440 or 480 samples (for 16, 32, 44 or 48 kHz sampling
    // rates respectively).
    virtual int ExternalPlayoutGetData(
        WebRtc_Word16 speechData10ms[], int samplingFreqHz,
        int current_delay_ms, int& lengthSamples) = 0;

protected:
    VoEExternalMedia() {}
    virtual ~VoEExternalMedia() {}
};

}  // namespace webrtc

#endif  //  WEBRTC_VOICE_ENGINE_VOE_EXTERNAL_MEDIA_H
