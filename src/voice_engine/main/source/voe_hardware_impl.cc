/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "voe_hardware_impl.h"

#include <cassert>

#include "cpu_wrapper.h"
#include "critical_section_wrapper.h"
#include "trace.h"
#include "voe_errors.h"
#include "voice_engine_impl.h"

namespace webrtc
{

VoEHardware* VoEHardware::GetInterface(VoiceEngine* voiceEngine)
{
#ifndef WEBRTC_VOICE_ENGINE_HARDWARE_API
    return NULL;
#else
    if (NULL == voiceEngine)
    {
        return NULL;
    }
    VoiceEngineImpl* s =
            reinterpret_cast<VoiceEngineImpl*> (voiceEngine);
    VoEHardwareImpl* d = s;
    (*d)++;
    return (d);
#endif
}

#ifdef WEBRTC_VOICE_ENGINE_HARDWARE_API

VoEHardwareImpl::VoEHardwareImpl() :
    _cpu(NULL)
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_instanceId, -1),
                 "VoEHardwareImpl() - ctor");

    _cpu = CpuWrapper::CreateCpu();
    if (_cpu)
    {
        _cpu->CpuUsage(); // init cpu usage
    }
}

VoEHardwareImpl::~VoEHardwareImpl()
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_instanceId, -1),
                 "~VoEHardwareImpl() - dtor");

    if (_cpu)
    {
        delete _cpu;
        _cpu = NULL;
    }
}

int VoEHardwareImpl::Release()
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId, -1),
                 "VoEHardwareImpl::Release()");
    (*this)--;
    int refCount = GetCount();
    if (refCount < 0)
    {
        Reset();
        _engineStatistics.SetLastError(VE_INTERFACE_NOT_FOUND,
                                       kTraceWarning);
        return (-1);
    }
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "VoEHardwareImpl reference counter = %d", refCount);
    return (refCount);
}

int VoEHardwareImpl::SetAudioDeviceLayer(AudioLayers audioLayer)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId, -1),
                 "SetAudioDeviceLayer(audioLayer=%d)", audioLayer);

    // Don't allow a change if VoE is initialized
    if (_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_ALREADY_INITED, kTraceError);
        return -1;
    }

    // Map to AudioDeviceModule::AudioLayer
    AudioDeviceModule::AudioLayer
        wantedLayer(AudioDeviceModule::kPlatformDefaultAudio);
    switch (audioLayer)
    {
        case kAudioPlatformDefault:
            // already set above
            break;
        case kAudioWindowsCore:
            wantedLayer = AudioDeviceModule::kWindowsCoreAudio;
            break;
        case kAudioWindowsWave:
            wantedLayer = AudioDeviceModule::kWindowsWaveAudio;
            break;
        case kAudioLinuxAlsa:
            wantedLayer = AudioDeviceModule::kLinuxAlsaAudio;
            break;
        case kAudioLinuxPulse:
            wantedLayer = AudioDeviceModule::kLinuxPulseAudio;
            break;
        default:
            _engineStatistics.SetLastError(VE_INVALID_ARGUMENT, kTraceError,
                                           "  invalid audio layer");
            return -1;
    }

    // Save the audio device layer for Init()
    _audioDeviceLayer = wantedLayer;

    return 0;
}

int VoEHardwareImpl::GetAudioDeviceLayer(AudioLayers& audioLayer)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId, -1),
               "GetAudioDeviceLayer(devices=?)");

    // Can always be called regardless of VoE state

    AudioDeviceModule::AudioLayer
        activeLayer(AudioDeviceModule::kPlatformDefaultAudio);

    if (_audioDevicePtr)
    {
        // Get active audio layer from ADM
        if (_audioDevicePtr->ActiveAudioLayer(&activeLayer) != 0)
        {
            _engineStatistics.SetLastError(VE_UNDEFINED_SC_ERR, kTraceError,
                                           "  Audio Device error");
            return -1;
        }
    }
    else
    {
        // Return VoE's internal layer setting
        activeLayer = _audioDeviceLayer;
    }

    // Map to AudioLayers
    switch (activeLayer)
    {
        case AudioDeviceModule::kPlatformDefaultAudio:
            audioLayer = kAudioPlatformDefault;
            break;
        case AudioDeviceModule::kWindowsCoreAudio:
            audioLayer = kAudioWindowsCore;
            break;
        case AudioDeviceModule::kWindowsWaveAudio:
            audioLayer = kAudioWindowsWave;
            break;
        case AudioDeviceModule::kLinuxAlsaAudio:
            audioLayer = kAudioLinuxAlsa;
            break;
        case AudioDeviceModule::kLinuxPulseAudio:
            audioLayer = kAudioLinuxPulse;
            break;
        default:
            _engineStatistics.SetLastError(VE_UNDEFINED_SC_ERR, kTraceError,
                                           "  unknown audio layer");
    }

    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId, -1),
               "  Output: audioLayer=%d", audioLayer);

    return 0;
}
int VoEHardwareImpl::GetNumOfRecordingDevices(int& devices)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId, -1),
                 "GetNumOfRecordingDevices(devices=?)");
    ANDROID_NOT_SUPPORTED(); IPHONE_NOT_SUPPORTED();

    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }

    devices = static_cast<int> (_audioDevicePtr->RecordingDevices());

    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "  Output: devices=%d", devices);

    return 0;
}

