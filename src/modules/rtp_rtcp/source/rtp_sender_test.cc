/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * This file includes unit tests for the RTPSender.
 */

#include <gtest/gtest.h>

#include "rtp_header_extension.h"
#include "rtp_rtcp_defines.h"
#include "rtp_sender.h"
#include "rtp_utility.h"
#include "typedefs.h"

namespace webrtc {

namespace {
const int kId = 1;
const int kTypeLength = TRANSMISSION_TIME_OFFSET_LENGTH_IN_BYTES;
const int kPayload = 100;
const uint32_t kTimestamp = 10;
const uint16_t kSeqNum = 33;
const int kTimeOffset = 22222;
const int kMaxPacketLength = 1500;
}  // namespace

class RtpSenderTest : public ::testing::Test {
 protected:
  RtpSenderTest()
    : rtp_sender_(new RTPSender(0, false, ModuleRTPUtility::GetSystemClock())),
      kMarkerBit(true),
      kType(TRANSMISSION_TIME_OFFSET) {
    EXPECT_EQ(0, rtp_sender_->SetSequenceNumber(kSeqNum));
  }
  ~RtpSenderTest() {
    delete rtp_sender_;
  }

  RTPSender* rtp_sender_;
  const bool kMarkerBit;
  RTPExtensionType kType;
  uint8_t packet_[kMaxPacketLength];

  void VerifyRTPHeaderCommon(const WebRtcRTPHeader& rtp_header) {
    EXPECT_EQ(kMarkerBit, rtp_header.header.markerBit);
    EXPECT_EQ(kPayload, rtp_header.header.payloadType);
    EXPECT_EQ(kSeqNum, rtp_header.header.sequenceNumber);
    EXPECT_EQ(kTimestamp, rtp_header.header.timestamp);
    EXPECT_EQ(rtp_sender_->SSRC(), rtp_header.header.ssrc);
    EXPECT_EQ(0, rtp_header.header.numCSRCs);
    EXPECT_EQ(0, rtp_header.header.paddingLength);
  }
};

TEST_F(RtpSenderTest, RegisterRtpHeaderExtension) {
  EXPECT_EQ(0, rtp_sender_->RtpHeaderExtensionTotalLength());
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(kType, kId));
  EXPECT_EQ(RTP_ONE_BYTE_HEADER_LENGTH_IN_BYTES + kTypeLength,
            rtp_sender_->RtpHeaderExtensionTotalLength());
  EXPECT_EQ(0, rtp_sender_->DeregisterRtpHeaderExtension(kType));
  EXPECT_EQ(0, rtp_sender_->RtpHeaderExtensionTotalLength());
}

TEST_F(RtpSenderTest, BuildRTPPacket) {
  WebRtc_Word32 length = rtp_sender_->BuildRTPheader(packet_,
                                                     kPayload,
                                                     kMarkerBit,
                                                     kTimestamp);
  EXPECT_EQ(12, length);

  // Verify
  webrtc::ModuleRTPUtility::RTPHeaderParser rtpParser(packet_, length);
  webrtc::WebRtcRTPHeader rtp_header;

  RtpHeaderExtensionMap map;
  map.Register(kType, kId);
  const bool valid_rtp_header = rtpParser.Parse(rtp_header, &map);

  ASSERT_TRUE(valid_rtp_header);
  ASSERT_FALSE(rtpParser.RTCP());
  VerifyRTPHeaderCommon(rtp_header);
  EXPECT_EQ(length, rtp_header.header.headerLength);
  EXPECT_EQ(0, rtp_header.extension.transmissionTimeOffset);
}

TEST_F(RtpSenderTest, BuildRTPPacketWithExtension) {
  EXPECT_EQ(0, rtp_sender_->SetTransmissionTimeOffset(kTimeOffset));
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(kType, kId));

  WebRtc_Word32 length = rtp_sender_->BuildRTPheader(packet_,
                                                     kPayload,
                                                     kMarkerBit,
                                                     kTimestamp);
  EXPECT_EQ(12 + rtp_sender_->RtpHeaderExtensionTotalLength(), length);

  // Verify
  webrtc::ModuleRTPUtility::RTPHeaderParser rtpParser(packet_, length);
  webrtc::WebRtcRTPHeader rtp_header;

  RtpHeaderExtensionMap map;
  map.Register(kType, kId);
  const bool valid_rtp_header = rtpParser.Parse(rtp_header, &map);

  ASSERT_TRUE(valid_rtp_header);
  ASSERT_FALSE(rtpParser.RTCP());
  VerifyRTPHeaderCommon(rtp_header);
  EXPECT_EQ(length, rtp_header.header.headerLength);
  EXPECT_EQ(kTimeOffset, rtp_header.extension.transmissionTimeOffset);

  // Parse without map extension
  webrtc::WebRtcRTPHeader rtp_header2;
  const bool valid_rtp_header2 = rtpParser.Parse(rtp_header2, NULL);

  ASSERT_TRUE(valid_rtp_header2);
  VerifyRTPHeaderCommon(rtp_header2);
  EXPECT_EQ(length, rtp_header2.header.headerLength);
  EXPECT_EQ(0, rtp_header2.extension.transmissionTimeOffset);
}
}  // namespace webrtc
