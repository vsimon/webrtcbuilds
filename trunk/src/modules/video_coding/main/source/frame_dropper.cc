/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "frame_dropper.h"
#include "internal_defines.h"
#include "trace.h"

namespace webrtc
{

VCMFrameDropper::VCMFrameDropper(WebRtc_Word32 vcmId)
:
_vcmId(vcmId),
_keyFrameSizeAvgKbits(0.9f),
_keyFrameRatio(0.99f),
_dropRatio(0.9f, 0.96f)
{
    Reset();
}

void
VCMFrameDropper::Reset()
{
    _keyFrameRatio.Reset(0.99f);
    _keyFrameRatio.Apply(1.0f, 1.0f/300.0f); // 1 key frame every 10th second in 30 fps
    _keyFrameSizeAvgKbits.Reset(0.9f);
    _keyFrameCount = 0;
    _accumulator = 0.0f;
    _accumulatorMax = 150.0f; // assume 300 kb/s and 0.5 s window
    _targetBitRate = 300.0f;
    _userFrameRate = 30;
    _keyFrameSpreadFrames = 0.5f * _userFrameRate;
    _dropNext = false;
    _dropRatio.Reset(0.9f);
    _dropRatio.Apply(0.0f, 0.0f); // Initialize to 0
    _dropCount = 0;
    _windowSize = 0.5f;
    _wasBelowMax = true;
    _enabled = true;
    _fastMode = false; // start with normal (non-aggressive) mode
}

void
VCMFrameDropper::Enable(bool enable)
{
    _enabled = enable;
}

void
VCMFrameDropper::Fill(WebRtc_UWord32 frameSizeBytes, bool deltaFrame)
{
    if (!_enabled)
    {
        return;
    }
    float frameSizeKbits = 8.0f * static_cast<float>(frameSizeBytes) / 1000.0f;
    if (!deltaFrame && !_fastMode) // fast mode does not treat key-frames any different
    {
        _keyFrameSizeAvgKbits.Apply(1, frameSizeKbits);
        _keyFrameRatio.Apply(1.0, 1.0);
        if (frameSizeKbits > _keyFrameSizeAvgKbits.Value())
        {
            // Remove the average key frame size since we
            // compensate for key frames when adding delta
            // frames.
            frameSizeKbits -= _keyFrameSizeAvgKbits.Value();
        }
        else
        {
            // Shouldn't be negative, so zero is the lower bound.
            frameSizeKbits = 0;
        }
        if (_keyFrameRatio.Value() > 1e-5 && 1 / _keyFrameRatio.Value() < _keyFrameSpreadFrames)
        {
            // We are sending key frames more often than our upper bound for
            // how much we allow the key frame compensation to be spread
            // out in time. Therefor we must use the key frame ratio rather
            // than keyFrameSpreadFrames.
            _keyFrameCount = static_cast<WebRtc_Word32>(1 / _keyFrameRatio.Value() + 0.5);
        }
        else
        {
            // Compensate for the key frame the following frames
            _keyFrameCount = static_cast<WebRtc_Word32>(_keyFrameSpreadFrames + 0.5);
        }
    }
    else
    {
        // Decrease the keyFrameRatio
        _keyFrameRatio.Apply(1.0, 0.0);
    }
    // Change the level of the accumulator (bucket)
    _accumulator += frameSizeKbits;
}

void
VCMFrameDropper::Leak(WebRtc_UWord32 inputFrameRate)
{
    if (!_enabled)
    {
        return;
    }
    if (inputFrameRate < 1)
    {
        return;
    }
    if (_targetBitRate < 0.0f)
    {
        return;
    }
    _keyFrameSpreadFrames = 0.5f * inputFrameRate;
    // T is the expected bits per frame (target). If all frames were the same size,
    // we would get T bits per frame. Notice that T is also weighted to be able to
    // force a lower frame rate if wanted.
    float T = _targetBitRate / inputFrameRate;
    if (_keyFrameCount > 0)
    {
        // Perform the key frame compensation
        if (_keyFrameRatio.Value() > 0 && 1 / _keyFrameRatio.Value() < _keyFrameSpreadFrames)
        {
            T -= _keyFrameSizeAvgKbits.Value() * _keyFrameRatio.Value();
        }
        else
        {
            T -= _keyFrameSizeAvgKbits.Value() / _keyFrameSpreadFrames;
        }
        _keyFrameCount--;
    }
    _accumulator -= T;
    UpdateRatio();

}

void
VCMFrameDropper::UpdateNack(WebRtc_UWord32 nackBytes)
{
    if (!_enabled)
    {
        return;
    }
    _accumulator += static_cast<float>(nackBytes) * 8.0f / 1000.0f;
}

void
VCMFrameDropper::FillBucket(float inKbits, float outKbits)
{
    _accumulator += (inKbits - outKbits);
}

void
VCMFrameDropper::UpdateRatio()
{
    if (_accumulator > 1.3f * _accumulatorMax)
    {
        // Too far above accumulator max, react faster
        _dropRatio.UpdateBase(0.8f);
    }
    else
    {
        // Go back to normal reaction
        _dropRatio.UpdateBase(0.9f);
    }
    if (_accumulator > _accumulatorMax)
    {
        // We are above accumulator max, and should ideally
        // drop a frame. Increase the dropRatio and drop
        // the frame later.
        if (_wasBelowMax)
        {
            _dropNext = true;
        }
        if (_fastMode)
        {
            // always drop in aggressive mode
            _dropNext = true;
        }

        _dropRatio.Apply(1.0f, 1.0f);
        _dropRatio.UpdateBase(0.9f);
    }
    else
    {
        _dropRatio.Apply(1.0f, 0.0f);
    }
    if (_accumulator < 0.0f)
    {
        _accumulator = 0.0f;
    }
    _wasBelowMax = _accumulator < _accumulatorMax;
    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding, VCMId(_vcmId),  "FrameDropper: dropRatio = %f accumulator = %f, accumulatorMax = %f", _dropRatio.Value(), _accumulator, _accumulatorMax);
}

