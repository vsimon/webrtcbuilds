/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_JITTER_BUFFER_H_
#define WEBRTC_MODULES_VIDEO_CODING_JITTER_BUFFER_H_

#include "typedefs.h"
#include "critical_section_wrapper.h"
#include "module_common_types.h"
#include "video_coding_defines.h"
#include "inter_frame_delay.h"
#include "event.h"
#include "frame_list.h"
#include "jitter_buffer_common.h"
#include "jitter_estimator.h"

namespace webrtc
{

enum VCMNackMode
{
    kNackInfinite,
    kNackHybrid,
    kNoNack
};

// forward declarations
class VCMFrameBuffer;
class VCMPacket;
class VCMEncodedFrame;

class VCMJitterSample
{
public:
    VCMJitterSample() : timestamp(0), frameSize(0), latestPacketTime(-1) {}
    WebRtc_UWord32 timestamp;
    WebRtc_UWord32 frameSize;
    WebRtc_Word64 latestPacketTime;
};

class VCMJitterBuffer
{
public:
    VCMJitterBuffer(WebRtc_Word32 vcmId = -1,
                    WebRtc_Word32 receiverId = -1,
                    bool master = true);
    virtual ~VCMJitterBuffer();

    VCMJitterBuffer& operator=(const VCMJitterBuffer& rhs);

    // We need a start and stop to break out of the wait event
    // used in GetCompleteFrameForDecoding
    void Start();
    void Stop();
    bool Running() const;

    // Empty the Jitter buffer of all its data
    void Flush();

    // Statistics, Get received key and delta frames
    WebRtc_Word32 GetFrameStatistics(WebRtc_UWord32& receivedDeltaFrames,
                                     WebRtc_UWord32& receivedKeyFrames) const;

    // Statistics, Calculate frame and bit rates
    WebRtc_Word32 GetUpdate(WebRtc_UWord32& frameRate, WebRtc_UWord32& bitRate);

    // Wait for the first packet in the next frame to arrive, blocks
    // for <= maxWaitTimeMS ms
    WebRtc_Word64 GetNextTimeStamp(WebRtc_UWord32 maxWaitTimeMS,
                                   FrameType& incomingFrameType,
                                   WebRtc_Word64& renderTimeMs);

    // Will the packet sequence be complete if the next frame is grabbed
    // for decoding right now? That is, have we lost a frame between the
    // last decoded frame and the next, or is the next frame missing one
    // or more packets?
    bool CompleteSequenceWithNextFrame();

    // Wait maxWaitTimeMS for a complete frame to arrive. After timeout NULL
    // is returned.
    VCMEncodedFrame* GetCompleteFrameForDecoding(WebRtc_UWord32 maxWaitTimeMS);

    // Get a frame for decoding (even an incomplete) without delay.
    VCMEncodedFrame* GetFrameForDecoding();

    VCMEncodedFrame* GetFrameForDecodingNACK();

    // Release frame (when done with decoding)
    void ReleaseFrame(VCMEncodedFrame* frame);

    // Get frame to use for this timestamp
    WebRtc_Word32 GetFrame(const VCMPacket& packet, VCMEncodedFrame*&);
    VCMEncodedFrame* GetFrame(const VCMPacket& packet); // deprecated

    // Returns the time in ms when the latest packet was inserted into the frame.
    // Retransmitted is set to true if any of the packets belonging to the frame
    // has been retransmitted.
    WebRtc_Word64 LastPacketTime(VCMEncodedFrame* frame,
                                 bool& retransmitted) const;

    // Insert a packet into a frame
    VCMFrameBufferEnum InsertPacket(VCMEncodedFrame* frame,
                                    const VCMPacket& packet);

    // Sync
    WebRtc_UWord32 GetEstimatedJitterMS();
    void UpdateRtt(WebRtc_UWord32 rttMs);

    // NACK
    void SetNackMode(VCMNackMode mode); // Enable/disable nack
    VCMNackMode GetNackMode() const;    // Get nack mode
    // Get list of missing sequence numbers (size in number of elements)
    WebRtc_UWord16* GetNackList(WebRtc_UWord16& nackSize,
                                bool& listExtended);

    WebRtc_Word64 LastDecodedTimestamp() const;
    static WebRtc_UWord32 LatestTimestamp(const WebRtc_UWord32 existingTimestamp,
                                          const WebRtc_UWord32 newTimestamp);

protected:

    // Misc help functions
    // Recycle (release) frame, used if we didn't receive whole frame
    void RecycleFrame(VCMFrameBuffer* frame);
    void ReleaseFrameInternal(VCMFrameBuffer* frame);
    // Flush and reset the jitter buffer. Call under critical section.
    void FlushInternal();
    VCMFrameListItem* FindOldestSequenceNum() const;

