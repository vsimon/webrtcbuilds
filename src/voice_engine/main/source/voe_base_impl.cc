/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "voe_base_impl.h"

#include "audio_coding_module.h"
#include "audio_device_impl.h"
#include "audio_processing.h"
#include "channel.h"
#include "critical_section_wrapper.h"
#include "file_wrapper.h"
#include "output_mixer.h"
#include "signal_processing_library.h"
#include "trace.h"
#include "transmit_mixer.h"
#include "utility.h"
#include "voe_errors.h"
#include "voice_engine_impl.h"

#if (defined(_WIN32) && defined(_DLL) && (_MSC_VER == 1400))
// Fix for VS 2005 MD/MDd link problem
#include <stdio.h>
extern "C"
    { FILE _iob[3] = {   __iob_func()[0], __iob_func()[1], __iob_func()[2]}; }
#endif

namespace webrtc
{

VoEBase* VoEBase::GetInterface(VoiceEngine* voiceEngine)
{
    if (NULL == voiceEngine)
    {
        return NULL;
    }
    VoiceEngineImpl* s = reinterpret_cast<VoiceEngineImpl*> (voiceEngine);
    VoEBaseImpl* d = s;
    (*d)++;
    return (d);
}

VoEBaseImpl::VoEBaseImpl() :
    _voiceEngineObserverPtr(NULL),
    _callbackCritSect(*CriticalSectionWrapper::CreateCriticalSection()),
    _voiceEngineObserver(false), _oldVoEMicLevel(0), _oldMicLevel(0)
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_instanceId, -1),
                 "VoEBaseImpl() - ctor");
}

VoEBaseImpl::~VoEBaseImpl()
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_instanceId, -1),
                 "~VoEBaseImpl() - dtor");

    TerminateInternal();

    delete &_callbackCritSect;
}

int VoEBaseImpl::Release()
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId, -1),
                 "VoEBaseImpl::Release()");
    (*this)--;
    int refCount = GetCount();
    if (refCount < 0)
    {
        Reset();
        _engineStatistics.SetLastError(VE_INTERFACE_NOT_FOUND, kTraceWarning);
        return (-1);
    }
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "VoEBaseImpl reference counter = %d", refCount);
    return (refCount);
}

void VoEBaseImpl::OnErrorIsReported(const ErrorCode error)
{
    CriticalSectionScoped cs(_callbackCritSect);
    if (_voiceEngineObserver)
    {
        if (_voiceEngineObserverPtr)
        {
            int errCode(0);
            if (error == AudioDeviceObserver::kRecordingError)
            {
                errCode = VE_RUNTIME_REC_ERROR;
                WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                             "VoEBaseImpl::OnErrorIsReported() => "
                                 "VE_RUNTIME_REC_ERROR");
            }
            else if (error == AudioDeviceObserver::kPlayoutError)
            {
                errCode = VE_RUNTIME_PLAY_ERROR;
                WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                             "VoEBaseImpl::OnErrorIsReported() => "
                                 "VE_RUNTIME_PLAY_ERROR");
            }
            // Deliver callback (-1 <=> no channel dependency)
            _voiceEngineObserverPtr->CallbackOnError(-1, errCode);
        }
    }
}

void VoEBaseImpl::OnWarningIsReported(const WarningCode warning)
{
    CriticalSectionScoped cs(_callbackCritSect);
    if (_voiceEngineObserver)
    {
        if (_voiceEngineObserverPtr)
        {
            int warningCode(0);
            if (warning == AudioDeviceObserver::kRecordingWarning)
            {
                warningCode = VE_RUNTIME_REC_WARNING;
                WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                             "VoEBaseImpl::OnErrorIsReported() => "
                                 "VE_RUNTIME_REC_WARNING");
            }
            else if (warning == AudioDeviceObserver::kPlayoutWarning)
            {
                warningCode = VE_RUNTIME_PLAY_WARNING;
                WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                             "VoEBaseImpl::OnErrorIsReported() => "
                                 "VE_RUNTIME_PLAY_WARNING");
            }
            // Deliver callback (-1 <=> no channel dependency)
            _voiceEngineObserverPtr->CallbackOnError(-1, warningCode);
        }
    }
}

WebRtc_Word32 VoEBaseImpl::RecordedDataIsAvailable(
        const WebRtc_Word8* audioSamples,
        const WebRtc_UWord32 nSamples,
        const WebRtc_UWord8 nBytesPerSample,
        const WebRtc_UWord8 nChannels,
        const WebRtc_UWord32 samplesPerSec,
        const WebRtc_UWord32 totalDelayMS,
        const WebRtc_Word32 clockDrift,
        const WebRtc_UWord32 currentMicLevel,
        WebRtc_UWord32& newMicLevel)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId, -1),
                 "VoEBaseImpl::RecordedDataIsAvailable(nSamples=%u, "
                     "nBytesPerSample=%u, nChannels=%u, samplesPerSec=%u, "
                     "totalDelayMS=%u, clockDrift=%d, currentMicLevel=%u)",
                 nSamples, nBytesPerSample, nChannels, samplesPerSec,
                 totalDelayMS, clockDrift, currentMicLevel);

    assert(_transmitMixerPtr != NULL);
    assert(_audioDevicePtr != NULL);

    bool isAnalogAGC(false);
    WebRtc_UWord32 maxVolume(0);
    WebRtc_UWord16 currentVoEMicLevel(0);
    WebRtc_UWord32 newVoEMicLevel(0);

    if (_audioProcessingModulePtr
            && (_audioProcessingModulePtr->gain_control()->mode()
                    == GainControl::kAdaptiveAnalog))
    {
        isAnalogAGC = true;
    }

    // Will only deal with the volume in adaptive analog mode
    if (isAnalogAGC)
    {
        // Scale from ADM to VoE level range
        if (_audioDevicePtr->MaxMicrophoneVolume(&maxVolume) == 0)
        {
            if (0 != maxVolume)
            {
                currentVoEMicLevel = (WebRtc_UWord16) ((currentMicLevel
                        * kMaxVolumeLevel + (int) (maxVolume / 2))
                        / (maxVolume));
            }
        }
        // We learned that on certain systems (e.g Linux) the currentVoEMicLevel
        // can be greater than the maxVolumeLevel therefore
        // we are going to cap the currentVoEMicLevel to the maxVolumeLevel
        // if it turns out that the currentVoEMicLevel is indeed greater
        // than the maxVolumeLevel
        if (currentVoEMicLevel > kMaxVolumeLevel)
        {
            currentVoEMicLevel = kMaxVolumeLevel;
        }
    }

    // Keep track if the MicLevel has been changed by the AGC, if not,
    // use the old value AGC returns to let AGC continue its trend,
    // so eventually the AGC is able to change the mic level. This handles
    // issues with truncation introduced by the scaling.
    if (_oldMicLevel == currentMicLevel)
    {
        currentVoEMicLevel = (WebRtc_UWord16) _oldVoEMicLevel;
    }

    // Perform channel-independent operations
    // (APM, mix with file, record to file, mute, etc.)
    _transmitMixerPtr->PrepareDemux(audioSamples, nSamples, nChannels,
                                    samplesPerSec,
                                    (WebRtc_UWord16) totalDelayMS, clockDrift,
                                    currentVoEMicLevel);

    // Copy the audio frame to each sending channel and perform
    // channel-dependent operations (file mixing, mute, etc.) to prepare
    // for encoding.
    _transmitMixerPtr->DemuxAndMix();
    // Do the encoding and packetize+transmit the RTP packet when encoding
    // is done.
    _transmitMixerPtr->EncodeAndSend();

    // Will only deal with the volume in adaptive analog mode
    if (isAnalogAGC)
    {
        // Scale from VoE to ADM level range
        newVoEMicLevel = _transmitMixerPtr->CaptureLevel();
        if (newVoEMicLevel != currentVoEMicLevel)
        {
            // Add (kMaxVolumeLevel/2) to round the value
            newMicLevel = (WebRtc_UWord32) ((newVoEMicLevel * maxVolume
                    + (int) (kMaxVolumeLevel / 2)) / (kMaxVolumeLevel));
        }
        else
        {
            // Pass zero if the level is unchanged
            newMicLevel = 0;
        }

        // Keep track of the value AGC returns
        _oldVoEMicLevel = newVoEMicLevel;
        _oldMicLevel = currentMicLevel;
    }

    return 0;
}

