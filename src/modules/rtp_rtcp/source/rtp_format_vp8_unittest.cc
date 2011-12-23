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
 * This file includes unit tests for the VP8 packetizer.
 */

#include <gtest/gtest.h>

#include "modules/rtp_rtcp/source/rtp_format_vp8.h"
#include "modules/rtp_rtcp/source/rtp_format_vp8_test_helper.h"
#include "typedefs.h"

namespace webrtc {

template <bool>
struct CompileAssert {
};

#undef COMPILE_ASSERT
#define COMPILE_ASSERT(expr, msg) \
  typedef CompileAssert<(bool(expr))> msg[bool(expr) ? 1 : -1]

class RtpFormatVp8Test : public ::testing::Test {
 protected:
  RtpFormatVp8Test() : helper_(NULL) {}
  virtual void TearDown() { delete helper_; }
  bool Init(const int* partition_sizes, int num_partitions) {
    hdr_info_.pictureId = kNoPictureId;
    hdr_info_.nonReference = false;
    hdr_info_.temporalIdx = kNoTemporalIdx;
    hdr_info_.layerSync = false;
    hdr_info_.tl0PicIdx = kNoTl0PicIdx;
    hdr_info_.keyIdx = kNoKeyIdx;
    if (helper_ != NULL) return false;
    helper_ = new test::RtpFormatVp8TestHelper(&hdr_info_);
    return helper_->Init(partition_sizes, num_partitions);
  }

