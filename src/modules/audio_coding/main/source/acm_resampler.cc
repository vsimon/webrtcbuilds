/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string.h>

#include "acm_resampler.h"
#include "critical_section_wrapper.h"
#include "resampler.h"
#include "signal_processing_library.h"
#include "trace.h"

namespace webrtc
{

ACMResampler::ACMResampler():

_resamplerCritSect(*CriticalSectionWrapper::CreateCriticalSection())
{
}

ACMResampler::~ACMResampler()
{

    delete &_resamplerCritSect;
}


WebRtc_Word16
ACMResampler::Resample10Msec(
    const WebRtc_Word16* inAudio,
    WebRtc_Word32        inFreqHz,
    WebRtc_Word16*       outAudio,
    WebRtc_Word32        outFreqHz,
    WebRtc_UWord8        numAudioChannels)
{

    CriticalSectionScoped cs(_resamplerCritSect);

    if(inFreqHz == outFreqHz)
    {
        memcpy(outAudio, inAudio, (inFreqHz*numAudioChannels / 100) * sizeof(WebRtc_Word16));
        return (WebRtc_Word16)(inFreqHz / 100);
    }

    int maxLen = 480 * numAudioChannels; //max number of samples for 10ms at 48kHz
    int lengthIn = (WebRtc_Word16)(inFreqHz / 100) * numAudioChannels;
    int outLen;

    WebRtc_Word32 ret;
    ResamplerType type;
    type = (numAudioChannels == 1)? kResamplerSynchronous:kResamplerSynchronousStereo;

    ret = _resampler.ResetIfNeeded(inFreqHz,outFreqHz,type);
    if (ret < 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
            "Error in reset of resampler");
        return -1;
    }

    ret = _resampler.Push(inAudio, lengthIn, outAudio, maxLen, outLen);
    if (ret < 0 )
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
            "Error in resampler: resampler.Push");
        return -1;
    }

   WebRtc_Word16 outAudioLenSmpl = (WebRtc_Word16) outLen / numAudioChannels;

   return outAudioLenSmpl;

}

void
ACMResampler::SetUniqueId(
    WebRtc_Word32 id)
{
    CriticalSectionScoped lock(_resamplerCritSect);
    _id = id;
}

} // namespace webrtc
