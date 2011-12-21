/* Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
*
*  Use of this source code is governed by a BSD-style license
*  that can be found in the LICENSE file in the root of the source
*  tree. An additional intellectual property rights grant can be found
*  in the file PATENTS.  All contributing project authors may
*  be found in the AUTHORS file in the root of the source tree.
*/
/*
* This file defines classes for doing temporal layers with VP8.
*/
#ifndef WEBRTC_MODULES_VIDEO_CODING_CODECS_VP8_TEMPORAL_LAYERS_H_
#define WEBRTC_MODULES_VIDEO_CODING_CODECS_VP8_TEMPORAL_LAYERS_H_

#include <typedefs.h>

 // VPX forward declaration
typedef struct vpx_codec_enc_cfg vpx_codec_enc_cfg_t;

namespace webrtc {

struct CodecSpecificInfoVP8;

class TemporalLayers {
 public:
  TemporalLayers(int number_of_temporal_layers);

  // Returns the recommended VP8 encode flags needed. May refresh the decoder
  // and/or update the reference buffers.
  int EncodeFlags();

  bool ConfigureBitrates(int bitrate_kbit, vpx_codec_enc_cfg_t* cfg);

  void PopulateCodecSpecific(bool key_frame, CodecSpecificInfoVP8 *vp8_info);

 private:
  enum TemporalReferences {
    // Highest enhancement layer.
    kTemporalUpdateNone = 5,
    // Second enhancement layer.
    kTemporalUpdateAltref = 4,
    // Second enhancement layer without dependency on previous frames in
    // the second enhancement layer.
    kTemporalUpdateAltrefWithoutDependency = 3,
    // First enhancement layer.
    kTemporalUpdateGolden = 2,
    // First enhancement layer without dependency on previous frames in
    // the first enhancement layer.
    kTemporalUpdateGoldenWithoutDependency = 1,
    // Base layer.
    kTemporalUpdateLast = 0,
  };
  enum { kMaxTemporalPattern = 16 };

  int number_of_temporal_layers_;
  int temporal_ids_length_;
  int temporal_ids_[kMaxTemporalPattern];
  int temporal_pattern_length_;
  TemporalReferences temporal_pattern_[kMaxTemporalPattern];
  uint8_t tl0_pic_idx_;
  uint8_t pattern_idx_;
};
}  // namespace webrtc
#endif  // WEBRTC_MODULES_VIDEO_CODING_CODECS_VP8_TEMPORAL_LAYERS_H_

