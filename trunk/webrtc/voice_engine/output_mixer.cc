/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "output_mixer.h"

#include "audio_processing.h"
#include "audio_frame_operations.h"
#include "critical_section_wrapper.h"
#include "file_wrapper.h"
#include "output_mixer_internal.h"
#include "statistics.h"
#include "trace.h"
#include "voe_external_media.h"

namespace webrtc {

namespace voe {

void
OutputMixer::NewMixedAudio(const WebRtc_Word32 id,
                           const AudioFrame& generalAudioFrame,
                           const AudioFrame** uniqueAudioFrames,
                           const WebRtc_UWord32 size)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,-1),
                 "OutputMixer::NewMixedAudio(id=%d, size=%u)", id, size);

    _audioFrame.CopyFrom(generalAudioFrame);
    _audioFrame.id_ = id;
}

void OutputMixer::MixedParticipants(
    const WebRtc_Word32 id,
    const ParticipantStatistics* participantStatistics,
    const WebRtc_UWord32 size)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,-1),
                 "OutputMixer::MixedParticipants(id=%d, size=%u)", id, size);
}

void OutputMixer::VADPositiveParticipants(
    const WebRtc_Word32 id,
    const ParticipantStatistics* participantStatistics,
    const WebRtc_UWord32 size)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,-1),
                 "OutputMixer::VADPositiveParticipants(id=%d, size=%u)",
                 id, size);
}

void OutputMixer::MixedAudioLevel(const WebRtc_Word32  id,
                                  const WebRtc_UWord32 level)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,-1),
                 "OutputMixer::MixedAudioLevel(id=%d, level=%u)", id, level);
}

void OutputMixer::PlayNotification(const WebRtc_Word32 id,
                                   const WebRtc_UWord32 durationMs)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,-1),
                 "OutputMixer::PlayNotification(id=%d, durationMs=%d)",
                 id, durationMs);
    // Not implement yet
}

void OutputMixer::RecordNotification(const WebRtc_Word32 id,
                                     const WebRtc_UWord32 durationMs)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,-1),
                 "OutputMixer::RecordNotification(id=%d, durationMs=%d)",
                 id, durationMs);

    // Not implement yet
}

void OutputMixer::PlayFileEnded(const WebRtc_Word32 id)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,-1),
                 "OutputMixer::PlayFileEnded(id=%d)", id);

    // not needed
}

void OutputMixer::RecordFileEnded(const WebRtc_Word32 id)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,-1),
                 "OutputMixer::RecordFileEnded(id=%d)", id);
    assert(id == _instanceId);

    CriticalSectionScoped cs(&_fileCritSect);
    _outputFileRecording = false;
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId,-1),
                 "OutputMixer::RecordFileEnded() =>"
                 "output file recorder module is shutdown");
}

WebRtc_Word32
OutputMixer::Create(OutputMixer*& mixer, const WebRtc_UWord32 instanceId)
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, instanceId,
                 "OutputMixer::Create(instanceId=%d)", instanceId);
    mixer = new OutputMixer(instanceId);
    if (mixer == NULL)
    {
        WEBRTC_TRACE(kTraceMemory, kTraceVoice, instanceId,
                     "OutputMixer::Create() unable to allocate memory for"
                     "mixer");
        return -1;
    }
    return 0;
}

OutputMixer::OutputMixer(const WebRtc_UWord32 instanceId) :
    _callbackCritSect(*CriticalSectionWrapper::CreateCriticalSection()),
    _fileCritSect(*CriticalSectionWrapper::CreateCriticalSection()),
    _mixerModule(*AudioConferenceMixer::Create(instanceId)),
    _audioLevel(),
    _dtmfGenerator(instanceId),
    _instanceId(instanceId),
    _externalMediaCallbackPtr(NULL),
    _externalMedia(false),
    _panLeft(1.0f),
    _panRight(1.0f),
    _mixingFrequencyHz(8000),
    _outputFileRecorderPtr(NULL),
    _outputFileRecording(false)
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_instanceId,-1),
                 "OutputMixer::OutputMixer() - ctor");

    if ((_mixerModule.RegisterMixedStreamCallback(*this) == -1) ||
        (_mixerModule.RegisterMixerStatusCallback(*this, 100) == -1))
    {
        WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId,-1),
                     "OutputMixer::OutputMixer() failed to register mixer"
                     "callbacks");
    }

    _dtmfGenerator.Init();
}