  RTPVideoHeaderVP8 hdr_info_;
  test::RtpFormatVp8TestHelper* helper_;
};

TEST_F(RtpFormatVp8Test, TestStrictMode) {
  const int kSizeVector[] = {10, 8, 27};
  const int kNumPartitions = sizeof(kSizeVector) / sizeof(kSizeVector[0]);
  ASSERT_TRUE(Init(kSizeVector, kNumPartitions));

  hdr_info_.pictureId = 200;  // > 0x7F should produce 2-byte PictureID.
  const int kMaxSize = 13;
  RtpFormatVp8 packetizer = RtpFormatVp8(helper_->payload_data(),
                                         helper_->payload_size(),
                                         hdr_info_,
                                         kMaxSize,
                                         *(helper_->fragmentation()),
                                         kStrict);

  // The expected sizes are obtained by running a verified good implementation.
  const int kExpectedSizes[] = {8, 10, 12, 11, 13, 8, 11};
  const int kExpectedPart[] = {0, 0, 1, 2, 2, 2, 2};
  const bool kExpectedFragStart[] =
      {true, false, true, true, false, false, false};
  const int kExpectedNum = sizeof(kExpectedSizes) / sizeof(kExpectedSizes[0]);
  COMPILE_ASSERT(kExpectedNum ==
      sizeof(kExpectedPart) / sizeof(kExpectedPart[0]),
      kExpectedPart_wrong_size);
  COMPILE_ASSERT(kExpectedNum ==
      sizeof(kExpectedFragStart) / sizeof(kExpectedFragStart[0]),
      kExpectedFragStart_wrong_size);

  helper_->GetAllPacketsAndCheck(&packetizer, kExpectedSizes, kExpectedPart,
                                 kExpectedFragStart, kExpectedNum);
}

TEST_F(RtpFormatVp8Test, TestAggregateMode) {
  const int kSizeVector[] = {60, 10, 10};
  const int kNumPartitions = sizeof(kSizeVector) / sizeof(kSizeVector[0]);
  ASSERT_TRUE(Init(kSizeVector, kNumPartitions));

  hdr_info_.pictureId = 20;  // <= 0x7F should produce 1-byte PictureID.
  const int kMaxSize = 25;
  RtpFormatVp8 packetizer = RtpFormatVp8(helper_->payload_data(),
                                         helper_->payload_size(),
                                         hdr_info_,
                                         kMaxSize,
                                         *(helper_->fragmentation()),
                                         kAggregate);

  // The expected sizes are obtained by running a verified good implementation.
  const int kExpectedSizes[] = {22, 23, 24, 23};
  const int kExpectedPart[] = {0, 0, 0, 1};
  const bool kExpectedFragStart[] = {true, false, false, true};
  const int kExpectedNum = sizeof(kExpectedSizes) / sizeof(kExpectedSizes[0]);
  COMPILE_ASSERT(kExpectedNum ==
      sizeof(kExpectedPart) / sizeof(kExpectedPart[0]),
      kExpectedPart_wrong_size);
  COMPILE_ASSERT(kExpectedNum ==
      sizeof(kExpectedFragStart) / sizeof(kExpectedFragStart[0]),
      kExpectedFragStart_wrong_size);

  helper_->GetAllPacketsAndCheck(&packetizer, kExpectedSizes, kExpectedPart,
                                 kExpectedFragStart, kExpectedNum);
}

TEST_F(RtpFormatVp8Test, TestSloppyMode) {
  const int kSizeVector[] = {10, 10, 10};
  const int kNumPartitions = sizeof(kSizeVector) / sizeof(kSizeVector[0]);
  ASSERT_TRUE(Init(kSizeVector, kNumPartitions));

  hdr_info_.pictureId = kNoPictureId;  // No PictureID.
  const int kMaxSize = 9;
  RtpFormatVp8 packetizer = RtpFormatVp8(helper_->payload_data(),
                                         helper_->payload_size(),
                                         hdr_info_,
                                         kMaxSize,
                                         *(helper_->fragmentation()),
                                         kSloppy);

  // The expected sizes are obtained by running a verified good implementation.
  const int kExpectedSizes[] = {9, 9, 9, 7};
  const int kExpectedPart[] = {0, 0, 1, 2};
  const bool kExpectedFragStart[] = {true, false, false, false};
  const int kExpectedNum = sizeof(kExpectedSizes) / sizeof(kExpectedSizes[0]);
  COMPILE_ASSERT(kExpectedNum ==
      sizeof(kExpectedPart) / sizeof(kExpectedPart[0]),
      kExpectedPart_wrong_size);
  COMPILE_ASSERT(kExpectedNum ==
      sizeof(kExpectedFragStart) / sizeof(kExpectedFragStart[0]),
      kExpectedFragStart_wrong_size);

  helper_->GetAllPacketsAndCheck(&packetizer, kExpectedSizes, kExpectedPart,
                                 kExpectedFragStart, kExpectedNum);
}

// Verify that sloppy mode is forced if fragmentation info is missing.
TEST_F(RtpFormatVp8Test, TestSloppyModeFallback) {
  const int kSizeVector[] = {10, 10, 10};
  const int kNumPartitions = sizeof(kSizeVector) / sizeof(kSizeVector[0]);
  ASSERT_TRUE(Init(kSizeVector, kNumPartitions));

  hdr_info_.pictureId = 200;  // > 0x7F should produce 2-byte PictureID
  const int kMaxSize = 12;  // Small enough to produce 4 packets.
  RtpFormatVp8 packetizer = RtpFormatVp8(helper_->payload_data(),
                                         helper_->payload_size(),
                                         hdr_info_,
                                         kMaxSize);

  // Expecting three full packets, and one with the remainder.
  const int kExpectedSizes[] = {12, 12, 12, 10};
  const int kExpectedPart[] = {0, 0, 0, 0};  // Always 0 for sloppy mode.
  // Frag start only true for first packet in sloppy mode.
  const bool kExpectedFragStart[] = {true, false, false, false};
  const int kExpectedNum = sizeof(kExpectedSizes) / sizeof(kExpectedSizes[0]);
  COMPILE_ASSERT(kExpectedNum ==
      sizeof(kExpectedPart) / sizeof(kExpectedPart[0]),
      kExpectedPart_wrong_size);
  COMPILE_ASSERT(kExpectedNum ==
      sizeof(kExpectedFragStart) / sizeof(kExpectedFragStart[0]),
      kExpectedFragStart_wrong_size);

  helper_->set_sloppy_partitioning(true);
  helper_->GetAllPacketsAndCheck(&packetizer, kExpectedSizes, kExpectedPart,
                                 kExpectedFragStart, kExpectedNum);
}

// Verify that non-reference bit is set. Sloppy mode fallback is expected.
TEST_F(RtpFormatVp8Test, TestNonReferenceBit) {
  const int kSizeVector[] = {10, 10, 10};
  const int kNumPartitions = sizeof(kSizeVector) / sizeof(kSizeVector[0]);
  ASSERT_TRUE(Init(kSizeVector, kNumPartitions));

  hdr_info_.nonReference = true;
  const int kMaxSize = 25;  // Small enough to produce two packets.
  RtpFormatVp8 packetizer = RtpFormatVp8(helper_->payload_data(),
                                         helper_->payload_size(),
                                         hdr_info_,
                                         kMaxSize);

  // Sloppy mode => First packet full; other not.
  const int kExpectedSizes[] = {25, 7};
  const int kExpectedPart[] = {0, 0};  // Always 0 for sloppy mode.
  // Frag start only true for first packet in sloppy mode.
  const bool kExpectedFragStart[] = {true, false};
  const int kExpectedNum = sizeof(kExpectedSizes) / sizeof(kExpectedSizes[0]);
  COMPILE_ASSERT(kExpectedNum ==
      sizeof(kExpectedPart) / sizeof(kExpectedPart[0]),
      kExpectedPart_wrong_size);
  COMPILE_ASSERT(kExpectedNum ==
      sizeof(kExpectedFragStart) / sizeof(kExpectedFragStart[0]),
      kExpectedFragStart_wrong_size);

  helper_->set_sloppy_partitioning(true);
  helper_->GetAllPacketsAndCheck(&packetizer, kExpectedSizes, kExpectedPart,
                                 kExpectedFragStart, kExpectedNum);
}

// Verify Tl0PicIdx and TID fields, and layerSync bit.
TEST_F(RtpFormatVp8Test, TestTl0PicIdxAndTID) {
  const int kSizeVector[] = {10, 10, 10};
  const int kNumPartitions = sizeof(kSizeVector) / sizeof(kSizeVector[0]);
  ASSERT_TRUE(Init(kSizeVector, kNumPartitions));

  hdr_info_.tl0PicIdx = 117;
  hdr_info_.temporalIdx = 2;
  hdr_info_.layerSync = true;
  // kMaxSize is only limited by allocated buffer size.
  const int kMaxSize = helper_->buffer_size();
  RtpFormatVp8 packetizer = RtpFormatVp8(helper_->payload_data(),
                                         helper_->payload_size(),
                                         hdr_info_,
                                         kMaxSize,
                                         *(helper_->fragmentation()),
                                         kAggregate);

  // Expect one single packet of payload_size() + 4 bytes header.
  const int kExpectedSizes[1] = {helper_->payload_size() + 4};
  const int kExpectedPart[1] = {0};  // Packet starts with partition 0.
  const bool kExpectedFragStart[1] = {true};
  const int kExpectedNum = sizeof(kExpectedSizes) / sizeof(kExpectedSizes[0]);
  COMPILE_ASSERT(kExpectedNum ==
      sizeof(kExpectedPart) / sizeof(kExpectedPart[0]),
      kExpectedPart_wrong_size);
  COMPILE_ASSERT(kExpectedNum ==
      sizeof(kExpectedFragStart) / sizeof(kExpectedFragStart[0]),
      kExpectedFragStart_wrong_size);

  helper_->GetAllPacketsAndCheck(&packetizer, kExpectedSizes, kExpectedPart,
                                 kExpectedFragStart, kExpectedNum);
}

// Verify KeyIdx field.
TEST_F(RtpFormatVp8Test, TestKeyIdx) {
  const int kSizeVector[] = {10, 10, 10};
  const int kNumPartitions = sizeof(kSizeVector) / sizeof(kSizeVector[0]);
  ASSERT_TRUE(Init(kSizeVector, kNumPartitions));

  hdr_info_.keyIdx = 17;
  // kMaxSize is only limited by allocated buffer size.
  const int kMaxSize = helper_->buffer_size();
  RtpFormatVp8 packetizer = RtpFormatVp8(helper_->payload_data(),
                                         helper_->payload_size(),
                                         hdr_info_,
                                         kMaxSize,
                                         *(helper_->fragmentation()),
                                         kAggregate);

  // Expect one single packet of payload_size() + 3 bytes header.
  const int kExpectedSizes[1] = {helper_->payload_size() + 3};
  const int kExpectedPart[1] = {0};  // Packet starts with partition 0.
  const bool kExpectedFragStart[1] = {true};
  const int kExpectedNum = sizeof(kExpectedSizes) / sizeof(kExpectedSizes[0]);
  COMPILE_ASSERT(kExpectedNum ==
      sizeof(kExpectedPart) / sizeof(kExpectedPart[0]),
      kExpectedPart_wrong_size);
  COMPILE_ASSERT(kExpectedNum ==
      sizeof(kExpectedFragStart) / sizeof(kExpectedFragStart[0]),
      kExpectedFragStart_wrong_size);

  helper_->GetAllPacketsAndCheck(&packetizer, kExpectedSizes, kExpectedPart,
                                 kExpectedFragStart, kExpectedNum);
}

// Verify TID field and KeyIdx field in combination.
TEST_F(RtpFormatVp8Test, TestTIDAndKeyIdx) {
  const int kSizeVector[] = {10, 10, 10};
  const int kNumPartitions = sizeof(kSizeVector) / sizeof(kSizeVector[0]);
  ASSERT_TRUE(Init(kSizeVector, kNumPartitions));

  hdr_info_.temporalIdx = 1;
  hdr_info_.keyIdx = 5;
  // kMaxSize is only limited by allocated buffer size.
  const int kMaxSize = helper_->buffer_size();
  RtpFormatVp8 packetizer = RtpFormatVp8(helper_->payload_data(),
                                         helper_->payload_size(),
                                         hdr_info_,
                                         kMaxSize,
                                         *(helper_->fragmentation()),
                                         kAggregate);

  // Expect one single packet of payload_size() + 3 bytes header.
  const int kExpectedSizes[1] = {helper_->payload_size() + 3};
  const int kExpectedPart[1] = {0};  // Packet starts with partition 0.
  const bool kExpectedFragStart[1] = {true};
  const int kExpectedNum = sizeof(kExpectedSizes) / sizeof(kExpectedSizes[0]);
  COMPILE_ASSERT(kExpectedNum ==
      sizeof(kExpectedPart) / sizeof(kExpectedPart[0]),
      kExpectedPart_wrong_size);
  COMPILE_ASSERT(kExpectedNum ==
      sizeof(kExpectedFragStart) / sizeof(kExpectedFragStart[0]),
      kExpectedFragStart_wrong_size);

  helper_->GetAllPacketsAndCheck(&packetizer, kExpectedSizes, kExpectedPart,
                                 kExpectedFragStart, kExpectedNum);
}

}  // namespace
