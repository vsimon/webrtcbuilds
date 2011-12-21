/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_video/libyuv/include/scaler.h"

// LibYuv
#ifdef WEBRTC_ANDROID
#include "libyuv/files/include/libyuv.h"
#else
#include "third_party/libyuv/include/libyuv.h"
#endif

namespace webrtc {

Scaler::Scaler()
    : method_(kScaleBox),
      src_width_(0),
      src_height_(0),
      dst_width_(0),
      dst_height_(0),
      set_(false) {}

Scaler::~Scaler() {}

int Scaler::Set(int src_width, int src_height,
                int dst_width, int dst_height,
                VideoType src_video_type, VideoType dst_video_type,
                ScaleMethod method) {
  set_ = false;
  if (src_width < 1 || src_height < 1 || dst_width < 1 || dst_height < 1)
    return -1;

  if (!SupportedVideoType(src_video_type, dst_video_type))
    return -1;

  src_width_ = src_width;
  src_height_ = src_height;
  dst_width_ = dst_width;
  dst_height_ = dst_height;
  method_ = method;
  set_ = true;
  return 0;
}

int Scaler::Scale(const uint8_t* src_frame,
                  uint8_t*& dst_frame,
                  int& dst_size) {
  if (src_frame == NULL)
    return -1;
  if (!set_)
    return -2;

  // Making sure that destination frame is of sufficient size
  int required_dst_size = dst_width_ * dst_height_ * 3 / 2;
  if (dst_frame && required_dst_size > dst_size) {
    // allocated buffer is too small
    delete [] dst_frame;
    dst_frame = NULL;
  }
  if (dst_frame == NULL) {
    dst_frame = new uint8_t[required_dst_size];
    dst_size = required_dst_size;
  }

  // Converting to planes:
  const uint8_t* src_yplane = src_frame;
  const uint8_t* src_uplane = src_frame + src_width_ * src_height_;
  const uint8_t* src_vplane = src_uplane + src_width_ * src_height_ / 4;

  uint8_t* dst_yplane = dst_frame;
  uint8_t* dst_uplane = dst_frame + dst_width_ * dst_height_;
  uint8_t* dst_vplane = dst_uplane + dst_width_ * dst_height_ / 4;

  return libyuv::I420Scale(src_yplane, src_width_,
                           src_uplane, src_width_ / 2,
                           src_vplane, src_width_ / 2,
                           src_width_, src_height_,
                           dst_yplane, dst_width_,
                           dst_uplane, dst_width_ / 2,
                           dst_vplane, dst_width_ / 2,
                           dst_width_, dst_height_,
                           libyuv::FilterMode(method_));
}

// TODO(mikhal): Add support for more types.
bool Scaler::SupportedVideoType(VideoType src_video_type,
                                VideoType dst_video_type) {
  if (src_video_type != dst_video_type)
    return false;

  if ((src_video_type == kI420) || (src_video_type == kIYUV) ||
      (src_video_type == kYV12))
    return true;

  return false;
}

}  // namespace webrtc
