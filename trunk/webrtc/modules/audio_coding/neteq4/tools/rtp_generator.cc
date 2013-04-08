/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>

#include "webrtc/modules/audio_coding/neteq4/tools/rtp_generator.h"

namespace webrtc {
namespace test {

uint32_t RtpGenerator::GetRtpHeader(uint8_t payload_type,
                                    size_t payload_length_samples,
                                    WebRtcRTPHeader* rtp_header) {
  assert(rtp_header);
  if (!rtp_header) {
    return 0;
  }
  rtp_header->header.sequenceNumber = seq_number_++;
  rtp_header->header.timestamp = timestamp_;
  timestamp_ += payload_length_samples;
  rtp_header->header.payloadType = payload_type;
  rtp_header->header.markerBit = false;
  rtp_header->header.ssrc = ssrc_;
  rtp_header->header.numCSRCs = 0;
  rtp_header->frameType = kAudioFrameSpeech;

  uint32_t this_send_time = next_send_time_ms_;
  assert(samples_per_ms_ > 0);
  next_send_time_ms_ += payload_length_samples / samples_per_ms_;
  return this_send_time;
}

}  // namespace test
}  // namespace webrtc
