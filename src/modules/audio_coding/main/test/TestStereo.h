/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_STEREO_H
#define TEST_STEREO_H

#include "ACMTest.h"
#include "Channel.h"
#include "PCMFile.h"

namespace webrtc {

class TestPackStereo : public AudioPacketizationCallback
{
public:
    TestPackStereo();
    ~TestPackStereo();
    
    void RegisterReceiverACM(AudioCodingModule* acm);
    
    virtual WebRtc_Word32 SendData(const FrameType frameType,
        const WebRtc_UWord8 payloadType,
        const WebRtc_UWord32 timeStamp,
        const WebRtc_UWord8* payloadData, 
        const WebRtc_UWord16 payloadSize,
        const RTPFragmentationHeader* fragmentation);

    WebRtc_UWord16 GetPayloadSize();
    WebRtc_UWord32 GetTimeStampDiff();
    void ResetPayloadSize();
    void SetCodecType(int codecType);


private:
    AudioCodingModule* _receiverACM;
    WebRtc_Word16            _seqNo;
    WebRtc_UWord8            _payloadData[60 * 32 * 2 * 2]; 
    WebRtc_UWord32           _timeStampDiff;
    WebRtc_UWord32           _lastInTimestamp;
    WebRtc_UWord64           _totalBytes;
    WebRtc_UWord16           _payloadSize;
    WebRtc_UWord16           _noChannels;
    int                    _codecType;
};

class TestStereo : public ACMTest
{
public:
    TestStereo(int testMode);
    ~TestStereo();

    void Perform();
private:
    // The default value of '-1' indicates that the registration is based only on codec name
    // and a sampling frequncy matching is not required. This is useful for codecs which support
    // several sampling frequency.
    WebRtc_Word16 RegisterSendCodec(char side, 
        char* codecName, 
        WebRtc_Word32 sampFreqHz,
        int rate,
        int packSize);

    void Run(TestPackStereo* channel);
    void OpenOutFile(WebRtc_Word16 testNumber);
    void DisplaySendReceiveCodec();

    WebRtc_Word32 SendData(
        const FrameType       frameType,
        const WebRtc_UWord8   payloadType,
        const WebRtc_UWord32  timeStamp,
        const WebRtc_UWord8*  payloadData, 
        const WebRtc_UWord16  payloadSize,
        const RTPFragmentationHeader* fragmentation);

    int                    _testMode;

    AudioCodingModule*     _acmA;
    AudioCodingModule*     _acmB;

    TestPackStereo*        _channelA2B;

    PCMFile                _inFileA;
    PCMFile                _outFileB;
    PCMFile                _inFileStereo;
    WebRtc_Word16          _testCntr;
    WebRtc_UWord16         _packSizeSamp;
    WebRtc_UWord16         _packSizeBytes;
    int                    _counter;
    int                    _codecType;
};

} // namespace webrtc

#endif