// This function signals when to drop frames to the caller. It makes use of the dropRatio
// to smooth out the drops over time.
bool
VCMFrameDropper::DropFrame()
{
    if (!_enabled)
    {
        return false;
    }
    if (_dropNext)
    {
        _dropNext = false;
        _dropCount = 0;
    }

    if (_dropRatio.Value() >= 0.5f) // Drops per keep
    {
        // limit is the number of frames we should drop between each kept frame
        // to keep our drop ratio. limit is positive in this case.
        float denom = 1.0f - _dropRatio.Value();
        if (denom < 1e-5)
        {
            denom = (float)1e-5;
        }
        WebRtc_Word32 limit = static_cast<WebRtc_Word32>(1.0f / denom - 1.0f + 0.5f);
        if (_dropCount < 0)
        {
            // Reset the _dropCount since it was negative and should be positive.
            if (_dropRatio.Value() > 0.4f)
            {
                _dropCount = -_dropCount;
            }
            else
            {
                _dropCount = 0;
            }
        }
        if (_dropCount < limit)
        {
            // As long we are below the limit we should drop frames.
            _dropCount++;
            return true;
        }
        else
        {
            // Only when we reset _dropCount a frame should be kept.
            _dropCount = 0;
            return false;
        }
    }
    else if (_dropRatio.Value() > 0.0f && _dropRatio.Value() < 0.5f) // Keeps per drop
    {
        // limit is the number of frames we should keep between each drop
        // in order to keep the drop ratio. limit is negative in this case,
        // and the _dropCount is also negative.
        float denom = _dropRatio.Value();
        if (denom < 1e-5)
        {
            denom = (float)1e-5;
        }
        WebRtc_Word32 limit = -static_cast<WebRtc_Word32>(1.0f / denom - 1.0f + 0.5f);
        if (_dropCount > 0)
        {
            // Reset the _dropCount since we have a positive
            // _dropCount, and it should be negative.
            if (_dropRatio.Value() < 0.6f)
            {
                _dropCount = -_dropCount;
            }
            else
            {
                _dropCount = 0;
            }
        }
        if (_dropCount > limit)
        {
            if (_dropCount == 0)
            {
                // Drop frames when we reset _dropCount.
                _dropCount--;
                return true;
            }
            else
            {
                // Keep frames as long as we haven't reached limit.
                _dropCount--;
                return false;
            }
        }
        else
        {
            _dropCount = 0;
            return false;
        }
    }
    _dropCount = 0;
    return false;

    // A simpler version, unfiltered and quicker
    //bool dropNext = _dropNext;
    //_dropNext = false;
    //return dropNext;
}

void
VCMFrameDropper::SetRates(float bitRate, float userFrameRate)
{
    // Bit rate of -1 means infinite bandwidth.
    _accumulatorMax = bitRate * _windowSize; // bitRate * windowSize (in seconds)
    if (_targetBitRate > 0.0f && bitRate < _targetBitRate && _accumulator > _accumulatorMax)
    {
        // Rescale the accumulator level if the accumulator max decreases
        _accumulator = bitRate / _targetBitRate * _accumulator;
    }
    _targetBitRate = bitRate;
    if (userFrameRate > 0.0f)
    {
        _userFrameRate = userFrameRate;
    }
}

float
VCMFrameDropper::ActualFrameRate(WebRtc_UWord32 inputFrameRate) const
{
    if (!_enabled)
    {
        return static_cast<float>(inputFrameRate);
    }
    return inputFrameRate * (1.0f - _dropRatio.Value());
}

}
