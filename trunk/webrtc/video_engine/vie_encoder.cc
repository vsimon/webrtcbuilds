/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_engine/vie_encoder.h"

#include <cassert>

#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "modules/pacing/include/paced_sender.h"
#include "modules/rtp_rtcp/interface/rtp_rtcp.h"
#include "modules/utility/interface/process_thread.h"
#include "modules/video_coding/codecs/interface/video_codec_interface.h"
#include "modules/video_coding/main/interface/video_coding.h"
#include "modules/video_coding/main/interface/video_coding_defines.h"
#include "system_wrappers/interface/critical_section_wrapper.h"
#include "system_wrappers/interface/logging.h"
#include "system_wrappers/interface/tick_util.h"
#include "system_wrappers/interface/trace.h"
#include "video_engine/include/vie_codec.h"
#include "video_engine/include/vie_image_process.h"
#include "video_engine/vie_defines.h"

namespace webrtc {

// Pace in kbits/s until we receive first estimate.
static const int kInitialPace = 2000;
// Allow packets to be transmitted in up to 2 times max video bitrate if the
// bandwidth estimate allows it.
// TODO(holmer): Expose transmission start, min and max bitrates in the
// VideoEngine API and remove the kTransmissionMaxBitrateMultiplier.
static const int kTransmissionMaxBitrateMultiplier = 2;

class QMVideoSettingsCallback : public VCMQMSettingsCallback {
 public:
  explicit QMVideoSettingsCallback(VideoProcessingModule* vpm);
  ~QMVideoSettingsCallback();

  // Update VPM with QM (quality modes: frame size & frame rate) settings.
  WebRtc_Word32 SetVideoQMSettings(const WebRtc_UWord32 frame_rate,
                                   const WebRtc_UWord32 width,
                                   const WebRtc_UWord32 height);

 private:
  VideoProcessingModule* vpm_;
};

class ViEBitrateObserver : public BitrateObserver {
 public:
  explicit ViEBitrateObserver(ViEEncoder* owner)
      : owner_(owner) {
  }
  // Implements BitrateObserver.
  virtual void OnNetworkChanged(const uint32_t bitrate_bps,
                                const uint8_t fraction_lost,
                                const uint32_t rtt) {
    owner_->OnNetworkChanged(bitrate_bps, fraction_lost, rtt);
  }
 private:
  ViEEncoder* owner_;
};

class ViEPacedSenderCallback : public PacedSender::Callback {
 public:
  explicit ViEPacedSenderCallback(ViEEncoder* owner)
      : owner_(owner) {
  }
  virtual void TimeToSendPacket(uint32_t ssrc, uint16_t sequence_number,
                                int64_t capture_time_ms) {
    owner_->TimeToSendPacket(ssrc, sequence_number, capture_time_ms);
  }
  virtual void TimeToSendPadding(int /*bytes*/) {
    // TODO(pwestin): Hook up this.
  }
 private:
  ViEEncoder* owner_;
};

ViEEncoder::ViEEncoder(WebRtc_Word32 engine_id,
                       WebRtc_Word32 channel_id,
                       WebRtc_UWord32 number_of_cores,
                       ProcessThread& module_process_thread,
                       BitrateController* bitrate_controller)
  : engine_id_(engine_id),
    channel_id_(channel_id),
    number_of_cores_(number_of_cores),
    vcm_(*webrtc::VideoCodingModule::Create(ViEModuleId(engine_id,
                                                        channel_id))),
    vpm_(*webrtc::VideoProcessingModule::Create(ViEModuleId(engine_id,
                                                            channel_id))),
    default_rtp_rtcp_(NULL),
    callback_cs_(CriticalSectionWrapper::CreateCriticalSection()),
    data_cs_(CriticalSectionWrapper::CreateCriticalSection()),
    bitrate_controller_(bitrate_controller),
    target_delay_ms_(0),
    network_is_transmitting_(true),
    encoder_paused_(false),
    channels_dropping_delta_frames_(0),
    drop_next_frame_(false),
    fec_enabled_(false),
    nack_enabled_(false),
    codec_observer_(NULL),
    effect_filter_(NULL),
    module_process_thread_(module_process_thread),
    has_received_sli_(false),
    picture_id_sli_(0),
    has_received_rpsi_(false),
    picture_id_rpsi_(0),
    file_recorder_(channel_id),
    qm_callback_(NULL) {
  WEBRTC_TRACE(webrtc::kTraceMemory, webrtc::kTraceVideo,
               ViEId(engine_id, channel_id),
               "%s(engine_id: %d) 0x%p - Constructor", __FUNCTION__, engine_id,
               this);

  RtpRtcp::Configuration configuration;
  configuration.id = ViEModuleId(engine_id_, channel_id_);
  configuration.audio = false;  // Video.

  default_rtp_rtcp_.reset(RtpRtcp::CreateRtpRtcp(configuration));
  bitrate_observer_.reset(new ViEBitrateObserver(this));
  pacing_callback_.reset(new ViEPacedSenderCallback(this));
  paced_sender_.reset(new PacedSender(pacing_callback_.get(), kInitialPace));
}

bool ViEEncoder::Init() {
  if (vcm_.InitializeSender() != 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_),
                 "%s InitializeSender failure", __FUNCTION__);
    return false;
  }
  vpm_.EnableTemporalDecimation(true);

  // Enable/disable content analysis: off by default for now.
  vpm_.EnableContentAnalysis(false);

  if (module_process_thread_.RegisterModule(&vcm_) != 0 ||
      module_process_thread_.RegisterModule(default_rtp_rtcp_.get()) != 0 ||
      module_process_thread_.RegisterModule(paced_sender_.get()) != 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_),
                 "%s RegisterModule failure", __FUNCTION__);
    return false;
  }
  if (qm_callback_) {
    delete qm_callback_;
  }
  qm_callback_ = new QMVideoSettingsCallback(&vpm_);

