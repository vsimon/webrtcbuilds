/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_TEST_RECEIVER_TESTS_H_
#define WEBRTC_MODULES_VIDEO_CODING_TEST_RECEIVER_TESTS_H_

#include "video_coding.h"
#include "module_common_types.h"
#include "common_types.h"
#include "rtp_rtcp.h"
#include "typedefs.h"
#include "rtp_player.h"
#include "test_util.h"

#include <string>
#include <stdio.h>

class RtpDataCallback : public webrtc::RtpData
{
public:
    RtpDataCallback(webrtc::VideoCodingModule* vcm)
        : _vcm(vcm) {};

    virtual WebRtc_Word32 OnReceivedPayloadData(const WebRtc_UWord8* payloadData,
                                              const WebRtc_UWord16 payloadSize,
                                              const webrtc::WebRtcRTPHeader* rtpHeader);
private:
    webrtc::VideoCodingModule* _vcm;
};

class FrameReceiveCallback : public webrtc::VCMReceiveCallback
{
public:
    FrameReceiveCallback(std::string outFilename) :
        _outFilename(outFilename),
        _outFile(NULL),
        _timingFile(NULL) {}

    virtual ~FrameReceiveCallback();

    WebRtc_Word32 FrameToRender(webrtc::VideoFrame& videoFrame);

private:
    std::string     _outFilename;
    FILE*           _outFile;
    FILE*           _timingFile;
};

class SharedState
{
public:
    SharedState(webrtc::VideoCodingModule& vcm, RTPPlayer& rtpPlayer) :
        _vcm(vcm),
        _rtpPlayer(rtpPlayer) {}
    webrtc::VideoCodingModule&  _vcm;
    RTPPlayer&              _rtpPlayer;
};

class SharedRTPState
{
public:
    SharedRTPState(webrtc::VideoCodingModule& vcm, webrtc::RtpRtcp& rtp) :
        _vcm(vcm),
        _rtp(rtp) {}
    webrtc::VideoCodingModule&  _vcm;
    webrtc::RtpRtcp&            _rtp;
};

int RtpPlay(CmdArgs& args);
int RtpPlayMT(CmdArgs& args,
              int releaseTest = 0,
              webrtc::VideoCodecType releaseTestVideoType = webrtc::kVideoCodecVP8);
int ReceiverTimingTests(CmdArgs& args);
int JitterBufferTest(CmdArgs& args);
int DecodeFromStorageTest(CmdArgs& args);

// Thread functions:
bool ProcessingThread(void* obj);
bool RtpReaderThread(void* obj);
bool DecodeThread(void* obj);
bool NackThread(void* obj);

#endif // WEBRTC_MODULES_VIDEO_CODING_TEST_RECEIVER_TESTS_H_
