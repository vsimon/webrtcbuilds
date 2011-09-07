/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>

#include <gtest/gtest.h>

#include "audio_processing.h"
#include "event_wrapper.h"
#include "module_common_types.h"
#include "signal_processing_library.h"
#include "thread_wrapper.h"
#include "trace.h"
#ifdef WEBRTC_ANDROID
#include "external/webrtc/src/modules/audio_processing/main/test/unit_test/unittest.pb.h"
#else
#include "webrtc/audio_processing/unittest.pb.h"
#endif

using webrtc::AudioProcessing;
using webrtc::AudioFrame;
using webrtc::GainControl;
using webrtc::NoiseSuppression;
using webrtc::EchoCancellation;
using webrtc::EventWrapper;
using webrtc::Trace;
using webrtc::LevelEstimator;
using webrtc::EchoCancellation;
using webrtc::EchoControlMobile;
using webrtc::VoiceDetection;

namespace {
// When false, this will compare the output data with the results stored to
// file. This is the typical case. When the file should be updated, it can
// be set to true with the command-line switch --write_output_data.
bool write_output_data = false;

#if defined(WEBRTC_APM_UNIT_TEST_FIXED_PROFILE)
const char kOutputFileName[] = "output_data_fixed.pb";
#elif defined(WEBRTC_APM_UNIT_TEST_FLOAT_PROFILE)
const char kOutputFileName[] = "output_data_float.pb";
#endif

class ApmEnvironment : public ::testing::Environment {
 public:
  virtual void SetUp() {
    Trace::CreateTrace();
    ASSERT_EQ(0, Trace::SetTraceFile("apm_trace.txt"));
  }

  virtual void TearDown() {
    Trace::ReturnTrace();
  }
};

class ApmTest : public ::testing::Test {
 protected:
  ApmTest();
  virtual void SetUp();
  virtual void TearDown();