#ifdef VIDEOCODEC_VP8
  VideoCodec video_codec;
  if (vcm_.Codec(webrtc::kVideoCodecVP8, &video_codec) != VCM_OK) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_),
                 "%s Codec failure", __FUNCTION__);
    return false;
  }
  if (vcm_.RegisterSendCodec(&video_codec, number_of_cores_,
                             default_rtp_rtcp_->MaxDataPayloadLength()) != 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_),
                 "%s RegisterSendCodec failure", __FUNCTION__);
    return false;
  }
  if (default_rtp_rtcp_->RegisterSendPayload(video_codec) != 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_),
                 "%s RegisterSendPayload failure", __FUNCTION__);
    return false;
  }
  if (default_rtp_rtcp_->RegisterSendRtpHeaderExtension(
      kRtpExtensionTransmissionTimeOffset, 1) != 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_),
                 "%s RegisterSendRtpHeaderExtension failure", __FUNCTION__);
    return false;
  }
#else
  VideoCodec video_codec;
  if (vcm_.Codec(webrtc::kVideoCodecI420, &video_codec) == VCM_OK) {
    vcm_.RegisterSendCodec(&video_codec, number_of_cores_,
                           default_rtp_rtcp_->MaxDataPayloadLength());
    default_rtp_rtcp_->RegisterSendPayload(video_codec);
  } else {
    return false;
  }
#endif

  if (vcm_.RegisterTransportCallback(this) != 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_),
                 "ViEEncoder: VCM::RegisterTransportCallback failure");
    return false;
  }
  if (vcm_.RegisterSendStatisticsCallback(this) != 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_),
                 "ViEEncoder: VCM::RegisterSendStatisticsCallback failure");
    return false;
  }
  if (vcm_.RegisterVideoQMCallback(qm_callback_) != 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_),
                 "VCM::RegisterQMCallback failure");
    return false;
  }
  return true;
}

ViEEncoder::~ViEEncoder() {
  WEBRTC_TRACE(webrtc::kTraceMemory, webrtc::kTraceVideo,
               ViEId(engine_id_, channel_id_),
               "ViEEncoder Destructor 0x%p, engine_id: %d", this, engine_id_);
  if (bitrate_controller_) {
    bitrate_controller_->RemoveBitrateObserver(bitrate_observer_.get());
  }
  module_process_thread_.DeRegisterModule(&vcm_);
  module_process_thread_.DeRegisterModule(&vpm_);
  module_process_thread_.DeRegisterModule(default_rtp_rtcp_.get());
  module_process_thread_.DeRegisterModule(paced_sender_.get());
  VideoCodingModule::Destroy(&vcm_);
  VideoProcessingModule::Destroy(&vpm_);
  delete qm_callback_;
}

int ViEEncoder::Owner() const {
  return channel_id_;
}

void ViEEncoder::SetNetworkTransmissionState(bool is_transmitting) {
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
               ViEId(engine_id_, channel_id_),
               "%s(%s)", __FUNCTION__,
               is_transmitting ? "transmitting" : "not transmitting");
  {
    CriticalSectionScoped cs(data_cs_.get());
    network_is_transmitting_ = is_transmitting;
  }
  if (is_transmitting) {
    paced_sender_->Resume();
  } else {
    paced_sender_->Pause();
  }
}

