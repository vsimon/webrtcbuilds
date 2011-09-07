/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "voe_dtmf_impl.h"

#include "channel.h"
#include "critical_section_wrapper.h"
#include "output_mixer.h"
#include "trace.h"
#include "transmit_mixer.h"
#include "voe_errors.h"
#include "voice_engine_impl.h"

namespace webrtc {

VoEDtmf* VoEDtmf::GetInterface(VoiceEngine* voiceEngine)
{
#ifndef WEBRTC_VOICE_ENGINE_DTMF_API
    return NULL;
#else
    if (NULL == voiceEngine)
    {
        return NULL;
    }
    VoiceEngineImpl* s =
        reinterpret_cast<VoiceEngineImpl*> (voiceEngine);
    VoEDtmfImpl* d = s;
    ( *d)++;
    return (d);
#endif
}

#ifdef WEBRTC_VOICE_ENGINE_DTMF_API

VoEDtmfImpl::VoEDtmfImpl() :
    _dtmfFeedback(true),
    _dtmfDirectFeedback(false)
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_instanceId,-1 ),
                 "VoEDtmfImpl::VoEDtmfImpl() - ctor");
}

VoEDtmfImpl::~VoEDtmfImpl()
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_instanceId,-1),
                 "VoEDtmfImpl::~VoEDtmfImpl() - dtor");
}

int VoEDtmfImpl::Release()
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "VoEDtmf::Release()");
    (*this)--;
    int refCount = GetCount();
    if (refCount < 0)
    {
        Reset(); // reset reference counter to zero => OK to delete VE
        _engineStatistics.SetLastError(
            VE_INTERFACE_NOT_FOUND, kTraceWarning);
        return (-1);
    }
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId,-1),
                 "VoEDtmf reference counter = %d", refCount);
    return (refCount);
}

int VoEDtmfImpl::SendTelephoneEvent(int channel,
                                    int eventCode,
                                    bool outOfBand,
                                    int lengthMs,
                                    int attenuationDb)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "SendTelephoneEvent(channel=%d, eventCode=%d, outOfBand=%d,"
                 "length=%d, attenuationDb=%d)",
                 channel, eventCode, (int)outOfBand, lengthMs, attenuationDb);
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_channelManager, channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _engineStatistics.SetLastError(
            VE_CHANNEL_NOT_VALID, kTraceError,
            "SendTelephoneEvent() failed to locate channel");
        return -1;
    }
    if (!channelPtr->Sending())
    {
        _engineStatistics.SetLastError(
            VE_NOT_SENDING, kTraceError,
            "SendTelephoneEvent() sending is not active");
        return -1;
    }

    // Sanity check
    const int maxEventCode = outOfBand ?
        static_cast<int>(kMaxTelephoneEventCode) :
        static_cast<int>(kMaxDtmfEventCode);
    const bool testFailed = ((eventCode < 0) ||
        (eventCode > maxEventCode) ||
        (lengthMs < kMinTelephoneEventDuration) ||
        (lengthMs > kMaxTelephoneEventDuration) ||
        (attenuationDb < kMinTelephoneEventAttenuation) ||
        (attenuationDb > kMaxTelephoneEventAttenuation));
    if (testFailed)
    {
        _engineStatistics.SetLastError(
            VE_INVALID_ARGUMENT, kTraceError,
            "SendTelephoneEvent() invalid parameter(s)");
        return -1;
    }

    const bool isDtmf =
        (eventCode >= 0) && (eventCode <= kMaxDtmfEventCode);
    const bool playDtmfToneDirect =
        isDtmf && (_dtmfFeedback && _dtmfDirectFeedback);

    if (playDtmfToneDirect)
    {
        // Mute the microphone signal while playing back the tone directly.
        // This is to reduce the risk of introducing echo from the added output.
        _transmitMixerPtr->UpdateMuteMicrophoneTime(lengthMs);

        // Play out local feedback tone directly (same approach for both inband
        // and outband).
        // Reduce the length of the the tone with 80ms to reduce risk of echo.
        // For non-direct feedback, outband and inband cases are handled
        // differently.
        _outputMixerPtr->PlayDtmfTone(eventCode, lengthMs-80, attenuationDb);
    }

    if (outOfBand)
    {
        // The RTP/RTCP module will always deliver OnPlayTelephoneEvent when
        // an event is transmitted. It is up to the VoE to utilize it or not.
        // This flag ensures that feedback/playout is enabled; however, the
        // channel object must still parse out the Dtmf events (0-15) from
        // all possible events (0-255).
        const bool playDTFMEvent = (_dtmfFeedback && !_dtmfDirectFeedback);

        return channelPtr->SendTelephoneEventOutband(eventCode,
                                                     lengthMs,
                                                     attenuationDb,
                                                     playDTFMEvent);
    }
    else
    {
        // For Dtmf tones, we want to ensure that inband tones are played out
        // in sync with the transmitted audio. This flag is utilized by the
        // channel object to determine if the queued Dtmf e vent shall also
        // be fed to the output mixer in the same step as input audio is
        // replaced by inband Dtmf tones.
        const bool playDTFMEvent =
            (isDtmf && _dtmfFeedback && !_dtmfDirectFeedback);

        return channelPtr->SendTelephoneEventInband(eventCode,
                                                    lengthMs,
                                                    attenuationDb,
                                                    playDTFMEvent);
    }
}