WebRtc_Word32 VoEBaseImpl::NeedMorePlayData(
        const WebRtc_UWord32 nSamples,
        const WebRtc_UWord8 nBytesPerSample,
        const WebRtc_UWord8 nChannels,
        const WebRtc_UWord32 samplesPerSec,
        WebRtc_Word8* audioSamples,
        WebRtc_UWord32& nSamplesOut)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId, -1),
                 "VoEBaseImpl::NeedMorePlayData(nSamples=%u, "
                     "nBytesPerSample=%d, nChannels=%d, samplesPerSec=%u)",
                 nSamples, nBytesPerSample, nChannels, samplesPerSec);

    assert(_outputMixerPtr != NULL);

    AudioFrame audioFrame;

    // Perform mixing of all active participants (channel-based mixing)
    _outputMixerPtr->MixActiveChannels();

    // Additional operations on the combined signal
    _outputMixerPtr->DoOperationsOnCombinedSignal();

    // Retrieve the final output mix (resampled to match the ADM)
    _outputMixerPtr->GetMixedAudio(samplesPerSec, nChannels, audioFrame);

    assert(nSamples == audioFrame._payloadDataLengthInSamples);
    assert(samplesPerSec ==
        static_cast<WebRtc_UWord32>(audioFrame._frequencyInHz));

    // Deliver audio (PCM) samples to the ADM
    memcpy(
           (WebRtc_Word16*) audioSamples,
           (const WebRtc_Word16*) audioFrame._payloadData,
           sizeof(WebRtc_Word16) * (audioFrame._payloadDataLengthInSamples
                   * audioFrame._audioChannel));

    nSamplesOut = audioFrame._payloadDataLengthInSamples;

    return 0;
}

int VoEBaseImpl::RegisterVoiceEngineObserver(VoiceEngineObserver& observer)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId, -1),
                 "RegisterVoiceEngineObserver(observer=0x%d)", &observer);
    CriticalSectionScoped cs(_callbackCritSect);
    if (_voiceEngineObserverPtr)
    {
        _engineStatistics.SetLastError(VE_INVALID_OPERATION, kTraceError,
                                       "RegisterVoiceEngineObserver() observer"
                                       " already enabled");
        return -1;
    }

    // Register the observer in all active channels
    voe::ScopedChannel sc(_channelManager);
    void* iterator(NULL);
    voe::Channel* channelPtr = sc.GetFirstChannel(iterator);
    while (channelPtr != NULL)
    {
        channelPtr->RegisterVoiceEngineObserver(observer);
        channelPtr = sc.GetNextChannel(iterator);
    }
    _transmitMixerPtr->RegisterVoiceEngineObserver(observer);

    _voiceEngineObserverPtr = &observer;
    _voiceEngineObserver = true;

    return 0;
}

int VoEBaseImpl::DeRegisterVoiceEngineObserver()
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId, -1),
                 "DeRegisterVoiceEngineObserver()");
    CriticalSectionScoped cs(_callbackCritSect);
    if (!_voiceEngineObserverPtr)
    {
        _engineStatistics.SetLastError(VE_INVALID_OPERATION, kTraceError,
            "DeRegisterVoiceEngineObserver() observer already disabled");
        return 0;
    }

    _voiceEngineObserver = false;
    _voiceEngineObserverPtr = NULL;

    // Deregister the observer in all active channels
    voe::ScopedChannel sc(_channelManager);
    void* iterator(NULL);
    voe::Channel* channelPtr = sc.GetFirstChannel(iterator);
    while (channelPtr != NULL)
    {
        channelPtr->DeRegisterVoiceEngineObserver();
        channelPtr = sc.GetNextChannel(iterator);
    }

    return 0;
}