int VoEHardwareImpl::GetNumOfPlayoutDevices(int& devices)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId, -1),
                 "GetNumOfPlayoutDevices(devices=?)");
    ANDROID_NOT_SUPPORTED(); IPHONE_NOT_SUPPORTED();

    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }

    devices = static_cast<int> (_audioDevicePtr->PlayoutDevices());

    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "  Output: devices=%d", devices);

    return 0;
}

int VoEHardwareImpl::GetRecordingDeviceName(int index,
                                            char strNameUTF8[128],
                                            char strGuidUTF8[128])
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId, -1),
                 "GetRecordingDeviceName(index=%d)", index);
    ANDROID_NOT_SUPPORTED();
    IPHONE_NOT_SUPPORTED();

    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    if (strNameUTF8 == NULL)
    {
        _engineStatistics.SetLastError(
            VE_INVALID_ARGUMENT, kTraceError,
            "GetRecordingDeviceName() invalid argument");
        return -1;
    }

    // Note that strGuidUTF8 is allowed to be NULL

    // Init len variable to length of supplied vectors
    const WebRtc_UWord16 strLen = 128;

    // Check if length has been changed in module
    assert(strLen == kAdmMaxDeviceNameSize);
    assert(strLen == kAdmMaxGuidSize);

    WebRtc_Word8 name[strLen];
    WebRtc_Word8 guid[strLen];

    // Get names from module
    if (_audioDevicePtr->RecordingDeviceName(index, name, guid) != 0)
    {
        _engineStatistics.SetLastError(
            VE_CANNOT_RETRIEVE_DEVICE_NAME, kTraceError,
            "GetRecordingDeviceName() failed to get device name");
        return -1;
    }

    // Copy to vectors supplied by user
    strncpy(strNameUTF8, name, strLen);
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "  Output: strNameUTF8=%s", strNameUTF8);

    if (strGuidUTF8 != NULL)
    {
        strncpy(strGuidUTF8, name, strLen);
        WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId, -1),
                     "  Output: strGuidUTF8=%s", strGuidUTF8);
    }

    return 0;
}

int VoEHardwareImpl::GetPlayoutDeviceName(int index,
                                          char strNameUTF8[128],
                                          char strGuidUTF8[128])
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId, -1),
                 "GetPlayoutDeviceName(index=%d)", index);
    ANDROID_NOT_SUPPORTED();
    IPHONE_NOT_SUPPORTED();

    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    if (strNameUTF8 == NULL)
    {
        _engineStatistics.SetLastError(
            VE_INVALID_ARGUMENT, kTraceError,
            "GetPlayoutDeviceName() invalid argument");
        return -1;
    }

    // Note that strGuidUTF8 is allowed to be NULL

    // Init len variable to length of supplied vectors
    const WebRtc_UWord16 strLen = 128;

    // Check if length has been changed in module
    assert(strLen == kAdmMaxDeviceNameSize);
    assert(strLen == kAdmMaxGuidSize);

    WebRtc_Word8 name[strLen];
    WebRtc_Word8 guid[strLen];

    // Get names from module
    if (_audioDevicePtr->PlayoutDeviceName(index, name, guid) != 0)
    {
        _engineStatistics.SetLastError(
            VE_CANNOT_RETRIEVE_DEVICE_NAME, kTraceError,
            "GetPlayoutDeviceName() failed to get device name");
        return -1;
    }

    // Copy to vectors supplied by user
    strncpy(strNameUTF8, name, strLen);
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "  Output: strNameUTF8=%s", strNameUTF8);

    if (strGuidUTF8 != NULL)
    {
        strncpy(strGuidUTF8, guid, strLen);
        WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId, -1),
                     "  Output: strGuidUTF8=%s", strGuidUTF8);
    }

    return 0;
}

