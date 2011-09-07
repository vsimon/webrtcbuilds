/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_JITTER_BUFFER_COMMON_H_
#define WEBRTC_MODULES_VIDEO_CODING_JITTER_BUFFER_COMMON_H_

namespace webrtc
{

enum { kMaxNumberOfFrames     = 100 };
enum { kStartNumberOfFrames   = 6 };    // in packets, 6 packets are approximately 198 ms,
                                        // we need at least one more for process
enum { kMaxVideoDelayMs       = 2000 }; // in ms

enum VCMJitterBufferEnum
{
    kMaxConsecutiveOldFrames        = 60,
    kMaxConsecutiveOldPackets       = 300,
    kMaxPacketsInJitterBuffer       = 800,
    kBufferIncStepSizeBytes         = 30000,       // >20 packets
    kMaxJBFrameSizeBytes            = 4000000      // sanity don't go above 4Mbyte
};

enum VCMFrameBufferEnum
{
    kStateError           = -4,
    kTimeStampError       = -2,
    kSizeError            = -1,
    kNoError              = 0,
    kIncomplete           = 1,    // Frame incomplete
    kFirstPacket          = 2,
    kCompleteSession      = 3,    // at least one layer in the frame complete
    kDuplicatePacket      = 5     // We're receiving a duplicate packet.
};

enum VCMFrameBufferStateEnum
{
    kStateFree,               // Unused frame in the JB
    kStateEmpty,              // frame popped by the RTP receiver
    kStateIncomplete,         // frame that have one or more packet(s) stored
    kStateComplete,           // frame that have all packets
    kStateDecoding,           // frame popped by the decoding thread
    kStateDecodable           // Hybrid mode - frame can be decoded
};

enum { kH264StartCodeLengthBytes = 4};

// Used to indicate if a received packet contain a complete NALU (or equivalent)
enum VCMNaluCompleteness
{
    kNaluUnset = 0,       //Packet has not been filled.
    kNaluComplete = 1,    //Packet can be decoded as is.
    kNaluStart,           // Packet contain beginning of NALU
    kNaluIncomplete,      //Packet is not beginning or end of NALU
    kNaluEnd,             // Packet is the end of a NALU
};

} // namespace webrtc

#endif // WEBRTC_MODULES_VIDEO_CODING_JITTER_BUFFER_COMMON_H_
