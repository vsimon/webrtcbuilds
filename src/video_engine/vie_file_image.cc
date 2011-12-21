/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Placed first to get WEBRTC_VIDEO_ENGINE_FILE_API.
#include "engine_configurations.h"

#ifdef WEBRTC_VIDEO_ENGINE_FILE_API

#include "video_engine/vie_file_image.h"

#include <stdio.h>

#include "common_video/interface/video_image.h"
#include "common_video/jpeg/main/interface/jpeg.h"
#include "system_wrappers/interface/trace.h"

namespace webrtc {

int ViEFileImage::ConvertJPEGToVideoFrame(int engine_id,
                                          const char* file_nameUTF8,
                                          VideoFrame& video_frame) {
  // Read jpeg file into temporary buffer.
  EncodedImage image_buffer;

  FILE* image_file = fopen(file_nameUTF8, "rb");
  if (!image_file) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, engine_id,
                 "%s could not open file %s", __FUNCTION__, file_nameUTF8);
    return -1;
  }
  fseek(image_file, 0, SEEK_END);
  image_buffer._size = ftell(image_file);
  fseek(image_file, 0, SEEK_SET);
  image_buffer._buffer = new WebRtc_UWord8[ image_buffer._size + 1];
  if (image_buffer._size != fread(image_buffer._buffer, sizeof(WebRtc_UWord8),
                                  image_buffer._size, image_file)) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, engine_id,
                 "%s could not read file %s", __FUNCTION__, file_nameUTF8);
    delete [] image_buffer._buffer;
    return -1;
  }
  fclose(image_file);

  JpegDecoder decoder;
  RawImage decoded_image;
  int ret = decoder.Decode(image_buffer, decoded_image);

  delete [] image_buffer._buffer;
  image_buffer._buffer = NULL;

  if (ret == -1) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, engine_id,
                 "%s could decode file %s from jpeg format", __FUNCTION__,
                 file_nameUTF8);
    return -1;
  } else if (ret == -3) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, engine_id,
                 "%s could not convert jpeg's data to i420 format",
                 __FUNCTION__, file_nameUTF8);
  }

  // Image length in I420.
  WebRtc_UWord32 image_length = (WebRtc_UWord32)(decoded_image._width *
                                                 decoded_image._height * 1.5);
  if (-1 == video_frame.Swap(decoded_image._buffer, image_length,
                             image_length)) {
    WEBRTC_TRACE(kTraceDebug, kTraceVideo, engine_id,
                 "%s could not copy frame image_decoded_buffer to video_frame ",
                 __FUNCTION__, file_nameUTF8);
    return -1;
  }

  if (decoded_image._buffer) {
    delete [] decoded_image._buffer;
    decoded_image._buffer = NULL;
  }

  video_frame.SetWidth(decoded_image._width);
  video_frame.SetHeight(decoded_image._height);
  return 0;
}

int ViEFileImage::ConvertPictureToVideoFrame(int engine_id,
                                             const ViEPicture& picture,
                                             VideoFrame& video_frame) {
  WebRtc_UWord32 picture_length = (WebRtc_UWord32)(picture.width *
                                                   picture.height * 1.5);
  video_frame.CopyFrame(picture_length, picture.data);
  video_frame.SetWidth(picture.width);
  video_frame.SetHeight(picture.height);
  video_frame.SetLength(picture_length);
  return 0;
}

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_FILE_API
