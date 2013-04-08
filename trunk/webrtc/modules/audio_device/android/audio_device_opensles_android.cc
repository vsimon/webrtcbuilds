/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_device/android/audio_device_opensles_android.h"

#ifdef WEBRTC_ANDROID_DEBUG
#include <android/log.h>
#endif
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <time.h>

#include "modules/audio_device/audio_device_utility.h"
#include "system_wrappers/interface/event_wrapper.h"
#include "system_wrappers/interface/thread_wrapper.h"
#include "system_wrappers/interface/trace.h"

#ifdef WEBRTC_ANDROID_DEBUG
#define WEBRTC_OPENSL_TRACE(a, b, c, ...)                               \
  __android_log_print(ANDROID_LOG_DEBUG, "WebRTC OpenSLES", __VA_ARGS__)
#else
#define WEBRTC_OPENSL_TRACE WEBRTC_TRACE
#endif

namespace webrtc {

AudioDeviceAndroidOpenSLES::AudioDeviceAndroidOpenSLES(const WebRtc_Word32 id)
    : voe_audio_buffer_(NULL),
      crit_sect_(*CriticalSectionWrapper::CreateCriticalSection()),
      id_(id),
      sles_engine_(NULL),
      sles_player_(NULL),
      sles_engine_itf_(NULL),
      sles_player_itf_(NULL),
      sles_player_sbq_itf_(NULL),
      sles_output_mixer_(NULL),
      sles_speaker_volume_(NULL),
      sles_recorder_(NULL),
      sles_recorder_itf_(NULL),
      sles_recorder_sbq_itf_(NULL),
      sles_mic_volume_(NULL),
      mic_dev_id_(0),
      play_warning_(0),
      play_error_(0),
      rec_warning_(0),
      rec_error_(0),
      is_recording_dev_specified_(false),
      is_playout_dev_specified_(false),
      is_initialized_(false),
      is_recording_(false),
      is_playing_(false),
      is_rec_initialized_(false),
      is_play_initialized_(false),
      is_mic_initialized_(false),
      is_speaker_initialized_(false),
      playout_delay_(0),
      recording_delay_(0),
      agc_enabled_(false),
      rec_timer_(*EventWrapper::Create()),
      mic_sampling_rate_(N_REC_SAMPLES_PER_SEC * 1000),
      speaker_sampling_rate_(N_PLAY_SAMPLES_PER_SEC * 1000),
      max_speaker_vol_(0),
      min_speaker_vol_(0),
      loundspeaker_on_(false) {
  WEBRTC_OPENSL_TRACE(kTraceMemory, kTraceAudioDevice, id, "%s created",
                      __FUNCTION__);
  memset(rec_buf_, 0, sizeof(rec_buf_));
  memset(play_buf_, 0, sizeof(play_buf_));
}

AudioDeviceAndroidOpenSLES::~AudioDeviceAndroidOpenSLES() {
  WEBRTC_OPENSL_TRACE(kTraceMemory, kTraceAudioDevice, id_, "%s destroyed",
                      __FUNCTION__);

  Terminate();

  delete &crit_sect_;
  delete &rec_timer_;
}

void AudioDeviceAndroidOpenSLES::AttachAudioBuffer(
    AudioDeviceBuffer* audioBuffer) {

  CriticalSectionScoped lock(&crit_sect_);

  voe_audio_buffer_ = audioBuffer;

  // Inform the AudioBuffer about default settings for this implementation.
  voe_audio_buffer_->SetRecordingSampleRate(N_REC_SAMPLES_PER_SEC);
  voe_audio_buffer_->SetPlayoutSampleRate(N_PLAY_SAMPLES_PER_SEC);
  voe_audio_buffer_->SetRecordingChannels(N_REC_CHANNELS);
  voe_audio_buffer_->SetPlayoutChannels(N_PLAY_CHANNELS);
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::ActiveAudioLayer(
    AudioDeviceModule::AudioLayer& audioLayer) const {

  audioLayer = AudioDeviceModule::kPlatformDefaultAudio;

  return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::Init() {
  CriticalSectionScoped lock(&crit_sect_);

  if (is_initialized_)
    return 0;

  SLEngineOption EngineOption[] = {
    { SL_ENGINEOPTION_THREADSAFE, static_cast<SLuint32>(SL_BOOLEAN_TRUE) },
  };
  WebRtc_Word32 res = slCreateEngine(&sles_engine_, 1, EngineOption, 0,
                                     NULL, NULL);

  if (res != SL_RESULT_SUCCESS) {
    WEBRTC_OPENSL_TRACE(kTraceError, kTraceAudioDevice, id_,
                        "  failed to create SL Engine Object");
    return -1;
  }

  // Realizing the SL Engine in synchronous mode.
  if ((*sles_engine_)->Realize(sles_engine_, SL_BOOLEAN_FALSE)
      != SL_RESULT_SUCCESS) {
    WEBRTC_OPENSL_TRACE(kTraceError, kTraceAudioDevice, id_,
                        "  failed to Realize SL Engine");
    return -1;
  }

  if ((*sles_engine_)->GetInterface(
          sles_engine_,
          SL_IID_ENGINE,
          &sles_engine_itf_) != SL_RESULT_SUCCESS) {
    WEBRTC_OPENSL_TRACE(kTraceError, kTraceAudioDevice, id_,
                        "  failed to get SL Engine interface");
    return -1;
  }

  // Check the sample rate to be used for playback and recording
  if (InitSampleRate() != 0) {
    WEBRTC_OPENSL_TRACE(kTraceError, kTraceAudioDevice, id_,
                        "%s: Failed to init samplerate", __FUNCTION__);
    return -1;
  }

  // Set the audio device buffer sampling rate, we assume we get the same
  // for play and record.
  if (voe_audio_buffer_->SetRecordingSampleRate(mic_sampling_rate_) < 0) {
    WEBRTC_OPENSL_TRACE(kTraceError, kTraceAudioDevice, id_,
                        "  Could not set mic audio device buffer "
                        "sampling rate (%d)", mic_sampling_rate_);
  }
  if (voe_audio_buffer_->SetPlayoutSampleRate(speaker_sampling_rate_) < 0) {
    WEBRTC_OPENSL_TRACE(kTraceError, kTraceAudioDevice, id_,
                        "  Could not set speaker audio device buffer "
                        "sampling rate (%d)", speaker_sampling_rate_);
  }

  is_initialized_ = true;

  return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::Terminate() {
  CriticalSectionScoped lock(&crit_sect_);

  if (!is_initialized_)
    return 0;

  // RECORDING
  StopRecording();

  is_mic_initialized_ = false;
  is_recording_dev_specified_ = false;

  // PLAYOUT
  StopPlayout();

  if (sles_engine_ != NULL) {
    (*sles_engine_)->Destroy(sles_engine_);
    sles_engine_ = NULL;
    sles_engine_itf_ = NULL;
  }

  is_initialized_ = false;
  return 0;
}

bool AudioDeviceAndroidOpenSLES::Initialized() const {
  return (is_initialized_);
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::SpeakerIsAvailable(
    bool& available) {
  // We always assume it's available
  available = true;

  return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::InitSpeaker() {
  CriticalSectionScoped lock(&crit_sect_);

  if (is_playing_) {
    WEBRTC_OPENSL_TRACE(kTraceWarning, kTraceAudioDevice, id_,
                        "  Playout already started");
    return -1;
  }

  if (!is_playout_dev_specified_) {
    WEBRTC_OPENSL_TRACE(kTraceError, kTraceAudioDevice, id_,
                        "  Playout device is not specified");
    return -1;
  }

  // Nothing needs to be done here, we use a flag to have consistent
  // behavior with other platforms.
  is_speaker_initialized_ = true;

  return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::MicrophoneIsAvailable(
    bool& available) {
  // We always assume it's available.
  available = true;
  return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::InitMicrophone() {
  CriticalSectionScoped lock(&crit_sect_);
  if (is_recording_) {
    WEBRTC_OPENSL_TRACE(kTraceWarning, kTraceAudioDevice, id_,
                        "  Recording already started");
    return -1;
  }
  if (!is_recording_dev_specified_) {
    WEBRTC_OPENSL_TRACE(kTraceError, kTraceAudioDevice, id_,
                        "  Recording device is not specified");
    return -1;
  }

  // Nothing needs to be done here, we use a flag to have consistent
  // behavior with other platforms.
  is_mic_initialized_ = true;
  return 0;
}

bool AudioDeviceAndroidOpenSLES::SpeakerIsInitialized() const {
  return is_speaker_initialized_;
}

bool AudioDeviceAndroidOpenSLES::MicrophoneIsInitialized() const {
  return is_mic_initialized_;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::SpeakerVolumeIsAvailable(
    bool& available) {
  available = true;  // We assume we are always be able to set/get volume.
  return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::SetSpeakerVolume(
    WebRtc_UWord32 volume) {
  if (!is_speaker_initialized_) {
    WEBRTC_OPENSL_TRACE(kTraceError, kTraceAudioDevice, id_,
                        "  Speaker not initialized");
    return -1;
  }

  if (sles_engine_ == NULL) {
    WEBRTC_OPENSL_TRACE(kTraceError, kTraceAudioDevice, id_,
                        "SetSpeakerVolume, SL Engine object doesnt exist");
    return -1;
  }

  if (sles_engine_itf_ == NULL) {
    if ((*sles_engine_)->GetInterface(
            sles_engine_,
            SL_IID_ENGINE,
            &sles_engine_itf_) != SL_RESULT_SUCCESS) {
      WEBRTC_OPENSL_TRACE(kTraceError, kTraceAudioDevice, id_,
                          "  failed to GetInterface SL Engine Interface");
      return -1;
    }
  }
  return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::SpeakerVolume(
    WebRtc_UWord32& volume) const {
  return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::SetWaveOutVolume(
    WebRtc_UWord16 volumeLeft,
    WebRtc_UWord16 volumeRight) {
  WEBRTC_OPENSL_TRACE(kTraceWarning, kTraceAudioDevice, id_,
                      "  API call not supported on this platform");
  return -1;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::WaveOutVolume(
    WebRtc_UWord16& volumeLeft,
    WebRtc_UWord16& volumeRight) const {
  WEBRTC_OPENSL_TRACE(kTraceWarning, kTraceAudioDevice, id_,
                      "  API call not supported on this platform");
  return -1;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::MaxSpeakerVolume(
    WebRtc_UWord32& maxVolume) const {
  if (!is_speaker_initialized_) {
    WEBRTC_OPENSL_TRACE(kTraceError, kTraceAudioDevice, id_,
                        "  Speaker not initialized");
    return -1;
  }

  maxVolume = max_speaker_vol_;

  return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::MinSpeakerVolume(
    WebRtc_UWord32& minVolume) const {
  if (!is_speaker_initialized_) {
    WEBRTC_OPENSL_TRACE(kTraceError, kTraceAudioDevice, id_,
                        "  Speaker not initialized");
    return -1;
  }
  minVolume = min_speaker_vol_;
  return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::SpeakerVolumeStepSize(
    WebRtc_UWord16& stepSize) const {
  if (!is_speaker_initialized_) {
    WEBRTC_OPENSL_TRACE(kTraceError, kTraceAudioDevice, id_,
                        "  Speaker not initialized");
    return -1;
  }
  stepSize = 1;
  return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::SpeakerMuteIsAvailable(
    bool& available) {
  available = false;  // Speaker mute not supported on Android.
  return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::SetSpeakerMute(bool enable) {
  WEBRTC_OPENSL_TRACE(kTraceWarning, kTraceAudioDevice, id_,
                      "  API call not supported on this platform");
  return -1;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::SpeakerMute(
    bool& enabled) const {
  WEBRTC_OPENSL_TRACE(kTraceWarning, kTraceAudioDevice, id_,
                      "  API call not supported on this platform");
  return -1;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::MicrophoneMuteIsAvailable(
    bool& available) {
  available = false;  // Mic mute not supported on Android
  return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::SetMicrophoneMute(bool enable) {
  WEBRTC_OPENSL_TRACE(kTraceWarning, kTraceAudioDevice, id_,
               "  API call not supported on this platform");
  return -1;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::MicrophoneMute(
    bool& enabled) const {
  WEBRTC_OPENSL_TRACE(kTraceWarning, kTraceAudioDevice, id_,
               "  API call not supported on this platform");
  return -1;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::MicrophoneBoostIsAvailable(
    bool& available) {
  available = false;  // Mic boost not supported on Android.
  return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::SetMicrophoneBoost(bool enable) {
  if (!is_mic_initialized_) {
    WEBRTC_OPENSL_TRACE(kTraceError, kTraceAudioDevice, id_,
                 "  Microphone not initialized");
    return -1;
  }
  if (enable) {
    WEBRTC_OPENSL_TRACE(kTraceError, kTraceAudioDevice, id_,
                 "  Enabling not available");
    return -1;
  }
  return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::MicrophoneBoost(
    bool& enabled) const {
  if (!is_mic_initialized_) {
    WEBRTC_OPENSL_TRACE(kTraceError, kTraceAudioDevice, id_,
                 "  Microphone not initialized");
    return -1;
  }
  enabled = false;
  return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::StereoRecordingIsAvailable(
    bool& available) {
  available = false;  // Stereo recording not supported on Android.
  return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::SetStereoRecording(bool enable) {
  if (enable) {
    WEBRTC_OPENSL_TRACE(kTraceError, kTraceAudioDevice, id_,
                 "  Enabling not available");
    return -1;
  }
  return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::StereoRecording(
    bool& enabled) const {
  enabled = false;
  return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::StereoPlayoutIsAvailable(
    bool& available) {
  // TODO(leozwang): This api is called before initplayout, we need
  // to detect audio device to find out if stereo is supported or not.
  available = false;
  return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::SetStereoPlayout(bool enable) {
  if (enable) {
    return 0;
  } else {
    // TODO(leozwang): Enforce mono.
    return 0;
  }
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::StereoPlayout(
    bool& enabled) const {
  enabled = (player_pcm_.numChannels == 2 ? true : false);
  return 0;
}


WebRtc_Word32 AudioDeviceAndroidOpenSLES::SetAGC(bool enable) {
  agc_enabled_ = enable;
  return 0;
}

bool AudioDeviceAndroidOpenSLES::AGC() const {
  return agc_enabled_;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::MicrophoneVolumeIsAvailable(
    bool& available) {
  available = true;
  return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::SetMicrophoneVolume(
    WebRtc_UWord32 volume) {
  WEBRTC_OPENSL_TRACE(kTraceWarning, kTraceAudioDevice, id_,
                      "  OpenSL doesn't support contolling Mic volume yet");
  // TODO(leozwang): Add microphone volume control when OpenSL apis
  // are available.
  return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::MicrophoneVolume(
    WebRtc_UWord32& volume) const {
  return -1;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::MaxMicrophoneVolume(
    WebRtc_UWord32& maxVolume) const {
  return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::MinMicrophoneVolume(
    WebRtc_UWord32& minVolume) const {
  minVolume = 0;
  return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::MicrophoneVolumeStepSize(
    WebRtc_UWord16& stepSize) const {
  stepSize = 1;
  return 0;
}

WebRtc_Word16 AudioDeviceAndroidOpenSLES::PlayoutDevices() {
  return 1;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::SetPlayoutDevice(
    WebRtc_UWord16 index) {
  if (is_play_initialized_) {
    WEBRTC_OPENSL_TRACE(kTraceError, kTraceAudioDevice, id_,
                 "  Playout already initialized");
    return -1;
  }
  if (0 != index) {
    WEBRTC_OPENSL_TRACE(kTraceError, kTraceAudioDevice, id_,
                 "  Device index is out of range [0,0]");
    return -1;
  }

  // Do nothing but set a flag, this is to have consistent behaviour
  // with other platforms.
  is_playout_dev_specified_ = true;

  return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::SetPlayoutDevice(
    AudioDeviceModule::WindowsDeviceType device) {

  WEBRTC_OPENSL_TRACE(kTraceWarning, kTraceAudioDevice, id_,
               "  API call not supported on this platform");
  return -1;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::PlayoutDeviceName(
    WebRtc_UWord16 index,
    char name[kAdmMaxDeviceNameSize],
    char guid[kAdmMaxGuidSize]) {
  if (0 != index) {
    WEBRTC_OPENSL_TRACE(kTraceError, kTraceAudioDevice, id_,
                 "  Device index is out of range [0,0]");
    return -1;
  }

  // Return empty string
  memset(name, 0, kAdmMaxDeviceNameSize);

  if (guid) {
    memset(guid, 0, kAdmMaxGuidSize);
  }
  return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::RecordingDeviceName(
    WebRtc_UWord16 index,
    char name[kAdmMaxDeviceNameSize],
    char guid[kAdmMaxGuidSize]) {
  if (0 != index) {
    WEBRTC_OPENSL_TRACE(kTraceError, kTraceAudioDevice, id_,
                 "  Device index is out of range [0,0]");
    return -1;
  }

  // Return empty string
  memset(name, 0, kAdmMaxDeviceNameSize);

  if (guid) {
    memset(guid, 0, kAdmMaxGuidSize);
  }
  return 0;
}

WebRtc_Word16 AudioDeviceAndroidOpenSLES::RecordingDevices() {
  return 1;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::SetRecordingDevice(
    WebRtc_UWord16 index) {
  if (is_rec_initialized_) {
    WEBRTC_OPENSL_TRACE(kTraceError, kTraceAudioDevice, id_,
                        "  Recording already initialized");
    return -1;
  }

  if (0 != index) {
    WEBRTC_OPENSL_TRACE(kTraceError, kTraceAudioDevice, id_,
                        "  Device index is out of range [0,0]");
    return -1;
  }

  // Do nothing but set a flag, this is to have consistent behaviour with
  // other platforms.
  is_recording_dev_specified_ = true;
  return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::SetRecordingDevice(
    AudioDeviceModule::WindowsDeviceType device) {
  WEBRTC_OPENSL_TRACE(kTraceWarning, kTraceAudioDevice, id_,
                      "  API call not supported on this platform");
  return -1;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::PlayoutIsAvailable(
    bool& available) {
  available = false;
  WebRtc_Word32 res = InitPlayout();
  StopPlayout();
  if (res != -1) {
    available = true;
  }
  return res;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::RecordingIsAvailable(
    bool& available) {
  available = false;
  WebRtc_Word32 res = InitRecording();
  StopRecording();
  if (res != -1) {
    available = true;
  }
  return res;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::InitPlayout() {
  CriticalSectionScoped lock(&crit_sect_);
  if (!is_initialized_) {
    WEBRTC_OPENSL_TRACE(kTraceError, kTraceAudioDevice, id_,
                        "  Not initialized");
    return -1;
  }

  if (is_playing_) {
    WEBRTC_OPENSL_TRACE(kTraceWarning, kTraceAudioDevice, id_,
                        "  Playout already started");
    return -1;
  }

  if (!is_playout_dev_specified_) {
    WEBRTC_OPENSL_TRACE(kTraceError, kTraceAudioDevice, id_,
                        "  Playout device is not specified");
    return -1;
  }

  if (is_play_initialized_) {
    WEBRTC_OPENSL_TRACE(kTraceInfo, kTraceAudioDevice, id_,
                        "  Playout already initialized");
    return 0;
  }

  // Initialize the speaker
  if (InitSpeaker() == -1) {
    WEBRTC_OPENSL_TRACE(kTraceWarning, kTraceAudioDevice, id_,
                        "  InitSpeaker() failed");
  }

  if (sles_engine_ == NULL || sles_engine_itf_ == NULL) {
    WEBRTC_OPENSL_TRACE(kTraceError, kTraceAudioDevice, id_,
                        "  SLObject or Engiine is NULL");
    return -1;
  }

  SLDataLocator_AndroidSimpleBufferQueue simple_buf_queue = {
    SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
    static_cast<SLuint32>(N_PLAY_QUEUE_BUFFERS)
  };
  SLDataSource audio_source = { &simple_buf_queue, &player_pcm_ };
  SLDataLocator_OutputMix locator_outputmix;
  SLDataSink audio_sink = { &locator_outputmix, NULL };

  // Create Output Mix object to be used by player.
  WebRtc_Word32 res = -1;
  res = (*sles_engine_itf_)->CreateOutputMix(sles_engine_itf_,
                                             &sles_output_mixer_,
                                             0,
                                             NULL,
                                             NULL);
  if (res != SL_RESULT_SUCCESS) {
    WEBRTC_OPENSL_TRACE(kTraceError, kTraceAudioDevice, id_,
                        "  failed to get SL Output Mix object");
    return -1;
  }
  // Realizing the Output Mix object in synchronous mode.
  res = (*sles_output_mixer_)->Realize(sles_output_mixer_, SL_BOOLEAN_FALSE);
  if (res != SL_RESULT_SUCCESS) {
    WEBRTC_OPENSL_TRACE(kTraceError, kTraceAudioDevice, id_,
                        "  failed to realize SL Output Mix object");
    return -1;
  }

  // The code below can be moved to startplayout instead
  // Setup the data source structure for the buffer queue.
  player_pcm_.formatType = SL_DATAFORMAT_PCM;
  player_pcm_.numChannels = N_PLAY_CHANNELS;
  if (speaker_sampling_rate_ == 44000) {
      player_pcm_.samplesPerSec = 44100 * 1000;
  } else {
    player_pcm_.samplesPerSec = speaker_sampling_rate_ * 1000;
  }
  player_pcm_.bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_16;
  player_pcm_.containerSize = SL_PCMSAMPLEFORMAT_FIXED_16;
  if (1 == player_pcm_.numChannels) {
    player_pcm_.channelMask = SL_SPEAKER_FRONT_CENTER;
  } else if (2 == player_pcm_.numChannels) {
    player_pcm_.channelMask = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
  } else {
    WEBRTC_OPENSL_TRACE(kTraceError, kTraceAudioDevice, id_,
                        "  %d player channels not supported", N_PLAY_CHANNELS);
  }

  player_pcm_.endianness = SL_BYTEORDER_LITTLEENDIAN;
  // Setup the data sink structure.
  locator_outputmix.locatorType = SL_DATALOCATOR_OUTPUTMIX;
  locator_outputmix.outputMix = sles_output_mixer_;

  SLInterfaceID ids[N_MAX_INTERFACES] = {
    SL_IID_BUFFERQUEUE, SL_IID_ANDROIDCONFIGURATION };
  SLboolean req[N_MAX_INTERFACES] = {
    SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE };
  res = (*sles_engine_itf_)->CreateAudioPlayer(sles_engine_itf_,
                                               &sles_player_, &audio_source,
                                               &audio_sink, 2, ids, req);
  if (res != SL_RESULT_SUCCESS) {
    WEBRTC_OPENSL_TRACE(kTraceError, kTraceAudioDevice, id_,
                        "  failed to create AudioPlayer");
    return -1;
  }

  // Realizing the player in synchronous mode.
  res = (*sles_player_)->Realize(sles_player_, SL_BOOLEAN_FALSE);
  if (res != SL_RESULT_SUCCESS) {
    WEBRTC_OPENSL_TRACE(kTraceError, kTraceAudioDevice, id_,
                        "  failed to realize the player");
    return -1;
  }
  res = (*sles_player_)->GetInterface(
      sles_player_, SL_IID_PLAY,
      static_cast<void*>(&sles_player_itf_));
  if (res != SL_RESULT_SUCCESS) {
    WEBRTC_OPENSL_TRACE(kTraceError, kTraceAudioDevice, id_,
                        "  failed to get Player interface");
    return -1;
  }
  res = (*sles_player_)->GetInterface(
      sles_player_, SL_IID_BUFFERQUEUE,
      static_cast<void*>(&sles_player_sbq_itf_));
  if (res != SL_RESULT_SUCCESS) {
    WEBRTC_OPENSL_TRACE(kTraceError, kTraceAudioDevice, id_,
                        "  failed to get Player SimpleBufferQueue interface");
    return -1;
  }

  // Setup to receive buffer queue event callbacks
  res = (*sles_player_sbq_itf_)->RegisterCallback(
      sles_player_sbq_itf_,
      PlayerSimpleBufferQueueCallback,
      this);
  if (res != SL_RESULT_SUCCESS) {
    WEBRTC_OPENSL_TRACE(kTraceError, kTraceAudioDevice, id_,
                        "  failed to register Player Callback");
    return -1;
  }
  is_play_initialized_ = true;
  return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::InitRecording() {
  CriticalSectionScoped lock(&crit_sect_);

  if (!is_initialized_) {
    WEBRTC_OPENSL_TRACE(kTraceError, kTraceAudioDevice, id_,
                        "  Not initialized");
    return -1;
  }

  if (is_recording_) {
    WEBRTC_OPENSL_TRACE(kTraceWarning, kTraceAudioDevice, id_,
                        "  Recording already started");
    return -1;
  }

  if (!is_recording_dev_specified_) {
    WEBRTC_OPENSL_TRACE(kTraceError, kTraceAudioDevice, id_,
                        "  Recording device is not specified");
    return -1;
  }

  if (is_rec_initialized_) {
    WEBRTC_OPENSL_TRACE(kTraceInfo, kTraceAudioDevice, id_,
                        "  Recording already initialized");
    return 0;
  }

  // Initialize the microphone
  if (InitMicrophone() == -1) {
    WEBRTC_OPENSL_TRACE(kTraceWarning, kTraceAudioDevice, id_,
                        "  InitMicrophone() failed");
  }

  if (sles_engine_ == NULL || sles_engine_itf_ == NULL) {
    WEBRTC_OPENSL_TRACE(kTraceError, kTraceAudioDevice, id_,
                        "  Recording object is NULL");
    return -1;
  }

  SLDataLocator_IODevice micLocator = {
    SL_DATALOCATOR_IODEVICE, SL_IODEVICE_AUDIOINPUT,
    SL_DEFAULTDEVICEID_AUDIOINPUT, NULL };
  SLDataSource audio_source = { &micLocator, NULL };
  SLDataLocator_AndroidSimpleBufferQueue simple_buf_queue = {
    SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
    static_cast<SLuint32>(N_REC_QUEUE_BUFFERS)
  };
  SLDataSink audio_sink = { &simple_buf_queue, &record_pcm_ };

  // Setup the format of the content in the buffer queue
  record_pcm_.formatType = SL_DATAFORMAT_PCM;
  record_pcm_.numChannels = N_REC_CHANNELS;
  if (speaker_sampling_rate_ == 44000) {
    record_pcm_.samplesPerSec = 44100 * 1000;
  } else {
    record_pcm_.samplesPerSec = speaker_sampling_rate_ * 1000;
  }
  record_pcm_.bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_16;
  record_pcm_.containerSize = SL_PCMSAMPLEFORMAT_FIXED_16;
  if (1 == record_pcm_.numChannels) {
    record_pcm_.channelMask = SL_SPEAKER_FRONT_CENTER;
  } else if (2 == record_pcm_.numChannels) {
    record_pcm_.channelMask = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
  } else {
    WEBRTC_OPENSL_TRACE(kTraceError, kTraceAudioDevice, id_,
                        "  %d rec channels not supported", N_REC_CHANNELS);
  }
  record_pcm_.endianness = SL_BYTEORDER_LITTLEENDIAN;

  const SLInterfaceID id[2] = {
    SL_IID_ANDROIDSIMPLEBUFFERQUEUE, SL_IID_ANDROIDCONFIGURATION };
  const SLboolean req[2] = {
    SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE };
  WebRtc_Word32 res = -1;
  res = (*sles_engine_itf_)->CreateAudioRecorder(sles_engine_itf_,
                                                 &sles_recorder_,
                                                 &audio_source,
                                                 &audio_sink,
                                                 2,
                                                 id,
                                                 req);
  if (res != SL_RESULT_SUCCESS) {
    WEBRTC_OPENSL_TRACE(kTraceError, kTraceAudioDevice, id_,
                        "  failed to create Recorder");
    return -1;
  }

  // Realizing the recorder in synchronous mode.
  res = (*sles_recorder_)->Realize(sles_recorder_, SL_BOOLEAN_FALSE);
  if (res != SL_RESULT_SUCCESS) {
    WEBRTC_OPENSL_TRACE(kTraceError, kTraceAudioDevice, id_,
                        "  failed to realize Recorder");
    return -1;
  }

  // Get the RECORD interface - it is an implicit interface
  res = (*sles_recorder_)->GetInterface(
      sles_recorder_, SL_IID_RECORD,
      static_cast<void*>(&sles_recorder_itf_));
  if (res != SL_RESULT_SUCCESS) {
    WEBRTC_OPENSL_TRACE(kTraceError, kTraceAudioDevice, id_,
                        "  failed to get Recorder interface");
    return -1;
  }

  // Get the simpleBufferQueue interface
  res = (*sles_recorder_)->GetInterface(
      sles_recorder_,
      SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
      static_cast<void*>(&sles_recorder_sbq_itf_));
  if (res != SL_RESULT_SUCCESS) {
    WEBRTC_OPENSL_TRACE(kTraceError, kTraceAudioDevice, id_,
                        "  failed to get Recorder Simple Buffer Queue");
    return -1;
  }

  // Setup to receive buffer queue event callbacks
  res = (*sles_recorder_sbq_itf_)->RegisterCallback(
      sles_recorder_sbq_itf_,
      RecorderSimpleBufferQueueCallback,
      this);
  if (res != SL_RESULT_SUCCESS) {
    WEBRTC_OPENSL_TRACE(kTraceError, kTraceAudioDevice, id_,
                        "  failed to register Recorder Callback");
    return -1;
  }

  is_rec_initialized_ = true;
  return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::StartRecording() {
  CriticalSectionScoped lock(&crit_sect_);

  if (!is_rec_initialized_) {
    WEBRTC_OPENSL_TRACE(kTraceError, kTraceAudioDevice, id_,
                        "  Recording not initialized");
    return -1;
  }

  if (is_recording_) {
    WEBRTC_OPENSL_TRACE(kTraceInfo, kTraceAudioDevice, id_,
                        "  Recording already started");
    return 0;
  }

  if (sles_recorder_itf_ == NULL) {
    WEBRTC_OPENSL_TRACE(kTraceError, kTraceAudioDevice, id_,
                        "  RecordITF is NULL");
    return -1;
  }

  if (sles_recorder_sbq_itf_ == NULL) {
    WEBRTC_OPENSL_TRACE(kTraceError, kTraceAudioDevice, id_,
                        "  Recorder Simple Buffer Queue is NULL");
    return -1;
  }

  memset(rec_buf_, 0, sizeof(rec_buf_));
  memset(rec_voe_buf_, 0, sizeof(rec_voe_buf_));
  WebRtc_UWord32 num_bytes =
      N_REC_CHANNELS * sizeof(int16_t) * mic_sampling_rate_ / 100;

  while (!rec_queue_.empty())
    rec_queue_.pop();
  while (!rec_voe_audio_queue_.empty())
    rec_voe_audio_queue_.pop();
  while (!rec_voe_ready_queue_.empty())
    rec_voe_ready_queue_.pop();

  for (int i = 0; i < N_REC_QUEUE_BUFFERS; ++i) {
    rec_voe_ready_queue_.push(rec_voe_buf_[i]);
  }

  WebRtc_Word32 res = -1;
  for (int i = 0; i < N_REC_QUEUE_BUFFERS; ++i) {
    // We assign 10ms buffer to each queue, size given in bytes.
    res = (*sles_recorder_sbq_itf_)->Enqueue(
        sles_recorder_sbq_itf_,
        static_cast<void*>(rec_buf_[i]),
        num_bytes);
    if (res != SL_RESULT_SUCCESS) {
      WEBRTC_OPENSL_TRACE(kTraceError, kTraceAudioDevice, id_,
                          "Recorder Enqueue failed:%d,%d", i, res);
      break;
    } else {
      rec_queue_.push(rec_buf_[i]);
    }
  }

  // Record the audio
  res = (*sles_recorder_itf_)->SetRecordState(sles_recorder_itf_,
                                              SL_RECORDSTATE_RECORDING);
  if (res != SL_RESULT_SUCCESS) {
    WEBRTC_OPENSL_TRACE(kTraceError, kTraceAudioDevice, id_,
                        "  failed to start recording");
    return -1;
  }

  // Start rec thread and playout thread
  rec_thread_ = ThreadWrapper::CreateThread(
      RecThreadFunc,
      this,
      kRealtimePriority,
      "opensl_capture_thread");
  if (rec_thread_ == NULL) {
    WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice, id_,
                 "  failed to create the rec audio thread");
    return -1;
  }

  unsigned int thread_id = 0;
  if (!rec_thread_->Start(thread_id)) {
    WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice, id_,
                 "  failed to start the rec audio thread");
    delete rec_thread_;
    rec_thread_ = NULL;
    return -1;
  }
  rec_thread_id_ = thread_id;

  is_recording_ = true;
  return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::StopRecording() {
  {
    CriticalSectionScoped lock(&crit_sect_);

    if (!is_rec_initialized_) {
      WEBRTC_OPENSL_TRACE(kTraceInfo, kTraceAudioDevice, id_,
                          "  Recording is not initialized");
      return 0;
    }

    if ((sles_recorder_itf_ != NULL) && (sles_recorder_ != NULL)) {
      WebRtc_Word32 res = (*sles_recorder_itf_)->SetRecordState(
          sles_recorder_itf_,
          SL_RECORDSTATE_STOPPED);
      if (res != SL_RESULT_SUCCESS) {
        WEBRTC_OPENSL_TRACE(kTraceError, kTraceAudioDevice, id_,
                            "  failed to stop recording");
        return -1;
      }
      res = (*sles_recorder_sbq_itf_)->Clear(
          sles_recorder_sbq_itf_);
      if (res != SL_RESULT_SUCCESS) {
        WEBRTC_OPENSL_TRACE(kTraceError, kTraceAudioDevice, id_,
                            "  failed to clear recorder buffer queue");
        return -1;
      }

      // Destroy the recorder object
      (*sles_recorder_)->Destroy(sles_recorder_);
      sles_recorder_ = NULL;
      sles_recorder_itf_ = NULL;
    }
  }

  // Stop the playout thread
  if (rec_thread_) {
    if (rec_thread_->Stop()) {
      delete rec_thread_;
      rec_thread_ = NULL;
    } else {
      WEBRTC_TRACE(kTraceError, kTraceAudioDevice, id_,
                   "Failed to stop recording thread ");
      return -1;
    }
  }

  CriticalSectionScoped lock(&crit_sect_);
  is_rec_initialized_ = false;
  is_recording_ = false;
  rec_warning_ = 0;
  rec_error_ = 0;

  return 0;
}

bool AudioDeviceAndroidOpenSLES::RecordingIsInitialized() const {
  return is_rec_initialized_;
}

bool AudioDeviceAndroidOpenSLES::Recording() const {
  return is_recording_;
}

bool AudioDeviceAndroidOpenSLES::PlayoutIsInitialized() const {
  return is_play_initialized_;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::StartPlayout() {
  int i;
  CriticalSectionScoped lock(&crit_sect_);

  if (!is_play_initialized_) {
    WEBRTC_OPENSL_TRACE(kTraceError, kTraceAudioDevice, id_,
                        "  Playout not initialized");
    return -1;
  }

  if (is_playing_) {
    WEBRTC_OPENSL_TRACE(kTraceInfo, kTraceAudioDevice, id_,
                        "  Playout already started");
    return 0;
  }

  if (sles_player_itf_ == NULL) {
    WEBRTC_OPENSL_TRACE(kTraceError, kTraceAudioDevice, id_,
                        "  PlayItf is NULL");
    return -1;
  }
  if (sles_player_sbq_itf_ == NULL) {
    WEBRTC_OPENSL_TRACE(kTraceError, kTraceAudioDevice, id_,
                        "  PlayerSimpleBufferQueue is NULL");
    return -1;
  }

  WebRtc_UWord32 num_bytes =
      N_PLAY_CHANNELS * sizeof(int16_t) * speaker_sampling_rate_ / 100;

  memset(play_buf_, 0, sizeof(play_buf_));

  while (!play_queue_.empty())
    play_queue_.pop();

  WebRtc_Word32 res = -1;
  for (i = 0; i < std::min(2, static_cast<int>(N_PLAY_QUEUE_BUFFERS)); ++i) {
    res = (*sles_player_sbq_itf_)->Enqueue(
        sles_player_sbq_itf_,
        static_cast<void*>(play_buf_[i]),
        num_bytes);
    if (res != SL_RESULT_SUCCESS) {
      WEBRTC_OPENSL_TRACE(kTraceWarning, kTraceAudioDevice, id_,
                          "  player simpler buffer Enqueue failed:%d,%d",
                          i, res);
      break;
    } else {
      play_queue_.push(play_buf_[i]);
    }
  }

  res = (*sles_player_itf_)->SetPlayState(
      sles_player_itf_, SL_PLAYSTATE_PLAYING);
  if (res != SL_RESULT_SUCCESS) {
    WEBRTC_OPENSL_TRACE(kTraceError, kTraceAudioDevice, id_,
                        "  failed to start playout");
    return -1;
  }

  play_warning_ = 0;
  play_error_ = 0;
  is_playing_ = true;

  return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::StopPlayout() {
  {
    CriticalSectionScoped lock(&crit_sect_);
    if (!is_play_initialized_) {
      WEBRTC_OPENSL_TRACE(kTraceInfo, kTraceAudioDevice, id_,
                          "  Playout is not initialized");
      return 0;
    }

    if (!sles_player_itf_ && !sles_output_mixer_ && !sles_player_) {
      // Make sure player is stopped
      WebRtc_Word32 res =
          (*sles_player_itf_)->SetPlayState(sles_player_itf_,
                                           SL_PLAYSTATE_STOPPED);
      if (res != SL_RESULT_SUCCESS) {
        WEBRTC_OPENSL_TRACE(kTraceError, kTraceAudioDevice, id_,
                            "  failed to stop playout");
        return -1;
      }
      res = (*sles_player_sbq_itf_)->Clear(sles_player_sbq_itf_);
      if (res != SL_RESULT_SUCCESS) {
        WEBRTC_OPENSL_TRACE(kTraceError, kTraceAudioDevice, id_,
                            "  failed to clear player buffer queue");
        return -1;
      }

      // Destroy the player
      (*sles_player_)->Destroy(sles_player_);
      // Destroy Output Mix object
      (*sles_output_mixer_)->Destroy(sles_output_mixer_);
      sles_player_ = NULL;
      sles_player_itf_ = NULL;
      sles_player_sbq_itf_ = NULL;
      sles_output_mixer_ = NULL;
    }
  }

  CriticalSectionScoped lock(&crit_sect_);
  is_play_initialized_ = false;
  is_playing_ = false;
  play_warning_ = 0;
  play_error_ = 0;

  return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::PlayoutDelay(
    WebRtc_UWord16& delayMS) const {
  delayMS = playout_delay_;
  return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::RecordingDelay(
    WebRtc_UWord16& delayMS) const {
  delayMS = recording_delay_;
  return 0;
}

bool AudioDeviceAndroidOpenSLES::Playing() const {
  return is_playing_;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::SetPlayoutBuffer(
    const AudioDeviceModule::BufferType type,
    WebRtc_UWord16 sizeMS) {
  WEBRTC_OPENSL_TRACE(kTraceWarning, kTraceAudioDevice, id_,
                      "  API call not supported on this platform");
  return -1;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::PlayoutBuffer(
    AudioDeviceModule::BufferType& type,
    WebRtc_UWord16& sizeMS) const {
  type = AudioDeviceModule::kAdaptiveBufferSize;
  sizeMS = playout_delay_;  // Set to current playout delay
  return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::CPULoad(
    WebRtc_UWord16& load) const {
  WEBRTC_OPENSL_TRACE(kTraceWarning, kTraceAudioDevice, id_,
                      "  API call not supported on this platform");
  return -1;
}

bool AudioDeviceAndroidOpenSLES::PlayoutWarning() const {
  return (play_warning_ > 0);
}

bool AudioDeviceAndroidOpenSLES::PlayoutError() const {
  return (play_error_ > 0);
}

bool AudioDeviceAndroidOpenSLES::RecordingWarning() const {
  return (rec_warning_ > 0);
}

bool AudioDeviceAndroidOpenSLES::RecordingError() const {
  return (rec_error_ > 0);
}

void AudioDeviceAndroidOpenSLES::ClearPlayoutWarning() {
  play_warning_ = 0;
}

void AudioDeviceAndroidOpenSLES::ClearPlayoutError() {
  play_error_ = 0;
}

void AudioDeviceAndroidOpenSLES::ClearRecordingWarning() {
  rec_warning_ = 0;
}

void AudioDeviceAndroidOpenSLES::ClearRecordingError() {
  rec_error_ = 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::SetLoudspeakerStatus(bool enable) {
  loundspeaker_on_ = enable;
  return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::GetLoudspeakerStatus(
    bool& enabled) const {
  enabled = loundspeaker_on_;
  return 0;
}

void AudioDeviceAndroidOpenSLES::PlayerSimpleBufferQueueCallback(
    SLAndroidSimpleBufferQueueItf queue_itf,
    void* p_context) {
  AudioDeviceAndroidOpenSLES* audio_device =
      static_cast<AudioDeviceAndroidOpenSLES*> (p_context);
  audio_device->PlayerSimpleBufferQueueCallbackHandler(queue_itf);
}

void AudioDeviceAndroidOpenSLES::PlayerSimpleBufferQueueCallbackHandler(
    SLAndroidSimpleBufferQueueItf queue_itf) {
  if (is_playing_) {
    const unsigned int num_samples = speaker_sampling_rate_ / 100;
    const unsigned int num_bytes =
        N_PLAY_CHANNELS * num_samples * sizeof(int16_t);
    WebRtc_Word8 buf[PLAY_MAX_TEMP_BUF_SIZE_PER_10ms];
    WebRtc_Word8* audio;

    audio = play_queue_.front();
    play_queue_.pop();

    int num_out = voe_audio_buffer_->RequestPlayoutData(num_samples);
    num_out = voe_audio_buffer_->GetPlayoutData(buf);
    if (num_samples != static_cast<unsigned int>(num_out)) {
      WEBRTC_OPENSL_TRACE(kTraceWarning, kTraceAudioDevice, id_,
                          "num (%u) != num_out (%d)", num_samples, num_out);
      play_warning_ = 1;
    }
    memcpy(audio, buf, num_bytes);
    UpdatePlayoutDelay(num_out);

    int res = (*queue_itf)->Enqueue(queue_itf,
                                    audio,
                                    num_bytes);
    if (res != SL_RESULT_SUCCESS) {
      WEBRTC_OPENSL_TRACE(kTraceWarning, kTraceAudioDevice, id_,
                          "  player callback Enqueue failed, %d", res);
      play_warning_ = 1;
    } else {
      play_queue_.push(audio);
    }
  }
}

bool AudioDeviceAndroidOpenSLES::RecThreadFunc(void* context) {
  return (static_cast<AudioDeviceAndroidOpenSLES*>(
      context)->RecThreadFuncImpl());
}

void AudioDeviceAndroidOpenSLES::RecorderSimpleBufferQueueCallback(
    SLAndroidSimpleBufferQueueItf queue_itf,
    void* p_context) {
  AudioDeviceAndroidOpenSLES* audio_device =
      static_cast<AudioDeviceAndroidOpenSLES*>(p_context);
  audio_device->RecorderSimpleBufferQueueCallbackHandler(queue_itf);
}

bool AudioDeviceAndroidOpenSLES::RecThreadFuncImpl() {
  if (is_recording_) {
    // TODO(leozwang): Add seting correct scheduling and thread priority.

    const unsigned int num_samples = mic_sampling_rate_ / 100;
    const unsigned int num_bytes =
        N_REC_CHANNELS * num_samples * sizeof(int16_t);
    const unsigned int total_bytes = num_bytes;
    WebRtc_Word8 buf[REC_MAX_TEMP_BUF_SIZE_PER_10ms];

    {
      CriticalSectionScoped lock(&crit_sect_);
      if (rec_voe_audio_queue_.size() <= 0) {
        rec_timer_.Wait(1);
        return true;
      }

      WebRtc_Word8* audio = rec_voe_audio_queue_.front();
      rec_voe_audio_queue_.pop();
      memcpy(buf, audio, total_bytes);
      memset(audio, 0, total_bytes);
      rec_voe_ready_queue_.push(audio);
    }

    UpdateRecordingDelay();
    voe_audio_buffer_->SetRecordedBuffer(buf, num_samples);
    voe_audio_buffer_->SetVQEData(playout_delay_, recording_delay_, 0);
    voe_audio_buffer_->DeliverRecordedData();
  }

  return true;
}

void AudioDeviceAndroidOpenSLES::RecorderSimpleBufferQueueCallbackHandler(
    SLAndroidSimpleBufferQueueItf queue_itf) {
  if (is_recording_) {
    const unsigned int num_samples = mic_sampling_rate_ / 100;
    const unsigned int num_bytes =
        N_REC_CHANNELS * num_samples * sizeof(int16_t);
    const unsigned int total_bytes = num_bytes;
    WebRtc_Word8* audio;

    {
      CriticalSectionScoped lock(&crit_sect_);
      audio = rec_queue_.front();
      rec_queue_.pop();
      rec_voe_audio_queue_.push(audio);

      if (rec_voe_ready_queue_.size() <= 0) {
        // Log Error.
        rec_error_ = 1;
        WEBRTC_OPENSL_TRACE(kTraceError, kTraceAudioDevice, id_,
                            "  Audio Rec thread buffers underrun");
      } else {
        audio = rec_voe_ready_queue_.front();
        rec_voe_ready_queue_.pop();
      }
    }

    WebRtc_Word32 res = (*queue_itf)->Enqueue(queue_itf,
                                              audio,
                                              total_bytes);
    if (res != SL_RESULT_SUCCESS) {
      WEBRTC_OPENSL_TRACE(kTraceWarning, kTraceAudioDevice, id_,
                          "  recorder callback Enqueue failed, %d", res);
      rec_warning_ = 1;
      return;
    } else {
      rec_queue_.push(audio);
    }

    // TODO(leozwang): OpenSL ES doesn't support AudioRecorder
    // volume control now, add it when it's ready.
  }
}

void AudioDeviceAndroidOpenSLES::CheckErr(SLresult res) {
  if (res != SL_RESULT_SUCCESS) {
    WEBRTC_OPENSL_TRACE(kTraceError, kTraceAudioDevice, id_,
                        "  AudioDeviceAndroidOpenSLES::CheckErr(%d)", res);
    exit(-1);
  }
}

void AudioDeviceAndroidOpenSLES::UpdatePlayoutDelay(
    WebRtc_UWord32 nSamplePlayed) {
  // TODO(leozwang): Add accurate delay estimat.
  playout_delay_ = (N_PLAY_QUEUE_BUFFERS - 0.5) * 10 +
      N_PLAY_QUEUE_BUFFERS * nSamplePlayed / (speaker_sampling_rate_ / 1000);
}

void AudioDeviceAndroidOpenSLES::UpdateRecordingDelay() {
  // TODO(leozwang): Add accurate delay estimat.
  recording_delay_ = 10;
  const WebRtc_UWord32 noSamp10ms = mic_sampling_rate_ / 100;
  recording_delay_ += (N_REC_QUEUE_BUFFERS * noSamp10ms) /
      (mic_sampling_rate_ / 1000);
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::InitSampleRate() {
  if (sles_engine_ == NULL) {
    WEBRTC_OPENSL_TRACE(kTraceError, kTraceAudioDevice, id_,
                        "  SL Object is NULL");
    return -1;
  }

  mic_sampling_rate_ = N_REC_SAMPLES_PER_SEC;
  speaker_sampling_rate_ = N_PLAY_SAMPLES_PER_SEC;

  WEBRTC_OPENSL_TRACE(kTraceStateInfo, kTraceAudioDevice, id_,
                      "  mic sample rate (%d), speaker sample rate (%d)",
                      mic_sampling_rate_, speaker_sampling_rate_);
  return 0;
}

}  // namespace webrtc