void
OutputMixer::Destroy(OutputMixer*& mixer)
{
    if (mixer)
    {
        delete mixer;
        mixer = NULL;
    }
}

OutputMixer::~OutputMixer()
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_instanceId,-1),
                 "OutputMixer::~OutputMixer() - dtor");
    if (_externalMedia)
    {
        DeRegisterExternalMediaProcessing();
    }
    {
        CriticalSectionScoped cs(&_fileCritSect);
        if (_outputFileRecorderPtr)
        {
            _outputFileRecorderPtr->RegisterModuleFileCallback(NULL);
            _outputFileRecorderPtr->StopRecording();
            FileRecorder::DestroyFileRecorder(_outputFileRecorderPtr);
            _outputFileRecorderPtr = NULL;
        }
    }
    _mixerModule.UnRegisterMixerStatusCallback();
    _mixerModule.UnRegisterMixedStreamCallback();
    delete &_mixerModule;
    delete &_callbackCritSect;
    delete &_fileCritSect;
}

WebRtc_Word32
OutputMixer::SetEngineInformation(voe::Statistics& engineStatistics)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,-1),
                 "OutputMixer::SetEngineInformation()");
    _engineStatisticsPtr = &engineStatistics;
    return 0;
}

WebRtc_Word32
OutputMixer::SetAudioProcessingModule(
    AudioProcessing* audioProcessingModule)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,-1),
                 "OutputMixer::SetAudioProcessingModule("
                 "audioProcessingModule=0x%x)", audioProcessingModule);
    _audioProcessingModulePtr = audioProcessingModule;
    return 0;
}

int OutputMixer::RegisterExternalMediaProcessing(
    VoEMediaProcess& proccess_object)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,-1),
               "OutputMixer::RegisterExternalMediaProcessing()");

    CriticalSectionScoped cs(&_callbackCritSect);
    _externalMediaCallbackPtr = &proccess_object;
    _externalMedia = true;

    return 0;
}

int OutputMixer::DeRegisterExternalMediaProcessing()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,-1),
                 "OutputMixer::DeRegisterExternalMediaProcessing()");

    CriticalSectionScoped cs(&_callbackCritSect);
    _externalMedia = false;
    _externalMediaCallbackPtr = NULL;

    return 0;
}

int OutputMixer::PlayDtmfTone(WebRtc_UWord8 eventCode, int lengthMs,
                              int attenuationDb)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "OutputMixer::PlayDtmfTone()");
    if (_dtmfGenerator.AddTone(eventCode, lengthMs, attenuationDb) != 0)
    {
        _engineStatisticsPtr->SetLastError(VE_STILL_PLAYING_PREV_DTMF,
                                           kTraceError,
                                           "OutputMixer::PlayDtmfTone()");
        return -1;
    }
    return 0;
}

int OutputMixer::StartPlayingDtmfTone(WebRtc_UWord8 eventCode,
                                      int attenuationDb)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "OutputMixer::StartPlayingDtmfTone()");
    if (_dtmfGenerator.StartTone(eventCode, attenuationDb) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_STILL_PLAYING_PREV_DTMF,
            kTraceError,
            "OutputMixer::StartPlayingDtmfTone())");
        return -1;
    }
    return 0;
}

int OutputMixer::StopPlayingDtmfTone()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "OutputMixer::StopPlayingDtmfTone()");
    return (_dtmfGenerator.StopTone());
}

WebRtc_Word32
OutputMixer::SetMixabilityStatus(MixerParticipant& participant,
                                 const bool mixable)
{
    return _mixerModule.SetMixabilityStatus(participant, mixable);
}

WebRtc_Word32
OutputMixer::SetAnonymousMixabilityStatus(MixerParticipant& participant,
                                          const bool mixable)
{
    return _mixerModule.SetAnonymousMixabilityStatus(participant,mixable);
}