void ViEEncoder::Pause() {
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
               ViEId(engine_id_, channel_id_),
               "%s", __FUNCTION__);
  CriticalSectionScoped cs(data_cs_.get());
  encoder_paused_ = true;
}

void ViEEncoder::Restart() {
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
               ViEId(engine_id_, channel_id_),
               "%s", __FUNCTION__);
  CriticalSectionScoped cs(data_cs_.get());
  encoder_paused_ = false;
}

WebRtc_Word32 ViEEncoder::DropDeltaAfterKey(bool enable) {
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
               ViEId(engine_id_, channel_id_),
               "%s(%d)", __FUNCTION__, enable);
  CriticalSectionScoped cs(data_cs_.get());

  if (enable) {
    channels_dropping_delta_frames_++;
  } else {
    channels_dropping_delta_frames_--;
    if (channels_dropping_delta_frames_ < 0) {
      channels_dropping_delta_frames_ = 0;
      WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
                   ViEId(engine_id_, channel_id_),
                   "%s: Called too many times", __FUNCTION__);
      return -1;
    }
  }
  return 0;
}

WebRtc_UWord8 ViEEncoder::NumberOfCodecs() {
  return vcm_.NumberOfCodecs();
}

WebRtc_Word32 ViEEncoder::GetCodec(WebRtc_UWord8 list_index,
                                   VideoCodec* video_codec) {
  if (vcm_.Codec(list_index, video_codec) != 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_), "%s: Could not get codec",
                 __FUNCTION__);
    return -1;
  }
  return 0;
}

WebRtc_Word32 ViEEncoder::RegisterExternalEncoder(webrtc::VideoEncoder* encoder,
                                                  WebRtc_UWord8 pl_type,
                                                  bool internal_source) {
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
               ViEId(engine_id_, channel_id_), "%s: pltype %u", __FUNCTION__,
               pl_type);

  if (encoder == NULL)
    return -1;

  if (vcm_.RegisterExternalEncoder(encoder, pl_type, internal_source) !=
          VCM_OK) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_),
                 "Could not register external encoder");
    return -1;
  }
  return 0;
}

WebRtc_Word32 ViEEncoder::DeRegisterExternalEncoder(WebRtc_UWord8 pl_type) {
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
               ViEId(engine_id_, channel_id_),
               "%s: pltype %u", __FUNCTION__, pl_type);

  webrtc::VideoCodec current_send_codec;
  if (vcm_.SendCodec(&current_send_codec) == VCM_OK) {
    uint32_t current_bitrate_bps = 0;
    if (vcm_.Bitrate(&current_bitrate_bps) != 0) {
      WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideo,
                   ViEId(engine_id_, channel_id_),
                   "Failed to get the current encoder target bitrate.");
    }
    current_send_codec.startBitrate = (current_bitrate_bps + 500) / 1000;
  }

  if (vcm_.RegisterExternalEncoder(NULL, pl_type) != VCM_OK) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_),
                 "Could not deregister external encoder");
    return -1;
  }

  // If the external encoder is the current send codeci, use vcm internal
  // encoder.
  if (current_send_codec.plType == pl_type) {
    WebRtc_UWord16 max_data_payload_length =
        default_rtp_rtcp_->MaxDataPayloadLength();
    if (vcm_.RegisterSendCodec(&current_send_codec, number_of_cores_,
                               max_data_payload_length) != VCM_OK) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(engine_id_, channel_id_),
                   "Could not use internal encoder");
      return -1;
    }
  }
  return 0;
}

WebRtc_Word32 ViEEncoder::SetEncoder(const webrtc::VideoCodec& video_codec) {
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
               ViEId(engine_id_, channel_id_),
               "%s: CodecType: %d, width: %u, height: %u", __FUNCTION__,
               video_codec.codecType, video_codec.width, video_codec.height);

  // Setting target width and height for VPM.
  if (vpm_.SetTargetResolution(video_codec.width, video_codec.height,
                               video_codec.maxFramerate) != VPM_OK) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_),
                 "Could not set VPM target dimensions");
    return -1;
  }

  if (default_rtp_rtcp_->RegisterSendPayload(video_codec) != 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_),
                 "Could register RTP module video payload");
    return -1;
  }
  // Convert from kbps to bps.
  default_rtp_rtcp_->SetTargetSendBitrate(video_codec.startBitrate * 1000);

  WebRtc_UWord16 max_data_payload_length =
      default_rtp_rtcp_->MaxDataPayloadLength();

  if (vcm_.RegisterSendCodec(&video_codec, number_of_cores_,
                             max_data_payload_length) != VCM_OK) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_),
                 "Could not register send codec");
    return -1;
  }

  // Set this module as sending right away, let the slave module in the channel
  // start and stop sending.
  if (default_rtp_rtcp_->Sending() == false) {
    if (default_rtp_rtcp_->SetSendingStatus(true) != 0) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(engine_id_, channel_id_),
                   "Could start RTP module sending");
      return -1;
    }
  }
  bitrate_controller_->SetBitrateObserver(bitrate_observer_.get(),
                                          video_codec.startBitrate * 1000,
                                          video_codec.minBitrate * 1000,
                                          kTransmissionMaxBitrateMultiplier *
                                          video_codec.maxBitrate * 1000);

  return 0;
}

