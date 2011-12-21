/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "before_initialization_fixture.h"

class VoeBaseMiscTest : public BeforeInitializationFixture {
};

using namespace testing;

TEST_F(VoeBaseMiscTest, MaxNumChannelsIs32) {
  EXPECT_EQ(32, voe_base_->MaxNumOfChannels());
}

TEST_F(VoeBaseMiscTest, GetVersionPrintsSomeUsefulInformation) {
  char char_buffer[1024];
  EXPECT_EQ(0, voe_base_->GetVersion(char_buffer));
  EXPECT_THAT(char_buffer, ContainsRegex("VoiceEngine [0-9].[0-9].[0-9]"));
}