int VoEHardwareImpl::SetRecordingDevice(int index,
                                        StereoChannel recordingChannel)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId, -1),
                 "SetRecordingDevice(index=%d, recordingChannel=%d)",
                 index, (int) recordingChannel);
    CriticalSectionScoped cs(*_apiCritPtr);
    ANDROID_NOT_SUPPORTED();
    IPHONE_NOT_SUPPORTED();

    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }

    bool isRecording(false);

    // Store state about activated recording to be able to restore it after the
    // recording device has been modified.
    if (_audioDevicePtr->Recording())
    {
        WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                     "SetRecordingDevice() device is modified while recording"
                     " is active...");
        isRecording = true;
        if (_audioDevicePtr->StopRecording() == -1)
        {
            _engineStatistics.SetLastError(
                VE_AUDIO_DEVICE_MODULE_ERROR, kTraceError,
                "SetRecordingDevice() unable to stop recording");
            return -1;
        }
    }

    // We let the module do the index sanity

    // Set recording channel
    AudioDeviceModule::ChannelType recCh =
        AudioDeviceModule::kChannelBoth;
    switch (recordingChannel)
    {
        case kStereoLeft:
            recCh = AudioDeviceModule::kChannelLeft;
            break;
        case kStereoRight:
            recCh = AudioDeviceModule::kChannelRight;
            break;
        case kStereoBoth:
            // default setting kChannelBoth (<=> mono)
            break;
        default:
            _engineStatistics.SetLastError(
                VE_INVALID_ARGUMENT, kTraceError,
                "SetRecordingDevice() unknown recording channel");
            return -1;
    }

    // Cannot return error because of sanity above
    _audioDevicePtr->RecordingChannel(&recCh);

    // Map indices to unsigned since underlying functions need that
    WebRtc_UWord16 indexU = static_cast<WebRtc_UWord16> (index);

    WebRtc_Word32 res(0);

    if (index == -1)
    {
        res = _audioDevicePtr->SetRecordingDevice(
            AudioDeviceModule::kDefaultCommunicationDevice);
    }
    else if (index == -2)
    {
        res = _audioDevicePtr->SetRecordingDevice(
            AudioDeviceModule::kDefaultDevice);
    }
    else
    {
        res = _audioDevicePtr->SetRecordingDevice(indexU);
    }

    if (res != 0)
    {
        _engineStatistics.SetLastError(
            VE_AUDIO_DEVICE_MODULE_ERROR, kTraceError,
            "SetRecordingDevice() unable to set the recording device");
        return -1;
    }

    // Init microphone, so user can do volume settings etc
    if (_audioDevicePtr->InitMicrophone() == -1)
    {
        _engineStatistics.SetLastError(
            VE_CANNOT_ACCESS_MIC_VOL, kTraceWarning,
            "SetRecordingDevice() cannot access microphone");
    }

    // Set number of channels
    bool available(false);
    _audioDevicePtr->StereoRecordingIsAvailable(&available);
    if (_audioDevicePtr->SetStereoRecording(available ? true : false) != 0)
    {
        _engineStatistics.SetLastError(
            VE_SOUNDCARD_ERROR, kTraceWarning,
            "SetRecordingDevice() failed to set mono recording mode");
    }

    // Restore recording if it was enabled already when calling this function.
    if (isRecording)
    {
        if (!_externalRecording)
        {
            WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                         "SetRecordingDevice() recording is now being "
                         "restored...");
            if (_audioDevicePtr->InitRecording() != 0)
            {
                WEBRTC_TRACE(kTraceError, kTraceVoice,
                             VoEId(_instanceId, -1),
                             "SetRecordingDevice() failed to initialize "
                             "recording");
                return -1;
            }
            if (_audioDevicePtr->StartRecording() != 0)
            {
                WEBRTC_TRACE(kTraceError, kTraceVoice,
                             VoEId(_instanceId, -1),
                             "SetRecordingDevice() failed to start recording");
                return -1;
            }
        }
    }

    return 0;
}

