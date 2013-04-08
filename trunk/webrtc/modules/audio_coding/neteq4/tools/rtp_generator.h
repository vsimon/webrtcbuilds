/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_NETEQ4_TOOLS_RTP_GENERATOR_H_
#define WEBRTC_MODULES_AUDIO_CODING_NETEQ4_TOOLS_RTP_GENERATOR_H_

#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/system_wrappers/interface/constructor_magic.h"
#include "webrtc/typedefs.h"

namespace webrtc {
namespace test {

// Class for generating RTP headers.
class RtpGenerator {
 public:
  RtpGenerator(int samples_per_ms,
               uint16_t start_seq_number = 0,
               uint32_t start_timestamp = 0,
               uint32_t start_send_time_ms = 0,
               uint32_t ssrc = 0x12345678)
      : seq_number_(start_seq_number),
        timestamp_(start_timestamp),
        next_send_time_ms_(start_send_time_ms),
        ssrc_(ssrc),
        samples_per_ms_(samples_per_ms) {
  }

  // Writes the next RTP header to |rtp_header|, which will be of type
  // |payload_type|. Returns the send time for this packet (in ms). The value of
  // |payload_length_samples| determines the send time for the next packet.
  uint32_t GetRtpHeader(uint8_t payload_type, size_t payload_length_samples,
                        WebRtcRTPHeader* rtp_header);

 private:
  uint16_t seq_number_;
  uint32_t timestamp_;
  uint32_t next_send_time_ms_;
  const uint32_t ssrc_;
  const int samples_per_ms_;
  DISALLOW_COPY_AND_ASSIGN(RtpGenerator);
};

}  // namespace test
}  // namespace webrtc
#endif  // WEBRTC_MODULES_AUDIO_CODING_NETEQ4_TOOLS_RTP_GENERATOR_H_
