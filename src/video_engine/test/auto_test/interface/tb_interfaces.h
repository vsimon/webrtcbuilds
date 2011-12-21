/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_INTERFACE_TB_INTERFACES_H_
#define WEBRTC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_INTERFACE_TB_INTERFACES_H_

#include "vie_autotest_defines.h"

#include "common_types.h"
#include "vie_base.h"
#include "vie_capture.h"
#include "vie_codec.h"
#include "vie_image_process.h"
#include "vie_network.h"
#include "vie_render.h"
#include "vie_rtp_rtcp.h"
#include "vie_encryption.h"
#include "vie_defines.h"

// This class deals with all the tedium of setting up video engine interfaces.
// It does its work in constructor and destructor, so keeping it in scope is
// enough.
class TbInterfaces
{
public:
    TbInterfaces(const char* test_name);
    ~TbInterfaces(void);

    webrtc::VideoEngine* video_engine;
    webrtc::ViEBase* base;
    webrtc::ViECapture* capture;
    webrtc::ViERender* render;
    webrtc::ViERTP_RTCP* rtp_rtcp;
    webrtc::ViECodec* codec;
    webrtc::ViENetwork* network;
    webrtc::ViEImageProcess* image_process;
    webrtc::ViEEncryption* encryption;

    int LastError() {
        return base->LastError();
    }
};

#endif  // WEBRTC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_INTERFACE_TB_INTERFACES_H_