int VoEBaseImpl::Init(AudioDeviceModule* external_adm)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId, -1), 
        "Init(external_adm=0x%p)", external_adm);
    CriticalSectionScoped cs(*_apiCritPtr);

    if (_engineStatistics.Initialized())
    {
        return 0;
    }

    if (_moduleProcessThreadPtr)
    {
        if (_moduleProcessThreadPtr->Start() != 0)
        {
            _engineStatistics.SetLastError(VE_THREAD_ERROR, kTraceError,
                "Init() failed to start module process thread");
            return -1;
        }
    }

    // Create an internal ADM if the user has not added an external
    // ADM implementation as input to Init().
    if (external_adm == NULL)
    {
        // Create the internal ADM implementation.
        _audioDevicePtr = AudioDeviceModuleImpl::Create(
            VoEId(_instanceId, -1), _audioDeviceLayer);

        if (_audioDevicePtr == NULL)
        {
            _engineStatistics.SetLastError(VE_NO_MEMORY, kTraceCritical,
                                           "Init() failed to create the ADM");
            return -1;
        }
    }
    else
    {
        // Use the already existing external ADM implementation.
        _audioDevicePtr = external_adm;
        WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
            "An external ADM implementation will be used in VoiceEngine");
    }

    // Increase the reference counter for both external and internal usage.
    _audioDevicePtr->AddRef();

    // Register the ADM to the process thread, which will drive the error
    // callback mechanism
    if (_moduleProcessThreadPtr->RegisterModule(_audioDevicePtr) != 0)
    {
        _engineStatistics.SetLastError(VE_AUDIO_DEVICE_MODULE_ERROR,
                                       kTraceError,
                                       "Init() failed to register the ADM");
        return -1;
    }

    bool available(false);

    // --------------------
    // Reinitialize the ADM

    // Register the AudioObserver implementation
    _audioDevicePtr->RegisterEventObserver(this);

    // Register the AudioTransport implementation
    _audioDevicePtr->RegisterAudioCallback(this);

    // ADM initialization
    if (_audioDevicePtr->Init() != 0)
    {
        _engineStatistics.SetLastError(VE_AUDIO_DEVICE_MODULE_ERROR,
                                       kTraceError,
                                       "Init() failed to initialize the ADM");
        return -1;
    }

    // Initialize the default speaker
    if (_audioDevicePtr->SetPlayoutDevice(WEBRTC_VOICE_ENGINE_DEFAULT_DEVICE)
            != 0)
    {
        _engineStatistics.SetLastError(VE_AUDIO_DEVICE_MODULE_ERROR, kTraceInfo,
            "Init() failed to set the default output device");
    }
    if (_audioDevicePtr->SpeakerIsAvailable(&available) != 0)
    {
        _engineStatistics.SetLastError(VE_CANNOT_ACCESS_SPEAKER_VOL, kTraceInfo,
            "Init() failed to check speaker availability, trying to "
            "initialize speaker anyway");
    }
    else if (!available)
    {
        _engineStatistics.SetLastError(VE_CANNOT_ACCESS_SPEAKER_VOL, kTraceInfo,
            "Init() speaker not available, trying to initialize speaker "
            "anyway");
    }
    if (_audioDevicePtr->InitSpeaker() != 0)
    {
        _engineStatistics.SetLastError(VE_CANNOT_ACCESS_SPEAKER_VOL, kTraceInfo,
            "Init() failed to initialize the speaker");
    }

    // Initialize the default microphone
    if (_audioDevicePtr->SetRecordingDevice(WEBRTC_VOICE_ENGINE_DEFAULT_DEVICE)
            != 0)
    {
        _engineStatistics.SetLastError(VE_SOUNDCARD_ERROR, kTraceInfo,
            "Init() failed to set the default input device");
    }
    if (_audioDevicePtr->MicrophoneIsAvailable(&available) != 0)
    {
        _engineStatistics.SetLastError(VE_CANNOT_ACCESS_MIC_VOL, kTraceInfo,
            "Init() failed to check microphone availability, trying to "
            "initialize microphone anyway");
    }
    else if (!available)
    {
        _engineStatistics.SetLastError(VE_CANNOT_ACCESS_MIC_VOL, kTraceInfo,
            "Init() microphone not available, trying to initialize "
            "microphone anyway");
    }
    if (_audioDevicePtr->InitMicrophone() != 0)
    {
        _engineStatistics.SetLastError(VE_CANNOT_ACCESS_MIC_VOL, kTraceInfo,
            "Init() failed to initialize the microphone");
    }

    // Set number of channels
    _audioDevicePtr->StereoPlayoutIsAvailable(&available);
    if (_audioDevicePtr->SetStereoPlayout(available) != 0)
    {
        _engineStatistics.SetLastError(VE_SOUNDCARD_ERROR, kTraceWarning,
            "Init() failed to set mono/stereo playout mode");
    }

    // TODO(andrew): These functions don't tell us whether stereo recording
    // is truly available. We simply set the AudioProcessing input to stereo
    // here, because we have to wait until receiving the first frame to
    // determine the actual number of channels anyway.
    //
    // These functions may be changed; tracked here:
    // http://code.google.com/p/webrtc/issues/detail?id=204
    _audioDevicePtr->StereoRecordingIsAvailable(&available);
    if (_audioDevicePtr->SetStereoRecording(available) != 0)
    {
        _engineStatistics.SetLastError(VE_SOUNDCARD_ERROR, kTraceWarning,
            "Init() failed to set mono/stereo recording mode");
    }

    // APM initialization done after sound card since we need
    // to know if we support stereo recording or not.

    // Create the AudioProcessing Module if it does not exist.

    if (_audioProcessingModulePtr == NULL)
    {
        _audioProcessingModulePtr = AudioProcessing::Create(
                VoEId(_instanceId, -1));
        if (_audioProcessingModulePtr == NULL)
        {
            _engineStatistics.SetLastError(VE_NO_MEMORY, kTraceCritical,
                "Init() failed to create the AP module");
            return -1;
        }
        voe::Utility::TraceModuleVersion(VoEId(_instanceId, -1),
                                         *_audioProcessingModulePtr);

        // Ensure that mixers in both directions has access to the created APM
        _transmitMixerPtr->SetAudioProcessingModule(_audioProcessingModulePtr);
        _outputMixerPtr->SetAudioProcessingModule(_audioProcessingModulePtr);

        if (_audioProcessingModulePtr->echo_cancellation()->
                set_device_sample_rate_hz(
                        kVoiceEngineAudioProcessingDeviceSampleRateHz))
        {
            _engineStatistics.SetLastError(VE_APM_ERROR, kTraceError,
                "Init() failed to set the device sample rate to 48K for AP "
                " module");
            return -1;
        }
        // Using 8 kHz as inital Fs. Might be changed already at first call.
        if (_audioProcessingModulePtr->set_sample_rate_hz(8000))
        {
            _engineStatistics.SetLastError(VE_APM_ERROR, kTraceError,
                "Init() failed to set the sample rate to 8K for AP module");
            return -1;
        }

        // Assume mono output until a send codec is set, and stereo input until
        // we receive the first captured frame. We set stereo input here to
        // avoid triggering a possible error in SetSendCodec when a stereo
        // codec is selected.
        if (_audioProcessingModulePtr->set_num_channels(2, 1) != 0)
        {
            _engineStatistics.SetLastError(VE_SOUNDCARD_ERROR, kTraceError,
                "Init() failed to set channels for the primary audio stream");
            return -1;
        }

        if (_audioProcessingModulePtr->set_num_reverse_channels(1) != 0)
        {
            _engineStatistics.SetLastError(VE_SOUNDCARD_ERROR, kTraceError,
                "Init() failed to set channels for the primary audio stream");
            return -1;
        }
        // high-pass filter
        if (_audioProcessingModulePtr->high_pass_filter()->Enable(
                WEBRTC_VOICE_ENGINE_HP_DEFAULT_STATE) != 0)
        {
            _engineStatistics.SetLastError(VE_APM_ERROR, kTraceError,
                "Init() failed to set the high-pass filter for AP module");
            return -1;
        }
        // Echo Cancellation
        if (_audioProcessingModulePtr->echo_cancellation()->
                enable_drift_compensation(false) != 0)
        {
            _engineStatistics.SetLastError(VE_APM_ERROR, kTraceError,
                "Init() failed to set drift compensation for AP module");
            return -1;
        }
        if (_audioProcessingModulePtr->echo_cancellation()->Enable(
                WEBRTC_VOICE_ENGINE_EC_DEFAULT_STATE))
        {
            _engineStatistics.SetLastError(VE_APM_ERROR, kTraceError,
                "Init() failed to set echo cancellation state for AP module");
            return -1;
        }
        // Noise Reduction
        if (_audioProcessingModulePtr->noise_suppression()->set_level(
                (NoiseSuppression::Level) WEBRTC_VOICE_ENGINE_NS_DEFAULT_MODE)
                != 0)
        {
            _engineStatistics.SetLastError(VE_APM_ERROR, kTraceError,
                "Init() failed to set noise reduction level for AP module");
            return -1;
        }
        if (_audioProcessingModulePtr->noise_suppression()->Enable(
                WEBRTC_VOICE_ENGINE_NS_DEFAULT_STATE) != 0)
        {
            _engineStatistics.SetLastError(VE_APM_ERROR, kTraceError,
                "Init() failed to set noise reduction state for AP module");
            return -1;
        }
        // Automatic Gain control
        if (_audioProcessingModulePtr->gain_control()->set_analog_level_limits(
                kMinVolumeLevel,kMaxVolumeLevel) != 0)
        {
            _engineStatistics.SetLastError(VE_APM_ERROR, kTraceError,
                "Init() failed to set AGC analog level for AP module");
            return -1;
        }
        if (_audioProcessingModulePtr->gain_control()->set_mode(
                (GainControl::Mode) WEBRTC_VOICE_ENGINE_AGC_DEFAULT_MODE)
                != 0)
        {
            _engineStatistics.SetLastError(VE_APM_ERROR, kTraceError,
                "Init() failed to set AGC mode for AP module");
            return -1;
        }
        if (_audioProcessingModulePtr->gain_control()->Enable(
                WEBRTC_VOICE_ENGINE_AGC_DEFAULT_STATE)
                != 0)
        {
            _engineStatistics.SetLastError(VE_APM_ERROR, kTraceError,
                "Init() failed to set AGC state for AP module");
            return -1;
        }
        // VAD
        if (_audioProcessingModulePtr->voice_detection()->Enable(
                WEBRTC_VOICE_ENGINE_VAD_DEFAULT_STATE)
                != 0)
        {
            _engineStatistics.SetLastError(VE_APM_ERROR, kTraceError,
                "Init() failed to set VAD state for AP module");
            return -1;
        }
    }

  // Set default AGC mode for the ADM
