/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_TMMBR_HELP_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_TMMBR_HELP_H_

#include "typedefs.h"

#include "critical_section_wrapper.h"

#ifndef NULL
    #define NULL    0
#endif

namespace webrtc {
class TMMBRSet
{
public:
    TMMBRSet();
    ~TMMBRSet();

    void VerifyAndAllocateSet(WebRtc_UWord32 minimumSize);

    WebRtc_UWord32*   ptrTmmbrSet;
    WebRtc_UWord32*   ptrPacketOHSet;
    WebRtc_UWord32*   ptrSsrcSet;
    WebRtc_UWord32    sizeOfSet;
    WebRtc_UWord32    lengthOfSet;
};

class TMMBRHelp
{
public:
    TMMBRHelp(const bool audio);
    virtual ~TMMBRHelp();

    TMMBRSet* BoundingSet(); // used for debuging
    TMMBRSet* CandidateSet();
    TMMBRSet* BoundingSetToSend();

    TMMBRSet* VerifyAndAllocateCandidateSet(const WebRtc_UWord32 minimumSize);
    WebRtc_Word32 FindTMMBRBoundingSet(TMMBRSet*& boundingSet);
    WebRtc_Word32 SetTMMBRBoundingSetToSend(const TMMBRSet* boundingSetToSend,
                                          const WebRtc_UWord32 maxBitrateKbit);

    bool        IsOwner(const WebRtc_UWord32 ssrc,
                        const WebRtc_UWord32 length) const;

    WebRtc_Word32 CalcMinMaxBitRate(const WebRtc_UWord32 totalPacketRate,
                                  const WebRtc_UWord32 lengthOfBoundingSet,
                                  WebRtc_UWord32& minBitrateKbit,
                                  WebRtc_UWord32& maxBitrateKbit) const;

protected:
    TMMBRSet*   VerifyAndAllocateBoundingSet(WebRtc_UWord32 minimumSize);
    WebRtc_Word32 VerifyAndAllocateBoundingSetToSend(WebRtc_UWord32 minimumSize);

    WebRtc_Word32 FindTMMBRBoundingSet(WebRtc_Word32 numCandidates, TMMBRSet& candidateSet);

private:
    CriticalSectionWrapper& _criticalSection;
    const bool              _audio;
    TMMBRSet                _candidateSet;
    TMMBRSet                _boundingSet;
    TMMBRSet                _boundingSetToSend;

    float*                  _ptrIntersectionBoundingSet;
    float*                  _ptrMaxPRBoundingSet;
};
} // namespace webrtc

#endif // WEBRTC_MODULES_RTP_RTCP_SOURCE_TMMBR_HELP_H_
