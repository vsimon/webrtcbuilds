/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "framedrop_primitives.h"
#include "gflags/gflags.h"
#include "gtest/gtest.h"
#include "testsupport/fileutils.h"
#include "testsupport/metrics/video_metrics.h"
#include "vie_autotest.h"
#include "vie_file_based_comparison_tests.h"
#include "vie_integration_test_base.h"
#include "vie_to_file_renderer.h"

namespace {

// The input file must be QCIF since I420 gets scaled to that in the tests
// (it is so bandwidth-heavy we have no choice). Our comparison algorithms
// wouldn't like scaling, so this will work when we compare with the original.
const int kInputWidth = 176;
const int kInputHeight = 144;

const double kMinPSNR420DefaultBitRateQCIF = 28;
const double kMinSSIM420DefaultBitRateQCIF = 0.95;
const double kMinPSNRCodecTestsQCIF = 20;
const double kMinSSIMCodecTestsQCIF = 0.7;
const double kMinPSNR50kbpsQCIF = 25;
const double kMinSSIM50kbpsQCIF = 0.8;

class ViEVideoVerificationTest : public testing::Test {
 protected:
  void SetUp() {
    input_file_ = webrtc::test::ResourcePath("paris_qcif", "yuv");
    local_file_renderer_ = new ViEToFileRenderer();
    remote_file_renderer_ = new ViEToFileRenderer();
    SetUpLocalFileRenderer(local_file_renderer_);
    SetUpRemoteFileRenderer(remote_file_renderer_);
  }

  void TearDown() {
    TearDownFileRenderer(local_file_renderer_);
    delete local_file_renderer_;
    TearDownFileRenderer(remote_file_renderer_);
    delete remote_file_renderer_;
  }

  void SetUpLocalFileRenderer(ViEToFileRenderer* file_renderer) {
    SetUpFileRenderer(file_renderer, "-local-preview.yuv");
  }

  void SetUpRemoteFileRenderer(ViEToFileRenderer* file_renderer) {
    SetUpFileRenderer(file_renderer, "-remote.yuv");
  }

  // Must be called manually inside the tests.
  void StopRenderers() {
    local_file_renderer_->StopRendering();
    remote_file_renderer_->StopRendering();
  }

  void TearDownFileRenderer(ViEToFileRenderer* file_renderer) {
    bool test_failed = ::testing::UnitTest::GetInstance()->
        current_test_info()->result()->Failed();
    if (test_failed) {
      // Leave the files for analysis if the test failed.
      file_renderer->SaveOutputFile("failed-");
    } else {
      // No reason to keep the files if we succeeded.
      file_renderer->DeleteOutputFile();
    }
  }

  void CompareFiles(const std::string& reference_file,
                    const std::string& test_file,
                    double minimum_psnr, double minimum_ssim) {
    QualityMetricsResult psnr;
    int psnr_error = PsnrFromFiles(reference_file.c_str(), test_file.c_str(),
                                   kInputWidth, kInputHeight, &psnr);
    ASSERT_EQ(0, psnr_error) << "PSNR routine failed - output files missing?";
    ASSERT_GT(psnr.average, minimum_psnr);

    QualityMetricsResult ssim;
    int ssim_error = SsimFromFiles(reference_file.c_str(), test_file.c_str(),
                                   kInputWidth, kInputHeight, &ssim);
    ASSERT_EQ(0, ssim_error) << "SSIM routine failed - output files missing?";
    ASSERT_GT(ssim.average, minimum_ssim);  // 1 = perfect, -1 = terrible

    ViETest::Log("Results: PSNR: %f (db)   SSIM: %f",
                 psnr.average, ssim.average);
  }

  std::string input_file_;
  ViEToFileRenderer* local_file_renderer_;
  ViEToFileRenderer* remote_file_renderer_;
  ViEFileBasedComparisonTests tests_;

