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

#include "webrtc/typedefs.h"

namespace webrtc {

enum { kMaxNumberOfFrames     = 300 };
enum { kStartNumberOfFrames   = 6 };
enum { kMaxVideoDelayMs       = 2000 };

enum VCMJitterBufferEnum {
  kMaxConsecutiveOldFrames        = 60,
  kMaxConsecutiveOldPackets       = 300,
  kMaxPacketsInSession            = 800,
  kBufferIncStepSizeBytes         = 30000,   // >20 packets.
  kMaxJBFrameSizeBytes            = 4000000  // sanity don't go above 4Mbyte.
};

enum VCMFrameBufferEnum {
  kStateError           = -4,
  kFlushIndicator       = -3,   // Indicator that a flush has occurred.
  kTimeStampError       = -2,
  kSizeError            = -1,
  kNoError              = 0,
  kIncomplete           = 1,    // Frame incomplete.
  kFirstPacket          = 2,
  kCompleteSession      = 3,    // at least one layer in the frame complete.
  kDecodableSession     = 4,    // Frame incomplete, but ready to be decoded
  kDuplicatePacket      = 5     // We're receiving a duplicate packet.
};

enum VCMFrameBufferStateEnum {
  kStateFree,               // Unused frame in the JB
  kStateEmpty,              // frame popped by the RTP receiver
  kStateIncomplete,         // frame that have one or more packet(s) stored
  kStateComplete,           // frame that have all packets
  kStateDecoding,           // frame popped by the decoding thread
  kStateDecodable           // Hybrid mode - frame can be decoded
};

enum { kH264StartCodeLengthBytes = 4};

// Used to indicate if a received packet contain a complete NALU (or equivalent)
enum VCMNaluCompleteness {
  kNaluUnset = 0,       // Packet has not been filled.
  kNaluComplete = 1,    // Packet can be decoded as is.
  kNaluStart,           // Packet contain beginning of NALU
  kNaluIncomplete,      // Packet is not beginning or end of NALU
  kNaluEnd,             // Packet is the end of a NALU
};

// Returns the latest of the two timestamps, compensating for wrap arounds.
// This function assumes that the two timestamps are close in time.
uint32_t LatestTimestamp(uint32_t timestamp1,
                               uint32_t timestamp2,
                               bool* has_wrapped);

// Returns the latest of the two sequence numbers, compensating for wrap
// arounds. This function assumes that the two sequence numbers are close in
// time.
int32_t LatestSequenceNumber(int32_t seq_num1,
                                   int32_t seq_num2,
                                   bool* has_wrapped);
}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_JITTER_BUFFER_COMMON_H_
