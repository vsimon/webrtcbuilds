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
#include <string.h>
#ifdef WEBRTC_ANDROID
#include <sys/stat.h>
#endif

#include "gtest/gtest.h"

#include "audio_processing.h"
#include "cpu_features_wrapper.h"
#include "module_common_types.h"
#include "scoped_ptr.h"
#include "tick_util.h"
#ifdef WEBRTC_ANDROID
#include "external/webrtc/src/modules/audio_processing/debug.pb.h"
#else
#include "webrtc/audio_processing/debug.pb.h"
#endif

using webrtc::AudioFrame;
using webrtc::AudioProcessing;
using webrtc::EchoCancellation;
using webrtc::GainControl;
using webrtc::NoiseSuppression;
using webrtc::scoped_array;
using webrtc::TickInterval;
using webrtc::TickTime;

using webrtc::audioproc::Event;
using webrtc::audioproc::Init;
using webrtc::audioproc::ReverseStream;
using webrtc::audioproc::Stream;

namespace {
// Returns true on success, false on error or end-of-file.
bool ReadMessageFromFile(FILE* file,
                        ::google::protobuf::MessageLite* msg) {
  // The "wire format" for the size is little-endian.
  // Assume process_test is running on a little-endian machine.
  int32_t size = 0;
  if (fread(&size, sizeof(int32_t), 1, file) != 1) {
    return false;
  }
  if (size <= 0) {
    return false;
  }
  const size_t usize = static_cast<size_t>(size);

  scoped_array<char> array(new char[usize]);
  if (fread(array.get(), sizeof(char), usize, file) != usize) {
    return false;
  }

  msg->Clear();
  return msg->ParseFromArray(array.get(), usize);
}

void PrintStat(const AudioProcessing::Statistic& stat) {
  printf("%d, %d, %d\n", stat.average,
                         stat.maximum,
                         stat.minimum);
}

void usage() {
  printf(
  "Usage: process_test [options] [-pb PROTOBUF_FILE]\n"
  "  [-ir REVERSE_FILE] [-i PRIMARY_FILE] [-o OUT_FILE]\n");
  printf(
  "process_test is a test application for AudioProcessing.\n\n"
  "When a protobuf debug file is available, specify it with -pb.\n"
  "Alternately, when -ir or -i is used, the specified files will be\n"
  "processed directly in a simulation mode. Otherwise the full set of\n"
  "legacy test files is expected to be present in the working directory.\n");
  printf("\n");
  printf("Options\n");
  printf("General configuration (only used for the simulation mode):\n");
  printf("  -fs SAMPLE_RATE_HZ\n");
  printf("  -ch CHANNELS_IN CHANNELS_OUT\n");
  printf("  -rch REVERSE_CHANNELS\n");
  printf("\n");
  printf("Component configuration:\n");
  printf(
  "All components are disabled by default. Each block below begins with a\n"
  "flag to enable the component with default settings. The subsequent flags\n"
  "in the block are used to provide configuration settings.\n");
  printf("\n  -aec     Echo cancellation\n");
  printf("  --drift_compensation\n");
  printf("  --no_drift_compensation\n");
  printf("  --no_echo_metrics\n");
  printf("  --no_delay_logging\n");
  printf("\n  -aecm    Echo control mobile\n");
  printf("  --aecm_echo_path_in_file FILE\n");
  printf("  --aecm_echo_path_out_file FILE\n");
  printf("\n  -agc     Gain control\n");
  printf("  --analog\n");
  printf("  --adaptive_digital\n");
  printf("  --fixed_digital\n");
  printf("  --target_level LEVEL\n");
  printf("  --compression_gain GAIN\n");
  printf("  --limiter\n");
  printf("  --no_limiter\n");
  printf("\n  -hpf     High pass filter\n");
  printf("\n  -ns      Noise suppression\n");
  printf("  --ns_low\n");
  printf("  --ns_moderate\n");
  printf("  --ns_high\n");
  printf("  --ns_very_high\n");
  printf("\n  -vad     Voice activity detection\n");
  printf("  --vad_out_file FILE\n");
  printf("\n Level metrics (enabled by default)\n");
  printf("  --no_level_metrics\n");
  printf("\n");
  printf("Modifiers:\n");
  printf("  --noasm            Disable SSE optimization.\n");
  printf("  --delay DELAY      Add DELAY ms to input value.\n");
  printf("  --perf             Measure performance.\n");
  printf("  --quiet            Suppress text output.\n");
  printf("  --no_progress      Suppress progress.\n");
  printf("  --debug_file FILE  Dump a debug recording.\n");
  printf("  --version          Print version information and exit.\n");
}

// void function for gtest.
void void_main(int argc, char* argv[]) {
  if (argc > 1 && strcmp(argv[1], "--help") == 0) {
    usage();
    return;
  }

  if (argc < 2) {
    printf("Did you mean to run without arguments?\n");
    printf("Try `process_test --help' for more information.\n\n");
  }

  AudioProcessing* apm = AudioProcessing::Create(0);
  ASSERT_TRUE(apm != NULL);

  WebRtc_Word8 version[1024];
  WebRtc_UWord32 version_bytes_remaining = sizeof(version);
  WebRtc_UWord32 version_position = 0;

  const char* pb_filename = NULL;
  const char* far_filename = NULL;
  const char* near_filename = NULL;
  const char* out_filename = NULL;
  const char* vad_out_filename = NULL;
  const char* aecm_echo_path_in_filename = NULL;
  const char* aecm_echo_path_out_filename = NULL;

  int32_t sample_rate_hz = 16000;
  int32_t device_sample_rate_hz = 16000;

  int num_capture_input_channels = 1;
  int num_capture_output_channels = 1;
  int num_render_channels = 1;

  int samples_per_channel = sample_rate_hz / 100;

  bool simulating = false;
  bool perf_testing = false;
  bool verbose = true;
  bool progress = true;
  int extra_delay_ms = 0;
  //bool interleaved = true;

  ASSERT_EQ(apm->kNoError, apm->level_estimator()->Enable(true));
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-pb") == 0) {
      i++;
      ASSERT_LT(i, argc) << "Specify protobuf filename after -pb";
      pb_filename = argv[i];

    } else if (strcmp(argv[i], "-ir") == 0) {
      i++;
      ASSERT_LT(i, argc) << "Specify filename after -ir";
      far_filename = argv[i];
      simulating = true;

    } else if (strcmp(argv[i], "-i") == 0) {
      i++;
      ASSERT_LT(i, argc) << "Specify filename after -i";
      near_filename = argv[i];
      simulating = true;

    } else if (strcmp(argv[i], "-o") == 0) {
      i++;
      ASSERT_LT(i, argc) << "Specify filename after -o";
      out_filename = argv[i];

    } else if (strcmp(argv[i], "-fs") == 0) {
      i++;
      ASSERT_LT(i, argc) << "Specify sample rate after -fs";
      ASSERT_EQ(1, sscanf(argv[i], "%d", &sample_rate_hz));
      samples_per_channel = sample_rate_hz / 100;

      ASSERT_EQ(apm->kNoError,
                apm->set_sample_rate_hz(sample_rate_hz));

    } else if (strcmp(argv[i], "-ch") == 0) {
      i++;
      ASSERT_LT(i + 1, argc) << "Specify number of channels after -ch";
      ASSERT_EQ(1, sscanf(argv[i], "%d", &num_capture_input_channels));
      i++;
      ASSERT_EQ(1, sscanf(argv[i], "%d", &num_capture_output_channels));

      ASSERT_EQ(apm->kNoError,
                apm->set_num_channels(num_capture_input_channels,
                                      num_capture_output_channels));

    } else if (strcmp(argv[i], "-rch") == 0) {
      i++;
      ASSERT_LT(i, argc) << "Specify number of channels after -rch";
      ASSERT_EQ(1, sscanf(argv[i], "%d", &num_render_channels));

      ASSERT_EQ(apm->kNoError,
                apm->set_num_reverse_channels(num_render_channels));

    } else if (strcmp(argv[i], "-aec") == 0) {
      ASSERT_EQ(apm->kNoError, apm->echo_cancellation()->Enable(true));
      ASSERT_EQ(apm->kNoError,
                apm->echo_cancellation()->enable_metrics(true));
      ASSERT_EQ(apm->kNoError,
                apm->echo_cancellation()->enable_delay_logging(true));

    } else if (strcmp(argv[i], "--drift_compensation") == 0) {
      ASSERT_EQ(apm->kNoError, apm->echo_cancellation()->Enable(true));
      // TODO(ajm): this is enabled in the VQE test app by default. Investigate
      //            why it can give better performance despite passing zeros.
      ASSERT_EQ(apm->kNoError,
                apm->echo_cancellation()->enable_drift_compensation(true));
    } else if (strcmp(argv[i], "--no_drift_compensation") == 0) {
      ASSERT_EQ(apm->kNoError, apm->echo_cancellation()->Enable(true));
      ASSERT_EQ(apm->kNoError,
                apm->echo_cancellation()->enable_drift_compensation(false));

    } else if (strcmp(argv[i], "--no_echo_metrics") == 0) {
      ASSERT_EQ(apm->kNoError, apm->echo_cancellation()->Enable(true));
      ASSERT_EQ(apm->kNoError,
                apm->echo_cancellation()->enable_metrics(false));

    } else if (strcmp(argv[i], "--no_delay_logging") == 0) {
      ASSERT_EQ(apm->kNoError, apm->echo_cancellation()->Enable(true));
      ASSERT_EQ(apm->kNoError,
                apm->echo_cancellation()->enable_delay_logging(false));

    } else if (strcmp(argv[i], "--no_level_metrics") == 0) {
      ASSERT_EQ(apm->kNoError, apm->level_estimator()->Enable(false));

    } else if (strcmp(argv[i], "-aecm") == 0) {
      ASSERT_EQ(apm->kNoError, apm->echo_control_mobile()->Enable(true));

    } else if (strcmp(argv[i], "--aecm_echo_path_in_file") == 0) {
      i++;
      ASSERT_LT(i, argc) << "Specify filename after --aecm_echo_path_in_file";
      aecm_echo_path_in_filename = argv[i];

    } else if (strcmp(argv[i], "--aecm_echo_path_out_file") == 0) {
      i++;
      ASSERT_LT(i, argc) << "Specify filename after --aecm_echo_path_out_file";
      aecm_echo_path_out_filename = argv[i];

    } else if (strcmp(argv[i], "-agc") == 0) {
      ASSERT_EQ(apm->kNoError, apm->gain_control()->Enable(true));

    } else if (strcmp(argv[i], "--analog") == 0) {
      ASSERT_EQ(apm->kNoError, apm->gain_control()->Enable(true));
      ASSERT_EQ(apm->kNoError,
                apm->gain_control()->set_mode(GainControl::kAdaptiveAnalog));

    } else if (strcmp(argv[i], "--adaptive_digital") == 0) {
      ASSERT_EQ(apm->kNoError, apm->gain_control()->Enable(true));
      ASSERT_EQ(apm->kNoError,
                apm->gain_control()->set_mode(GainControl::kAdaptiveDigital));

    } else if (strcmp(argv[i], "--fixed_digital") == 0) {
      ASSERT_EQ(apm->kNoError, apm->gain_control()->Enable(true));
      ASSERT_EQ(apm->kNoError,
                apm->gain_control()->set_mode(GainControl::kFixedDigital));

    } else if (strcmp(argv[i], "--target_level") == 0) {
      i++;
      int level;
      ASSERT_EQ(1, sscanf(argv[i], "%d", &level));

      ASSERT_EQ(apm->kNoError, apm->gain_control()->Enable(true));
      ASSERT_EQ(apm->kNoError,
                apm->gain_control()->set_target_level_dbfs(level));

    } else if (strcmp(argv[i], "--compression_gain") == 0) {
      i++;
      int gain;
      ASSERT_EQ(1, sscanf(argv[i], "%d", &gain));

      ASSERT_EQ(apm->kNoError, apm->gain_control()->Enable(true));
      ASSERT_EQ(apm->kNoError,
                apm->gain_control()->set_compression_gain_db(gain));

    } else if (strcmp(argv[i], "--limiter") == 0) {
      ASSERT_EQ(apm->kNoError, apm->gain_control()->Enable(true));
      ASSERT_EQ(apm->kNoError,
                apm->gain_control()->enable_limiter(true));

    } else if (strcmp(argv[i], "--no_limiter") == 0) {
      ASSERT_EQ(apm->kNoError, apm->gain_control()->Enable(true));
      ASSERT_EQ(apm->kNoError,
                apm->gain_control()->enable_limiter(false));

    } else if (strcmp(argv[i], "-hpf") == 0) {
      ASSERT_EQ(apm->kNoError, apm->high_pass_filter()->Enable(true));

    } else if (strcmp(argv[i], "-ns") == 0) {
      ASSERT_EQ(apm->kNoError, apm->noise_suppression()->Enable(true));

    } else if (strcmp(argv[i], "--ns_low") == 0) {
      ASSERT_EQ(apm->kNoError, apm->noise_suppression()->Enable(true));
      ASSERT_EQ(apm->kNoError,
          apm->noise_suppression()->set_level(NoiseSuppression::kLow));

    } else if (strcmp(argv[i], "--ns_moderate") == 0) {
      ASSERT_EQ(apm->kNoError, apm->noise_suppression()->Enable(true));
      ASSERT_EQ(apm->kNoError,
          apm->noise_suppression()->set_level(NoiseSuppression::kModerate));

    } else if (strcmp(argv[i], "--ns_high") == 0) {
      ASSERT_EQ(apm->kNoError, apm->noise_suppression()->Enable(true));
      ASSERT_EQ(apm->kNoError,
          apm->noise_suppression()->set_level(NoiseSuppression::kHigh));

    } else if (strcmp(argv[i], "--ns_very_high") == 0) {
      ASSERT_EQ(apm->kNoError, apm->noise_suppression()->Enable(true));
      ASSERT_EQ(apm->kNoError,
          apm->noise_suppression()->set_level(NoiseSuppression::kVeryHigh));

    } else if (strcmp(argv[i], "-vad") == 0) {
      ASSERT_EQ(apm->kNoError, apm->voice_detection()->Enable(true));

    } else if (strcmp(argv[i], "--vad_out_file") == 0) {
      i++;
      ASSERT_LT(i, argc) << "Specify filename after --vad_out_file";
      vad_out_filename = argv[i];

    } else if (strcmp(argv[i], "--noasm") == 0) {
      WebRtc_GetCPUInfo = WebRtc_GetCPUInfoNoASM;
      // We need to reinitialize here if components have already been enabled.
      ASSERT_EQ(apm->kNoError, apm->Initialize());

    } else if (strcmp(argv[i], "--delay") == 0) {
      i++;
      ASSERT_EQ(1, sscanf(argv[i], "%d", &extra_delay_ms));

    } else if (strcmp(argv[i], "--perf") == 0) {
      perf_testing = true;

    } else if (strcmp(argv[i], "--quiet") == 0) {
      verbose = false;
      progress = false;

    } else if (strcmp(argv[i], "--no_progress") == 0) {
      progress = false;

    } else if (strcmp(argv[i], "--version") == 0) {
      ASSERT_EQ(apm->kNoError, apm->Version(version,
                                            version_bytes_remaining,
                                            version_position));
      printf("%s\n", version);
      return;

    } else if (strcmp(argv[i], "--debug_file") == 0) {
      i++;
      ASSERT_LT(i, argc) << "Specify filename after --debug_file";
      ASSERT_EQ(apm->kNoError, apm->StartDebugRecording(argv[i]));
    } else {
      FAIL() << "Unrecognized argument " << argv[i];
    }
  }
  // If we're reading a protobuf file, ensure a simulation hasn't also
  // been requested (which makes no sense...)
  ASSERT_FALSE(pb_filename && simulating);

  if (verbose) {
    printf("Sample rate: %d Hz\n", sample_rate_hz);
    printf("Primary channels: %d (in), %d (out)\n",
           num_capture_input_channels,
           num_capture_output_channels);
    printf("Reverse channels: %d \n", num_render_channels);
  }

  const char far_file_default[] = "apm_far.pcm";
  const char near_file_default[] = "apm_near.pcm";
  const char out_file_default[] = "out.pcm";
  const char event_filename[] = "apm_event.dat";
  const char delay_filename[] = "apm_delay.dat";
  const char drift_filename[] = "apm_drift.dat";
  const char vad_file_default[] = "vad_out.dat";

  if (!simulating) {
    far_filename = far_file_default;
    near_filename = near_file_default;
  }

  if (!out_filename) {
    out_filename = out_file_default;
  }

  if (!vad_out_filename) {
    vad_out_filename = vad_file_default;
  }

  FILE* pb_file = NULL;
  FILE* far_file = NULL;
  FILE* near_file = NULL;
  FILE* out_file = NULL;
  FILE* event_file = NULL;
  FILE* delay_file = NULL;
  FILE* drift_file = NULL;
  FILE* vad_out_file = NULL;
  FILE* aecm_echo_path_in_file = NULL;
  FILE* aecm_echo_path_out_file = NULL;

  if (pb_filename) {
    pb_file = fopen(pb_filename, "rb");
    ASSERT_TRUE(NULL != pb_file) << "Unable to open protobuf file "
                                 << pb_filename;
  } else {
    if (far_filename) {
      far_file = fopen(far_filename, "rb");
      ASSERT_TRUE(NULL != far_file) << "Unable to open far-end audio file "
                                    << far_filename;
    }

    near_file = fopen(near_filename, "rb");
    ASSERT_TRUE(NULL != near_file) << "Unable to open near-end audio file "
                                   << near_filename;
    if (!simulating) {
      event_file = fopen(event_filename, "rb");
      ASSERT_TRUE(NULL != event_file) << "Unable to open event file "
                                      << event_filename;

      delay_file = fopen(delay_filename, "rb");
      ASSERT_TRUE(NULL != delay_file) << "Unable to open buffer file "
                                      << delay_filename;

      drift_file = fopen(drift_filename, "rb");
      ASSERT_TRUE(NULL != drift_file) << "Unable to open drift file "
                                      << drift_filename;
    }
  }

  out_file = fopen(out_filename, "wb");
  ASSERT_TRUE(NULL != out_file) << "Unable to open output audio file "
                                << out_filename;

  int near_size_bytes = 0;
  if (pb_file) {
    struct stat st;
    stat(pb_filename, &st);
    // Crude estimate, but should be good enough.
    near_size_bytes = st.st_size / 3;
  } else {
    struct stat st;
    stat(near_filename, &st);
    near_size_bytes = st.st_size;
  }

  if (apm->voice_detection()->is_enabled()) {
    vad_out_file = fopen(vad_out_filename, "wb");
    ASSERT_TRUE(NULL != vad_out_file) << "Unable to open VAD output file "
                                      << vad_out_file;
  }

  if (aecm_echo_path_in_filename != NULL) {
    aecm_echo_path_in_file = fopen(aecm_echo_path_in_filename, "rb");
    ASSERT_TRUE(NULL != aecm_echo_path_in_file) << "Unable to open file "
                                                << aecm_echo_path_in_filename;

    const size_t path_size =
        apm->echo_control_mobile()->echo_path_size_bytes();
    scoped_array<char> echo_path(new char[path_size]);
    ASSERT_EQ(path_size, fread(echo_path.get(),
                               sizeof(char),
                               path_size,
                               aecm_echo_path_in_file));
    EXPECT_EQ(apm->kNoError,
              apm->echo_control_mobile()->SetEchoPath(echo_path.get(),
                                                      path_size));
    fclose(aecm_echo_path_in_file);
    aecm_echo_path_in_file = NULL;
  }

  if (aecm_echo_path_out_filename != NULL) {
    aecm_echo_path_out_file = fopen(aecm_echo_path_out_filename, "wb");
    ASSERT_TRUE(NULL != aecm_echo_path_out_file) << "Unable to open file "
                                                 << aecm_echo_path_out_filename;
  }

  size_t read_count = 0;
  int reverse_count = 0;
  int primary_count = 0;
  int near_read_bytes = 0;
  TickInterval acc_ticks;

  AudioFrame far_frame;
  AudioFrame near_frame;

  int delay_ms = 0;
  int drift_samples = 0;
  int capture_level = 127;
  int8_t stream_has_voice = 0;

  TickTime t0 = TickTime::Now();
  TickTime t1 = t0;
  WebRtc_Word64 max_time_us = 0;
  WebRtc_Word64 max_time_reverse_us = 0;
  WebRtc_Word64 min_time_us = 1e6;
  WebRtc_Word64 min_time_reverse_us = 1e6;

  // TODO(ajm): Ideally we would refactor this block into separate functions,
  //            but for now we want to share the variables.
  if (pb_file) {
    Event event_msg;
    while (ReadMessageFromFile(pb_file, &event_msg)) {
      std::ostringstream trace_stream;
      trace_stream << "Processed frames: " << reverse_count << " (reverse), "
                   << primary_count << " (primary)";
      SCOPED_TRACE(trace_stream.str());

      if (event_msg.type() == Event::INIT) {
        ASSERT_TRUE(event_msg.has_init());
        const Init msg = event_msg.init();

        ASSERT_TRUE(msg.has_sample_rate());
        ASSERT_EQ(apm->kNoError,
            apm->set_sample_rate_hz(msg.sample_rate()));

        ASSERT_TRUE(msg.has_device_sample_rate());
        ASSERT_EQ(apm->kNoError,
                  apm->echo_cancellation()->set_device_sample_rate_hz(
                      msg.device_sample_rate()));

        ASSERT_TRUE(msg.has_num_input_channels());
        ASSERT_TRUE(msg.has_num_output_channels());
        ASSERT_EQ(apm->kNoError,
            apm->set_num_channels(msg.num_input_channels(),
                                  msg.num_output_channels()));

        ASSERT_TRUE(msg.has_num_reverse_channels());
        ASSERT_EQ(apm->kNoError,
            apm->set_num_reverse_channels(msg.num_reverse_channels()));

        samples_per_channel = msg.sample_rate() / 100;
        far_frame._frequencyInHz = msg.sample_rate();
        far_frame._payloadDataLengthInSamples = samples_per_channel;
        far_frame._audioChannel = msg.num_reverse_channels();
        near_frame._frequencyInHz = msg.sample_rate();
        near_frame._payloadDataLengthInSamples = samples_per_channel;

        if (verbose) {
          printf("Init at frame: %d (primary), %d (reverse)\n",
              primary_count, reverse_count);
          printf("  Sample rate: %d Hz\n", msg.sample_rate());
          printf("  Primary channels: %d (in), %d (out)\n",
                 msg.num_input_channels(),
                 msg.num_output_channels());
          printf("  Reverse channels: %d \n", msg.num_reverse_channels());
        }

      } else if (event_msg.type() == Event::REVERSE_STREAM) {
        ASSERT_TRUE(event_msg.has_reverse_stream());
        const ReverseStream msg = event_msg.reverse_stream();
        reverse_count++;

        ASSERT_TRUE(msg.has_data());
        ASSERT_EQ(sizeof(int16_t) * samples_per_channel *
            far_frame._audioChannel, msg.data().size());
        memcpy(far_frame._payloadData, msg.data().data(), msg.data().size());

        if (perf_testing) {
          t0 = TickTime::Now();
        }

        ASSERT_EQ(apm->kNoError,
                  apm->AnalyzeReverseStream(&far_frame));

        if (perf_testing) {
          t1 = TickTime::Now();
          TickInterval tick_diff = t1 - t0;
          acc_ticks += tick_diff;
          if (tick_diff.Microseconds() > max_time_reverse_us) {
            max_time_reverse_us = tick_diff.Microseconds();
          }
          if (tick_diff.Microseconds() < min_time_reverse_us) {
            min_time_reverse_us = tick_diff.Microseconds();
          }
        }

      } else if (event_msg.type() == Event::STREAM) {
        ASSERT_TRUE(event_msg.has_stream());
        const Stream msg = event_msg.stream();
        primary_count++;

        // ProcessStream could have changed this for the output frame.
        near_frame._audioChannel = apm->num_input_channels();

        ASSERT_TRUE(msg.has_input_data());
        ASSERT_EQ(sizeof(int16_t) * samples_per_channel *
            near_frame._audioChannel, msg.input_data().size());
        memcpy(near_frame._payloadData,
               msg.input_data().data(),
               msg.input_data().size());

        near_read_bytes += msg.input_data().size();
        if (progress && primary_count % 100 == 0) {
          printf("%.0f%% complete\r",
              (near_read_bytes * 100.0) / near_size_bytes);
          fflush(stdout);
        }

        if (perf_testing) {
          t0 = TickTime::Now();
        }

        ASSERT_EQ(apm->kNoError,
                  apm->gain_control()->set_stream_analog_level(msg.level()));
        ASSERT_EQ(apm->kNoError,
                  apm->set_stream_delay_ms(msg.delay() + extra_delay_ms));
        ASSERT_EQ(apm->kNoError,
            apm->echo_cancellation()->set_stream_drift_samples(msg.drift()));

        int err = apm->ProcessStream(&near_frame);
        if (err == apm->kBadStreamParameterWarning) {
          printf("Bad parameter warning. %s\n", trace_stream.str().c_str());
        }
        ASSERT_TRUE(err == apm->kNoError ||
                    err == apm->kBadStreamParameterWarning);
        ASSERT_TRUE(near_frame._audioChannel == apm->num_output_channels());

        capture_level = apm->gain_control()->stream_analog_level();

        stream_has_voice =
            static_cast<int8_t>(apm->voice_detection()->stream_has_voice());
        if (vad_out_file != NULL) {
          ASSERT_EQ(1u, fwrite(&stream_has_voice,
                               sizeof(stream_has_voice),
                               1,
                               vad_out_file));
        }

        if (apm->gain_control()->mode() != GainControl::kAdaptiveAnalog) {
          ASSERT_EQ(msg.level(), capture_level);
        }

        if (perf_testing) {
          t1 = TickTime::Now();
          TickInterval tick_diff = t1 - t0;
          acc_ticks += tick_diff;
          if (tick_diff.Microseconds() > max_time_us) {
            max_time_us = tick_diff.Microseconds();
          }
          if (tick_diff.Microseconds() < min_time_us) {
            min_time_us = tick_diff.Microseconds();
          }
        }

        size_t size = samples_per_channel * near_frame._audioChannel;
        ASSERT_EQ(size, fwrite(near_frame._payloadData,
                               sizeof(int16_t),
                               size,
                               out_file));
      }
    }

    ASSERT_TRUE(feof(pb_file));

  } else {
    enum Events {
      kInitializeEvent,
      kRenderEvent,
      kCaptureEvent,
      kResetEventDeprecated
    };
    int16_t event = 0;
    while (simulating || feof(event_file) == 0) {
      std::ostringstream trace_stream;
      trace_stream << "Processed frames: " << reverse_count << " (reverse), "
                   << primary_count << " (primary)";
      SCOPED_TRACE(trace_stream.str());

      if (simulating) {
        if (far_file == NULL) {
          event = kCaptureEvent;
        } else {
          if (event == kRenderEvent) {
            event = kCaptureEvent;
          } else {
            event = kRenderEvent;
          }
        }
      } else {
        read_count = fread(&event, sizeof(event), 1, event_file);
        if (read_count != 1) {
          break;
        }
      }

      far_frame._frequencyInHz = sample_rate_hz;
      far_frame._payloadDataLengthInSamples = samples_per_channel;
      far_frame._audioChannel = num_render_channels;
      near_frame._frequencyInHz = sample_rate_hz;
      near_frame._payloadDataLengthInSamples = samples_per_channel;

      if (event == kInitializeEvent || event == kResetEventDeprecated) {
        ASSERT_EQ(1u,
            fread(&sample_rate_hz, sizeof(sample_rate_hz), 1, event_file));
        samples_per_channel = sample_rate_hz / 100;

        ASSERT_EQ(1u,
            fread(&device_sample_rate_hz,
                  sizeof(device_sample_rate_hz),
                  1,
                  event_file));

        ASSERT_EQ(apm->kNoError,
            apm->set_sample_rate_hz(sample_rate_hz));

        ASSERT_EQ(apm->kNoError,
                  apm->echo_cancellation()->set_device_sample_rate_hz(
                      device_sample_rate_hz));

        far_frame._frequencyInHz = sample_rate_hz;
        far_frame._payloadDataLengthInSamples = samples_per_channel;
        far_frame._audioChannel = num_render_channels;
        near_frame._frequencyInHz = sample_rate_hz;
        near_frame._payloadDataLengthInSamples = samples_per_channel;

        if (verbose) {
          printf("Init at frame: %d (primary), %d (reverse)\n",
              primary_count, reverse_count);
          printf("  Sample rate: %d Hz\n", sample_rate_hz);
        }

      } else if (event == kRenderEvent) {
        reverse_count++;

        size_t size = samples_per_channel * num_render_channels;
        read_count = fread(far_frame._payloadData,
                           sizeof(int16_t),
                           size,
                           far_file);

        if (simulating) {
          if (read_count != size) {
            // Read an equal amount from the near file to avoid errors due to
            // not reaching end-of-file.
            EXPECT_EQ(0, fseek(near_file, read_count * sizeof(int16_t),
                      SEEK_CUR));
            break; // This is expected.
          }
        } else {
          ASSERT_EQ(size, read_count);
        }

        if (perf_testing) {
          t0 = TickTime::Now();
        }

        ASSERT_EQ(apm->kNoError,
                  apm->AnalyzeReverseStream(&far_frame));

        if (perf_testing) {
          t1 = TickTime::Now();
          TickInterval tick_diff = t1 - t0;
          acc_ticks += tick_diff;
          if (tick_diff.Microseconds() > max_time_reverse_us) {
            max_time_reverse_us = tick_diff.Microseconds();
          }
          if (tick_diff.Microseconds() < min_time_reverse_us) {
            min_time_reverse_us = tick_diff.Microseconds();
          }
        }

      } else if (event == kCaptureEvent) {
        primary_count++;
        near_frame._audioChannel = num_capture_input_channels;

        size_t size = samples_per_channel * num_capture_input_channels;
        read_count = fread(near_frame._payloadData,
                           sizeof(int16_t),
                           size,
                           near_file);

        near_read_bytes += read_count * sizeof(int16_t);
        if (progress && primary_count % 100 == 0) {
          printf("%.0f%% complete\r",
              (near_read_bytes * 100.0) / near_size_bytes);
          fflush(stdout);
        }
        if (simulating) {
          if (read_count != size) {
            break; // This is expected.
          }

          delay_ms = 0;
          drift_samples = 0;
        } else {
          ASSERT_EQ(size, read_count);

          // TODO(ajm): sizeof(delay_ms) for current files?
          ASSERT_EQ(1u,
              fread(&delay_ms, 2, 1, delay_file));
          ASSERT_EQ(1u,
              fread(&drift_samples, sizeof(drift_samples), 1, drift_file));
        }

        if (perf_testing) {
          t0 = TickTime::Now();
        }

        // TODO(ajm): fake an analog gain while simulating.

        int capture_level_in = capture_level;
        ASSERT_EQ(apm->kNoError,
                  apm->gain_control()->set_stream_analog_level(capture_level));
        ASSERT_EQ(apm->kNoError,
                  apm->set_stream_delay_ms(delay_ms + extra_delay_ms));
        ASSERT_EQ(apm->kNoError,
            apm->echo_cancellation()->set_stream_drift_samples(drift_samples));

        int err = apm->ProcessStream(&near_frame);
        if (err == apm->kBadStreamParameterWarning) {
          printf("Bad parameter warning. %s\n", trace_stream.str().c_str());
        }
        ASSERT_TRUE(err == apm->kNoError ||
                    err == apm->kBadStreamParameterWarning);
        ASSERT_TRUE(near_frame._audioChannel == apm->num_output_channels());

        capture_level = apm->gain_control()->stream_analog_level();

        stream_has_voice =
            static_cast<int8_t>(apm->voice_detection()->stream_has_voice());
        if (vad_out_file != NULL) {
          ASSERT_EQ(1u, fwrite(&stream_has_voice,
                               sizeof(stream_has_voice),
                               1,
                               vad_out_file));
        }

        if (apm->gain_control()->mode() != GainControl::kAdaptiveAnalog) {
          ASSERT_EQ(capture_level_in, capture_level);
        }

        if (perf_testing) {
          t1 = TickTime::Now();
          TickInterval tick_diff = t1 - t0;
          acc_ticks += tick_diff;
          if (tick_diff.Microseconds() > max_time_us) {
            max_time_us = tick_diff.Microseconds();
          }
          if (tick_diff.Microseconds() < min_time_us) {
            min_time_us = tick_diff.Microseconds();
          }
        }

        size = samples_per_channel * near_frame._audioChannel;
        ASSERT_EQ(size, fwrite(near_frame._payloadData,
                               sizeof(int16_t),
                               size,
                               out_file));
      }
      else {
        FAIL() << "Event " << event << " is unrecognized";
      }
    }
  }
  printf("100%% complete\r");

  if (aecm_echo_path_out_file != NULL) {
    const size_t path_size =
        apm->echo_control_mobile()->echo_path_size_bytes();
    scoped_array<char> echo_path(new char[path_size]);
    apm->echo_control_mobile()->GetEchoPath(echo_path.get(), path_size);
    ASSERT_EQ(path_size, fwrite(echo_path.get(),
                                sizeof(char),
                                path_size,
                                aecm_echo_path_out_file));
    fclose(aecm_echo_path_out_file);
    aecm_echo_path_out_file = NULL;
  }

  if (verbose) {
    printf("\nProcessed frames: %d (primary), %d (reverse)\n",
        primary_count, reverse_count);

    if (apm->level_estimator()->is_enabled()) {
      printf("\n--Level metrics--\n");
      printf("RMS: %d dBFS\n", -apm->level_estimator()->RMS());
    }
    if (apm->echo_cancellation()->are_metrics_enabled()) {
      EchoCancellation::Metrics metrics;
      apm->echo_cancellation()->GetMetrics(&metrics);
      printf("\n--Echo metrics--\n");
      printf("(avg, max, min)\n");
      printf("ERL:  ");
      PrintStat(metrics.echo_return_loss);
      printf("ERLE: ");
      PrintStat(metrics.echo_return_loss_enhancement);
      printf("ANLP: ");
      PrintStat(metrics.a_nlp);
    }
    if (apm->echo_cancellation()->is_delay_logging_enabled()) {
      int median = 0;
      int std = 0;
      apm->echo_cancellation()->GetDelayMetrics(&median, &std);
      printf("\n--Delay metrics--\n");
      printf("Median:             %3d\n", median);
      printf("Standard deviation: %3d\n", std);
    }
  }

  if (!pb_file) {
    int8_t temp_int8;
    if (far_file) {
      read_count = fread(&temp_int8, sizeof(temp_int8), 1, far_file);
      EXPECT_NE(0, feof(far_file)) << "Far-end file not fully processed";
    }

    read_count = fread(&temp_int8, sizeof(temp_int8), 1, near_file);
    EXPECT_NE(0, feof(near_file)) << "Near-end file not fully processed";

    if (!simulating) {
      read_count = fread(&temp_int8, sizeof(temp_int8), 1, event_file);
      EXPECT_NE(0, feof(event_file)) << "Event file not fully processed";
      read_count = fread(&temp_int8, sizeof(temp_int8), 1, delay_file);
      EXPECT_NE(0, feof(delay_file)) << "Delay file not fully processed";
      read_count = fread(&temp_int8, sizeof(temp_int8), 1, drift_file);
      EXPECT_NE(0, feof(drift_file)) << "Drift file not fully processed";
    }
  }

  if (perf_testing) {
    if (primary_count > 0) {
      WebRtc_Word64 exec_time = acc_ticks.Milliseconds();
      printf("\nTotal time: %.3f s, file time: %.2f s\n",
        exec_time * 0.001, primary_count * 0.01);
      printf("Time per frame: %.3f ms (average), %.3f ms (max),"
             " %.3f ms (min)\n",
          (exec_time * 1.0) / primary_count,
          (max_time_us + max_time_reverse_us) / 1000.0,
          (min_time_us + min_time_reverse_us) / 1000.0);
    } else {
      printf("Warning: no capture frames\n");
    }
  }

  AudioProcessing::Destroy(apm);
  apm = NULL;
}
}  // namespace

int main(int argc, char* argv[])
{
  void_main(argc, argv);

  // Optional, but removes memory leak noise from Valgrind.
  google::protobuf::ShutdownProtobufLibrary();
  return 0;
}
