/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "voe_encryption_impl.h"


#include "channel.h"
#include "critical_section_wrapper.h"
#include "trace.h"
#include "voe_errors.h"
#include "voice_engine_impl.h"

namespace webrtc {

VoEEncryption* VoEEncryption::GetInterface(VoiceEngine* voiceEngine)
{
#ifndef WEBRTC_VOICE_ENGINE_ENCRYPTION_API
    return NULL;
#else
    if (NULL == voiceEngine)
    {
        return NULL;
    }
    VoiceEngineImpl* s = static_cast<VoiceEngineImpl*>(voiceEngine);
    s->AddRef();
    return s;
#endif
}

#ifdef WEBRTC_VOICE_ENGINE_ENCRYPTION_API

VoEEncryptionImpl::VoEEncryptionImpl(voe::SharedData* shared) : _shared(shared)
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "VoEEncryptionImpl::VoEEncryptionImpl() - ctor");
}

VoEEncryptionImpl::~VoEEncryptionImpl()
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "VoEEncryptionImpl::~VoEEncryptionImpl() - dtor");
}

int VoEEncryptionImpl::RegisterExternalEncryption(int channel,
                                                  Encryption& encryption)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "RegisterExternalEncryption(channel=%d, encryption=0x%x)",
                 channel, &encryption);
    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_shared->channel_manager(), channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
            "RegisterExternalEncryption() failed to locate channel");
        return -1;
    }
    return channelPtr->RegisterExternalEncryption(encryption);
}

int VoEEncryptionImpl::DeRegisterExternalEncryption(int channel)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "DeRegisterExternalEncryption(channel=%d)", channel);
    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_shared->channel_manager(), channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
            "DeRegisterExternalEncryption() failed to locate channel");
        return -1;
    }
    return channelPtr->DeRegisterExternalEncryption();
}

#endif  // #ifdef WEBRTC_VOICE_ENGINE_ENCRYPTION_API

// EOF
}  // namespace webrtc
