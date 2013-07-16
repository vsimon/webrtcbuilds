/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/main/source/frame_buffer.h"
#include "webrtc/modules/video_coding/main/source/packet.h"

#include <cassert>
#include <string.h>

namespace webrtc {

VCMFrameBuffer::VCMFrameBuffer()
  :
    _state(kStateEmpty),
    _frameCounted(false),
    _nackCount(0),
    _latestPacketTimeMs(-1) {
}

VCMFrameBuffer::~VCMFrameBuffer() {
}

VCMFrameBuffer::VCMFrameBuffer(VCMFrameBuffer& rhs)
:
VCMEncodedFrame(rhs),
_state(rhs._state),
_frameCounted(rhs._frameCounted),
_sessionInfo(),
_nackCount(rhs._nackCount),
_latestPacketTimeMs(rhs._latestPacketTimeMs)
{
    _sessionInfo = rhs._sessionInfo;
    _sessionInfo.UpdateDataPointers(rhs._buffer, _buffer);
}

webrtc::FrameType
VCMFrameBuffer::FrameType() const
{
    return _sessionInfo.FrameType();
}

int32_t
VCMFrameBuffer::GetLowSeqNum() const
{
    return _sessionInfo.LowSequenceNumber();
}

int32_t
VCMFrameBuffer::GetHighSeqNum() const
{
    return _sessionInfo.HighSequenceNumber();
}

int VCMFrameBuffer::PictureId() const {
  return _sessionInfo.PictureId();
}

int VCMFrameBuffer::TemporalId() const {
  return _sessionInfo.TemporalId();
}

bool VCMFrameBuffer::LayerSync() const {
  return _sessionInfo.LayerSync();
}

int VCMFrameBuffer::Tl0PicId() const {
  return _sessionInfo.Tl0PicId();
}

bool VCMFrameBuffer::NonReference() const {
  return _sessionInfo.NonReference();
}

bool
VCMFrameBuffer::IsSessionComplete() const
{
    return _sessionInfo.complete();
}

// Insert packet
VCMFrameBufferEnum
VCMFrameBuffer::InsertPacket(const VCMPacket& packet, int64_t timeInMs,
                             bool enableDecodableState, uint32_t rttMS)
{
    // is this packet part of this frame
    if (TimeStamp() && (TimeStamp() != packet.timestamp))
    {
        return kTimeStampError;
    }

    // sanity checks
    if (_size + packet.sizeBytes +
        (packet.insertStartCode ?  kH264StartCodeLengthBytes : 0 )
        > kMaxJBFrameSizeBytes)
    {
        return kSizeError;
    }
    if (NULL == packet.dataPtr && packet.sizeBytes > 0)
    {
        return kSizeError;
    }
    if (packet.dataPtr != NULL)
    {
        _payloadType = packet.payloadType;
    }

    if (kStateEmpty == _state)
    {
        // First packet (empty and/or media) inserted into this frame.
        // store some info and set some initial values.
        _timeStamp = packet.timestamp;
        _codec = packet.codec;
        if (packet.frameType != kFrameEmpty)
        {
            // first media packet
            SetState(kStateIncomplete);
        }
    }

    uint32_t requiredSizeBytes = Length() + packet.sizeBytes +
                   (packet.insertStartCode ? kH264StartCodeLengthBytes : 0);
    if (requiredSizeBytes >= _size)
    {
        const uint8_t* prevBuffer = _buffer;
        const uint32_t increments = requiredSizeBytes /
                                          kBufferIncStepSizeBytes +
                                        (requiredSizeBytes %
                                         kBufferIncStepSizeBytes > 0);
        const uint32_t newSize = _size +
                                       increments * kBufferIncStepSizeBytes;
        if (newSize > kMaxJBFrameSizeBytes)
        {
            return kSizeError;
        }
        if (VerifyAndAllocate(newSize) == -1)
        {
            return kSizeError;
        }
        _sessionInfo.UpdateDataPointers(prevBuffer, _buffer);
    }

    if (packet.width > 0 && packet.height > 0) {
      _encodedWidth = packet.width;
      _encodedHeight = packet.height;
    }

    CopyCodecSpecific(&packet.codecSpecificHeader);

    int retVal = _sessionInfo.InsertPacket(packet, _buffer,
                                           enableDecodableState,
                                           rttMS);
    if (retVal == -1)
    {
        return kSizeError;
    }
    else if (retVal == -2)
    {
        return kDuplicatePacket;
    }
    // update length
    _length = Length() + static_cast<uint32_t>(retVal);

    _latestPacketTimeMs = timeInMs;

    if (_sessionInfo.complete()) {
      SetState(kStateComplete);
      return kCompleteSession;
    } else if (_sessionInfo.decodable()) {
      SetState(kStateDecodable);
      return kDecodableSession;
    }
    return kIncomplete;
}

int64_t
VCMFrameBuffer::LatestPacketTimeMs() const
{
    return _latestPacketTimeMs;
}

void
VCMFrameBuffer::IncrementNackCount()
{
    _nackCount++;
}

int16_t
VCMFrameBuffer::GetNackCount() const
{
    return _nackCount;
}

bool
VCMFrameBuffer::HaveFirstPacket() const
{
    return _sessionInfo.HaveFirstPacket();
}

bool
VCMFrameBuffer::HaveLastPacket() const
{
    return _sessionInfo.HaveLastPacket();
}

void
VCMFrameBuffer::Reset()
{
    _length = 0;
    _timeStamp = 0;
    _sessionInfo.Reset();
    _frameCounted = false;
    _payloadType = 0;
    _nackCount = 0;
    _latestPacketTimeMs = -1;
    _state = kStateEmpty;
    VCMEncodedFrame::Reset();
}

// Set state of frame
void
VCMFrameBuffer::SetState(VCMFrameBufferStateEnum state)
{
    if (_state == state)
    {
        return;
    }
    switch (state)
    {
    case kStateIncomplete:
        // we can go to this state from state kStateEmpty
        assert(_state == kStateEmpty);

        // Do nothing, we received a packet
        break;

    case kStateComplete:
        assert(_state == kStateEmpty ||
               _state == kStateIncomplete ||
               _state == kStateDecodable);

        break;

    case kStateEmpty:
        // Should only be set to empty through Reset().
        assert(false);
        break;

    case kStateDecodable:
        assert(_state == kStateEmpty ||
               _state == kStateIncomplete);
        break;
    }
    _state = state;
}

int32_t
VCMFrameBuffer::ExtractFromStorage(const EncodedVideoData& frameFromStorage)
{
    _frameType = ConvertFrameType(frameFromStorage.frameType);
    _timeStamp = frameFromStorage.timeStamp;
    _payloadType = frameFromStorage.payloadType;
    _encodedWidth = frameFromStorage.encodedWidth;
    _encodedHeight = frameFromStorage.encodedHeight;
    _missingFrame = frameFromStorage.missingFrame;
    _completeFrame = frameFromStorage.completeFrame;
    _renderTimeMs = frameFromStorage.renderTimeMs;
    _codec = frameFromStorage.codec;
    const uint8_t *prevBuffer = _buffer;
    if (VerifyAndAllocate(frameFromStorage.payloadSize) < 0)
    {
        return VCM_MEMORY;
    }
    _sessionInfo.UpdateDataPointers(prevBuffer, _buffer);
    memcpy(_buffer, frameFromStorage.payloadData, frameFromStorage.payloadSize);
    _length = frameFromStorage.payloadSize;
    return VCM_OK;
}

int VCMFrameBuffer::NotDecodablePackets() const {
  return _sessionInfo.packets_not_decodable();
}

// Set counted status (as counted by JB or not)
void VCMFrameBuffer::SetCountedFrame(bool frameCounted)
{
    _frameCounted = frameCounted;
}

bool VCMFrameBuffer::GetCountedFrame() const
{
    return _frameCounted;
}

// Get current state of frame
VCMFrameBufferStateEnum
VCMFrameBuffer::GetState() const
{
    return _state;
}

// Get current state of frame
VCMFrameBufferStateEnum
VCMFrameBuffer::GetState(uint32_t& timeStamp) const
{
    timeStamp = TimeStamp();
    return GetState();
}

bool
VCMFrameBuffer::IsRetransmitted() const
{
    return _sessionInfo.session_nack();
}

void
VCMFrameBuffer::PrepareForDecode(bool continuous)
{
#ifdef INDEPENDENT_PARTITIONS
    if (_codec == kVideoCodecVP8)
    {
        _length =
            _sessionInfo.BuildVP8FragmentationHeader(_buffer, _length,
                                                     &_fragmentation);
    } else {
        int bytes_removed = _sessionInfo.MakeDecodable();
        _length -= bytes_removed;
    }
#else
    int bytes_removed = _sessionInfo.MakeDecodable();
    _length -= bytes_removed;
#endif
    // Transfer frame information to EncodedFrame and create any codec
    // specific information.
    _frameType = ConvertFrameType(_sessionInfo.FrameType());
    _completeFrame = _sessionInfo.complete();
    _missingFrame = !continuous;
}

}