#ifdef WEBRTC_VOICE_ENGINE_AGC
    bool enable(false);
    if (_audioProcessingModulePtr->gain_control()->mode()
            != GainControl::kFixedDigital)
    {
        enable = _audioProcessingModulePtr->gain_control()->is_enabled();
        // Only set the AGC mode for the ADM when Adaptive AGC mode is selected
        if (_audioDevicePtr->SetAGC(enable) != 0)
        {
            _engineStatistics.SetLastError(VE_AUDIO_DEVICE_MODULE_ERROR,
                kTraceError, "Init() failed to set default AGC mode in ADM 0");
        }
    }
#endif

    return _engineStatistics.SetInitialized();
}

int VoEBaseImpl::Terminate()
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId, -1),
                 "Terminate()");
    CriticalSectionScoped cs(*_apiCritPtr);
    return TerminateInternal();
}

int VoEBaseImpl::MaxNumOfChannels()
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId, -1),
                 "MaxNumOfChannels()");
    WebRtc_Word32 maxNumOfChannels = _channelManager.MaxNumOfChannels();
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "MaxNumOfChannels() => %d", maxNumOfChannels);
    return (maxNumOfChannels);
}

int VoEBaseImpl::CreateChannel()
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId, -1),
                 "CreateChannel()");
    CriticalSectionScoped cs(*_apiCritPtr);

    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }

    WebRtc_Word32 channelId = -1;

    if (!_channelManager.CreateChannel(channelId))
    {
        _engineStatistics.SetLastError(VE_CHANNEL_NOT_CREATED, kTraceError,
                                       "CreateChannel() failed to allocate "
                                           "memory for channel");
        return -1;
    }

    bool destroyChannel(false);
    {
        voe::ScopedChannel sc(_channelManager, channelId);
        voe::Channel* channelPtr = sc.ChannelPtr();
        if (channelPtr == NULL)
        {
            _engineStatistics.SetLastError(VE_CHANNEL_NOT_CREATED, kTraceError,
                                           "CreateChannel() failed to allocate"
                                           " memory for channel");
            return -1;
        }
        else if (channelPtr->SetEngineInformation(_engineStatistics,
                                                  *_outputMixerPtr,
                                                  *_transmitMixerPtr,
                                                  *_moduleProcessThreadPtr,
                                                  *_audioDevicePtr,
                                                  _voiceEngineObserverPtr,
                                                  &_callbackCritSect) != 0)
        {
            destroyChannel = true;
            _engineStatistics.SetLastError(VE_CHANNEL_NOT_CREATED, kTraceError,
                                           "CreateChannel() failed to "
                                           "associate engine and channel."
                                           " Destroying channel.");
        }
        else if (channelPtr->Init() != 0)
        {
            destroyChannel = true;
            _engineStatistics.SetLastError(VE_CHANNEL_NOT_CREATED, kTraceError,
                                           "CreateChannel() failed to "
                                           "initialize channel. Destroying"
                                           " channel.");
        }
    }
    if (destroyChannel)
    {
        _channelManager.DestroyChannel(channelId);
        return -1;
    }
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "CreateChannel() => %d", channelId);
    return channelId;
}

int VoEBaseImpl::DeleteChannel(int channel)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId, -1),
                 "DeleteChannel(channel=%d)", channel);
    CriticalSectionScoped cs(*_apiCritPtr);

    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }

    {
        voe::ScopedChannel sc(_channelManager, channel);
        voe::Channel* channelPtr = sc.ChannelPtr();
        if (channelPtr == NULL)
        {
            _engineStatistics.SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                                           "DeleteChannel() failed to locate "
                                           "channel");
            return -1;
        }
    }

    if (_channelManager.DestroyChannel(channel) != 0)
    {
        _engineStatistics.SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                                       "DeleteChannel() failed to destroy "
                                       "channel");
        return -1;
    }

    if (StopSend() != 0)
    {
        return -1;
    }

    if (StopPlayout() != 0)
    {
        return -1;
    }

    return 0;
}

int VoEBaseImpl::SetLocalReceiver(int channel, int port, int RTCPport,
                                  const char ipAddr[64],
                                  const char multiCastAddr[64])
{
    //  Inititialize local receive sockets (RTP and RTCP).
    //
    //  The sockets are always first closed and then created again by this
    //  function call. The created sockets are by default also used for
    // transmission (unless source port is set in SetSendDestination).
    //
    //  Note that, sockets can also be created automatically if a user calls
    //  SetSendDestination and StartSend without having called SetLocalReceiver
    // first. The sockets are then created at the first packet transmission.

    CriticalSectionScoped cs(*_apiCritPtr);
    if (ipAddr == NULL && multiCastAddr == NULL)
    {
        WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId, -1),
                     "SetLocalReceiver(channel=%d, port=%d, RTCPport=%d)",
                     channel, port, RTCPport);
    }
    else if (ipAddr != NULL && multiCastAddr == NULL)
    {
        WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId, -1),
                     "SetLocalReceiver(channel=%d, port=%d, RTCPport=%d, "
                         "ipAddr=%s)", channel, port, RTCPport, ipAddr);
    }
    else if (ipAddr == NULL && multiCastAddr != NULL)
    {
        WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId, -1),
                     "SetLocalReceiver(channel=%d, port=%d, RTCPport=%d, "
                         "multiCastAddr=%s)", channel, port, RTCPport,
                     multiCastAddr);
    }
    else
    {
        WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId, -1),
                     "SetLocalReceiver(channel=%d, port=%d, RTCPport=%d, "
                         "ipAddr=%s, multiCastAddr=%s)", channel, port,
                     RTCPport, ipAddr, multiCastAddr);
    }
