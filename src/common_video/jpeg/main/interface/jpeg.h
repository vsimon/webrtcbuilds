/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * JPEG wrapper
 */

#ifndef WEBRTC_COMMON_VIDEO_JPEG
#define WEBRTC_COMMON_VIDEO_JPEG

#include "typedefs.h"
#include "video_image.h"

// jpeg forward declaration
struct jpeg_compress_struct;
struct jpeg_decompress_struct;

namespace webrtc
{

class JpegEncoder
{
public:
    JpegEncoder();
    ~JpegEncoder();

// SetFileName
// Input:
//  - fileName - Pointer to input vector (should be less than 256) to which the
//               compressed  file will be written to
//    Output:
//    - 0             : OK
//    - (-1)          : Error
    WebRtc_Word32 SetFileName(const WebRtc_Word8* fileName);

// Encode an I420 image. The encoded image is saved to a file
//
// Input:
//          - inputImage        : Image to be encoded
//
//    Output:
//    - 0             : OK
//    - (-1)          : Error
    WebRtc_Word32 Encode(const RawImage& inputImage);

private:

    jpeg_compress_struct*   _cinfo;
    WebRtc_Word8            _fileName[256];
};

class JpegDecoder
{
 public:
    JpegDecoder();
    ~JpegDecoder();

// Decodes a JPEG-stream
// Supports 1 image component. 3 interleaved image components,
// YCbCr sub-sampling  4:4:4, 4:2:2, 4:2:0.
//
// Input:
//    - inputImage        : encoded image to be decoded.
//    - outputImage       : RawImage to store decoded output
//
//    Output:
//    - 0             : OK
//    - (-1)          : Error
    WebRtc_Word32 Decode(const EncodedImage& inputImage,
                         RawImage& outputImage);
 private:
    jpeg_decompress_struct*    _cinfo;
};


}
#endif /* WEBRTC_COMMON_VIDEO_JPEG  */