WebRtc_Word32 ViEEncoder::GetEncoder(VideoCodec* video_codec) {
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
               ViEId(engine_id_, channel_id_), "%s", __FUNCTION__);

  if (vcm_.SendCodec(video_codec) != 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_),
                 "Could not get VCM send codec");
    return -1;
  }
  return 0;
}

WebRtc_Word32 ViEEncoder::GetCodecConfigParameters(
    unsigned char config_parameters[kConfigParameterSize],
    unsigned char& config_parameters_size) {
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
               ViEId(engine_id_, channel_id_), "%s", __FUNCTION__);

  WebRtc_Word32 num_parameters =
      vcm_.CodecConfigParameters(config_parameters, kConfigParameterSize);
  if (num_parameters <= 0) {
    config_parameters_size = 0;
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_),
                 "Could not get config parameters");
    return -1;
  }
  config_parameters_size = static_cast<unsigned char>(num_parameters);
  return 0;
}

WebRtc_Word32 ViEEncoder::ScaleInputImage(bool enable) {
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
               ViEId(engine_id_, channel_id_), "%s(enable %d)", __FUNCTION__,
               enable);

  VideoFrameResampling resampling_mode = kFastRescaling;
  if (enable == true) {
    // kInterpolation is currently not supported.
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_), "%s not supported",
                 __FUNCTION__, enable);
    return -1;
  }
  vpm_.SetInputFrameResampleMode(resampling_mode);

  return 0;
}

void ViEEncoder::TimeToSendPacket(uint32_t ssrc, uint16_t sequence_number,
                                  int64_t capture_time_ms) {
  default_rtp_rtcp_->TimeToSendPacket(ssrc, sequence_number, capture_time_ms);
}

bool ViEEncoder::EncoderPaused() const {
  // Pause video if paused by caller or as long as the network is down and the
  // pacer queue has grown too large.
  const bool max_send_buffer_reached =
      paced_sender_->QueueInMs() >= target_delay_ms_;
  return encoder_paused_ ||
      (!network_is_transmitting_ && max_send_buffer_reached);
}

RtpRtcp* ViEEncoder::SendRtpRtcpModule() {
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
               ViEId(engine_id_, channel_id_), "%s", __FUNCTION__);

  return default_rtp_rtcp_.get();
}

void ViEEncoder::DeliverFrame(int id,
                              I420VideoFrame* video_frame,
                              int num_csrcs,
                              const WebRtc_UWord32 CSRC[kRtpCsrcSize]) {
  WEBRTC_TRACE(webrtc::kTraceStream,
               webrtc::kTraceVideo,
               ViEId(engine_id_, channel_id_),
               "%s: %llu", __FUNCTION__,
               video_frame->timestamp());
  {
    CriticalSectionScoped cs(data_cs_.get());
    if (EncoderPaused() || default_rtp_rtcp_->SendingMedia() == false) {
      // We've paused or we have no channels attached, don't encode.
      return;
    }
    if (drop_next_frame_) {
      // Drop this frame.
      WEBRTC_TRACE(webrtc::kTraceStream,
                   webrtc::kTraceVideo,
                   ViEId(engine_id_, channel_id_),
                   "%s: Dropping frame %llu after a key fame", __FUNCTION__,
                   video_frame->timestamp());
      drop_next_frame_ = false;
      return;
    }
  }

  // Convert render time, in ms, to RTP timestamp.
  const int kMsToRtpTimestamp = 90;
  const WebRtc_UWord32 time_stamp =
      kMsToRtpTimestamp *
      static_cast<WebRtc_UWord32>(video_frame->render_time_ms());
  video_frame->set_timestamp(time_stamp);
  {
    CriticalSectionScoped cs(callback_cs_.get());
    if (effect_filter_) {
      unsigned int length = CalcBufferSize(kI420,
                                           video_frame->width(),
                                           video_frame->height());
      scoped_array<uint8_t> video_buffer(new uint8_t[length]);
      ExtractBuffer(*video_frame, length, video_buffer.get());
      effect_filter_->Transform(length,
                                video_buffer.get(),
                                video_frame->timestamp(),
                                video_frame->width(),
                                video_frame->height());
    }
  }
  // Record raw frame.
  file_recorder_.RecordVideoFrame(*video_frame);

  // Make sure the CSRC list is correct.
  if (num_csrcs > 0) {
    WebRtc_UWord32 tempCSRC[kRtpCsrcSize];
    for (int i = 0; i < num_csrcs; i++) {
      if (CSRC[i] == 1) {
        tempCSRC[i] = default_rtp_rtcp_->SSRC();
      } else {
        tempCSRC[i] = CSRC[i];
      }
    }
    default_rtp_rtcp_->SetCSRCs(tempCSRC, (WebRtc_UWord8) num_csrcs);
  }
  // Pass frame via preprocessor.
  I420VideoFrame* decimated_frame = NULL;
  const int ret = vpm_.PreprocessFrame(*video_frame, &decimated_frame);
  if (ret == 1) {
    // Drop this frame.
    return;
  }
  if (ret != VPM_OK) {
    WEBRTC_TRACE(webrtc::kTraceError,
                 webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_),
                 "%s: Error preprocessing frame %u", __FUNCTION__,
                 video_frame->timestamp());
    return;
  }
  // Frame was not sampled => use original.
  if (decimated_frame == NULL)  {
    decimated_frame = video_frame;
  }