WebRtc_Word32
OutputMixer::MixActiveChannels()
{
    return _mixerModule.Process();
}

int
OutputMixer::GetSpeechOutputLevel(WebRtc_UWord32& level)
{
    WebRtc_Word8 currentLevel = _audioLevel.Level();
    level = static_cast<WebRtc_UWord32> (currentLevel);
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId,-1),
                 "GetSpeechOutputLevel() => level=%u", level);
    return 0;
}

int
OutputMixer::GetSpeechOutputLevelFullRange(WebRtc_UWord32& level)
{
    WebRtc_Word16 currentLevel = _audioLevel.LevelFullRange();
    level = static_cast<WebRtc_UWord32> (currentLevel);
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId,-1),
                 "GetSpeechOutputLevelFullRange() => level=%u", level);
    return 0;
}

int
OutputMixer::SetOutputVolumePan(float left, float right)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,-1),
                 "OutputMixer::SetOutputVolumePan()");
    _panLeft = left;
    _panRight = right;
    return 0;
}

int
OutputMixer::GetOutputVolumePan(float& left, float& right)
{
    left = _panLeft;
    right = _panRight;
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId,-1),
                 "GetOutputVolumePan() => left=%2.1f, right=%2.1f",
                 left, right);
    return 0;
}

int OutputMixer::StartRecordingPlayout(const char* fileName,
                                       const CodecInst* codecInst)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,-1),
                 "OutputMixer::StartRecordingPlayout(fileName=%s)", fileName);

    if (_outputFileRecording)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId,-1),
                     "StartRecordingPlayout() is already recording");
        return 0;
    }

    FileFormats format;
    const WebRtc_UWord32 notificationTime(0);
    CodecInst dummyCodec={100,"L16",16000,320,1,320000};

    if ((codecInst != NULL) &&
      ((codecInst->channels < 1) || (codecInst->channels > 2)))
    {
        _engineStatisticsPtr->SetLastError(
            VE_BAD_ARGUMENT, kTraceError,
            "StartRecordingPlayout() invalid compression");
        return(-1);
    }
    if(codecInst == NULL)
    {
        format = kFileFormatPcm16kHzFile;
        codecInst=&dummyCodec;
    }
    else if((STR_CASE_CMP(codecInst->plname,"L16") == 0) ||
        (STR_CASE_CMP(codecInst->plname,"PCMU") == 0) ||
        (STR_CASE_CMP(codecInst->plname,"PCMA") == 0))
    {
        format = kFileFormatWavFile;
    }
    else
    {
        format = kFileFormatCompressedFile;
    }

    CriticalSectionScoped cs(&_fileCritSect);

    // Destroy the old instance
    if (_outputFileRecorderPtr)
    {
        _outputFileRecorderPtr->RegisterModuleFileCallback(NULL);
        FileRecorder::DestroyFileRecorder(_outputFileRecorderPtr);
        _outputFileRecorderPtr = NULL;
    }

    _outputFileRecorderPtr = FileRecorder::CreateFileRecorder(
        _instanceId,
        (const FileFormats)format);
    if (_outputFileRecorderPtr == NULL)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_ARGUMENT, kTraceError,
            "StartRecordingPlayout() fileRecorder format isnot correct");
        return -1;
    }

    if (_outputFileRecorderPtr->StartRecordingAudioFile(
        fileName,
        (const CodecInst&)*codecInst,
        notificationTime) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_BAD_FILE, kTraceError,
            "StartRecordingAudioFile() failed to start file recording");
        _outputFileRecorderPtr->StopRecording();
        FileRecorder::DestroyFileRecorder(_outputFileRecorderPtr);
        _outputFileRecorderPtr = NULL;
        return -1;
    }
    _outputFileRecorderPtr->RegisterModuleFileCallback(this);
    _outputFileRecording = true;

    return 0;
}

