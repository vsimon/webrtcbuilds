/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_coding.h"
#include "trace.h"
#include "generic_decoder.h"
#include "internal_defines.h"
#include "tick_time_base.h"

namespace webrtc {

VCMDecodedFrameCallback::VCMDecodedFrameCallback(VCMTiming& timing,
                                                 TickTimeBase* clock)
:
_critSect(CriticalSectionWrapper::CreateCriticalSection()),
_clock(clock),
_receiveCallback(NULL),
_timing(timing),
_timestampMap(kDecoderFrameMemoryLength)
{
}

VCMDecodedFrameCallback::~VCMDecodedFrameCallback()
{
    delete _critSect;
}

void VCMDecodedFrameCallback::SetUserReceiveCallback(
    VCMReceiveCallback* receiveCallback)
{
    CriticalSectionScoped cs(_critSect);
    _receiveCallback = receiveCallback;
}

WebRtc_Word32 VCMDecodedFrameCallback::Decoded(RawImage& decodedImage)
{
    // TODO(holmer): We should improve this so that we can handle multiple
    // callbacks from one call to Decode().
    CriticalSectionScoped cs(_critSect);
    VCMFrameInformation* frameInfo = static_cast<VCMFrameInformation*>(
        _timestampMap.Pop(decodedImage._timeStamp));
    if (frameInfo == NULL)
    {
        // The map should never be empty or full if this callback is called.
        return WEBRTC_VIDEO_CODEC_ERROR;
    }

    _timing.StopDecodeTimer(
        decodedImage._timeStamp,
        frameInfo->decodeStartTimeMs,
        _clock->MillisecondTimestamp());

    if (_receiveCallback != NULL)
    {
        _frame.Swap(decodedImage._buffer,
                    decodedImage._length,
                    decodedImage._size);
        _frame.SetWidth(decodedImage._width);
        _frame.SetHeight(decodedImage._height);
        _frame.SetTimeStamp(decodedImage._timeStamp);
        _frame.SetRenderTime(frameInfo->renderTimeMs);
        // Convert raw image to video frame
        WebRtc_Word32 callbackReturn = _receiveCallback->FrameToRender(_frame);
        if (callbackReturn < 0)
        {
            WEBRTC_TRACE(webrtc::kTraceDebug,
                         webrtc::kTraceVideoCoding,
                         -1,
                         "Render callback returned error: %d", callbackReturn);
        }
    }
    return WEBRTC_VIDEO_CODEC_OK;
}

WebRtc_Word32
VCMDecodedFrameCallback::ReceivedDecodedReferenceFrame(
    const WebRtc_UWord64 pictureId)
{
    CriticalSectionScoped cs(_critSect);
    if (_receiveCallback != NULL)
    {
        return _receiveCallback->ReceivedDecodedReferenceFrame(pictureId);
    }
    return -1;
}

WebRtc_Word32
VCMDecodedFrameCallback::ReceivedDecodedFrame(const WebRtc_UWord64 pictureId)
{
    _lastReceivedPictureID = pictureId;
    return 0;
}

WebRtc_UWord64 VCMDecodedFrameCallback::LastReceivedPictureID() const
{
    return _lastReceivedPictureID;
}

WebRtc_Word32 VCMDecodedFrameCallback::Map(WebRtc_UWord32 timestamp, VCMFrameInformation* frameInfo)
{
    CriticalSectionScoped cs(_critSect);
    return _timestampMap.Add(timestamp, frameInfo);
}

WebRtc_Word32 VCMDecodedFrameCallback::Pop(WebRtc_UWord32 timestamp)
{
    CriticalSectionScoped cs(_critSect);
    if (_timestampMap.Pop(timestamp) == NULL)
    {
        return VCM_GENERAL_ERROR;
    }
    return VCM_OK;
}

VCMGenericDecoder::VCMGenericDecoder(VideoDecoder& decoder, WebRtc_Word32 id, bool isExternal)
:
_id(id),
_callback(NULL),
_frameInfos(),
_nextFrameInfoIdx(0),
_decoder(decoder),
_codecType(kVideoCodecUnknown),
_isExternal(isExternal),
_requireKeyFrame(false),
_keyFrameDecoded(false)
{
}

VCMGenericDecoder::~VCMGenericDecoder()
{
}

WebRtc_Word32 VCMGenericDecoder::InitDecode(const VideoCodec* settings,
                                            WebRtc_Word32 numberOfCores,
                                            bool requireKeyFrame)
{
    _requireKeyFrame = requireKeyFrame;
    _keyFrameDecoded = false;
    _codecType = settings->codecType;

    return _decoder.InitDecode(settings, numberOfCores);
}

WebRtc_Word32 VCMGenericDecoder::Decode(const VCMEncodedFrame& frame,
                                        int64_t nowMs)
{
    if (_requireKeyFrame &&
        !_keyFrameDecoded &&
        frame.FrameType() != kVideoFrameKey &&
        frame.FrameType() != kVideoFrameGolden)
    {
        // Require key frame is enabled, meaning that one key frame must be decoded
        // before we can decode delta frames.
        return VCM_CODEC_ERROR;
    }
    _frameInfos[_nextFrameInfoIdx].decodeStartTimeMs = nowMs;
    _frameInfos[_nextFrameInfoIdx].renderTimeMs = frame.RenderTimeMs();
    _callback->Map(frame.TimeStamp(), &_frameInfos[_nextFrameInfoIdx]);

    WEBRTC_TRACE(webrtc::kTraceDebug,
                 webrtc::kTraceVideoCoding,
                 VCMId(_id),
                 "Decoding timestamp %u", frame.TimeStamp());

    _nextFrameInfoIdx = (_nextFrameInfoIdx + 1) % kDecoderFrameMemoryLength;

    WebRtc_Word32 ret = _decoder.Decode(frame.EncodedImage(),
                                        frame.MissingFrame(),
                                        frame.FragmentationHeader(),
                                        frame.CodecSpecific(),
                                        frame.RenderTimeMs());

    if (ret < WEBRTC_VIDEO_CODEC_OK)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCoding, VCMId(_id), "Decoder error: %d\n", ret);
        _callback->Pop(frame.TimeStamp());
        return ret;
    }
    else if (ret == WEBRTC_VIDEO_CODEC_NO_OUTPUT ||
             ret == WEBRTC_VIDEO_CODEC_REQUEST_SLI)
    {
        // No output
        _callback->Pop(frame.TimeStamp());
    }
    // Update the key frame decoded variable so that we know whether or not we've decoded a key frame since reset.
    _keyFrameDecoded = (frame.FrameType() == kVideoFrameKey || frame.FrameType() == kVideoFrameGolden);
    return ret;
}

WebRtc_Word32
VCMGenericDecoder::Release()
{
    _keyFrameDecoded = false;
    return _decoder.Release();
}

WebRtc_Word32 VCMGenericDecoder::Reset()
{
    _keyFrameDecoded = false;
    return _decoder.Reset();
}

WebRtc_Word32 VCMGenericDecoder::SetCodecConfigParameters(const WebRtc_UWord8* buffer, WebRtc_Word32 size)
{
    return _decoder.SetCodecConfigParameters(buffer, size);
}

WebRtc_Word32 VCMGenericDecoder::RegisterDecodeCompleteCallback(VCMDecodedFrameCallback* callback)
{
    _callback = callback;
    return _decoder.RegisterDecodeCompleteCallback(callback);
}

bool VCMGenericDecoder::External() const
{
    return _isExternal;
}

} // namespace