#ifdef VIDEOCODEC_VP8
  if (vcm_.SendCodec() == webrtc::kVideoCodecVP8) {
    webrtc::CodecSpecificInfo codec_specific_info;
    codec_specific_info.codecType = webrtc::kVideoCodecVP8;
    codec_specific_info.codecSpecific.VP8.hasReceivedRPSI =
        has_received_rpsi_;
    codec_specific_info.codecSpecific.VP8.hasReceivedSLI =
        has_received_sli_;
    codec_specific_info.codecSpecific.VP8.pictureIdRPSI =
        picture_id_rpsi_;
    codec_specific_info.codecSpecific.VP8.pictureIdSLI  =
        picture_id_sli_;
    has_received_sli_ = false;
    has_received_rpsi_ = false;

    if (vcm_.AddVideoFrame(*decimated_frame,
                           vpm_.ContentMetrics(),
                           &codec_specific_info) != VCM_OK) {
      WEBRTC_TRACE(webrtc::kTraceError,
                   webrtc::kTraceVideo,
                   ViEId(engine_id_, channel_id_),
                   "%s: Error encoding frame %u", __FUNCTION__,
                   video_frame->timestamp());
    }
    return;
  }
#endif
  if (vcm_.AddVideoFrame(*decimated_frame) != VCM_OK) {
    WEBRTC_TRACE(webrtc::kTraceError,
                 webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_),
                 "%s: Error encoding frame %u", __FUNCTION__,
                 video_frame->timestamp());
  }
}

void ViEEncoder::DelayChanged(int id, int frame_delay) {
  WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceVideo,
               ViEId(engine_id_, channel_id_), "%s: %u", __FUNCTION__,
               frame_delay);

  default_rtp_rtcp_->SetCameraDelay(frame_delay);
  file_recorder_.SetFrameDelay(frame_delay);
}

int ViEEncoder::GetPreferedFrameSettings(int* width,
                                         int* height,
                                         int* frame_rate) {
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
               ViEId(engine_id_, channel_id_), "%s", __FUNCTION__);

  webrtc::VideoCodec video_codec;
  memset(&video_codec, 0, sizeof(video_codec));
  if (vcm_.SendCodec(&video_codec) != VCM_OK) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_),
                 "Could not get VCM send codec");
    return -1;
  }

  *width = video_codec.width;
  *height = video_codec.height;
  *frame_rate = video_codec.maxFramerate;
  return 0;
}

int ViEEncoder::SendKeyFrame() {
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
               ViEId(engine_id_, channel_id_), "%s", __FUNCTION__);
  return vcm_.IntraFrameRequest(0);
}

WebRtc_Word32 ViEEncoder::SendCodecStatistics(
    WebRtc_UWord32* num_key_frames, WebRtc_UWord32* num_delta_frames) {
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
               ViEId(engine_id_, channel_id_), "%s", __FUNCTION__);

  webrtc::VCMFrameCount sent_frames;
  if (vcm_.SentFrameCount(sent_frames) != VCM_OK) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_),
                 "%s: Could not get sent frame information", __FUNCTION__);
    return -1;
  }
  *num_key_frames = sent_frames.numKeyFrames;
  *num_delta_frames = sent_frames.numDeltaFrames;
  return 0;
}

