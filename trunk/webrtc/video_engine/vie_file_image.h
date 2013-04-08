/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_VIE_FILE_IMAGE_H_
#define WEBRTC_VIDEO_ENGINE_VIE_FILE_IMAGE_H_

#include "common_video/interface/i420_video_frame.h"
#include "typedefs.h"  // NOLINT
#include "video_engine/include/vie_file.h"

namespace webrtc {

class ViEFileImage {
 public:
  static int ConvertJPEGToVideoFrame(int engine_id,
                                     const char* file_nameUTF8,
                                     I420VideoFrame* video_frame);
  static int ConvertPictureToI420VideoFrame(int engine_id,
                                            const ViEPicture& picture,
                                            I420VideoFrame* video_frame);
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_VIE_FILE_IMAGE_H_