int OutputMixer::StartRecordingPlayout(OutStream* stream,
                                       const CodecInst* codecInst)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,-1),
                 "OutputMixer::StartRecordingPlayout()");

    if (_outputFileRecording)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId,-1),
                     "StartRecordingPlayout() is already recording");
        return 0;
    }

    FileFormats format;
    const WebRtc_UWord32 notificationTime(0);
    CodecInst dummyCodec={100,"L16",16000,320,1,320000};

    if (codecInst != NULL && codecInst->channels != 1)
    {
        _engineStatisticsPtr->SetLastError(
            VE_BAD_ARGUMENT, kTraceError,
            "StartRecordingPlayout() invalid compression");
        return(-1);
    }
    if(codecInst == NULL)
    {
        format = kFileFormatPcm16kHzFile;
        codecInst=&dummyCodec;
    }
    else if((STR_CASE_CMP(codecInst->plname,"L16") == 0) ||
        (STR_CASE_CMP(codecInst->plname,"PCMU") == 0) ||
        (STR_CASE_CMP(codecInst->plname,"PCMA") == 0))
    {
        format = kFileFormatWavFile;
    }
    else
    {
        format = kFileFormatCompressedFile;
    }

    CriticalSectionScoped cs(&_fileCritSect);

    // Destroy the old instance
    if (_outputFileRecorderPtr)
    {
        _outputFileRecorderPtr->RegisterModuleFileCallback(NULL);
        FileRecorder::DestroyFileRecorder(_outputFileRecorderPtr);
        _outputFileRecorderPtr = NULL;
    }

    _outputFileRecorderPtr = FileRecorder::CreateFileRecorder(
        _instanceId,
        (const FileFormats)format);
    if (_outputFileRecorderPtr == NULL)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_ARGUMENT, kTraceError,
            "StartRecordingPlayout() fileRecorder format isnot correct");
        return -1;
    }

    if (_outputFileRecorderPtr->StartRecordingAudioFile(*stream,
                                                        *codecInst,
                                                        notificationTime) != 0)
    {
       _engineStatisticsPtr->SetLastError(VE_BAD_FILE, kTraceError,
           "StartRecordingAudioFile() failed to start file recording");
        _outputFileRecorderPtr->StopRecording();
        FileRecorder::DestroyFileRecorder(_outputFileRecorderPtr);
        _outputFileRecorderPtr = NULL;
        return -1;
    }

    _outputFileRecorderPtr->RegisterModuleFileCallback(this);
    _outputFileRecording = true;

    return 0;
}

int OutputMixer::StopRecordingPlayout()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,-1),
                 "OutputMixer::StopRecordingPlayout()");

    if (!_outputFileRecording)
    {
        WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId,-1),
                     "StopRecordingPlayout() file isnot recording");
        return -1;
    }

    CriticalSectionScoped cs(&_fileCritSect);

    if (_outputFileRecorderPtr->StopRecording() != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_STOP_RECORDING_FAILED, kTraceError,
            "StopRecording(), could not stop recording");
        return -1;
    }
    _outputFileRecorderPtr->RegisterModuleFileCallback(NULL);
    FileRecorder::DestroyFileRecorder(_outputFileRecorderPtr);
    _outputFileRecorderPtr = NULL;
    _outputFileRecording = false;

    return 0;
}

int OutputMixer::GetMixedAudio(int sample_rate_hz,
                               int num_channels,
                               AudioFrame* frame) {
  WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,-1),
               "OutputMixer::GetMixedAudio(sample_rate_hz=%d, num_channels=%d)",
               sample_rate_hz, num_channels);

  // --- Record playout if enabled
  {
    CriticalSectionScoped cs(&_fileCritSect);
    if (_outputFileRecording && _outputFileRecorderPtr)
      _outputFileRecorderPtr->RecordAudioToFile(_audioFrame);
  }

  frame->num_channels_ = num_channels;
  frame->sample_rate_hz_ = sample_rate_hz;
  // TODO(andrew): Ideally the downmixing would occur much earlier, in
  // AudioCodingModule.
  return RemixAndResample(_audioFrame, &_resampler, frame);
}