WebRtc_Word32 ViEEncoder::EstimatedSendBandwidth(
    WebRtc_UWord32* available_bandwidth) const {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);

  if (!bitrate_controller_->AvailableBandwidth(available_bandwidth)) {
    return -1;
  }
  return 0;
}

int ViEEncoder::CodecTargetBitrate(WebRtc_UWord32* bitrate) const {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);
  if (vcm_.Bitrate(bitrate) != 0)
    return -1;
  return 0;
}

WebRtc_Word32 ViEEncoder::UpdateProtectionMethod() {
  bool fec_enabled = false;
  WebRtc_UWord8 dummy_ptype_red = 0;
  WebRtc_UWord8 dummy_ptypeFEC = 0;

  // Updated protection method to VCM to get correct packetization sizes.
  // FEC has larger overhead than NACK -> set FEC if used.
  WebRtc_Word32 error = default_rtp_rtcp_->GenericFECStatus(fec_enabled,
                                                           dummy_ptype_red,
                                                           dummy_ptypeFEC);
  if (error) {
    return -1;
  }

  bool nack_enabled = (default_rtp_rtcp_->NACK() == kNackOff) ? false : true;
  if (fec_enabled_ == fec_enabled && nack_enabled_ == nack_enabled) {
    // No change needed, we're already in correct state.
    return 0;
  }
  fec_enabled_ = fec_enabled;
  nack_enabled_ = nack_enabled;

  // Set Video Protection for VCM.
  if (fec_enabled && nack_enabled) {
    vcm_.SetVideoProtection(webrtc::kProtectionNackFEC, true);
  } else {
    vcm_.SetVideoProtection(webrtc::kProtectionFEC, fec_enabled_);
    vcm_.SetVideoProtection(webrtc::kProtectionNack, nack_enabled_);
    vcm_.SetVideoProtection(webrtc::kProtectionNackFEC, false);
  }

  if (fec_enabled || nack_enabled) {
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_), "%s: FEC status ",
                 __FUNCTION__, fec_enabled);
    vcm_.RegisterProtectionCallback(this);
    // The send codec must be registered to set correct MTU.
    webrtc::VideoCodec codec;
    if (vcm_.SendCodec(&codec) == 0) {
      WebRtc_UWord16 max_pay_load = default_rtp_rtcp_->MaxDataPayloadLength();
      uint32_t current_bitrate_bps = 0;
      if (vcm_.Bitrate(&current_bitrate_bps) != 0) {
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideo,
                     ViEId(engine_id_, channel_id_),
                     "Failed to get the current encoder target bitrate.");
      }
      // Convert to start bitrate in kbps.
      codec.startBitrate = (current_bitrate_bps + 500) / 1000;
      if (vcm_.RegisterSendCodec(&codec, number_of_cores_, max_pay_load) != 0) {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(engine_id_, channel_id_),
                     "%s: Failed to update Sendcodec when enabling FEC",
                     __FUNCTION__, fec_enabled);
        return -1;
      }
    }
    return 0;
  } else {
    // FEC and NACK are disabled.
    vcm_.RegisterProtectionCallback(NULL);
  }
  return 0;
}

void ViEEncoder::SetSenderBufferingMode(int target_delay_ms) {
  {
    CriticalSectionScoped cs(data_cs_.get());
    target_delay_ms_ = target_delay_ms;
  }
  if (target_delay_ms > 0) {
    // Disable external frame-droppers.
    vcm_.EnableFrameDropper(false);
    vpm_.EnableTemporalDecimation(false);
  } else {
    // Real-time mode - enable frame droppers.
    vpm_.EnableTemporalDecimation(true);
    vcm_.EnableFrameDropper(true);
  }
}

WebRtc_Word32 ViEEncoder::SendData(
    const FrameType frame_type,
    const WebRtc_UWord8 payload_type,
    const WebRtc_UWord32 time_stamp,
    int64_t capture_time_ms,
    const WebRtc_UWord8* payload_data,
    const WebRtc_UWord32 payload_size,
    const webrtc::RTPFragmentationHeader& fragmentation_header,
    const RTPVideoHeader* rtp_video_hdr) {
  {
    CriticalSectionScoped cs(data_cs_.get());
    if (EncoderPaused()) {
      // Paused, don't send this packet.
      return 0;
    }
    if (channels_dropping_delta_frames_ &&
        frame_type == webrtc::kVideoFrameKey) {
      WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceVideo,
                   ViEId(engine_id_, channel_id_),
                   "%s: Sending key frame, drop next frame", __FUNCTION__);
      drop_next_frame_ = true;
    }
  }

  // New encoded data, hand over to the rtp module.
  return default_rtp_rtcp_->SendOutgoingData(frame_type,
                                             payload_type,
                                             time_stamp,
                                             capture_time_ms,
                                             payload_data,
                                             payload_size,
                                             &fragmentation_header,
                                             rtp_video_hdr);
}