#ifndef WEBRTC_EXTERNAL_TRANSPORT
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    if ((port < 0) || (port > 65535))
    {
        _engineStatistics.SetLastError(VE_INVALID_PORT_NMBR, kTraceError,
                                       "SetLocalReceiver() invalid RTP port");
        return -1;
    }
    if (((RTCPport != kVoEDefault) && (RTCPport < 0)) || ((RTCPport
            != kVoEDefault) && (RTCPport > 65535)))
    {
        _engineStatistics.SetLastError(VE_INVALID_PORT_NMBR, kTraceError,
                                       "SetLocalReceiver() invalid RTCP port");
        return -1;
    }
    voe::ScopedChannel sc(_channelManager, channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _engineStatistics.SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                                       "SetLocalReceiver() failed to locate "
                                           "channel");
        return -1;
    }

    // Cast RTCP port. In the RTP module 0 corresponds to RTP port + 1 in
    // the module, which is the default.
    WebRtc_UWord16 rtcpPortUW16(0);
    if (RTCPport != kVoEDefault)
    {
        rtcpPortUW16 = static_cast<WebRtc_UWord16> (RTCPport);
    }

    return channelPtr->SetLocalReceiver(port, rtcpPortUW16, ipAddr,
                                        multiCastAddr);
#else
    _engineStatistics.SetLastError(VE_EXTERNAL_TRANSPORT_ENABLED,
            kTraceWarning,
            "SetLocalReceiver() VoE is built for "
            "external transport");
    return -1;
#endif
}

int VoEBaseImpl::GetLocalReceiver(int channel, int& port, int& RTCPport,
                                  char ipAddr[64])
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId, -1),
                 "GetLocalReceiver(channel=%d, ipAddr[]=?)", channel);
#ifndef WEBRTC_EXTERNAL_TRANSPORT
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_channelManager, channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _engineStatistics.SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                                       "SetLocalReceiver() failed to locate "
                                       "channel");
        return -1;
    }
    WebRtc_Word32 ret = channelPtr->GetLocalReceiver(port, RTCPport, ipAddr);
    if (ipAddr != NULL)
    {
        WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId, -1),
                     "GetLocalReceiver() => port=%d, RTCPport=%d, ipAddr=%s",
                     port, RTCPport, ipAddr);
    }
    else
    {
        WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId, -1),
                     "GetLocalReceiver() => port=%d, RTCPport=%d", port,
                     RTCPport);
    }
    return ret;
#else
    _engineStatistics.SetLastError(VE_EXTERNAL_TRANSPORT_ENABLED,
                                   kTraceWarning,
                                   "SetLocalReceiver() VoE is built for "
                                   "external transport");
    return -1;
#endif
}

int VoEBaseImpl::SetSendDestination(int channel, int port, const char* ipaddr,
                                    int sourcePort, int RTCPport)
{
    WEBRTC_TRACE(
                 kTraceApiCall,
                 kTraceVoice,
                 VoEId(_instanceId, -1),
                 "SetSendDestination(channel=%d, port=%d, ipaddr=%s,"
                 "sourcePort=%d, RTCPport=%d)",
                 channel, port, ipaddr, sourcePort, RTCPport);
    CriticalSectionScoped cs(*_apiCritPtr);
#ifndef WEBRTC_EXTERNAL_TRANSPORT
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_channelManager, channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _engineStatistics.SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                                       "SetSendDestination() failed to locate "
                                       "channel");
        return -1;
    }
    if ((port < 0) || (port > 65535))
    {
        _engineStatistics.SetLastError(VE_INVALID_PORT_NMBR, kTraceError,
                                       "SetSendDestination() invalid RTP port");
        return -1;
    }
    if (((RTCPport != kVoEDefault) && (RTCPport < 0)) || ((RTCPport
            != kVoEDefault) && (RTCPport > 65535)))
    {
        _engineStatistics.SetLastError(VE_INVALID_PORT_NMBR, kTraceError,
                                       "SetSendDestination() invalid RTCP "
                                       "port");
        return -1;
    }
    if (((sourcePort != kVoEDefault) && (sourcePort < 0)) || ((sourcePort
            != kVoEDefault) && (sourcePort > 65535)))
    {
        _engineStatistics.SetLastError(VE_INVALID_PORT_NMBR, kTraceError,
                                       "SetSendDestination() invalid source "
                                       "port");
        return -1;
    }

    // Cast RTCP port. In the RTP module 0 corresponds to RTP port + 1 in the
    // module, which is the default.
    WebRtc_UWord16 rtcpPortUW16(0);
    if (RTCPport != kVoEDefault)
    {
        rtcpPortUW16 = static_cast<WebRtc_UWord16> (RTCPport);
        WEBRTC_TRACE(
                     kTraceInfo,
                     kTraceVoice,
                     VoEId(_instanceId, channel),
                     "SetSendDestination() non default RTCP port %u will be "
                     "utilized",
                     rtcpPortUW16);
    }

    return channelPtr->SetSendDestination(port, ipaddr, sourcePort,
                                          rtcpPortUW16);
#else
    _engineStatistics.SetLastError(VE_EXTERNAL_TRANSPORT_ENABLED,
                                   kTraceWarning,
                                   "SetSendDestination() VoE is built for "
                                   "external transport");
    return -1;
#endif
}

int VoEBaseImpl::GetSendDestination(int channel, int& port, char ipAddr[64],
                                    int& sourcePort, int& RTCPport)
{
    WEBRTC_TRACE(
                 kTraceApiCall,
                 kTraceVoice,
                 VoEId(_instanceId, -1),
                 "GetSendDestination(channel=%d, ipAddr[]=?, sourcePort=?,"
                 "RTCPport=?)",
                 channel);
#ifndef WEBRTC_EXTERNAL_TRANSPORT
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_channelManager, channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _engineStatistics.SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                                       "GetSendDestination() failed to locate "
                                       "channel");
        return -1;
    }
    WebRtc_Word32 ret = channelPtr->GetSendDestination(port, ipAddr,
                                                       sourcePort, RTCPport);
    if (ipAddr != NULL)
    {
        WEBRTC_TRACE(
                     kTraceStateInfo,
                     kTraceVoice,
                     VoEId(_instanceId, -1),
                     "GetSendDestination() => port=%d, RTCPport=%d, ipAddr=%s, "
                     "sourcePort=%d, RTCPport=%d",
                     port, RTCPport, ipAddr, sourcePort, RTCPport);
    }
    else
    {
        WEBRTC_TRACE(
                     kTraceStateInfo,
                     kTraceVoice,
                     VoEId(_instanceId, -1),
                     "GetSendDestination() => port=%d, RTCPport=%d, "
                     "sourcePort=%d, RTCPport=%d",
                     port, RTCPport, sourcePort, RTCPport);
    }
    return ret;