WebRtc_Word32
OutputMixer::DoOperationsOnCombinedSignal()
{
    if (_audioFrame.sample_rate_hz_ != _mixingFrequencyHz)
    {
        WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,-1),
                     "OutputMixer::DoOperationsOnCombinedSignal() => "
                     "mixing frequency = %d", _audioFrame.sample_rate_hz_);
        _mixingFrequencyHz = _audioFrame.sample_rate_hz_;
    }

    // --- Insert inband Dtmf tone
    if (_dtmfGenerator.IsAddingTone())
    {
        InsertInbandDtmfTone();
    }

    // Scale left and/or right channel(s) if balance is active
    if (_panLeft != 1.0 || _panRight != 1.0)
    {
        if (_audioFrame.num_channels_ == 1)
        {
            AudioFrameOperations::MonoToStereo(&_audioFrame);
        }
        else
        {
            // Pure stereo mode (we are receiving a stereo signal).
        }

        assert(_audioFrame.num_channels_ == 2);
        AudioFrameOperations::Scale(_panLeft, _panRight, _audioFrame);
    }

    // --- Far-end Voice Quality Enhancement (AudioProcessing Module)

    APMAnalyzeReverseStream();

    // --- External media processing

    if (_externalMedia)
    {
        CriticalSectionScoped cs(&_callbackCritSect);
        const bool isStereo = (_audioFrame.num_channels_ == 2);
        if (_externalMediaCallbackPtr)
        {
            _externalMediaCallbackPtr->Process(
                -1,
                kPlaybackAllChannelsMixed,
                (WebRtc_Word16*)_audioFrame.data_,
                _audioFrame.samples_per_channel_,
                _audioFrame.sample_rate_hz_,
                isStereo);
        }
    }

    // --- Measure audio level (0-9) for the combined signal
    _audioLevel.ComputeLevel(_audioFrame);

    return 0;
}

// ----------------------------------------------------------------------------
//                             Private methods
// ----------------------------------------------------------------------------

void OutputMixer::APMAnalyzeReverseStream() {
  // Convert from mixing to AudioProcessing sample rate, determined by the send
  // side. Downmix to mono.
  AudioFrame frame;
  frame.num_channels_ = 1;
  frame.sample_rate_hz_ = _audioProcessingModulePtr->sample_rate_hz();
  if (RemixAndResample(_audioFrame, &_apmResampler, &frame) == -1)
    return;

  if (_audioProcessingModulePtr->AnalyzeReverseStream(&frame) == -1) {
    WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId,-1),
                 "AudioProcessingModule::AnalyzeReverseStream() => error");
  }
}

int
OutputMixer::InsertInbandDtmfTone()
{
    WebRtc_UWord16 sampleRate(0);
    _dtmfGenerator.GetSampleRate(sampleRate);
    if (sampleRate != _audioFrame.sample_rate_hz_)
    {
        // Update sample rate of Dtmf tone since the mixing frequency changed.
        _dtmfGenerator.SetSampleRate(
            (WebRtc_UWord16)(_audioFrame.sample_rate_hz_));
        // Reset the tone to be added taking the new sample rate into account.
        _dtmfGenerator.ResetTone();
    }

    WebRtc_Word16 toneBuffer[320];
    WebRtc_UWord16 toneSamples(0);
    if (_dtmfGenerator.Get10msTone(toneBuffer, toneSamples) == -1)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, -1),
                     "OutputMixer::InsertInbandDtmfTone() inserting Dtmf"
                     "tone failed");
        return -1;
    }

    // replace mixed audio with Dtmf tone
    if (_audioFrame.num_channels_ == 1)
    {
        // mono
        memcpy(_audioFrame.data_, toneBuffer, sizeof(WebRtc_Word16)
            * toneSamples);
    } else
    {
        // stereo
        for (int i = 0; i < _audioFrame.samples_per_channel_; i++)
        {
            _audioFrame.data_[2 * i] = toneBuffer[i];
            _audioFrame.data_[2 * i + 1] = 0;
        }
    }
    assert(_audioFrame.samples_per_channel_ == toneSamples);

    return 0;
}

}  //  namespace voe

}  //  namespace webrtc