  webrtc::AudioProcessing* apm_;
  webrtc::AudioFrame* frame_;
  webrtc::AudioFrame* revframe_;
  FILE* far_file_;
  FILE* near_file_;
};

ApmTest::ApmTest()
    : apm_(NULL),
      frame_(NULL),
      revframe_(NULL),
      far_file_(NULL),
      near_file_(NULL) {}

void ApmTest::SetUp() {
  apm_ = AudioProcessing::Create(0);
  ASSERT_TRUE(apm_ != NULL);

  frame_ = new AudioFrame();
  revframe_ = new AudioFrame();

  ASSERT_EQ(apm_->kNoError, apm_->set_sample_rate_hz(32000));
  ASSERT_EQ(apm_->kNoError, apm_->set_num_channels(2, 2));
  ASSERT_EQ(apm_->kNoError, apm_->set_num_reverse_channels(2));

  frame_->_payloadDataLengthInSamples = 320;
  frame_->_audioChannel = 2;
  frame_->_frequencyInHz = 32000;
  revframe_->_payloadDataLengthInSamples = 320;
  revframe_->_audioChannel = 2;
  revframe_->_frequencyInHz = 32000;

  far_file_ = fopen("aec_far.pcm", "rb");
  ASSERT_TRUE(far_file_ != NULL) << "Could not open input file aec_far.pcm\n";
  near_file_ = fopen("aec_near.pcm", "rb");
  ASSERT_TRUE(near_file_ != NULL) << "Could not open input file aec_near.pcm\n";
}

void ApmTest::TearDown() {
  if (frame_) {
    delete frame_;
  }
  frame_ = NULL;

  if (revframe_) {
    delete revframe_;
  }
  revframe_ = NULL;

  if (far_file_) {
    ASSERT_EQ(0, fclose(far_file_));
  }
  far_file_ = NULL;

  if (near_file_) {
    ASSERT_EQ(0, fclose(near_file_));
  }
  near_file_ = NULL;

  if (apm_ != NULL) {
    AudioProcessing::Destroy(apm_);
  }
  apm_ = NULL;
}

void MixStereoToMono(const WebRtc_Word16* stereo,
                     WebRtc_Word16* mono,
                     int num_samples) {
  for (int i = 0; i < num_samples; i++) {
    int int32 = (static_cast<int>(stereo[i * 2]) +
                 static_cast<int>(stereo[i * 2 + 1])) >> 1;
    mono[i] = static_cast<WebRtc_Word16>(int32);
  }
}

template <class T>
T MaxValue(T a, T b) {
  return a > b ? a : b;
}

template <class T>
T AbsValue(T a) {
  return a > 0 ? a : -a;
}

WebRtc_Word16 MaxAudioFrame(const AudioFrame& frame) {
  const int length = frame._payloadDataLengthInSamples * frame._audioChannel;
  WebRtc_Word16 max = AbsValue(frame._payloadData[0]);
  for (int i = 1; i < length; i++) {
    max = MaxValue(max, AbsValue(frame._payloadData[i]));
  }

  return max;
}

void TestStats(const AudioProcessing::Statistic& test,
               const webrtc::audioproc::Test::Statistic& reference) {
  EXPECT_EQ(reference.instant(), test.instant);
  EXPECT_EQ(reference.average(), test.average);
  EXPECT_EQ(reference.maximum(), test.maximum);
  EXPECT_EQ(reference.minimum(), test.minimum);
}

void WriteStatsMessage(const AudioProcessing::Statistic& output,
                       webrtc::audioproc::Test::Statistic* message) {
  message->set_instant(output.instant);
  message->set_average(output.average);
  message->set_maximum(output.maximum);
  message->set_minimum(output.minimum);
}

void WriteMessageLiteToFile(const char* filename,
                            const ::google::protobuf::MessageLite& message) {
  assert(filename != NULL);

  FILE* file = fopen(filename, "wb");
  ASSERT_TRUE(file != NULL) << "Could not open " << filename;
  int size = message.ByteSize();
  ASSERT_GT(size, 0);
  unsigned char* array = new unsigned char[size];
  ASSERT_TRUE(message.SerializeToArray(array, size));

  ASSERT_EQ(1u, fwrite(&size, sizeof(int), 1, file));
  ASSERT_EQ(static_cast<size_t>(size),
      fwrite(array, sizeof(unsigned char), size, file));

  delete [] array;
  fclose(file);
}

void ReadMessageLiteFromFile(const char* filename,
                             ::google::protobuf::MessageLite* message) {
  assert(filename != NULL);
  assert(message != NULL);

  FILE* file = fopen(filename, "rb");
  ASSERT_TRUE(file != NULL) << "Could not open " << filename;
  int size = 0;
  ASSERT_EQ(1u, fread(&size, sizeof(int), 1, file));
  ASSERT_GT(size, 0);
  unsigned char* array = new unsigned char[size];
  ASSERT_EQ(static_cast<size_t>(size),
      fread(array, sizeof(unsigned char), size, file));

  ASSERT_TRUE(message->ParseFromArray(array, size));

  delete [] array;
  fclose(file);
}

struct ThreadData {
  ThreadData(int thread_num_, AudioProcessing* ap_)
      : thread_num(thread_num_),
        error(false),
        ap(ap_) {}
  int thread_num;
  bool error;
  AudioProcessing* ap;
};

// Don't use GTest here; non-thread-safe on Windows (as of 1.5.0).
bool DeadlockProc(void* thread_object) {
  ThreadData* thread_data = static_cast<ThreadData*>(thread_object);
  AudioProcessing* ap = thread_data->ap;
  int err = ap->kNoError;

  AudioFrame primary_frame;
  AudioFrame reverse_frame;
  primary_frame._payloadDataLengthInSamples = 320;
  primary_frame._audioChannel = 2;
  primary_frame._frequencyInHz = 32000;
  reverse_frame._payloadDataLengthInSamples = 320;
  reverse_frame._audioChannel = 2;
  reverse_frame._frequencyInHz = 32000;

  ap->echo_cancellation()->Enable(true);
  ap->gain_control()->Enable(true);
  ap->high_pass_filter()->Enable(true);
  ap->level_estimator()->Enable(true);
  ap->noise_suppression()->Enable(true);
  ap->voice_detection()->Enable(true);

  if (thread_data->thread_num % 2 == 0) {
    err = ap->AnalyzeReverseStream(&reverse_frame);
    if (err != ap->kNoError) {
      printf("Error in AnalyzeReverseStream(): %d\n", err);
      thread_data->error = true;
      return false;
    }
  }

  if (thread_data->thread_num % 2 == 1) {
    ap->set_stream_delay_ms(0);
    ap->echo_cancellation()->set_stream_drift_samples(0);
    ap->gain_control()->set_stream_analog_level(0);
    err = ap->ProcessStream(&primary_frame);
    if (err == ap->kStreamParameterNotSetError) {
      printf("Expected kStreamParameterNotSetError in ProcessStream(): %d\n",
          err);
    } else if (err != ap->kNoError) {
      printf("Error in ProcessStream(): %d\n", err);
      thread_data->error = true;
      return false;
    }
    ap->gain_control()->stream_analog_level();
  }

  EventWrapper* event = EventWrapper::Create();
  event->Wait(1);
  delete event;
  event = NULL;

  return true;
}

/*TEST_F(ApmTest, Deadlock) {
  const int num_threads = 16;
  std::vector<ThreadWrapper*> threads(num_threads);
  std::vector<ThreadData*> thread_data(num_threads);

  ASSERT_EQ(apm_->kNoError, apm_->set_sample_rate_hz(32000));
  ASSERT_EQ(apm_->kNoError, apm_->set_num_channels(2, 2));
  ASSERT_EQ(apm_->kNoError, apm_->set_num_reverse_channels(2));

  for (int i = 0; i < num_threads; i++) {
    thread_data[i] = new ThreadData(i, apm_);
    threads[i] = ThreadWrapper::CreateThread(DeadlockProc,
                                             thread_data[i],
                                             kNormalPriority,
                                             0);
    ASSERT_TRUE(threads[i] != NULL);
    unsigned int thread_id = 0;
    threads[i]->Start(thread_id);
  }

  EventWrapper* event = EventWrapper::Create();
  ASSERT_EQ(kEventTimeout, event->Wait(5000));
  delete event;
  event = NULL;

  for (int i = 0; i < num_threads; i++) {
    // This will return false if the thread has deadlocked.
    ASSERT_TRUE(threads[i]->Stop());
    ASSERT_FALSE(thread_data[i]->error);
    delete threads[i];
    threads[i] = NULL;
    delete thread_data[i];
    thread_data[i] = NULL;
  }
}*/

TEST_F(ApmTest, StreamParameters) {
  // No errors when the components are disabled.
  EXPECT_EQ(apm_->kNoError,
            apm_->ProcessStream(frame_));

  // Missing agc level
  EXPECT_EQ(apm_->kNoError, apm_->Initialize());
  EXPECT_EQ(apm_->kNoError, apm_->gain_control()->Enable(true));
  EXPECT_EQ(apm_->kStreamParameterNotSetError,
            apm_->ProcessStream(frame_));
  EXPECT_EQ(apm_->kNoError, apm_->set_stream_delay_ms(100));
  EXPECT_EQ(apm_->kNoError,
            apm_->echo_cancellation()->set_stream_drift_samples(0));
  EXPECT_EQ(apm_->kStreamParameterNotSetError,
            apm_->ProcessStream(frame_));
  EXPECT_EQ(apm_->kNoError, apm_->gain_control()->Enable(false));

  // Missing delay
  EXPECT_EQ(apm_->kNoError, apm_->Initialize());
  EXPECT_EQ(apm_->kNoError, apm_->echo_cancellation()->Enable(true));
  EXPECT_EQ(apm_->kStreamParameterNotSetError,
            apm_->ProcessStream(frame_));
  EXPECT_EQ(apm_->kNoError, apm_->gain_control()->Enable(true));
  EXPECT_EQ(apm_->kNoError,
            apm_->echo_cancellation()->set_stream_drift_samples(0));
  EXPECT_EQ(apm_->kNoError,
            apm_->gain_control()->set_stream_analog_level(127));
  EXPECT_EQ(apm_->kStreamParameterNotSetError,
            apm_->ProcessStream(frame_));
  EXPECT_EQ(apm_->kNoError, apm_->gain_control()->Enable(false));

  // Missing drift
  EXPECT_EQ(apm_->kNoError, apm_->Initialize());
  EXPECT_EQ(apm_->kNoError,
            apm_->echo_cancellation()->enable_drift_compensation(true));
  EXPECT_EQ(apm_->kStreamParameterNotSetError,
            apm_->ProcessStream(frame_));
  EXPECT_EQ(apm_->kNoError, apm_->gain_control()->Enable(true));
  EXPECT_EQ(apm_->kNoError, apm_->set_stream_delay_ms(100));
  EXPECT_EQ(apm_->kNoError,
            apm_->gain_control()->set_stream_analog_level(127));
  EXPECT_EQ(apm_->kStreamParameterNotSetError,
            apm_->ProcessStream(frame_));

  // No stream parameters
  EXPECT_EQ(apm_->kNoError, apm_->Initialize());
  EXPECT_EQ(apm_->kNoError,
            apm_->AnalyzeReverseStream(revframe_));
  EXPECT_EQ(apm_->kStreamParameterNotSetError,
            apm_->ProcessStream(frame_));

  // All there
  EXPECT_EQ(apm_->kNoError, apm_->gain_control()->Enable(true));
  EXPECT_EQ(apm_->kNoError, apm_->Initialize());
  EXPECT_EQ(apm_->kNoError, apm_->set_stream_delay_ms(100));
  EXPECT_EQ(apm_->kNoError,
            apm_->echo_cancellation()->set_stream_drift_samples(0));
  EXPECT_EQ(apm_->kNoError,
            apm_->gain_control()->set_stream_analog_level(127));
  EXPECT_EQ(apm_->kNoError, apm_->ProcessStream(frame_));
}

TEST_F(ApmTest, Channels) {
  // Testing number of invalid channels
  EXPECT_EQ(apm_->kBadParameterError, apm_->set_num_channels(0, 1));
  EXPECT_EQ(apm_->kBadParameterError, apm_->set_num_channels(1, 0));
  EXPECT_EQ(apm_->kBadParameterError, apm_->set_num_channels(3, 1));
  EXPECT_EQ(apm_->kBadParameterError, apm_->set_num_channels(1, 3));
  EXPECT_EQ(apm_->kBadParameterError, apm_->set_num_reverse_channels(0));
  EXPECT_EQ(apm_->kBadParameterError, apm_->set_num_reverse_channels(3));
  // Testing number of valid channels
  for (int i = 1; i < 3; i++) {
    for (int j = 1; j < 3; j++) {
      if (j > i) {
        EXPECT_EQ(apm_->kBadParameterError, apm_->set_num_channels(i, j));
      } else {
        EXPECT_EQ(apm_->kNoError, apm_->set_num_channels(i, j));
        EXPECT_EQ(j, apm_->num_output_channels());
      }
    }
    EXPECT_EQ(i, apm_->num_input_channels());
    EXPECT_EQ(apm_->kNoError, apm_->set_num_reverse_channels(i));
    EXPECT_EQ(i, apm_->num_reverse_channels());
  }
}

TEST_F(ApmTest, SampleRates) {
  // Testing invalid sample rates
  EXPECT_EQ(apm_->kBadParameterError, apm_->set_sample_rate_hz(10000));
  // Testing valid sample rates
  int fs[] = {8000, 16000, 32000};
  for (size_t i = 0; i < sizeof(fs) / sizeof(*fs); i++) {
    EXPECT_EQ(apm_->kNoError, apm_->set_sample_rate_hz(fs[i]));
    EXPECT_EQ(fs[i], apm_->sample_rate_hz());
  }
}

TEST_F(ApmTest, Process) {
  GOOGLE_PROTOBUF_VERIFY_VERSION;
  webrtc::audioproc::OutputData output_data;

  if (!write_output_data) {
    ReadMessageLiteFromFile(kOutputFileName, &output_data);
  } else {
    // We don't have a file; add the required tests to the protobuf.
    // TODO(ajm): vary the output channels as well?
    const int channels[] = {1, 2};
    const size_t channels_size = sizeof(channels) / sizeof(*channels);
#if defined(WEBRTC_APM_UNIT_TEST_FIXED_PROFILE)
    // AECM doesn't support super-wb.
    const int sample_rates[] = {8000, 16000};
#elif defined(WEBRTC_APM_UNIT_TEST_FLOAT_PROFILE)
    const int sample_rates[] = {8000, 16000, 32000};
#endif
    const size_t sample_rates_size = sizeof(sample_rates) / sizeof(*sample_rates);
    for (size_t i = 0; i < channels_size; i++) {
      for (size_t j = 0; j < channels_size; j++) {
        for (size_t k = 0; k < sample_rates_size; k++) {
          webrtc::audioproc::Test* test = output_data.add_test();
          test->set_num_reverse_channels(channels[i]);
          test->set_num_input_channels(channels[j]);
          test->set_num_output_channels(channels[j]);
          test->set_sample_rate(sample_rates[k]);
        }
      }
    }
  }

#if defined(WEBRTC_APM_UNIT_TEST_FIXED_PROFILE)
  EXPECT_EQ(apm_->kNoError, apm_->set_sample_rate_hz(16000));
  EXPECT_EQ(apm_->kNoError, apm_->echo_control_mobile()->Enable(true));

  EXPECT_EQ(apm_->kNoError,
            apm_->gain_control()->set_mode(GainControl::kAdaptiveDigital));
  EXPECT_EQ(apm_->kNoError, apm_->gain_control()->Enable(true));
#elif defined(WEBRTC_APM_UNIT_TEST_FLOAT_PROFILE)
  EXPECT_EQ(apm_->kNoError,
            apm_->echo_cancellation()->enable_drift_compensation(true));
  EXPECT_EQ(apm_->kNoError,
            apm_->echo_cancellation()->enable_metrics(true));
  EXPECT_EQ(apm_->kNoError, apm_->echo_cancellation()->Enable(true));

  EXPECT_EQ(apm_->kNoError,
            apm_->gain_control()->set_mode(GainControl::kAdaptiveAnalog));
  EXPECT_EQ(apm_->kNoError,
            apm_->gain_control()->set_analog_level_limits(0, 255));
  EXPECT_EQ(apm_->kNoError, apm_->gain_control()->Enable(true));
#endif

  EXPECT_EQ(apm_->kNoError,
            apm_->high_pass_filter()->Enable(true));

  //EXPECT_EQ(apm_->kNoError,
  //          apm_->level_estimator()->Enable(true));

  EXPECT_EQ(apm_->kNoError,
            apm_->noise_suppression()->Enable(true));

  EXPECT_EQ(apm_->kNoError,
            apm_->voice_detection()->Enable(true));

  for (int i = 0; i < output_data.test_size(); i++) {
    printf("Running test %d of %d...\n", i + 1, output_data.test_size());

    webrtc::audioproc::Test* test = output_data.mutable_test(i);
    const int num_samples = test->sample_rate() / 100;
    revframe_->_payloadDataLengthInSamples = num_samples;
    revframe_->_audioChannel = test->num_reverse_channels();
    revframe_->_frequencyInHz = test->sample_rate();
    frame_->_payloadDataLengthInSamples = num_samples;
    frame_->_audioChannel = test->num_input_channels();
    frame_->_frequencyInHz = test->sample_rate();

    EXPECT_EQ(apm_->kNoError, apm_->Initialize());
    ASSERT_EQ(apm_->kNoError, apm_->set_sample_rate_hz(test->sample_rate()));
    ASSERT_EQ(apm_->kNoError, apm_->set_num_channels(frame_->_audioChannel,
                                                     frame_->_audioChannel));
    ASSERT_EQ(apm_->kNoError,
        apm_->set_num_reverse_channels(revframe_->_audioChannel));

    int frame_count = 0;
    int has_echo_count = 0;
    int has_voice_count = 0;
    int is_saturated_count = 0;
    int analog_level = 127;
    int analog_level_average = 0;
    int max_output_average = 0;

    while (1) {
      WebRtc_Word16 temp_data[640];

      // Read far-end frame
      size_t read_count = fread(temp_data,
                                sizeof(WebRtc_Word16),
                                num_samples * 2,
                                far_file_);
      if (read_count != static_cast<size_t>(num_samples * 2)) {
        // Check that the file really ended.
        ASSERT_NE(0, feof(far_file_));
        break; // This is expected.
      }

      if (revframe_->_audioChannel == 1) {
        MixStereoToMono(temp_data, revframe_->_payloadData,
            revframe_->_payloadDataLengthInSamples);
      } else {
        memcpy(revframe_->_payloadData,
               &temp_data[0],
               sizeof(WebRtc_Word16) * read_count);
      }

      EXPECT_EQ(apm_->kNoError,
          apm_->AnalyzeReverseStream(revframe_));

      EXPECT_EQ(apm_->kNoError, apm_->set_stream_delay_ms(0));
      EXPECT_EQ(apm_->kNoError,
          apm_->echo_cancellation()->set_stream_drift_samples(0));
      EXPECT_EQ(apm_->kNoError,
          apm_->gain_control()->set_stream_analog_level(analog_level));

      // Read near-end frame
      read_count = fread(temp_data,
                         sizeof(WebRtc_Word16),
                         num_samples * 2,
                         near_file_);
      if (read_count != static_cast<size_t>(num_samples * 2)) {
        // Check that the file really ended.
        ASSERT_NE(0, feof(near_file_));
        break; // This is expected.
      }

      if (frame_->_audioChannel == 1) {
        MixStereoToMono(temp_data, frame_->_payloadData, num_samples);
      } else {
        memcpy(frame_->_payloadData,
               &temp_data[0],
               sizeof(WebRtc_Word16) * read_count);
      }

      EXPECT_EQ(apm_->kNoError, apm_->ProcessStream(frame_));

      max_output_average += MaxAudioFrame(*frame_);

      if (apm_->echo_cancellation()->stream_has_echo()) {
        has_echo_count++;
      }

      analog_level = apm_->gain_control()->stream_analog_level();
      analog_level_average += analog_level;
      if (apm_->gain_control()->stream_is_saturated()) {
        is_saturated_count++;
      }
      if (apm_->voice_detection()->stream_has_voice()) {
        has_voice_count++;
      }

      frame_count++;
    }
    max_output_average /= frame_count;
    analog_level_average /= frame_count;

    //LevelEstimator::Metrics far_metrics;
    //LevelEstimator::Metrics near_metrics;
    //EXPECT_EQ(apm_->kNoError,
    //          apm_->level_estimator()->GetMetrics(&near_metrics,

#if defined(WEBRTC_APM_UNIT_TEST_FLOAT_PROFILE)
    EchoCancellation::Metrics echo_metrics;
    EXPECT_EQ(apm_->kNoError,
              apm_->echo_cancellation()->GetMetrics(&echo_metrics));
#endif

    if (!write_output_data) {
      EXPECT_EQ(test->has_echo_count(), has_echo_count);
      EXPECT_EQ(test->has_voice_count(), has_voice_count);
      EXPECT_EQ(test->is_saturated_count(), is_saturated_count);

      EXPECT_EQ(test->analog_level_average(), analog_level_average);
      EXPECT_EQ(test->max_output_average(), max_output_average);

#if defined(WEBRTC_APM_UNIT_TEST_FLOAT_PROFILE)
      webrtc::audioproc::Test::EchoMetrics reference =
          test->echo_metrics();
      TestStats(echo_metrics.residual_echo_return_loss,
                reference.residual_echo_return_loss());
      TestStats(echo_metrics.echo_return_loss,
                reference.echo_return_loss());
      TestStats(echo_metrics.echo_return_loss_enhancement,
                reference.echo_return_loss_enhancement());
      TestStats(echo_metrics.a_nlp,
                reference.a_nlp());
#endif
    } else {
      test->set_has_echo_count(has_echo_count);
      test->set_has_voice_count(has_voice_count);
      test->set_is_saturated_count(is_saturated_count);

      test->set_analog_level_average(analog_level_average);
      test->set_max_output_average(max_output_average);

#if defined(WEBRTC_APM_UNIT_TEST_FLOAT_PROFILE)
      webrtc::audioproc::Test::EchoMetrics* message =
          test->mutable_echo_metrics();
      WriteStatsMessage(echo_metrics.residual_echo_return_loss,
                        message->mutable_residual_echo_return_loss());
      WriteStatsMessage(echo_metrics.echo_return_loss,
                        message->mutable_echo_return_loss());
      WriteStatsMessage(echo_metrics.echo_return_loss_enhancement,
                        message->mutable_echo_return_loss_enhancement());
      WriteStatsMessage(echo_metrics.a_nlp,
                        message->mutable_a_nlp());
#endif
    }

    rewind(far_file_);
    rewind(near_file_);
  }

  if (write_output_data) {
    WriteMessageLiteToFile(kOutputFileName, output_data);
  }
}

TEST_F(ApmTest, EchoCancellation) {
  EXPECT_EQ(apm_->kNoError,
            apm_->echo_cancellation()->enable_drift_compensation(true));
  EXPECT_TRUE(apm_->echo_cancellation()->is_drift_compensation_enabled());
  EXPECT_EQ(apm_->kNoError,
            apm_->echo_cancellation()->enable_drift_compensation(false));
  EXPECT_FALSE(apm_->echo_cancellation()->is_drift_compensation_enabled());

  EXPECT_EQ(apm_->kBadParameterError,
      apm_->echo_cancellation()->set_device_sample_rate_hz(4000));
  EXPECT_EQ(apm_->kBadParameterError,
      apm_->echo_cancellation()->set_device_sample_rate_hz(100000));

  int rate[] = {16000, 44100, 48000};
  for (size_t i = 0; i < sizeof(rate)/sizeof(*rate); i++) {
    EXPECT_EQ(apm_->kNoError,
        apm_->echo_cancellation()->set_device_sample_rate_hz(rate[i]));
    EXPECT_EQ(rate[i],
        apm_->echo_cancellation()->device_sample_rate_hz());
  }

  EXPECT_EQ(apm_->kBadParameterError,
      apm_->echo_cancellation()->set_suppression_level(
          static_cast<EchoCancellation::SuppressionLevel>(-1)));

  EXPECT_EQ(apm_->kBadParameterError,
      apm_->echo_cancellation()->set_suppression_level(
          static_cast<EchoCancellation::SuppressionLevel>(4)));

  EchoCancellation::SuppressionLevel level[] = {
    EchoCancellation::kLowSuppression,
    EchoCancellation::kModerateSuppression,
    EchoCancellation::kHighSuppression,
  };
  for (size_t i = 0; i < sizeof(level)/sizeof(*level); i++) {
    EXPECT_EQ(apm_->kNoError,
        apm_->echo_cancellation()->set_suppression_level(level[i]));
    EXPECT_EQ(level[i],
        apm_->echo_cancellation()->suppression_level());
  }

  EchoCancellation::Metrics metrics;
  EXPECT_EQ(apm_->kNotEnabledError,
            apm_->echo_cancellation()->GetMetrics(&metrics));

  EXPECT_EQ(apm_->kNoError,
            apm_->echo_cancellation()->enable_metrics(true));
  EXPECT_TRUE(apm_->echo_cancellation()->are_metrics_enabled());
  EXPECT_EQ(apm_->kNoError,
            apm_->echo_cancellation()->enable_metrics(false));
  EXPECT_FALSE(apm_->echo_cancellation()->are_metrics_enabled());

  EXPECT_EQ(apm_->kNoError, apm_->echo_cancellation()->Enable(true));
  EXPECT_TRUE(apm_->echo_cancellation()->is_enabled());
  EXPECT_EQ(apm_->kNoError, apm_->echo_cancellation()->Enable(false));
  EXPECT_FALSE(apm_->echo_cancellation()->is_enabled());
}

TEST_F(ApmTest, EchoControlMobile) {
  // AECM won't use super-wideband.
  EXPECT_EQ(apm_->kNoError, apm_->set_sample_rate_hz(32000));
  EXPECT_EQ(apm_->kBadSampleRateError, apm_->echo_control_mobile()->Enable(true));
  EXPECT_EQ(apm_->kNoError, apm_->set_sample_rate_hz(16000));
  // Turn AECM on (and AEC off)
  EXPECT_EQ(apm_->kNoError, apm_->echo_control_mobile()->Enable(true));
  EXPECT_TRUE(apm_->echo_control_mobile()->is_enabled());

  EXPECT_EQ(apm_->kBadParameterError,
      apm_->echo_control_mobile()->set_routing_mode(
      static_cast<EchoControlMobile::RoutingMode>(-1)));
  EXPECT_EQ(apm_->kBadParameterError,
      apm_->echo_control_mobile()->set_routing_mode(
      static_cast<EchoControlMobile::RoutingMode>(5)));

  // Toggle routing modes
  EchoControlMobile::RoutingMode mode[] = {
      EchoControlMobile::kQuietEarpieceOrHeadset,
      EchoControlMobile::kEarpiece,
      EchoControlMobile::kLoudEarpiece,
      EchoControlMobile::kSpeakerphone,
      EchoControlMobile::kLoudSpeakerphone,
  };
  for (size_t i = 0; i < sizeof(mode)/sizeof(*mode); i++) {
    EXPECT_EQ(apm_->kNoError,
        apm_->echo_control_mobile()->set_routing_mode(mode[i]));
    EXPECT_EQ(mode[i],
        apm_->echo_control_mobile()->routing_mode());
  }
  // Turn comfort noise off/on
  EXPECT_EQ(apm_->kNoError,
      apm_->echo_control_mobile()->enable_comfort_noise(false));
  EXPECT_FALSE(apm_->echo_control_mobile()->is_comfort_noise_enabled());
  EXPECT_EQ(apm_->kNoError,
      apm_->echo_control_mobile()->enable_comfort_noise(true));
  EXPECT_TRUE(apm_->echo_control_mobile()->is_comfort_noise_enabled());
  // Set and get echo path
  const size_t echo_path_size =
      apm_->echo_control_mobile()->echo_path_size_bytes();
  unsigned char echo_path_in[echo_path_size];
  unsigned char echo_path_out[echo_path_size];
  EXPECT_EQ(apm_->kNullPointerError,
            apm_->echo_control_mobile()->SetEchoPath(NULL, echo_path_size));
  EXPECT_EQ(apm_->kNullPointerError,
            apm_->echo_control_mobile()->GetEchoPath(NULL, echo_path_size));
  EXPECT_EQ(apm_->kBadParameterError,
            apm_->echo_control_mobile()->GetEchoPath(echo_path_out, 1));
  EXPECT_EQ(apm_->kNoError,
            apm_->echo_control_mobile()->GetEchoPath(echo_path_out,
                                                     echo_path_size));
  for (size_t i = 0; i < echo_path_size; i++) {
    echo_path_in[i] = echo_path_out[i] + 1;
  }
  EXPECT_EQ(apm_->kBadParameterError,
            apm_->echo_control_mobile()->SetEchoPath(echo_path_in, 1));
  EXPECT_EQ(apm_->kNoError,
            apm_->echo_control_mobile()->SetEchoPath(echo_path_in, echo_path_size));
  EXPECT_EQ(apm_->kNoError,
            apm_->echo_control_mobile()->GetEchoPath(echo_path_out, echo_path_size));
  for (size_t i = 0; i < echo_path_size; i++) {
    EXPECT_EQ(echo_path_in[i], echo_path_out[i]);
  }
  // Turn AECM off
  EXPECT_EQ(apm_->kNoError, apm_->echo_control_mobile()->Enable(false));
  EXPECT_FALSE(apm_->echo_control_mobile()->is_enabled());
}

TEST_F(ApmTest, GainControl) {
  // Testing gain modes
  EXPECT_EQ(apm_->kBadParameterError,
      apm_->gain_control()->set_mode(static_cast<GainControl::Mode>(-1)));

  EXPECT_EQ(apm_->kBadParameterError,
      apm_->gain_control()->set_mode(static_cast<GainControl::Mode>(3)));

  EXPECT_EQ(apm_->kNoError,
      apm_->gain_control()->set_mode(
      apm_->gain_control()->mode()));

  GainControl::Mode mode[] = {
    GainControl::kAdaptiveAnalog,
    GainControl::kAdaptiveDigital,
    GainControl::kFixedDigital
  };
  for (size_t i = 0; i < sizeof(mode)/sizeof(*mode); i++) {
    EXPECT_EQ(apm_->kNoError,
        apm_->gain_control()->set_mode(mode[i]));
    EXPECT_EQ(mode[i], apm_->gain_control()->mode());
  }
  // Testing invalid target levels
  EXPECT_EQ(apm_->kBadParameterError,
      apm_->gain_control()->set_target_level_dbfs(-3));
  EXPECT_EQ(apm_->kBadParameterError,
      apm_->gain_control()->set_target_level_dbfs(-40));
  // Testing valid target levels
  EXPECT_EQ(apm_->kNoError,
      apm_->gain_control()->set_target_level_dbfs(
      apm_->gain_control()->target_level_dbfs()));

  int level_dbfs[] = {0, 6, 31};
  for (size_t i = 0; i < sizeof(level_dbfs)/sizeof(*level_dbfs); i++) {
    EXPECT_EQ(apm_->kNoError,
        apm_->gain_control()->set_target_level_dbfs(level_dbfs[i]));
    EXPECT_EQ(level_dbfs[i], apm_->gain_control()->target_level_dbfs());
  }

  // Testing invalid compression gains
  EXPECT_EQ(apm_->kBadParameterError,
      apm_->gain_control()->set_compression_gain_db(-1));
  EXPECT_EQ(apm_->kBadParameterError,
      apm_->gain_control()->set_compression_gain_db(100));

  // Testing valid compression gains
  EXPECT_EQ(apm_->kNoError,
      apm_->gain_control()->set_compression_gain_db(
      apm_->gain_control()->compression_gain_db()));

  int gain_db[] = {0, 10, 90};
  for (size_t i = 0; i < sizeof(gain_db)/sizeof(*gain_db); i++) {
    EXPECT_EQ(apm_->kNoError,
        apm_->gain_control()->set_compression_gain_db(gain_db[i]));
    EXPECT_EQ(gain_db[i], apm_->gain_control()->compression_gain_db());
  }

  // Testing limiter off/on
  EXPECT_EQ(apm_->kNoError, apm_->gain_control()->enable_limiter(false));
  EXPECT_FALSE(apm_->gain_control()->is_limiter_enabled());
  EXPECT_EQ(apm_->kNoError, apm_->gain_control()->enable_limiter(true));
  EXPECT_TRUE(apm_->gain_control()->is_limiter_enabled());

  // Testing invalid level limits
  EXPECT_EQ(apm_->kBadParameterError,
      apm_->gain_control()->set_analog_level_limits(-1, 512));
  EXPECT_EQ(apm_->kBadParameterError,
      apm_->gain_control()->set_analog_level_limits(100000, 512));
  EXPECT_EQ(apm_->kBadParameterError,
      apm_->gain_control()->set_analog_level_limits(512, -1));
  EXPECT_EQ(apm_->kBadParameterError,
      apm_->gain_control()->set_analog_level_limits(512, 100000));
  EXPECT_EQ(apm_->kBadParameterError,
      apm_->gain_control()->set_analog_level_limits(512, 255));

  // Testing valid level limits
  EXPECT_EQ(apm_->kNoError,
      apm_->gain_control()->set_analog_level_limits(
      apm_->gain_control()->analog_level_minimum(),
      apm_->gain_control()->analog_level_maximum()));

  int min_level[] = {0, 255, 1024};
  for (size_t i = 0; i < sizeof(min_level)/sizeof(*min_level); i++) {
    EXPECT_EQ(apm_->kNoError,
        apm_->gain_control()->set_analog_level_limits(min_level[i], 1024));
    EXPECT_EQ(min_level[i], apm_->gain_control()->analog_level_minimum());
  }

  int max_level[] = {0, 1024, 65535};
  for (size_t i = 0; i < sizeof(min_level)/sizeof(*min_level); i++) {
    EXPECT_EQ(apm_->kNoError,
        apm_->gain_control()->set_analog_level_limits(0, max_level[i]));
    EXPECT_EQ(max_level[i], apm_->gain_control()->analog_level_maximum());
  }

  // TODO(ajm): stream_is_saturated() and stream_analog_level()

  // Turn AGC off
  EXPECT_EQ(apm_->kNoError, apm_->gain_control()->Enable(false));
  EXPECT_FALSE(apm_->gain_control()->is_enabled());
}

TEST_F(ApmTest, NoiseSuppression) {
  // Tesing invalid suppression levels
  EXPECT_EQ(apm_->kBadParameterError,
      apm_->noise_suppression()->set_level(
          static_cast<NoiseSuppression::Level>(-1)));

  EXPECT_EQ(apm_->kBadParameterError,
      apm_->noise_suppression()->set_level(
          static_cast<NoiseSuppression::Level>(5)));

  // Tesing valid suppression levels
  NoiseSuppression::Level level[] = {
    NoiseSuppression::kLow,
    NoiseSuppression::kModerate,
    NoiseSuppression::kHigh,
    NoiseSuppression::kVeryHigh
  };
  for (size_t i = 0; i < sizeof(level)/sizeof(*level); i++) {
    EXPECT_EQ(apm_->kNoError,
        apm_->noise_suppression()->set_level(level[i]));
    EXPECT_EQ(level[i], apm_->noise_suppression()->level());
  }

  // Turing NS on/off
  EXPECT_EQ(apm_->kNoError, apm_->noise_suppression()->Enable(true));
  EXPECT_TRUE(apm_->noise_suppression()->is_enabled());
  EXPECT_EQ(apm_->kNoError, apm_->noise_suppression()->Enable(false));
  EXPECT_FALSE(apm_->noise_suppression()->is_enabled());
}

TEST_F(ApmTest, HighPassFilter) {
  // Turing HP filter on/off
  EXPECT_EQ(apm_->kNoError, apm_->high_pass_filter()->Enable(true));
  EXPECT_TRUE(apm_->high_pass_filter()->is_enabled());
  EXPECT_EQ(apm_->kNoError, apm_->high_pass_filter()->Enable(false));
  EXPECT_FALSE(apm_->high_pass_filter()->is_enabled());
}

TEST_F(ApmTest, LevelEstimator) {
  // Turing Level estimator on/off
  EXPECT_EQ(apm_->kUnsupportedComponentError,
            apm_->level_estimator()->Enable(true));
  EXPECT_FALSE(apm_->level_estimator()->is_enabled());
  EXPECT_EQ(apm_->kUnsupportedComponentError,
            apm_->level_estimator()->Enable(false));
  EXPECT_FALSE(apm_->level_estimator()->is_enabled());
}

TEST_F(ApmTest, VoiceDetection) {
  // Test external VAD
  EXPECT_EQ(apm_->kNoError,
            apm_->voice_detection()->set_stream_has_voice(true));
  EXPECT_TRUE(apm_->voice_detection()->stream_has_voice());
  EXPECT_EQ(apm_->kNoError,
            apm_->voice_detection()->set_stream_has_voice(false));
  EXPECT_FALSE(apm_->voice_detection()->stream_has_voice());

  // Tesing invalid likelihoods
  EXPECT_EQ(apm_->kBadParameterError,
      apm_->voice_detection()->set_likelihood(
          static_cast<VoiceDetection::Likelihood>(-1)));

  EXPECT_EQ(apm_->kBadParameterError,
      apm_->voice_detection()->set_likelihood(
          static_cast<VoiceDetection::Likelihood>(5)));

  // Tesing valid likelihoods
  VoiceDetection::Likelihood likelihood[] = {
      VoiceDetection::kVeryLowLikelihood,
      VoiceDetection::kLowLikelihood,
      VoiceDetection::kModerateLikelihood,
      VoiceDetection::kHighLikelihood
  };
  for (size_t i = 0; i < sizeof(likelihood)/sizeof(*likelihood); i++) {
    EXPECT_EQ(apm_->kNoError,
              apm_->voice_detection()->set_likelihood(likelihood[i]));
    EXPECT_EQ(likelihood[i], apm_->voice_detection()->likelihood());
  }

  /* TODO(bjornv): Enable once VAD supports other frame lengths than 10 ms
  // Tesing invalid frame sizes
  EXPECT_EQ(apm_->kBadParameterError,
      apm_->voice_detection()->set_frame_size_ms(12));

  // Tesing valid frame sizes
  for (int i = 10; i <= 30; i += 10) {
    EXPECT_EQ(apm_->kNoError,
        apm_->voice_detection()->set_frame_size_ms(i));
    EXPECT_EQ(i, apm_->voice_detection()->frame_size_ms());
  }
  */

  // Turing VAD on/off
  EXPECT_EQ(apm_->kNoError, apm_->voice_detection()->Enable(true));
  EXPECT_TRUE(apm_->voice_detection()->is_enabled());
  EXPECT_EQ(apm_->kNoError, apm_->voice_detection()->Enable(false));
  EXPECT_FALSE(apm_->voice_detection()->is_enabled());

  // TODO(bjornv): Add tests for streamed voice; stream_has_voice()
}

// Below are some ideas for tests from VPM.

/*TEST_F(VideoProcessingModuleTest, GetVersionTest)
{
}

TEST_F(VideoProcessingModuleTest, HandleNullBuffer)
{
}

TEST_F(VideoProcessingModuleTest, HandleBadSize)
{
}

TEST_F(VideoProcessingModuleTest, IdenticalResultsAfterReset)
{
}
*/
}  // namespace

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  ApmEnvironment* env = new ApmEnvironment; // GTest takes ownership.
  ::testing::AddGlobalTestEnvironment(env);

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--write_output_data") == 0) {
      write_output_data = true;
    }
  }

  int err = RUN_ALL_TESTS();

  // Optional, but removes memory leak noise from Valgrind.
  google::protobuf::ShutdownProtobufLibrary();
  return err;
}