    // Help functions for insert packet
    // Get empty frame, creates new (i.e. increases JB size) if necessary
    VCMFrameBuffer* GetEmptyFrame();
    // Recycle oldest frames up to a key frame, used if JB is completely full
    bool RecycleFramesUntilKeyFrame();
    // Update frame state
    // (set as complete or reconstructable if conditions are met)
    void UpdateFrameState(VCMFrameBuffer* frameListItem);

    // Help functions for getting a frame
    // Find oldest complete frame, used for getting next frame to decode
    VCMFrameListItem* FindOldestCompleteContinuousFrame();

    // Check if a frame is missing the markerbit but is complete
    bool CheckForCompleteFrame(VCMFrameListItem* oldestFrameItem);


    void CleanUpOldFrames();
    void CleanUpSizeZeroFrames();

    void VerifyAndSetPreviousFrameLost(VCMFrameBuffer& frame);
    bool IsPacketRetransmitted(const VCMPacket& packet) const;
    void UpdateJitterAndDelayEstimates(VCMJitterSample& sample,
                                       bool incompleteFrame);
    void UpdateJitterAndDelayEstimates(VCMFrameBuffer& frame,
                                       bool incompleteFrame);
    void UpdateJitterAndDelayEstimates(WebRtc_Word64 latestPacketTimeMs,
                                       WebRtc_UWord32 timestamp,
                                       WebRtc_UWord32 frameSize,
                                       bool incompleteFrame);
    void UpdateOldJitterSample(const VCMPacket& packet);
    WebRtc_UWord32 GetEstimatedJitterMsInternal();

    // NACK help
    WebRtc_UWord16* CreateNackList(WebRtc_UWord16& nackSize,
                                   bool& listExtended);
    WebRtc_Word32 GetLowHighSequenceNumbers(WebRtc_Word32& lowSeqNum,
                                            WebRtc_Word32& highSeqNum) const;

    void UpdateLastDecodedWithEmpty(const VCMPacket& packet);

private:

    static bool FrameEqualTimestamp(VCMFrameBuffer* frame,
                                    const void* timestamp);
    static bool CompleteDecodableKeyFrameCriteria(VCMFrameBuffer* frame,
                                                  const void* notUsed);
    // Decide whether should wait for NACK (mainly relevant for hybrid mode)
    bool WaitForNack();

    WebRtc_Word32                 _vcmId;
    WebRtc_Word32                 _receiverId;
    // If we are running (have started) or not
    bool                          _running;
    CriticalSectionWrapper&       _critSect;
    bool                          _master;
    // Event to signal when we have a frame ready for decoder
    VCMEvent                      _frameEvent;
    // Event to signal when we have received a packet
    VCMEvent                      _packetEvent;
    // Number of allocated frames
    WebRtc_Word32                 _maxNumberOfFrames;
    // Array of pointers to the frames in JB
    VCMFrameBuffer*               _frameBuffers[kMaxNumberOfFrames];
    VCMFrameListTimestampOrderAsc _frameBuffersTSOrder;

    // timing
    // Sequence number of last frame that was given to decoder
    WebRtc_Word32           _lastDecodedSeqNum;
    // Timestamp of last frame that was given to decoder
    WebRtc_Word64           _lastDecodedTimeStamp;

    // Statistics
    // Frame counter for each type (key, delta, golden, key-delta)
    WebRtc_UWord8           _receiveStatistics[4];
    // Latest calculated frame rates of incoming stream
    WebRtc_UWord8           _incomingFrameRate;
    // Frame counter, reset in GetUpdate
    WebRtc_UWord32          _incomingFrameCount;
    // Real time for last _frameCount reset
    WebRtc_Word64           _timeLastIncomingFrameCount;
    // Received bits counter, reset in GetUpdate
    WebRtc_UWord32          _incomingBitCount;
    WebRtc_UWord32          _incomingBitRate;
    WebRtc_UWord32          _dropCount;            // Frame drop counter
    // Number of frames in a row that have been too old
    WebRtc_UWord32          _numConsecutiveOldFrames;
    // Number of packets in a row that have been too old
    WebRtc_UWord32          _numConsecutiveOldPackets;
    // Filters for estimating jitter
    VCMJitterEstimator      _jitterEstimate;
    // Calculates network delays used for jitter calculations
    VCMInterFrameDelay      _delayEstimate;
    VCMJitterSample         _waitingForCompletion;
    WebRtc_UWord32          _rttMs;

    // NACK
    VCMNackMode             _nackMode;
    // Holds the internal nack list (the missing sequence numbers)
    WebRtc_Word32           _NACKSeqNumInternal[kNackHistoryLength];
    WebRtc_UWord16          _NACKSeqNum[kNackHistoryLength];
    WebRtc_UWord32          _NACKSeqNumLength;

    bool                    _missingMarkerBits;
    bool                    _firstPacket;
};

} // namespace webrtc

#endif // WEBRTC_MODULES_VIDEO_CODING_JITTER_BUFFER_H_