int VoEHardwareImpl::SetPlayoutDevice(int index)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId, -1),
                 "SetPlayoutDevice(index=%d)", index);
    CriticalSectionScoped cs(*_apiCritPtr);
    ANDROID_NOT_SUPPORTED(); IPHONE_NOT_SUPPORTED();

    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }

    bool isPlaying(false);

    // Store state about activated playout to be able to restore it after the
    // playout device has been modified.
    if (_audioDevicePtr->Playing())
    {
        WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                     "SetPlayoutDevice() device is modified while playout is "
                     "active...");
        isPlaying = true;
        if (_audioDevicePtr->StopPlayout() == -1)
        {
            _engineStatistics.SetLastError(
                VE_AUDIO_DEVICE_MODULE_ERROR, kTraceError,
                "SetPlayoutDevice() unable to stop playout");
            return -1;
        }
    }

    // We let the module do the index sanity

    // Map indices to unsigned since underlying functions need that
    WebRtc_UWord16 indexU = static_cast<WebRtc_UWord16> (index);

    WebRtc_Word32 res(0);

    if (index == -1)
    {
        res = _audioDevicePtr->SetPlayoutDevice(
            AudioDeviceModule::kDefaultCommunicationDevice);
    }
    else if (index == -2)
    {
        res = _audioDevicePtr->SetPlayoutDevice(
            AudioDeviceModule::kDefaultDevice);
    }
    else
    {
        res = _audioDevicePtr->SetPlayoutDevice(indexU);
    }

    if (res != 0)
    {
        _engineStatistics.SetLastError(
            VE_SOUNDCARD_ERROR, kTraceError,
            "SetPlayoutDevice() unable to set the playout device");
        return -1;
    }

    // Init speaker, so user can do volume settings etc
    if (_audioDevicePtr->InitSpeaker() == -1)
    {
        _engineStatistics.SetLastError(
            VE_CANNOT_ACCESS_SPEAKER_VOL, kTraceWarning,
            "SetPlayoutDevice() cannot access speaker");
    }

    // Set number of channels
    bool available(false);
    _audioDevicePtr->StereoPlayoutIsAvailable(&available);
    if (_audioDevicePtr->SetStereoPlayout(available ? true : false) != 0)
    {
        _engineStatistics.SetLastError(
            VE_SOUNDCARD_ERROR, kTraceWarning,
            "SetPlayoutDevice() failed to set stereo playout mode");
    }

    // Restore playout if it was enabled already when calling this function.
    if (isPlaying)
    {
        if (!_externalPlayout)
        {
            WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                         "SetPlayoutDevice() playout is now being restored...");
            if (_audioDevicePtr->InitPlayout() != 0)
            {
                WEBRTC_TRACE(kTraceError, kTraceVoice,
                             VoEId(_instanceId, -1),
                             "SetPlayoutDevice() failed to initialize playout");
                return -1;
            }
            if (_audioDevicePtr->StartPlayout() != 0)
            {
                WEBRTC_TRACE(kTraceError, kTraceVoice,
                             VoEId(_instanceId, -1),
                             "SetPlayoutDevice() failed to start playout");
                return -1;
            }
        }
    }

    return 0;
}

int VoEHardwareImpl::GetRecordingDeviceStatus(bool& isAvailable)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId, -1),
                 "GetRecordingDeviceStatus()");
    ANDROID_NOT_SUPPORTED();
    IPHONE_NOT_SUPPORTED();

    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }

    // We let the module do isRecording sanity

    bool available(false);

    // Check availability
    if (_audioDevicePtr->RecordingIsAvailable(&available) != 0)
    {
        _engineStatistics.SetLastError(VE_UNDEFINED_SC_REC_ERR, kTraceError,
                                       "  Audio Device error");
        return -1;
    }

    isAvailable = available;

    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "  Output: isAvailable = %d)", (int) isAvailable);

    return 0;
}

int VoEHardwareImpl::GetPlayoutDeviceStatus(bool& isAvailable)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId, -1),
                 "GetPlayoutDeviceStatus()");
    ANDROID_NOT_SUPPORTED();
    IPHONE_NOT_SUPPORTED();

    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }

    // We let the module do isPlaying sanity

    bool available(false);

    // Check availability
    if (_audioDevicePtr->PlayoutIsAvailable(&available) != 0)
    {
        _engineStatistics.SetLastError(VE_PLAY_UNDEFINED_SC_ERR,
                                       kTraceError, "  Audio Device error");
        return -1;
    }

    isAvailable = available;

    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "  Output: isAvailable = %d)", (int) isAvailable);

    return 0;
}