WebRtc_Word32 ViEEncoder::ProtectionRequest(
    const FecProtectionParams* delta_fec_params,
    const FecProtectionParams* key_fec_params,
    WebRtc_UWord32* sent_video_rate_bps,
    WebRtc_UWord32* sent_nack_rate_bps,
    WebRtc_UWord32* sent_fec_rate_bps) {
  WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceVideo,
               ViEId(engine_id_, channel_id_),
               "%s, deltaFECRate: %u, key_fecrate: %u, "
               "delta_use_uep_protection: %d, key_use_uep_protection: %d, "
               "delta_max_fec_frames: %d, key_max_fec_frames: %d, "
               "delta_mask_type: %d, key_mask_type: %d, ",
               __FUNCTION__,
               delta_fec_params->fec_rate,
               key_fec_params->fec_rate,
               delta_fec_params->use_uep_protection,
               key_fec_params->use_uep_protection,
               delta_fec_params->max_fec_frames,
               key_fec_params->max_fec_frames,
               delta_fec_params->fec_mask_type,
               key_fec_params->fec_mask_type);
  if (default_rtp_rtcp_->SetFecParameters(delta_fec_params,
                                         key_fec_params) != 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_),
                 "%s: Could not update FEC parameters", __FUNCTION__);
  }
  default_rtp_rtcp_->BitrateSent(NULL,
                                sent_video_rate_bps,
                                sent_fec_rate_bps,
                                sent_nack_rate_bps);
  return 0;
}

WebRtc_Word32 ViEEncoder::SendStatistics(const WebRtc_UWord32 bit_rate,
                                         const WebRtc_UWord32 frame_rate) {
  CriticalSectionScoped cs(callback_cs_.get());
  if (codec_observer_) {
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_), "%s: bitrate %u, framerate %u",
                 __FUNCTION__, bit_rate, frame_rate);
    codec_observer_->OutgoingRate(channel_id_, frame_rate, bit_rate);
  }
  return 0;
}

WebRtc_Word32 ViEEncoder::RegisterCodecObserver(ViEEncoderObserver* observer) {
  CriticalSectionScoped cs(callback_cs_.get());
  if (observer) {
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_), "%s: observer added",
                 __FUNCTION__);
    if (codec_observer_) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(engine_id_, channel_id_), "%s: observer already set.",
                   __FUNCTION__);
      return -1;
    }
    codec_observer_ = observer;
  } else {
    if (codec_observer_ == NULL) {
      WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
                   ViEId(engine_id_, channel_id_),
                   "%s: observer does not exist.", __FUNCTION__);
      return -1;
    }
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_), "%s: observer removed",
                 __FUNCTION__);
    codec_observer_ = NULL;
  }
  return 0;
}

void ViEEncoder::OnReceivedSLI(uint32_t /*ssrc*/,
                               uint8_t picture_id) {
  picture_id_sli_ = picture_id;
  has_received_sli_ = true;
}

void ViEEncoder::OnReceivedRPSI(uint32_t /*ssrc*/,
                                uint64_t picture_id) {
  picture_id_rpsi_ = picture_id;
  has_received_rpsi_ = true;
}

void ViEEncoder::OnReceivedIntraFrameRequest(uint32_t ssrc) {
  // Key frame request from remote side, signal to VCM.
  WEBRTC_TRACE(webrtc::kTraceStateInfo, webrtc::kTraceVideo,
               ViEId(engine_id_, channel_id_), "%s", __FUNCTION__);

  int idx = 0;
  {
    CriticalSectionScoped cs(data_cs_.get());
    std::map<unsigned int, int>::iterator stream_it = ssrc_streams_.find(ssrc);
    if (stream_it == ssrc_streams_.end()) {
      LOG_F(LS_WARNING) << "ssrc not found: " << ssrc << ", map size "
                        << ssrc_streams_.size();
      return;
    }
    std::map<unsigned int, WebRtc_Word64>::iterator time_it =
        time_last_intra_request_ms_.find(ssrc);
    if (time_it == time_last_intra_request_ms_.end()) {
      time_last_intra_request_ms_[ssrc] = 0;
    }

    WebRtc_Word64 now = TickTime::MillisecondTimestamp();
    if (time_last_intra_request_ms_[ssrc] + kViEMinKeyRequestIntervalMs > now) {
      WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceVideo,
                   ViEId(engine_id_, channel_id_),
                   "%s: Not encoding new intra due to timing", __FUNCTION__);
      return;
    }
    time_last_intra_request_ms_[ssrc] = now;
    idx = stream_it->second;
  }
  // Release the critsect before triggering key frame.
  vcm_.IntraFrameRequest(idx);
}

