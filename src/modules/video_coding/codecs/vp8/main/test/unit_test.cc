/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "unit_test.h"

#include <string.h>

#include "../../../test_framework/video_source.h"
#include "gtest/gtest.h"
#include "testsupport/fileutils.h"
#include "vp8.h"

using namespace webrtc;

VP8UnitTest::VP8UnitTest()
:
UnitTest("VP8UnitTest", "Unit test")
{
}

VP8UnitTest::VP8UnitTest(std::string name, std::string description)
:
UnitTest(name, description)
{
}

void
VP8UnitTest::Print()
{
    WebRtc_Word8 versionStr[64];

    // GetVersion tests.

      EXPECT_TRUE(_encoder->Version(versionStr, sizeof(versionStr)) > 0);
//    printf("\n%s", versionStr);
//    UnitTest::Print();
}

WebRtc_UWord32
VP8UnitTest::CodecSpecific_SetBitrate(WebRtc_UWord32 bitRate, WebRtc_UWord32 /*frameRate*/)
{
    int rate = _encoder->SetRates(bitRate, _inst.maxFramerate);
    EXPECT_TRUE(rate >= 0);
    return rate;
}

bool
VP8UnitTest::CheckIfBitExact(const void* ptrA, unsigned int aLengthBytes,
                             const void* ptrB, unsigned int bLengthBytes)
{
    const unsigned char* cPtrA = (const unsigned char*)ptrA;
    const unsigned char* cPtrB = (const unsigned char*)ptrB;
    // Skip picture ID before comparing
    int aSkip = PicIdLength(cPtrA);
    int bSkip = PicIdLength(cPtrB);
    return UnitTest::CheckIfBitExact(cPtrA + aSkip, aLengthBytes,
                                     cPtrB + bSkip, bLengthBytes);
}

int
VP8UnitTest::PicIdLength(const unsigned char* ptr)
{
    WebRtc_UWord8 numberOfBytes;
    WebRtc_UWord64 pictureID = 0;
    for (numberOfBytes = 0; (ptr[numberOfBytes] & 0x80) && numberOfBytes < 8; numberOfBytes++)
    {
        pictureID += ptr[numberOfBytes] & 0x7f;
        pictureID <<= 7;
    }
    pictureID += ptr[numberOfBytes] & 0x7f;
    numberOfBytes++;
    return numberOfBytes;
}

void
VP8UnitTest::Perform()
{
    Setup();
    FILE *outFile = NULL;
    std::string outFileName;
    VP8Encoder* enc = (VP8Encoder*)_encoder;
    VP8Decoder* dec = (VP8Decoder*)_decoder;

    //----- Encoder parameter tests -----
    //-- Calls before InitEncode() --
    EXPECT_EQ(enc->Release(), WEBRTC_VIDEO_CODEC_OK);
    EXPECT_EQ(enc->SetRates(_bitRate, _inst.maxFramerate),
              WEBRTC_VIDEO_CODEC_UNINITIALIZED);

    EXPECT_EQ(enc->SetRates(_bitRate, _inst.maxFramerate),
              WEBRTC_VIDEO_CODEC_UNINITIALIZED);
   // EXPECT_TRUE(enc->GetCodecConfigParameters(configParameters, sizeof(configParameters)) ==
   //     WEBRTC_VIDEO_CODEC_UNINITIALIZED);


    VideoCodec codecInst;
    strncpy(codecInst.plName, "VP8", 31);
    codecInst.plType = 126;
    codecInst.maxBitrate = 0;
    codecInst.minBitrate = 0;
    codecInst.width = 1440;
    codecInst.height = 1080;
    codecInst.maxFramerate = 30;
    codecInst.startBitrate = 300;
    codecInst.codecSpecific.VP8.complexity = kComplexityNormal;
    EXPECT_EQ(enc->InitEncode(&codecInst, 1, 1440), WEBRTC_VIDEO_CODEC_OK);


    //-- Test two problematic level settings --
    strncpy(codecInst.plName, "VP8", 31);
    codecInst.plType = 126;
    codecInst.maxBitrate = 0;
    codecInst.minBitrate = 0;
    codecInst.width = 352;
    codecInst.height = 288;
    codecInst.maxFramerate = 30;
    codecInst.codecSpecific.VP8.complexity = kComplexityNormal;
    codecInst.startBitrate = 300;
    EXPECT_EQ(enc->InitEncode(&codecInst, 1, 1440), WEBRTC_VIDEO_CODEC_OK);

    // Settings not correct for this profile
    strncpy(codecInst.plName, "VP8", 31);
    codecInst.plType = 126;
    codecInst.maxBitrate = 0;
    codecInst.minBitrate = 0;
    codecInst.width = 176;
    codecInst.height = 144;
    codecInst.maxFramerate = 15;
    codecInst.codecSpecific.VP8.complexity = kComplexityNormal;
    codecInst.startBitrate = 300;
    //EXPECT_TRUE(enc->InitEncode(&codecInst, 1, 1440) == WEBRTC_VIDEO_CODEC_LEVEL_EXCEEDED);

    ASSERT_EQ(enc->InitEncode(&_inst, 1, 1440), WEBRTC_VIDEO_CODEC_OK);


    //-- ProcessNewBitrate() errors --
    // Bad bitrate.
    EXPECT_EQ(enc->SetRates(_inst.maxBitrate + 1, _inst.maxFramerate),
              WEBRTC_VIDEO_CODEC_OK);

   // Signaling not used.

    // Bad packetloss.
//    EXPECT_TRUE(enc->SetPacketLoss(300) < 0);

    //----- Decoder parameter tests -----
    //-- Calls before InitDecode() --
    EXPECT_TRUE(dec->Release() == 0);
    ASSERT_TRUE(dec->InitDecode(&_inst, 1) == WEBRTC_VIDEO_CODEC_OK);

    //-- SetCodecConfigParameters() errors --
    unsigned char tmpBuf[128];
    EXPECT_TRUE(dec->SetCodecConfigParameters(NULL, sizeof(tmpBuf)) == -1);
    EXPECT_TRUE(dec->SetCodecConfigParameters(tmpBuf, 1) == -1);
   // Garbage data.
    EXPECT_TRUE(dec->SetCodecConfigParameters(tmpBuf, sizeof(tmpBuf)) == -1);

    //----- Function tests -----
    outFileName = webrtc::test::OutputPath() + _source->GetName() + "-errResTest.yuv";
    outFile = fopen(outFileName.c_str(), "wb");
    ASSERT_TRUE(outFile != NULL);

    UnitTest::Perform();
    Teardown();

}
