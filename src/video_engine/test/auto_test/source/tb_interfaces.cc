/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "tb_interfaces.h"

#include "gtest/gtest.h"

TbInterfaces::TbInterfaces(const char* test_name) {
    std::string trace_file_path =
        (ViETest::GetResultOutputPath() + test_name) + "_trace.txt";

    ViETest::Log("Creating ViE Interfaces for test %s\n", test_name);

    video_engine = webrtc::VideoEngine::Create();
    EXPECT_TRUE(video_engine != NULL);

    EXPECT_EQ(0, video_engine->SetTraceFile(trace_file_path.c_str()));
    EXPECT_EQ(0, video_engine->SetTraceFilter(webrtc::kTraceAll));

    base = webrtc::ViEBase::GetInterface(video_engine);
    EXPECT_TRUE(base != NULL);

    EXPECT_EQ(0, base->Init());

    capture = webrtc::ViECapture::GetInterface(video_engine);
    EXPECT_TRUE(capture != NULL);

    rtp_rtcp = webrtc::ViERTP_RTCP::GetInterface(video_engine);
    EXPECT_TRUE(rtp_rtcp != NULL);

    render = webrtc::ViERender::GetInterface(video_engine);
    EXPECT_TRUE(render != NULL);

    codec = webrtc::ViECodec::GetInterface(video_engine);
    EXPECT_TRUE(codec != NULL);

    network = webrtc::ViENetwork::GetInterface(video_engine);
    EXPECT_TRUE(network != NULL);

    image_process = webrtc::ViEImageProcess::GetInterface(video_engine);
    EXPECT_TRUE(image_process != NULL);

    encryption = webrtc::ViEEncryption::GetInterface(video_engine);
    EXPECT_TRUE(encryption != NULL);
}

TbInterfaces::~TbInterfaces(void)
{
    EXPECT_EQ(0, encryption->Release());
    EXPECT_EQ(0, image_process->Release());
    EXPECT_EQ(0, codec->Release());
    EXPECT_EQ(0, capture->Release());
    EXPECT_EQ(0, render->Release());
    EXPECT_EQ(0, rtp_rtcp->Release());
    EXPECT_EQ(0, network->Release());
    EXPECT_EQ(0, base->Release());
    EXPECT_TRUE(webrtc::VideoEngine::Delete(video_engine)) <<
        "Since we have released all interfaces at this point, deletion "
        "should be successful.";

}