 private:
  void SetUpFileRenderer(ViEToFileRenderer* file_renderer,
                         const std::string& suffix) {
    std::string test_case_name =
        ::testing::UnitTest::GetInstance()->current_test_info()->name();

    std::string output_path = ViETest::GetResultOutputPath();
    std::string filename = test_case_name + suffix;

    if (!file_renderer->PrepareForRendering(output_path, filename)) {
      FAIL() << "Could not open output file " << filename <<
          " for writing.";
    }
  }
};

TEST_F(ViEVideoVerificationTest, RunsBaseStandardTestWithoutErrors)  {
  ASSERT_TRUE(tests_.TestCallSetup(input_file_, kInputWidth, kInputHeight,
                                   local_file_renderer_,
                                   remote_file_renderer_));
  std::string output_file = remote_file_renderer_->GetFullOutputPath();
  StopRenderers();
  CompareFiles(input_file_, output_file, kMinPSNR420DefaultBitRateQCIF,
               kMinSSIM420DefaultBitRateQCIF);
}

TEST_F(ViEVideoVerificationTest, RunsCodecTestWithoutErrors)  {
  ASSERT_TRUE(tests_.TestCodecs(input_file_, kInputWidth, kInputHeight,
                                local_file_renderer_,
                                remote_file_renderer_));
  std::string reference_file = local_file_renderer_->GetFullOutputPath();
  std::string output_file = remote_file_renderer_->GetFullOutputPath();
  StopRenderers();

  // We compare the local and remote here instead of with the original.
  // The reason is that it is hard to say when the three consecutive tests
  // switch over into each other, at which point we would have to restart the
  // original to get a fair comparison.
  CompareFiles(reference_file, output_file, kMinPSNRCodecTestsQCIF,
               kMinSSIMCodecTestsQCIF);

  // TODO(phoglund): The values should be higher. Investigate why the remote
  // file turns out 6 seconds shorter than the local file (frame dropping?).
}

// Runs a whole stack processing with tracking of which frames are dropped
// in the encoder. The local and remote file will not be of equal size because
// of unknown reasons. Tests show that they start at the same frame, which is
// the important thing when doing frame-to-frame comparison with PSNR/SSIM.
TEST_F(ViEVideoVerificationTest, RunsFullStackWithoutErrors)  {
  // Use our own FrameDropMonitoringRemoteFileRenderer instead of the
  // ViEToFileRenderer from the test fixture:
  // TODO(kjellander): Find a better way to reuse this code without duplication.
  remote_file_renderer_->StopRendering();
  TearDownFileRenderer(remote_file_renderer_);
  delete remote_file_renderer_;

  FrameDropDetector detector;
  remote_file_renderer_ = new FrameDropMonitoringRemoteFileRenderer(&detector);
  SetUpRemoteFileRenderer(remote_file_renderer_);

  // Set a low bit rate so the encoder budget will be tight, causing it to drop
  // frames every now and then.
  const int kBitRateKbps = 50;
  ViETest::Log("Bit rate: %d kbps.\n", kBitRateKbps);
  tests_.TestFullStack(input_file_, kInputWidth, kInputHeight, kBitRateKbps,
                       local_file_renderer_, remote_file_renderer_, &detector);
  const std::string reference_file = local_file_renderer_->GetFullOutputPath();
  const std::string output_file = remote_file_renderer_->GetFullOutputPath();
  StopRenderers();

  ASSERT_EQ(detector.GetFramesDroppedAtRenderStep().size(),
      detector.GetFramesDroppedAtDecodeStep().size())
      << "The number of dropped frames on the decode and render are not equal, "
      "this may be because we have a major problem in the jitter buffer?";

  detector.PrintReport();

  // We may have dropped frames during the processing, which means the output
  // file does not contain all the frames that are present in the input file.
  // To make the quality measurement correct, we must adjust the output file to
  // that by copying the last successful frame into the place where the dropped
  // frame would be, for all dropped frames.
  const int frame_length_in_bytes = 3 * kInputHeight * kInputWidth / 2;
  int num_frames = detector.NumberSentFrames();
  ViETest::Log("Frame length: %d bytes\n", frame_length_in_bytes);
  FixOutputFileForComparison(output_file, num_frames, frame_length_in_bytes,
                             detector.GetFramesDroppedAtDecodeStep());

  // Verify all sent frames are present in the output file.
  size_t output_file_size = webrtc::test::GetFileSize(output_file);
  EXPECT_EQ(num_frames,
      static_cast<int>(output_file_size / frame_length_in_bytes))
      << "The output file size is incorrect. It should be equal to the number"
      "of frames multiplied by the frame size. This will likely affect "
      "PSNR/SSIM calculations in a bad way.";
  CompareFiles(reference_file, output_file, kMinPSNR50kbpsQCIF,
               kMinSSIM50kbpsQCIF);
}

}  // namespace
