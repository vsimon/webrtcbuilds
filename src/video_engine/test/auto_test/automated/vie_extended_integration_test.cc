/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/**
 * Runs "extended" integration tests.
 */

#include "gtest/gtest.h"
#include "vie_integration_test_base.h"
#include "vie_autotest.h"

namespace {

class ViEExtendedIntegrationTest: public ViEIntegrationTest {
};

TEST_F(ViEExtendedIntegrationTest, RunsBaseTestWithoutErrors) {
  tests_->ViEBaseExtendedTest();
}

TEST_F(ViEExtendedIntegrationTest, RunsCaptureTestWithoutErrors) {
  tests_->ViECaptureExtendedTest();
}

TEST_F(ViEExtendedIntegrationTest, RunsCodecTestWithoutErrors) {
  tests_->ViECodecExtendedTest();
}

TEST_F(ViEExtendedIntegrationTest, RunsEncryptionTestWithoutErrors) {
  tests_->ViEEncryptionExtendedTest();
}

TEST_F(ViEExtendedIntegrationTest, RunsFileTestWithoutErrors) {
  tests_->ViEFileExtendedTest();
}

TEST_F(ViEExtendedIntegrationTest, RunsImageProcessTestWithoutErrors) {
  tests_->ViEImageProcessExtendedTest();
}

TEST_F(ViEExtendedIntegrationTest, RunsNetworkTestWithoutErrors) {
  tests_->ViENetworkExtendedTest();
}

TEST_F(ViEExtendedIntegrationTest, RunsRenderTestWithoutErrors) {
  tests_->ViERenderExtendedTest();
}

TEST_F(ViEExtendedIntegrationTest, RunsRtpRtcpTestWithoutErrors) {
  tests_->ViERtpRtcpExtendedTest();
}

} // namespace