int VoEDtmfImpl::SetSendTelephoneEventPayloadType(int channel,
                                                  unsigned char type)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "SetSendTelephoneEventPayloadType(channel=%d, type=%u)",
                 channel, type);
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_channelManager, channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _engineStatistics.SetLastError(
            VE_CHANNEL_NOT_VALID, kTraceError,
            "SetSendTelephoneEventPayloadType() failed to locate channel");
        return -1;
    }
    return channelPtr->SetSendTelephoneEventPayloadType(type);
}

int VoEDtmfImpl::GetSendTelephoneEventPayloadType(int channel,
                                                  unsigned char& type)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "GetSendTelephoneEventPayloadType(channel=%d)", channel);
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_channelManager, channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _engineStatistics.SetLastError(
            VE_CHANNEL_NOT_VALID, kTraceError,
            "GetSendTelephoneEventPayloadType() failed to locate channel");
        return -1;
    }
    return channelPtr->GetSendTelephoneEventPayloadType(type);
}

int VoEDtmfImpl::PlayDtmfTone(int eventCode,
                              int lengthMs,
                              int attenuationDb)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "PlayDtmfTone(eventCode=%d, lengthMs=%d, attenuationDb=%d)",
                 eventCode, lengthMs, attenuationDb);

    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    if (!_audioDevicePtr->Playing())
    {
        _engineStatistics.SetLastError(
            VE_NOT_PLAYING, kTraceError,
            "PlayDtmfTone() no channel is playing out");
        return -1;
    }
    if ((eventCode < kMinDtmfEventCode) ||
        (eventCode > kMaxDtmfEventCode) ||
        (lengthMs < kMinTelephoneEventDuration) ||
        (lengthMs > kMaxTelephoneEventDuration) ||
        (attenuationDb <kMinTelephoneEventAttenuation) ||
        (attenuationDb > kMaxTelephoneEventAttenuation))
    {
        _engineStatistics.SetLastError(
            VE_INVALID_ARGUMENT, kTraceError,
        "PlayDtmfTone() invalid tone parameter(s)");
        return -1;
    }
    return _outputMixerPtr->PlayDtmfTone(eventCode, lengthMs, attenuationDb);
}

int VoEDtmfImpl::StartPlayingDtmfTone(int eventCode,
                                      int attenuationDb)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "StartPlayingDtmfTone(eventCode=%d, attenuationDb=%d)",
                 eventCode, attenuationDb);

    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    if (!_audioDevicePtr->Playing())
    {
        _engineStatistics.SetLastError(
            VE_NOT_PLAYING, kTraceError,
            "StartPlayingDtmfTone() no channel is playing out");
        return -1;
    }
    if ((eventCode < kMinDtmfEventCode) ||
        (eventCode > kMaxDtmfEventCode) ||
        (attenuationDb < kMinTelephoneEventAttenuation) ||
        (attenuationDb > kMaxTelephoneEventAttenuation))
    {
        _engineStatistics.SetLastError(
            VE_INVALID_ARGUMENT, kTraceError,
            "StartPlayingDtmfTone() invalid tone parameter(s)");
        return -1;
    }
    return _outputMixerPtr->StartPlayingDtmfTone(eventCode, attenuationDb);
}

int VoEDtmfImpl::StopPlayingDtmfTone()
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "StopPlayingDtmfTone()");

    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    return _outputMixerPtr->StopPlayingDtmfTone();
}

int VoEDtmfImpl::RegisterTelephoneEventDetection(
    int channel,
    TelephoneEventDetectionMethods detectionMethod,
    VoETelephoneEventObserver& observer)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "RegisterTelephoneEventDetection(channel=%d, detectionMethod=%d,"
                 "observer=0x%x)", channel, detectionMethod, &observer);
