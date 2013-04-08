/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_VIDEO_ENGINE_TEST_LIBVIETEST_INCLUDE_VIE_EXTERNAL_RENDER_H_
#define WEBRTC_VIDEO_ENGINE_TEST_LIBVIETEST_INCLUDE_VIE_EXTERNAL_RENDER_H_

#include "webrtc/system_wrappers/interface/tick_util.h"
#include "webrtc/video_engine/include/vie_render.h"

namespace webrtc {

// A render filter which passes frames directly to an external renderer. This
// is different from plugging the external renderer directly into the sending
// side since this will only run on frames that actually get sent and not on
// frames that only get captured.
class ExternalRendererEffectFilter : public webrtc::ViEEffectFilter {
 public:
  explicit ExternalRendererEffectFilter(webrtc::ExternalRenderer* renderer)
      : width_(0), height_(0), renderer_(renderer) {}
  virtual ~ExternalRendererEffectFilter() {}
  virtual int Transform(int size, unsigned char* frame_buffer,
                        unsigned int time_stamp90KHz, unsigned int width,
                        unsigned int height) {
    if (width != width_ || height_ != height) {
      renderer_->FrameSizeChange(width, height, 1);
      width_ = width;
      height_ = height;
    }
    return renderer_->DeliverFrame(frame_buffer,
                                   size,
                                   time_stamp90KHz,
                                   webrtc::TickTime::MillisecondTimestamp());
  }

 private:
  unsigned int width_;
  unsigned int height_;
  webrtc::ExternalRenderer* renderer_;
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_TEST_LIBVIETEST_INCLUDE_VIE_EXTERNAL_RENDER_H_
