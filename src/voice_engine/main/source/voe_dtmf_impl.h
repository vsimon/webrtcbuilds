/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_VOE_DTMF_IMPL_H
#define WEBRTC_VOICE_ENGINE_VOE_DTMF_IMPL_H

#include "voe_dtmf.h"

#include "ref_count.h"
#include "shared_data.h"

namespace webrtc
{

class VoEDtmfImpl : public virtual voe::SharedData,
                    public VoEDtmf,
                    public voe::RefCount
{
public:
    virtual int Release();

    virtual int SendTelephoneEvent(
        int channel,
        int eventCode,
        bool outOfBand = true,
        int lengthMs = 160,
        int attenuationDb = 10);

    virtual int SetSendTelephoneEventPayloadType(int channel,
                                                 unsigned char type);

    virtual int GetSendTelephoneEventPayloadType(int channel,
                                                 unsigned char& type);

    virtual int SetDtmfFeedbackStatus(bool enable,
        bool directFeedback = false);

    virtual int GetDtmfFeedbackStatus(bool& enabled, bool& directFeedback);

    virtual int PlayDtmfTone(int eventCode,
                             int lengthMs = 200,
                             int attenuationDb = 10);

    virtual int StartPlayingDtmfTone(int eventCode,
                                     int attenuationDb = 10);

    virtual int StopPlayingDtmfTone();

    virtual int RegisterTelephoneEventDetection(
        int channel,
        TelephoneEventDetectionMethods detectionMethod,
        VoETelephoneEventObserver& observer);

    virtual int DeRegisterTelephoneEventDetection(int channel);

    virtual int GetTelephoneEventDetectionStatus(
        int channel,
        bool& enabled,
        TelephoneEventDetectionMethods& detectionMethod);

    virtual int SetDtmfPlayoutStatus(int channel, bool enable);

    virtual int GetDtmfPlayoutStatus(int channel, bool& enabled);

protected:
    VoEDtmfImpl();
    virtual ~VoEDtmfImpl();

private:
    bool _dtmfFeedback;
    bool _dtmfDirectFeedback;
};

} // namespace webrtc

#endif  // WEBRTC_VOICE_ENGINE_VOE_DTMF_IMPL_H
