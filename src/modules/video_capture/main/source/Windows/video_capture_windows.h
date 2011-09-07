/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_WINDOWS_VIDEO_CAPTURE_WINDOWS_H_
#define WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_WINDOWS_VIDEO_CAPTURE_WINDOWS_H_

#include "../video_capture_impl.h"
#include <tchar.h>

#include "device_info_windows.h"

#define CAPTURE_FILTER_NAME L"VideoCaptureFilter"
#define SINK_FILTER_NAME L"SinkFilter"

namespace webrtc
{
namespace videocapturemodule
{
// Forward declaraion
class CaptureSinkFilter;

class VideoCaptureDS: public VideoCaptureImpl
{
public:

    static VideoCaptureModule* Create(const WebRtc_Word32 id,
                                      const WebRtc_UWord8* deviceUniqueIdUTF8);

    VideoCaptureDS(const WebRtc_Word32 id);

    virtual WebRtc_Word32 Init(const WebRtc_Word32 id,
                               const WebRtc_UWord8* deviceUniqueIdUTF8);

    /*************************************************************************
     *
     *   Start/Stop
     *
     *************************************************************************/
    virtual WebRtc_Word32
        StartCapture(const VideoCaptureCapability& capability);
    virtual WebRtc_Word32 StopCapture();

    /**************************************************************************
     *
     *   Properties of the set device
     *
     **************************************************************************/

    virtual bool CaptureStarted();
    virtual WebRtc_Word32 CaptureSettings(VideoCaptureCapability& settings);

protected:
    virtual ~VideoCaptureDS();

    // Help functions

    WebRtc_Word32
        SetCameraOutput(const VideoCaptureCapability& requestedCapability);
    WebRtc_Word32 DisconnectGraph();
    HRESULT VideoCaptureDS::ConnectDVCamera();

    DeviceInfoWindows _dsInfo;

    IBaseFilter* _captureFilter;
    IGraphBuilder* _graphBuilder;
    IMediaControl* _mediaControl;
    CaptureSinkFilter* _sinkFilter;
    IPin* _inputSendPin;
    IPin* _outputCapturePin;

    // used when using a MJPEG decoder
    IBaseFilter* _mjpgJPGFilter;
    IPin* _inputMjpgPin;
    IPin* _outputMjpgPin;

    // Microsoft DV interface (external DV cameras)
    IBaseFilter* _dvFilter;
    IPin* _inputDvPin;
    IPin* _outputDvPin;

};
} // namespace videocapturemodule
} //namespace webrtc
#endif // WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_WINDOWS_VIDEO_CAPTURE_WINDOWS_H_