#else
    _engineStatistics.SetLastError(VE_EXTERNAL_TRANSPORT_ENABLED,
                                   kTraceWarning,
                                   "GetSendDestination() VoE is built for "
                                   "external transport");
    return -1;
#endif
}

int VoEBaseImpl::StartReceive(int channel)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId, -1),
                 "StartReceive(channel=%d)", channel);
    CriticalSectionScoped cs(*_apiCritPtr);
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_channelManager, channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _engineStatistics.SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                                       "StartReceive() failed to locate "
                                       "channel");
        return -1;
    }
    return channelPtr->StartReceiving();
}

int VoEBaseImpl::StopReceive(int channel)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId, -1),
                 "StopListen(channel=%d)", channel);
    CriticalSectionScoped cs(*_apiCritPtr);
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_channelManager, channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _engineStatistics.SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                                       "SetLocalReceiver() failed to locate "
                                       "channel");
        return -1;
    }
    return channelPtr->StopReceiving();
}

int VoEBaseImpl::StartPlayout(int channel)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId, -1),
                 "StartPlayout(channel=%d)", channel);
    CriticalSectionScoped cs(*_apiCritPtr);
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_channelManager, channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _engineStatistics.SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                                       "StartPlayout() failed to locate "
                                       "channel");
        return -1;
    }
    if (channelPtr->Playing())
    {
        return 0;
    }
    if (StartPlayout() != 0)
    {
        _engineStatistics.SetLastError(VE_AUDIO_DEVICE_MODULE_ERROR,
                                       kTraceError,
                                       "StartPlayout() failed to start "
                                       "playout");
        return -1;
    }
    return channelPtr->StartPlayout();
}

int VoEBaseImpl::StopPlayout(int channel)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId, -1),
                 "StopPlayout(channel=%d)", channel);
    CriticalSectionScoped cs(*_apiCritPtr);
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_channelManager, channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _engineStatistics.SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                                       "StopPlayout() failed to locate "
                                       "channel");
        return -1;
    }
    if (channelPtr->StopPlayout() != 0)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, -1),
                     "StopPlayout() failed to stop playout for channel %d",
                     channel);
    }
    return StopPlayout();
}

int VoEBaseImpl::StartSend(int channel)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId, -1),
                 "StartSend(channel=%d)", channel);
    CriticalSectionScoped cs(*_apiCritPtr);
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_channelManager, channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _engineStatistics.SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                                       "StartSend() failed to locate channel");
        return -1;
    }
    if (channelPtr->Sending())
    {
        return 0;
    }
#ifndef WEBRTC_EXTERNAL_TRANSPORT
    if (!channelPtr->ExternalTransport()
            && !channelPtr->SendSocketsInitialized())
    {
        _engineStatistics.SetLastError(VE_DESTINATION_NOT_INITED, kTraceError,
                                       "StartSend() must set send destination "
                                       "first");
        return -1;
    }
#endif
    if (StartSend() != 0)
    {
        _engineStatistics.SetLastError(VE_AUDIO_DEVICE_MODULE_ERROR,
                                       kTraceError,
                                       "StartSend() failed to start recording");
        return -1;
    }
    return channelPtr->StartSend();
}

int VoEBaseImpl::StopSend(int channel)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId, -1),
                 "StopSend(channel=%d)", channel);
    CriticalSectionScoped cs(*_apiCritPtr);
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_channelManager, channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _engineStatistics.SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                                       "StopSend() failed to locate channel");
        return -1;
    }
    if (channelPtr->StopSend() != 0)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, -1),
                     "StopSend() failed to stop sending for channel %d",
                     channel);
    }
    return StopSend();
}

int VoEBaseImpl::GetVersion(char version[1024])
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId, -1),
                 "GetVersion(version=?)");
    assert(kVoiceEngineVersionMaxMessageSize == 1024);

    if (version == NULL)
    {
        _engineStatistics.SetLastError(VE_INVALID_ARGUMENT, kTraceError);
        return (-1);
    }

    char versionBuf[kVoiceEngineVersionMaxMessageSize];
    char* versionPtr = versionBuf;

    WebRtc_Word32 len = 0;
    WebRtc_Word32 accLen = 0;

    len = AddVoEVersion(versionPtr);
    if (len == -1)
    {
        return -1;
    }
    versionPtr += len;
    accLen += len;
    assert(accLen < kVoiceEngineVersionMaxMessageSize);

    len = AddBuildInfo(versionPtr);
    if (len == -1)
    {
        return -1;
    }
    versionPtr += len;
    accLen += len;
    assert(accLen < kVoiceEngineVersionMaxMessageSize);

#ifdef WEBRTC_EXTERNAL_TRANSPORT
    len = AddExternalTransportBuild(versionPtr);
    if (len == -1)
    {
        return -1;
    }
    versionPtr += len;
    accLen += len;
    assert(accLen < kVoiceEngineVersionMaxMessageSize);
#endif

#ifdef WEBRTC_VOE_EXTERNAL_REC_AND_PLAYOUT
    len = AddExternalRecAndPlayoutBuild(versionPtr);
    if (len == -1)
    {
        return -1;
    }
    versionPtr += len;
    accLen += len;
    assert(accLen < kVoiceEngineVersionMaxMessageSize);
#endif

    len = AddADMVersion(versionPtr);
    if (len == -1)
    {
        return -1;
    }
    versionPtr += len;
    accLen += len;
    assert(accLen < kVoiceEngineVersionMaxMessageSize);

#ifndef WEBRTC_EXTERNAL_TRANSPORT
    len = AddSocketModuleVersion(versionPtr);
    if (len == -1)
    {
        return -1;
    }
    versionPtr += len;
    accLen += len;
    assert(accLen < kVoiceEngineVersionMaxMessageSize);
#endif

#ifdef WEBRTC_SRTP
    len = AddSRTPModuleVersion(versionPtr);
    if (len == -1)
    {
        return -1;
    }
    versionPtr += len;
    accLen += len;
    assert(accLen < kVoiceEngineVersionMaxMessageSize);
