/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "tb_video_channel.h"

TbVideoChannel::TbVideoChannel(TbInterfaces& Engine,
                               webrtc::VideoCodecType sendCodec, int width,
                               int height, int frameRate, int startBitrate) :
    videoChannel(-1),  ViE(Engine)
{
    EXPECT_EQ(0, ViE.base->CreateChannel(videoChannel));

    webrtc::VideoCodec videoCodec;
    memset(&videoCodec, 0, sizeof(webrtc::VideoCodec));
    bool sendCodecSet = false;
    for (int idx = 0; idx < ViE.codec->NumberOfCodecs(); idx++)
    {
        EXPECT_EQ(0, ViE.codec->GetCodec(idx, videoCodec));
        videoCodec.width = width;
        videoCodec.height = height;
        videoCodec.maxFramerate = frameRate;

        if (videoCodec.codecType == sendCodec && sendCodecSet == false)
        {
            if(videoCodec.codecType != webrtc::kVideoCodecI420 )
            {
                videoCodec.startBitrate = startBitrate;
                videoCodec.maxBitrate = startBitrate * 3;
            }
            EXPECT_EQ(0, ViE.codec->SetSendCodec(videoChannel, videoCodec));
            sendCodecSet = true;
        }
        if (videoCodec.codecType == webrtc::kVideoCodecVP8)
        {
            videoCodec.width = 352;
            videoCodec.height = 288;
        }
        EXPECT_EQ(0, ViE.codec->SetReceiveCodec(videoChannel, videoCodec));
    }
    EXPECT_TRUE(sendCodecSet);
}

TbVideoChannel::~TbVideoChannel(void)
{
    EXPECT_EQ(0, ViE.base->DeleteChannel(videoChannel));
}

void TbVideoChannel::StartSend(const unsigned short rtpPort /*= 11000*/,
                               const char* ipAddress /*= "127.0.0.1"*/)
{
    EXPECT_EQ(0, ViE.network->SetSendDestination(videoChannel, ipAddress,
                                                 rtpPort));

    EXPECT_EQ(0, ViE.base->StartSend(videoChannel));
}

void TbVideoChannel::SetFrameSettings(int width, int height, int frameRate)
{
    webrtc::VideoCodec videoCodec;
    EXPECT_EQ(0, ViE.codec->GetSendCodec(videoChannel, videoCodec));
    videoCodec.width = width;
    videoCodec.height = height;
    videoCodec.maxFramerate = frameRate;

    EXPECT_EQ(0, ViE.codec->SetSendCodec(videoChannel, videoCodec));
    EXPECT_EQ(0, ViE.codec->SetReceiveCodec(videoChannel, videoCodec));
}

void TbVideoChannel::StopSend()
{
    EXPECT_EQ(0, ViE.base->StopSend(videoChannel));
}

void TbVideoChannel::StartReceive(const unsigned short rtpPort /*= 11000*/)
{
    EXPECT_EQ(0, ViE.network->SetLocalReceiver(videoChannel, rtpPort));
    EXPECT_EQ(0, ViE.base->StartReceive(videoChannel));
}

void TbVideoChannel::StopReceive()
{
    EXPECT_EQ(0, ViE.base->StopReceive(videoChannel));
}