#ifdef WEBRTC_DTMF_DETECTION
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_channelManager, channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _engineStatistics.SetLastError(
            VE_CHANNEL_NOT_VALID, kTraceError,
            "RegisterTelephoneEventDetection() failed to locate channel");
        return -1;
    }
    return channelPtr->RegisterTelephoneEventDetection(detectionMethod,
                                                       observer);
#else
    _engineStatistics.SetLastError(
        VE_FUNC_NOT_SUPPORTED, kTraceError,
        "SetTelephoneEventDetectionStatus() Dtmf detection is not supported");
    return -1;
#endif
}

int VoEDtmfImpl::DeRegisterTelephoneEventDetection(int channel)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
            "DeRegisterTelephoneEventDetection(channel=%d)", channel);
#ifdef WEBRTC_DTMF_DETECTION
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_channelManager, channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _engineStatistics.SetLastError(
            VE_CHANNEL_NOT_VALID, kTraceError,
            "DeRegisterTelephoneEventDe tection() failed to locate channel");
            return -1;
    }
    return channelPtr->DeRegisterTelephoneEventDetection();
#else
    _engineStatistics.SetLastError(
        VE_FUNC_NOT_SUPPORTED, kTraceError,
        "DeRegisterTelephoneEventDetection() Dtmf detection is not supported");
    return -1;
#endif
}


int VoEDtmfImpl::GetTelephoneEventDetectionStatus(
    int channel,
    bool& enabled,
    TelephoneEventDetectionMethods& detectionMethod)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
               "GetTelephoneEventDetectionStatus(channel=%d)", channel);
#ifdef WEBRTC_DTMF_DETECTION
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_channelManager, channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _engineStatistics.SetLastError(
            VE_CHANNEL_NOT_VALID, kTraceError,
            "GetTelephoneEventDetectionStatus() failed to locate channel");
        return -1;
    }
    return channelPtr->GetTelephoneEventDetectionStatus(enabled, detectionMethod);
#else
    _engineStatistics.SetLastError(
        VE_FUNC_NOT_SUPPORTED, kTraceError,
        "GetTelephoneEventDetectionStatus() Dtmf detection is not supported");
    return -1;
#endif
}

int VoEDtmfImpl::SetDtmfFeedbackStatus(bool enable, bool directFeedback)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "SetDtmfFeedbackStatus(enable=%d, directFeeback=%d)",
                 (int)enable, (int)directFeedback);

    CriticalSectionScoped sc(*_apiCritPtr);

    _dtmfFeedback = enable;
    _dtmfDirectFeedback = directFeedback;

    return 0;
}

int VoEDtmfImpl::GetDtmfFeedbackStatus(bool& enabled, bool& directFeedback)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "GetDtmfFeedbackStatus()");

    CriticalSectionScoped sc(*_apiCritPtr);

    enabled = _dtmfFeedback;
    directFeedback = _dtmfDirectFeedback;

    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId,-1),
                 "GetDtmfFeedbackStatus() => enabled=%d, directFeedback=%d",
                 enabled, directFeedback);
    return 0;
}

int VoEDtmfImpl::SetDtmfPlayoutStatus(int channel, bool enable)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "SetDtmfPlayoutStatus(channel=%d, enable=%d)",
                 channel, enable);
    IPHONE_NOT_SUPPORTED();

    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_channelManager, channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _engineStatistics.SetLastError(
            VE_CHANNEL_NOT_VALID, kTraceError,
            "SetDtmfPlayoutStatus() failed to locate channel");
        return -1;
    }
    return channelPtr->SetDtmfPlayoutStatus(enable);
}

int VoEDtmfImpl::GetDtmfPlayoutStatus(int channel, bool& enabled)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "GetDtmfPlayoutStatus(channel=%d, enabled=?)", channel);
    IPHONE_NOT_SUPPORTED();
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_channelManager, channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _engineStatistics.SetLastError(
            VE_CHANNEL_NOT_VALID, kTraceError,
            "GetDtmfPlayoutStatus() failed to locate channel");
        return -1;
    }
    enabled = channelPtr->DtmfPlayoutStatus();
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId,-1),
                 "GetDtmfPlayoutStatus() => enabled=%d", enabled);
    return 0;
}

#endif  // #ifdef WEBRTC_VOICE_ENGINE_DTMF_API

}  // namespace webrtc