void ViEEncoder::OnLocalSsrcChanged(uint32_t old_ssrc, uint32_t new_ssrc) {
  CriticalSectionScoped cs(data_cs_.get());
  std::map<unsigned int, int>::iterator it = ssrc_streams_.find(old_ssrc);
  if (it == ssrc_streams_.end()) {
    return;
  }

  ssrc_streams_[new_ssrc] = it->second;
  ssrc_streams_.erase(it);

  std::map<unsigned int, int64_t>::iterator time_it =
      time_last_intra_request_ms_.find(old_ssrc);
  int64_t last_intra_request_ms = 0;
  if (time_it != time_last_intra_request_ms_.end()) {
    last_intra_request_ms = time_it->second;
    time_last_intra_request_ms_.erase(time_it);
  }
  time_last_intra_request_ms_[new_ssrc] = last_intra_request_ms;
}

bool ViEEncoder::SetSsrcs(const std::list<unsigned int>& ssrcs) {
  VideoCodec codec;
  if (vcm_.SendCodec(&codec) != 0)
    return false;

  if (codec.numberOfSimulcastStreams > 0 &&
      ssrcs.size() != codec.numberOfSimulcastStreams) {
    return false;
  }

  CriticalSectionScoped cs(data_cs_.get());
  ssrc_streams_.clear();
  time_last_intra_request_ms_.clear();
  int idx = 0;
  for (std::list<unsigned int>::const_iterator it = ssrcs.begin();
       it != ssrcs.end(); ++it, ++idx) {
    unsigned int ssrc = *it;
    ssrc_streams_[ssrc] = idx;
  }
  return true;
}

// Called from ViEBitrateObserver.
void ViEEncoder::OnNetworkChanged(const uint32_t bitrate_bps,
                                  const uint8_t fraction_lost,
                                  const uint32_t round_trip_time_ms) {
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
               ViEId(engine_id_, channel_id_),
               "%s(bitrate_bps: %u, fraction_lost: %u, rtt_ms: %u",
               __FUNCTION__, bitrate_bps, fraction_lost, round_trip_time_ms);

  vcm_.SetChannelParameters(bitrate_bps, fraction_lost, round_trip_time_ms);
  int bitrate_kbps = bitrate_bps / 1000;
  paced_sender_->UpdateBitrate(bitrate_kbps);
  default_rtp_rtcp_->SetTargetSendBitrate(bitrate_bps);
}

PacedSender* ViEEncoder::GetPacedSender() {
  return paced_sender_.get();
}

WebRtc_Word32 ViEEncoder::RegisterEffectFilter(ViEEffectFilter* effect_filter) {
  CriticalSectionScoped cs(callback_cs_.get());
  if (effect_filter == NULL) {
    if (effect_filter_ == NULL) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(engine_id_, channel_id_), "%s: no effect filter added",
                   __FUNCTION__);
      return -1;
    }
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_), "%s: deregister effect filter",
                 __FUNCTION__);
  } else {
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_), "%s: register effect",
                 __FUNCTION__);
    if (effect_filter_) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(engine_id_, channel_id_),
                   "%s: effect filter already added ", __FUNCTION__);
      return -1;
    }
  }
  effect_filter_ = effect_filter;
  return 0;
}

ViEFileRecorder& ViEEncoder::GetOutgoingFileRecorder() {
  return file_recorder_;
}

int ViEEncoder::StartDebugRecording(const char* fileNameUTF8) {
  return vcm_.StartDebugRecording(fileNameUTF8);
}

int ViEEncoder::StopDebugRecording() {
  return vcm_.StopDebugRecording();
}

QMVideoSettingsCallback::QMVideoSettingsCallback(VideoProcessingModule* vpm)
    : vpm_(vpm) {
}

QMVideoSettingsCallback::~QMVideoSettingsCallback() {
}

WebRtc_Word32 QMVideoSettingsCallback::SetVideoQMSettings(
    const WebRtc_UWord32 frame_rate,
    const WebRtc_UWord32 width,
    const WebRtc_UWord32 height) {
  return vpm_->SetTargetResolution(width, height, frame_rate);
}

}  // namespace webrtc