#endif

    len = AddRtpRtcpModuleVersion(versionPtr);
    if (len == -1)
    {
        return -1;
    }
    versionPtr += len;
    accLen += len;
    assert(accLen < kVoiceEngineVersionMaxMessageSize);

    len = AddConferenceMixerVersion(versionPtr);
    if (len == -1)
    {
        return -1;
    }
    versionPtr += len;
    accLen += len;
    assert(accLen < kVoiceEngineVersionMaxMessageSize);

    len = AddAudioProcessingModuleVersion(versionPtr);
    if (len == -1)
    {
        return -1;
    }
    versionPtr += len;
    accLen += len;
    assert(accLen < kVoiceEngineVersionMaxMessageSize);

    len = AddACMVersion(versionPtr);
    if (len == -1)
    {
        return -1;
    }
    versionPtr += len;
    accLen += len;
    assert(accLen < kVoiceEngineVersionMaxMessageSize);

    len = AddSPLIBVersion(versionPtr);
    if (len == -1)
    {
        return -1;
    }
    versionPtr += len;
    accLen += len;
    assert(accLen < kVoiceEngineVersionMaxMessageSize);

    memcpy(version, versionBuf, accLen);
    version[accLen] = '\0';

    // to avoid the truncation in the trace, split the string into parts
    char partOfVersion[256];
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "GetVersion() =>");
    for (int partStart = 0; partStart < accLen;)
    {
        memset(partOfVersion, 0, sizeof(partOfVersion));
        int partEnd = partStart + 180;
        while (version[partEnd] != '\n' && version[partEnd] != '\0')
        {
            partEnd--;
        }
        if (partEnd < accLen)
        {
            memcpy(partOfVersion, &version[partStart], partEnd - partStart);
        }
        else
        {
            memcpy(partOfVersion, &version[partStart], accLen - partStart);
        }
        partStart = partEnd;
        WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId, -1),
                     "%s", partOfVersion);
    }

    return 0;
}

WebRtc_Word32 VoEBaseImpl::AddBuildInfo(char* str) const
{
    return sprintf(str, "Build: %s\n", BUILDINFO);
}

WebRtc_Word32 VoEBaseImpl::AddVoEVersion(char* str) const
{
    return sprintf(str, "VoiceEngine 4.1.0\n");
}

WebRtc_Word32 VoEBaseImpl::AddSPLIBVersion(char* str) const
{
    char version[16];
    unsigned int len(16);
    WebRtcSpl_get_version(version, len);
    return sprintf(str, "SPLIB\t%s\n", version);
}

#ifdef WEBRTC_EXTERNAL_TRANSPORT
WebRtc_Word32 VoEBaseImpl::AddExternalTransportBuild(char* str) const
{
    return sprintf(str, "External transport build\n");
}
#endif

#ifdef WEBRTC_VOE_EXTERNAL_REC_AND_PLAYOUT
WebRtc_Word32 VoEBaseImpl::AddExternalRecAndPlayoutBuild(char* str) const
{
    return sprintf(str, "External recording and playout build\n");
}
#endif

WebRtc_Word32 VoEBaseImpl::AddModuleVersion(Module* module, char* str) const
{
    WebRtc_Word8 version[kVoiceEngineMaxModuleVersionSize];
    WebRtc_UWord32 remainingBufferInBytes(kVoiceEngineMaxModuleVersionSize);
    WebRtc_UWord32 position(0);
    if (module->Version(version, remainingBufferInBytes, position) == 0)
    {
        return sprintf(str, "%s\n", version);
    }
    return -1;
}

WebRtc_Word32 VoEBaseImpl::AddADMVersion(char* str) const
{
    AudioDeviceModule* admPtr(_audioDevicePtr);
    if (_audioDevicePtr == NULL)
    {
        admPtr = AudioDeviceModuleImpl::Create(-1);
    }
    admPtr->AddRef();
    int len = AddModuleVersion(admPtr, str);
    admPtr->Release();
    return len;
}

int VoEBaseImpl::AddAudioProcessingModuleVersion(char* str) const
{
    AudioProcessing* vpmPtr(_audioProcessingModulePtr);
    if (_audioProcessingModulePtr == NULL)
    {
        vpmPtr = AudioProcessing::Create(-1);
    }
    int len = AddModuleVersion(vpmPtr, str);
    if (_audioProcessingModulePtr == NULL)
    {
        AudioProcessing::Destroy(vpmPtr);
    }
    return len;
}

WebRtc_Word32 VoEBaseImpl::AddACMVersion(char* str) const
{
    AudioCodingModule* acmPtr = AudioCodingModule::Create(-1);
    int len = AddModuleVersion(acmPtr, str);
    AudioCodingModule::Destroy(acmPtr);
    return len;
}

WebRtc_Word32 VoEBaseImpl::AddConferenceMixerVersion(char* str) const
{
    AudioConferenceMixer* mixerPtr = AudioConferenceMixer::Create(-1);
    int len = AddModuleVersion(mixerPtr, str);
    delete mixerPtr;
    return len;
}

#ifndef WEBRTC_EXTERNAL_TRANSPORT
WebRtc_Word32 VoEBaseImpl::AddSocketModuleVersion(char* str) const
{
    WebRtc_UWord8 numSockThreads(1);
    UdpTransport* socketPtr = UdpTransport::Create(-1, numSockThreads);
    int len = AddModuleVersion(socketPtr, str);
    UdpTransport::Destroy(socketPtr);
    return len;
}
#endif

#ifdef WEBRTC_SRTP
WebRtc_Word32 VoEBaseImpl::AddSRTPModuleVersion(char* str) const
{
    SrtpModule* srtpPtr = SrtpModule::CreateSrtpModule(-1);
    int len = AddModuleVersion(srtpPtr, str);
    SrtpModule::DestroySrtpModule(srtpPtr);
    return len;
}
#endif

WebRtc_Word32 VoEBaseImpl::AddRtpRtcpModuleVersion(char* str) const
{
    RtpRtcp* rtpRtcpPtr = RtpRtcp::CreateRtpRtcp(-1, true);
    int len = AddModuleVersion(rtpRtcpPtr, str);
    RtpRtcp::DestroyRtpRtcp(rtpRtcpPtr);
    return len;
}

int VoEBaseImpl::LastError()
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId, -1),
                 "LastError()");
    return (_engineStatistics.LastError());
}


int VoEBaseImpl::SetNetEQPlayoutMode(int channel, NetEqModes mode)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId, -1),
                 "SetNetEQPlayoutMode(channel=%i, mode=%i)", channel, mode);
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_channelManager, channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _engineStatistics.SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                                       "SetNetEQPlayoutMode() failed to locate"
                                       " channel");
        return -1;
    }
    return channelPtr->SetNetEQPlayoutMode(mode);
}

int VoEBaseImpl::GetNetEQPlayoutMode(int channel, NetEqModes& mode)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId, -1),
                 "GetNetEQPlayoutMode(channel=%i, mode=?)", channel);
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_channelManager, channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _engineStatistics.SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                                       "GetNetEQPlayoutMode() failed to locate"
                                       " channel");
        return -1;
    }
    return channelPtr->GetNetEQPlayoutMode(mode);
}

int VoEBaseImpl::SetNetEQBGNMode(int channel, NetEqBgnModes mode)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId, -1),
                 "SetNetEQBGNMode(channel=%i, mode=%i)", channel, mode);
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_channelManager, channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _engineStatistics.SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                                       "SetNetEQBGNMode() failed to locate "
                                       "channel");
        return -1;
    }
    return channelPtr->SetNetEQBGNMode(mode);
}

