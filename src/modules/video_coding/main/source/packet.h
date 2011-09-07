/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_PACKET_H_
#define WEBRTC_MODULES_VIDEO_CODING_PACKET_H_

#include "typedefs.h"
#include "module_common_types.h"
#include "jitter_buffer_common.h"

namespace webrtc
{

class VCMPacket
{
public:
    VCMPacket(const WebRtc_UWord8* ptr,
                   const WebRtc_UWord32 size,
                   const WebRtcRTPHeader& rtpHeader);
    VCMPacket(const WebRtc_UWord8* ptr,
                   WebRtc_UWord32 size,
                   WebRtc_UWord16 seqNum,
                   WebRtc_UWord32 timestamp,
                   bool markerBit);

    WebRtc_UWord8           payloadType;
    WebRtc_UWord32          timestamp;
    WebRtc_UWord16          seqNum;
    const WebRtc_UWord8*    dataPtr;
    WebRtc_UWord32          sizeBytes;
    bool                    markerBit;

    FrameType               frameType;
    webrtc::VideoCodecType  codec;

    bool isFirstPacket;                 // Is this first packet in a frame.
    VCMNaluCompleteness completeNALU;   // Default is kNaluIncomplete.
    bool insertStartCode;               // True if a start code should be inserted before this
                                        // packet.
    bool bits;                          // The first bits of this packets are zero and the
                                        // first
                                        // byte should be ORed with the last packet of the
                                        // previous frame.
    const RTPVideoHeader *codecSpecificHeader;

protected:
    void CopyCodecSpecifics(const RTPVideoHeader& videoHeader);
};

} // namespace webrtc

#endif // WEBRTC_MODULES_VIDEO_CODING_PACKET_H_
