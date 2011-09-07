/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This sub-API supports the following functionalities:
//
//  - Noise Suppression (NS).
//  - Automatic Gain Control (AGC).
//  - Echo Control (EC).
//  - Receiving side VAD, NS and AGC.
//  - Measurements of instantaneous speech, noise and echo levels.
//  - Generation of AP debug recordings.
//  - Detection of keyboard typing which can disrupt a voice conversation.
//
// Usage example, omitting error checking:
//
//  using namespace webrtc;
//  VoiceEngine* voe = VoiceEngine::Create();
//  VoEBase* base = VoEBase::GetInterface();
//  VoEAudioProcessing* ap = VoEAudioProcessing::GetInterface(voe);
//  base->Init();
//  ap->SetEcStatus(true, kAgcAdaptiveAnalog);
//  ...
//  base->Terminate();
//  base->Release();
//  ap->Release();
//  VoiceEngine::Delete(voe);
//
#ifndef WEBRTC_VOICE_ENGINE_VOE_AUDIO_PROCESSING_H
#define WEBRTC_VOICE_ENGINE_VOE_AUDIO_PROCESSING_H

#include "common_types.h"

namespace webrtc {

class VoiceEngine;

// VoERxVadCallback
class WEBRTC_DLLEXPORT VoERxVadCallback
{
public:
    virtual void OnRxVad(int channel, int vadDecision) = 0;

protected:
    virtual ~VoERxVadCallback() {}
};

// VoEAudioProcessing
class WEBRTC_DLLEXPORT VoEAudioProcessing
{
public:
    // Factory for the VoEAudioProcessing sub-API. Increases an internal
    // reference counter if successful. Returns NULL if the API is not
    // supported or if construction fails.
    static VoEAudioProcessing* GetInterface(VoiceEngine* voiceEngine);

    // Releases the VoEAudioProcessing sub-API and decreases an internal
    // reference counter. Returns the new reference count. This value should
    // be zero for all sub-API:s before the VoiceEngine object can be safely
    // deleted.
    virtual int Release() = 0;

    // Sets Noise Suppression (NS) status and mode.
    // The NS reduces noise in the microphone signal.
    virtual int SetNsStatus(bool enable, NsModes mode = kNsUnchanged) = 0;

    // Gets the NS status and mode.
    virtual int GetNsStatus(bool& enabled, NsModes& mode) = 0;

    // Sets the Automatic Gain Control (AGC) status and mode.
    // The AGC adjusts the microphone signal to an appropriate level.
    virtual int SetAgcStatus(bool enable, AgcModes mode = kAgcUnchanged) = 0;

    // Gets the AGC status and mode.
    virtual int GetAgcStatus(bool& enabled, AgcModes& mode) = 0;

    // Sets the AGC configuration.
    // Should only be used in situations where the working environment
    // is well known.
    virtual int SetAgcConfig(const AgcConfig config) = 0;

    // Gets the AGC configuration.
    virtual int GetAgcConfig(AgcConfig& config) = 0;

    // Sets the Echo Control (EC) status and mode.
    // The EC mitigates acoustic echo where a user can hear their own
    // speech repeated back due to an acoustic coupling between the
    // speaker and the microphone at the remote end.
    virtual int SetEcStatus(bool enable, EcModes mode = kEcUnchanged) = 0;

    // Gets the EC status and mode.
    virtual int GetEcStatus(bool& enabled, EcModes& mode) = 0;

    // Modifies settings for the AEC designed for mobile devices (AECM).
    virtual int SetAecmMode(AecmModes mode = kAecmSpeakerphone,
                            bool enableCNG = true) = 0;

    // Gets settings for the AECM.
    virtual int GetAecmMode(AecmModes& mode, bool& enabledCNG) = 0;

    // Sets status and mode of the receiving-side (Rx) NS.
    // The Rx NS reduces noise in the received signal for the specified
    // |channel|. Intended for advanced usage only.
    virtual int SetRxNsStatus(int channel,
                              bool enable,
                              NsModes mode = kNsUnchanged) = 0;

    // Gets status and mode of the receiving-side NS.
    virtual int GetRxNsStatus(int channel,
                              bool& enabled,
                              NsModes& mode) = 0;

    // Sets status and mode of the receiving-side (Rx) AGC.
    // The Rx AGC adjusts the received signal to an appropriate level
    // for the specified |channel|. Intended for advanced usage only.
    virtual int SetRxAgcStatus(int channel,
                               bool enable,
                               AgcModes mode = kAgcUnchanged) = 0;

    // Gets status and mode of the receiving-side AGC.
    virtual int GetRxAgcStatus(int channel,
                               bool& enabled,
                               AgcModes& mode) = 0;

    // Modifies the AGC configuration on the receiving side for the
    // specified |channel|.
    virtual int SetRxAgcConfig(int channel, const AgcConfig config) = 0;

    // Gets the AGC configuration on the receiving side.
    virtual int GetRxAgcConfig(int channel, AgcConfig& config) = 0;

    // Registers a VoERxVadCallback |observer| instance and enables Rx VAD
    // notifications for the specified |channel|.
    virtual int RegisterRxVadObserver(int channel,
                                      VoERxVadCallback &observer) = 0;

    // Deregisters the VoERxVadCallback |observer| and disables Rx VAD
    // notifications for the specified |channel|.
    virtual int DeRegisterRxVadObserver(int channel) = 0;

    // Gets the VAD/DTX activity for the specified |channel|.
    // The returned value is 1 if frames of audio contains speech
    // and 0 if silence. The output is always 1 if VAD is disabled.
    virtual int VoiceActivityIndicator(int channel) = 0;

    // Enables or disables the possibility to retrieve instantaneous
    // speech, noise and echo metrics during an active call.
    virtual int SetMetricsStatus(bool enable) = 0;

    // Gets the current speech, noise and echo metric status.
    virtual int GetMetricsStatus(bool& enabled) = 0;

    // Gets the instantaneous speech level metrics for the transmitted
    // and received signals.
    virtual int GetSpeechMetrics(int& levelTx, int& levelRx) = 0;

    // Gets the instantaneous noise level metrics for the transmitted
    // and received signals.
    virtual int GetNoiseMetrics(int& levelTx, int& levelRx) = 0;

    // Gets the instantaneous echo level metrics for the near-end and
    // far-end signals.
    virtual int GetEchoMetrics(int& ERL, int& ERLE, int& RERL, int& A_NLP) = 0;

    // Enables recording of Audio Processing (AP) debugging information.
    // The file can later be used for off-line analysis of the AP performance.
    virtual int StartDebugRecording(const char* fileNameUTF8) = 0;

    // Disables recording of AP debugging information.
    virtual int StopDebugRecording() = 0;

    // Enables or disables detection of disturbing keyboard typing.
    // An error notification will be given as a callback upon detection.
    virtual int SetTypingDetectionStatus(bool enable) = 0;

    // Gets the current typing detection status.
    virtual int GetTypingDetectionStatus(bool& enabled) = 0;

protected:
    VoEAudioProcessing() {}
    virtual ~VoEAudioProcessing() {}
};

}  //  namespace webrtc

#endif  // WEBRTC_VOICE_ENGINE_VOE_AUDIO_PROCESSING_H