int VoEBaseImpl::GetNetEQBGNMode(int channel, NetEqBgnModes& mode)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId, -1),
                 "GetNetEQBGNMode(channel=%i, mode=?)", channel);
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_channelManager, channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _engineStatistics.SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                                       "GetNetEQBGNMode() failed to locate "
                                       "channel");
        return -1;
    }
    return channelPtr->GetNetEQBGNMode(mode);
}

int VoEBaseImpl::SetOnHoldStatus(int channel, bool enable, OnHoldModes mode)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId, -1),
                 "SetOnHoldStatus(channel=%d, enable=%d, mode=%d)", channel,
                 enable, mode);
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_channelManager, channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _engineStatistics.SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                                       "SetOnHoldStatus() failed to locate "
                                       "channel");
        return -1;
    }
    return channelPtr->SetOnHoldStatus(enable, mode);
}

int VoEBaseImpl::GetOnHoldStatus(int channel, bool& enabled, OnHoldModes& mode)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId, -1),
                 "GetOnHoldStatus(channel=%d, enabled=?, mode=?)", channel);
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_channelManager, channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _engineStatistics.SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                                       "GetOnHoldStatus() failed to locate "
                                       "channel");
        return -1;
    }
    return channelPtr->GetOnHoldStatus(enabled, mode);
}

WebRtc_Word32 VoEBaseImpl::StartPlayout()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "VoEBaseImpl::StartPlayout()");
    if (_audioDevicePtr->Playing())
    {
        return 0;
    }
    if (!_externalPlayout)
    {
        if (_audioDevicePtr->InitPlayout() != 0)
        {
            WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId, -1),
                         "StartPlayout() failed to initialize playout");
            return -1;
        }
        if (_audioDevicePtr->StartPlayout() != 0)
        {
            WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId, -1),
                         "StartPlayout() failed to start playout");
            return -1;
        }
    }
    return 0;
}

WebRtc_Word32 VoEBaseImpl::StopPlayout()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "VoEBaseImpl::StopPlayout()");

    WebRtc_Word32 numOfChannels = _channelManager.NumOfChannels();
    if (numOfChannels <= 0)
    {
        return 0;
    }

    WebRtc_UWord16 nChannelsPlaying(0);
    WebRtc_Word32* channelsArray = new WebRtc_Word32[numOfChannels];

    // Get number of playing channels
    _channelManager.GetChannelIds(channelsArray, numOfChannels);
    for (int i = 0; i < numOfChannels; i++)
    {
        voe::ScopedChannel sc(_channelManager, channelsArray[i]);
        voe::Channel* chPtr = sc.ChannelPtr();
        if (chPtr)
        {
            if (chPtr->Playing())
            {
                nChannelsPlaying++;
            }
        }
    }
    delete[] channelsArray;

    // Stop audio-device playing if no channel is playing out
    if (nChannelsPlaying == 0)
    {
        if (_audioDevicePtr->StopPlayout() != 0)
        {
            _engineStatistics.SetLastError(VE_CANNOT_STOP_PLAYOUT, kTraceError,
                                           "StopPlayout() failed to stop "
                                           "playout");
            return -1;
        }
    }
    return 0;
}

WebRtc_Word32 VoEBaseImpl::StartSend()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "VoEBaseImpl::StartSend()");
    if (_audioDevicePtr->Recording())
    {
        return 0;
    }
    if (!_externalRecording)
    {
        if (_audioDevicePtr->InitRecording() != 0)
        {
            WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId, -1),
                         "StartSend() failed to initialize recording");
            return -1;
        }
        if (_audioDevicePtr->StartRecording() != 0)
        {
            WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId, -1),
                         "StartSend() failed to start recording");
            return -1;
        }
    }

    return 0;
}

WebRtc_Word32 VoEBaseImpl::StopSend()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "VoEBaseImpl::StopSend()");

    if ((NumOfSendingChannels() == 0) && !_transmitMixerPtr->IsRecordingMic())
    {
        // Stop audio-device recording if no channel is recording
        if (_audioDevicePtr->StopRecording() != 0)
        {
            _engineStatistics.SetLastError(VE_CANNOT_STOP_RECORDING,
                                           kTraceError,
                                           "StopSend() failed to stop "
                                           "recording");
            return -1;
        }
        _transmitMixerPtr->StopSend();
    }

    return 0;
}

WebRtc_Word32 VoEBaseImpl::TerminateInternal()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "VoEBaseImpl::TerminateInternal()");

    // Delete any remaining channel objects
    WebRtc_Word32 numOfChannels = _channelManager.NumOfChannels();
    if (numOfChannels > 0)
    {
        WebRtc_Word32* channelsArray = new WebRtc_Word32[numOfChannels];
        _channelManager.GetChannelIds(channelsArray, numOfChannels);
        for (int i = 0; i < numOfChannels; i++)
        {
            DeleteChannel(channelsArray[i]);
        }
        delete[] channelsArray;
    }

    if (_moduleProcessThreadPtr)
    {
        if (_audioDevicePtr)
        {
            if (_moduleProcessThreadPtr->DeRegisterModule(_audioDevicePtr) != 0)
            {
                _engineStatistics.SetLastError(VE_THREAD_ERROR, kTraceError,
                                               "TerminateInternal() failed to "
                                               "deregister ADM");
            }
        }
        if (_moduleProcessThreadPtr->Stop() != 0)
        {
            _engineStatistics.SetLastError(VE_THREAD_ERROR, kTraceError,
                                           "TerminateInternal() failed to stop "
                                           "module process thread");
        }
    }

    // Audio Device Module

    if (_audioDevicePtr != NULL)
    {
        if (_audioDevicePtr->StopPlayout() != 0)
        {
            _engineStatistics.SetLastError(VE_SOUNDCARD_ERROR, kTraceWarning,
                                           "TerminateInternal() failed to stop "
                                           "playout");
        }
        if (_audioDevicePtr->StopRecording() != 0)
        {
            _engineStatistics.SetLastError(VE_SOUNDCARD_ERROR, kTraceWarning,
                                           "TerminateInternal() failed to stop "
                                           "recording");
        }
        _audioDevicePtr->RegisterEventObserver(NULL);
        _audioDevicePtr->RegisterAudioCallback(NULL);
        if (_audioDevicePtr->Terminate() != 0)
        {
            _engineStatistics.SetLastError(VE_AUDIO_DEVICE_MODULE_ERROR,
                                           kTraceError,
                                           "TerminateInternal() failed to "
                                           "terminate the ADM");
        }
       
        _audioDevicePtr->Release();
        _audioDevicePtr = NULL;
    }

    // AP module

    if (_audioProcessingModulePtr != NULL)
    {
        _transmitMixerPtr->SetAudioProcessingModule(NULL);
        AudioProcessing::Destroy(_audioProcessingModulePtr);
        _audioProcessingModulePtr = NULL;
    }

    return _engineStatistics.SetUnInitialized();
}

} // namespace webrtc
