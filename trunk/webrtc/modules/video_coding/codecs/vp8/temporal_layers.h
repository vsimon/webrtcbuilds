/* Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
*
*  Use of this source code is governed by a BSD-style license
*  that can be found in the LICENSE file in the root of the source
*  tree. An additional intellectual property rights grant can be found
*  in the file PATENTS.  All contributing project authors may
*  be found in the AUTHORS file in the root of the source tree.
*/
/*
* This file defines the interface for doing temporal layers with VP8.
*/
#ifndef WEBRTC_MODULES_VIDEO_CODING_CODECS_VP8_TEMPORAL_LAYERS_H_
#define WEBRTC_MODULES_VIDEO_CODING_CODECS_VP8_TEMPORAL_LAYERS_H_

#include "webrtc/common_video/interface/video_image.h"
#include "webrtc/typedefs.h"

// libvpx forward declaration.
typedef struct vpx_codec_enc_cfg vpx_codec_enc_cfg_t;

namespace webrtc {

struct CodecSpecificInfoVP8;

class TemporalLayers {
 public:
  virtual ~TemporalLayers() {}

  // Returns the recommended VP8 encode flags needed. May refresh the decoder
  // and/or update the reference buffers.
  virtual int EncodeFlags(uint32_t timestamp) = 0;

  virtual bool ConfigureBitrates(int bitrate_kbit,
                                 int max_bitrate_kbit,
                                 int framerate,
                                 vpx_codec_enc_cfg_t* cfg) = 0;

  virtual void PopulateCodecSpecific(bool base_layer_sync,
                                     CodecSpecificInfoVP8* vp8_info,
                                     uint32_t timestamp) = 0;

  virtual void FrameEncoded(unsigned int size, uint32_t timestamp) = 0;
};

}  // namespace webrtc
#endif  // WEBRTC_MODULES_VIDEO_CODING_CODECS_VP8_TEMPORAL_LAYERS_H_