int VoEHardwareImpl::ResetAudioDevice()
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId, -1),
                 "ResetAudioDevice()");
    ANDROID_NOT_SUPPORTED();

    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }

#if defined(MAC_IPHONE)
    if (_audioDevicePtr->ResetAudioDevice() < 0)
    {
        _engineStatistics.SetLastError(VE_SOUNDCARD_ERROR, kTraceError,
                                       "  Failed to reset sound device");
        return -1;
    }
#else
    _engineStatistics.SetLastError(VE_FUNC_NOT_SUPPORTED, kTraceError,
                                   "  no support for resetting sound device");
    return -1;
#endif

    return 0;
}

int VoEHardwareImpl::AudioDeviceControl(unsigned int par1, unsigned int par2,
                                        unsigned int par3)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId, -1),
                 "AudioDeviceControl(%i, %i, %i)", par1, par2, par3);
    ANDROID_NOT_SUPPORTED();
    IPHONE_NOT_SUPPORTED();
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    _engineStatistics.SetLastError(VE_FUNC_NOT_SUPPORTED, kTraceError,
                                   "  no support for resetting sound device");
    return -1;
}

int VoEHardwareImpl::SetLoudspeakerStatus(bool enable)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId, -1),
                 "SetLoudspeakerStatus(enable=%i)", (int) enable);
    IPHONE_NOT_SUPPORTED();

    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
#if defined(WEBRTC_ANDROID)
    if (_audioDevicePtr->SetLoudspeakerStatus(enable) < 0)
    {
        _engineStatistics.SetLastError(VE_IGNORED_FUNCTION, kTraceError,
                                       "  Failed to set loudspeaker status");
        return -1;
    }

    return 0;
#else
    _engineStatistics.SetLastError(VE_FUNC_NOT_SUPPORTED, kTraceError,
                                   "  no support for setting loudspeaker"
                                   " status");
    return -1;
#endif
}

int VoEHardwareImpl::GetLoudspeakerStatus(bool& enabled)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId, -1),
                 "GetLoudspeakerStatus()");
    IPHONE_NOT_SUPPORTED();

#if defined(WEBRTC_ANDROID)
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }

    if (_audioDevicePtr->GetLoudspeakerStatus(&enabled) < 0)
    {
        _engineStatistics.SetLastError(VE_IGNORED_FUNCTION, kTraceError,
                                       "  Failed to get loudspeaker status");
        return -1;
    }

    return 0;
#else
    _engineStatistics.SetLastError(VE_FUNC_NOT_SUPPORTED, kTraceError,
                                   "  no support for setting loudspeaker "
                                   "status");
    return -1;
#endif
}

int VoEHardwareImpl::GetCPULoad(int& loadPercent)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId, -1),
                 "GetCPULoad()");
    ANDROID_NOT_SUPPORTED(); IPHONE_NOT_SUPPORTED();

    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }

    // Get CPU load from ADM
    WebRtc_UWord16 load(0);
    if (_audioDevicePtr->CPULoad(&load) != 0)
    {
        _engineStatistics.SetLastError(VE_CPU_INFO_ERROR, kTraceError,
                                       "  error getting system CPU load");
        return -1;
    }

    loadPercent = static_cast<int> (load);

    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId, -1),
               "  Output: loadPercent = %d", loadPercent);

    return 0;
}

int VoEHardwareImpl::GetSystemCPULoad(int& loadPercent)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId, -1),
                 "GetSystemCPULoad(loadPercent=?)");
    ANDROID_NOT_SUPPORTED(); IPHONE_NOT_SUPPORTED();

    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }

    // Check if implemented for this platform
    if (!_cpu)
    {
        _engineStatistics.SetLastError(VE_FUNC_NOT_SUPPORTED, kTraceError,
                                       "  no support for getting system CPU "
                                       "load");
        return -1;
    }

    // Get CPU load
    WebRtc_Word32 load = _cpu->CpuUsage();
    if (load < 0)
    {
        _engineStatistics.SetLastError(VE_CPU_INFO_ERROR, kTraceError,
                                       "  error getting system CPU load");
        return -1;
    }

    loadPercent = static_cast<int> (load);

    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "  Output: loadPercent = %d", loadPercent);

    return 0;
}

#endif  // WEBRTC_VOICE_ENGINE_HARDWARE_API

} // namespace webrtc
