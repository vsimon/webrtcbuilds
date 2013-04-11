/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_TRANSMIT_MIXER_H
#define WEBRTC_VOICE_ENGINE_TRANSMIT_MIXER_H

#include "common_types.h"
#include "voe_base.h"
#include "file_player.h"
#include "file_recorder.h"
#include "level_indicator.h"
#include "module_common_types.h"
#include "monitor_module.h"
#include "resampler.h"
#include "voice_engine_defines.h"


namespace webrtc {

class AudioProcessing;
class ProcessThread;
class VoEExternalMedia;
class VoEMediaProcess;

namespace voe {

class ChannelManager;
class MixedAudio;
class Statistics;

class TransmitMixer : public MonitorObserver,
                      public FileCallback

{
public:
    static int32_t Create(TransmitMixer*& mixer, const uint32_t instanceId);

    static void Destroy(TransmitMixer*& mixer);

    int32_t SetEngineInformation(ProcessThread& processThread,
                                 Statistics& engineStatistics,
                                 ChannelManager& channelManager);

    int32_t SetAudioProcessingModule(
        AudioProcessing* audioProcessingModule);

    int32_t PrepareDemux(const void* audioSamples,
                         const uint32_t nSamples,
                         const uint8_t  nChannels,
                         const uint32_t samplesPerSec,
                         const uint16_t totalDelayMS,
                         const int32_t  clockDrift,
                         const uint16_t currentMicLevel);


    int32_t DemuxAndMix();

    int32_t EncodeAndSend();

    uint32_t CaptureLevel() const;

    int32_t StopSend();

    // VoEDtmf
    void UpdateMuteMicrophoneTime(const uint32_t lengthMs);

    // VoEExternalMedia
    int RegisterExternalMediaProcessing(VoEMediaProcess* object,
                                        ProcessingTypes type);
    int DeRegisterExternalMediaProcessing(ProcessingTypes type);

    int GetMixingFrequency();

    // VoEVolumeControl
    int SetMute(const bool enable);

    bool Mute() const;

    int8_t AudioLevel() const;

    int16_t AudioLevelFullRange() const;

    bool IsRecordingCall();

    bool IsRecordingMic();

    int StartPlayingFileAsMicrophone(const char* fileName,
                                     const bool loop,
                                     const FileFormats format,
                                     const int startPosition,
                                     const float volumeScaling,
                                     const int stopPosition,
                                     const CodecInst* codecInst);

    int StartPlayingFileAsMicrophone(InStream* stream,
                                     const FileFormats format,
                                     const int startPosition,
                                     const float volumeScaling,
                                     const int stopPosition,
                                     const CodecInst* codecInst);

    int StopPlayingFileAsMicrophone();

    int IsPlayingFileAsMicrophone() const;

    int ScaleFileAsMicrophonePlayout(const float scale);

    int StartRecordingMicrophone(const char* fileName,
                                 const CodecInst* codecInst);

    int StartRecordingMicrophone(OutStream* stream,
                                 const CodecInst* codecInst);

    int StopRecordingMicrophone();

    int StartRecordingCall(const char* fileName, const CodecInst* codecInst);

    int StartRecordingCall(OutStream* stream, const CodecInst* codecInst);

    int StopRecordingCall();

    void SetMixWithMicStatus(bool mix);

    int32_t RegisterVoiceEngineObserver(VoiceEngineObserver& observer);

    virtual ~TransmitMixer();

    // MonitorObserver
    void OnPeriodicProcess();


    // FileCallback
    void PlayNotification(const int32_t id,
                          const uint32_t durationMs);

    void RecordNotification(const int32_t id,
                            const uint32_t durationMs);

    void PlayFileEnded(const int32_t id);

    void RecordFileEnded(const int32_t id);

#ifdef WEBRTC_VOICE_ENGINE_TYPING_DETECTION
    // Typing detection
    int TimeSinceLastTyping(int &seconds);
    int SetTypingDetectionParameters(int timeWindow,
                                     int costPerTyping,
                                     int reportingThreshold,
                                     int penaltyDecay,
                                     int typeEventDelay);
#endif

  void EnableStereoChannelSwapping(bool enable);
  bool IsStereoChannelSwappingEnabled();

private:
    TransmitMixer(const uint32_t instanceId);

    // Gets the maximum sample rate and number of channels over all currently
    // sending codecs.
    void GetSendCodecInfo(int* max_sample_rate, int* max_channels);

    int GenerateAudioFrame(const int16_t audioSamples[],
                           int nSamples,
                           int nChannels,
                           int samplesPerSec);
    int32_t RecordAudioToFile(const uint32_t mixingFrequency);

    int32_t MixOrReplaceAudioWithFile(
        const int mixingFrequency);

    void ProcessAudio(int delay_ms, int clock_drift, int current_mic_level);

#ifdef WEBRTC_VOICE_ENGINE_TYPING_DETECTION
    int TypingDetection();
#endif

    // uses
    Statistics* _engineStatisticsPtr;
    ChannelManager* _channelManagerPtr;
    AudioProcessing* audioproc_;
    VoiceEngineObserver* _voiceEngineObserverPtr;
    ProcessThread* _processThreadPtr;

    // owns
    MonitorModule _monitorModule;
    AudioFrame _audioFrame;
    Resampler _audioResampler; // ADM sample rate -> mixing rate
    FilePlayer* _filePlayerPtr;
    FileRecorder* _fileRecorderPtr;
    FileRecorder* _fileCallRecorderPtr;
    int _filePlayerId;
    int _fileRecorderId;
    int _fileCallRecorderId;
    bool _filePlaying;
    bool _fileRecording;
    bool _fileCallRecording;
    voe::AudioLevel _audioLevel;
    // protect file instances and their variables in MixedParticipants()
    CriticalSectionWrapper& _critSect;
    CriticalSectionWrapper& _callbackCritSect;

#ifdef WEBRTC_VOICE_ENGINE_TYPING_DETECTION
    int32_t _timeActive;
    int32_t _timeSinceLastTyping;
    int32_t _penaltyCounter;
    bool _typingNoiseWarning;

    // Tunable treshold values
    int _timeWindow; // nr of10ms slots accepted to count as a hit.
    int _costPerTyping; // Penalty added for a typing + activity coincide.
    int _reportingThreshold; // Threshold for _penaltyCounter.
    int _penaltyDecay; // How much we reduce _penaltyCounter every 10 ms.
    int _typeEventDelay; // How old typing events we allow

#endif
    bool _saturationWarning;

    int _instanceId;
    bool _mixFileWithMicrophone;
    uint32_t _captureLevel;
    VoEMediaProcess* external_postproc_ptr_;
    VoEMediaProcess* external_preproc_ptr_;
    bool _mute;
    int32_t _remainingMuteMicTimeMs;
    bool stereo_codec_;
    bool swap_stereo_channels_;
};

#endif // WEBRTC_VOICE_ENGINE_TRANSMIT_MIXER_H

}  //  namespace voe

}  // namespace webrtc
