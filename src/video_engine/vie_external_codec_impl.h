/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_VIE_EXTERNAL_CODEC_IMPL_H_
#define WEBRTC_VIDEO_ENGINE_VIE_EXTERNAL_CODEC_IMPL_H_

#include "video_engine/include/vie_external_codec.h"
#include "video_engine/vie_ref_count.h"
#include "video_engine/vie_shared_data.h"

namespace webrtc {

class ViEExternalCodecImpl
    : public virtual ViESharedData,
      public ViEExternalCodec,
      public ViERefCount {
 public:
  // Implements ViEExternalCodec.
  virtual int Release();
  virtual int RegisterExternalSendCodec(const int video_channel,
                                        const unsigned char pl_type,
                                        VideoEncoder* encoder);
  virtual int DeRegisterExternalSendCodec(const int video_channel,
                                          const unsigned char pl_type);
  virtual int RegisterExternalReceiveCodec(const int video_channel,
                                           const unsigned int pl_type,
                                           VideoDecoder* decoder,
                                           bool decoder_render = false,
                                           int render_delay = 0);
  virtual int DeRegisterExternalReceiveCodec(const int video_channel,
                                             const unsigned char pl_type);
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_VIE_EXTERNAL_CODEC_IMPL_H_
