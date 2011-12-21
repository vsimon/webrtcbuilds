/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_engine/vie_channel.h"

#include <algorithm>

#include "modules/rtp_rtcp/interface/rtp_rtcp.h"
#include "modules/udp_transport/interface/udp_transport.h"
#include "modules/utility/interface/process_thread.h"
#include "modules/video_coding/main/interface/video_coding.h"
#include "modules/video_processing/main/interface/video_processing.h"
#include "modules/video_render/main/interface/video_render_defines.h"
#include "system_wrappers/interface/critical_section_wrapper.h"
#include "system_wrappers/interface/thread_wrapper.h"
#include "system_wrappers/interface/trace.h"
#include "video_engine/main/interface/vie_codec.h"
#include "video_engine/main/interface/vie_errors.h"
#include "video_engine/main/interface/vie_image_process.h"
#include "video_engine/main/interface/vie_rtp_rtcp.h"
#include "video_engine/vie_defines.h"
#include "video_engine/vie_receiver.h"
#include "video_engine/vie_sender.h"
#include "video_engine/vie_sync_module.h"

namespace webrtc {

const int kMaxDecodeWaitTimeMs = 50;

ViEChannel::ViEChannel(WebRtc_Word32 channel_id,
                       WebRtc_Word32 engine_id,
                       WebRtc_UWord32 number_of_cores,
                       ProcessThread& module_process_thread)
    : ViEFrameProviderBase(channel_id, engine_id),
      channel_id_(channel_id),
      engine_id_(engine_id),
      number_of_cores_(number_of_cores),
      num_socket_threads_(kViESocketThreads),
      callbackCritsect_(*CriticalSectionWrapper::CreateCriticalSection()),
      rtp_rtcp_(*RtpRtcp::CreateRtpRtcp(ViEModuleId(engine_id, channel_id),
                                        false)),
#ifndef WEBRTC_EXTERNAL_TRANSPORT
      socket_transport_(*UdpTransport::Create(
          ViEModuleId(engine_id, channel_id), num_socket_threads_)),
#endif
  vcm_(*VideoCodingModule::Create(ViEModuleId(engine_id, channel_id))),
  vie_receiver_(*(new ViEReceiver(engine_id, channel_id, rtp_rtcp_, vcm_))),
  vie_sender_(*(new ViESender(engine_id, channel_id))),
  vie_sync_(*(new ViESyncModule(ViEId(engine_id, channel_id), vcm_,
                                rtp_rtcp_))),
  module_process_thread_(module_process_thread),
  codec_observer_(NULL),
  do_key_frame_callbackRequest_(false),
  rtp_observer_(NULL),
  rtcp_observer_(NULL),
  networkObserver_(NULL),
  rtp_packet_timeout_(false),
  using_packet_spread_(false),
  external_transport_(NULL),
  decoder_reset_(true),
  wait_for_key_frame_(false),
  decode_thread_(NULL),
  external_encryption_(NULL),
  effect_filter_(NULL),
  color_enhancement_(true),
  vcm_rttreported_(TickTime::Now()),
  file_recorder_(channel_id) {
  WEBRTC_TRACE(kTraceMemory, kTraceVideo, ViEId(engine_id, channel_id),
               "ViEChannel::ViEChannel(channel_id: %d, engine_id: %d)",
               channel_id, engine_id);
}

WebRtc_Word32 ViEChannel::Init() {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s: channel_id: %d, engine_id: %d)", __FUNCTION__, channel_id_,
               engine_id_);
  // RTP/RTCP initialization.
  if (rtp_rtcp_.InitSender() != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: RTP::InitSender failure", __FUNCTION__);
    return -1;
  }
  if (rtp_rtcp_.SetSendingMediaStatus(false) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: RTP::SetSendingMediaStatus failure", __FUNCTION__);
    return -1;
  }
  if (rtp_rtcp_.InitReceiver() != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: RTP::InitReceiver failure", __FUNCTION__);
    return -1;
  }
  if (rtp_rtcp_.RegisterIncomingDataCallback(
      static_cast<RtpData*>(&vie_receiver_)) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: RTP::RegisterIncomingDataCallback failure", __FUNCTION__);
    return -1;
  }
  if (rtp_rtcp_.RegisterSendTransport(
      static_cast<Transport*>(&vie_sender_)) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: RTP::RegisterSendTransport failure", __FUNCTION__);
    return -1;
  }
  if (module_process_thread_.RegisterModule(&rtp_rtcp_) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: RTP::RegisterModule failure", __FUNCTION__);
    return -1;
  }
  if (rtp_rtcp_.SetKeyFrameRequestMethod(kKeyFrameReqFirRtp) != 0) {
    WEBRTC_TRACE(kTraceWarning, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: RTP::SetKeyFrameRequestMethod failure", __FUNCTION__);
  }
  if (rtp_rtcp_.SetRTCPStatus(kRtcpCompound) != 0) {
    WEBRTC_TRACE(kTraceWarning, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: RTP::SetRTCPStatus failure", __FUNCTION__);
  }
  if (rtp_rtcp_.RegisterIncomingRTPCallback(this) != 0) {
    WEBRTC_TRACE(kTraceWarning, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: RTP::RegisterIncomingRTPCallback failure", __FUNCTION__);
    return -1;
  }
  if (rtp_rtcp_.RegisterIncomingRTCPCallback(this) != 0) {
    WEBRTC_TRACE(kTraceWarning, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: RTP::RegisterIncomingRTCPCallback failure", __FUNCTION__);
    return -1;
  }

  // VCM initialization
  if (vcm_.InitializeReceiver() != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(engine_id_, channel_id_),
                 "%s: VCM::InitializeReceiver failure", __FUNCTION__);
    return -1;
  }
  if (vcm_.RegisterReceiveCallback(this) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: VCM::RegisterReceiveCallback failure", __FUNCTION__);
    return -1;
  }
  if (vcm_.RegisterFrameTypeCallback(this) != 0) {
    WEBRTC_TRACE(kTraceWarning, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: VCM::RegisterFrameTypeCallback failure", __FUNCTION__);
  }
  if (vcm_.RegisterReceiveStatisticsCallback(this) != 0) {
    WEBRTC_TRACE(kTraceWarning, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: VCM::RegisterReceiveStatisticsCallback failure",
                 __FUNCTION__);
  }
  if (vcm_.SetRenderDelay(kViEDefaultRenderDelayMs) != 0) {
    WEBRTC_TRACE(kTraceWarning, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: VCM::SetRenderDelay failure", __FUNCTION__);
  }
  if (module_process_thread_.RegisterModule(&vcm_) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: VCM::RegisterModule(vcm) failure", __FUNCTION__);
    return -1;
  }
#ifdef VIDEOCODEC_VP8
  VideoCodec video_codec;
  if (vcm_.Codec(kVideoCodecVP8, &video_codec) == VCM_OK) {
    rtp_rtcp_.RegisterSendPayload(video_codec);
    rtp_rtcp_.RegisterReceivePayload(video_codec);
    vcm_.RegisterReceiveCodec(&video_codec, number_of_cores_);
    vcm_.RegisterSendCodec(&video_codec, number_of_cores_,
                           rtp_rtcp_.MaxDataPayloadLength());
  } else {
    assert(false);
  }
#endif

  return 0;
}

ViEChannel::~ViEChannel() {
  WEBRTC_TRACE(kTraceMemory, kTraceVideo, ViEId(engine_id_, channel_id_),
               "ViEChannel Destructor, channel_id: %d, engine_id: %d",
               channel_id_, engine_id_);

  // Make sure we don't get more callbacks from the RTP module.
  rtp_rtcp_.RegisterIncomingRTPCallback(NULL);
  rtp_rtcp_.RegisterSendTransport(NULL);
#ifndef WEBRTC_EXTERNAL_TRANSPORT
  socket_transport_.StopReceiving();
#endif
  module_process_thread_.DeRegisterModule(&rtp_rtcp_);
  module_process_thread_.DeRegisterModule(&vcm_);
  module_process_thread_.DeRegisterModule(&vie_sync_);
  while (simulcast_rtp_rtcp_.size() > 0) {
    std::list<RtpRtcp*>::iterator it = simulcast_rtp_rtcp_.begin();
    RtpRtcp* rtp_rtcp = *it;
    rtp_rtcp->RegisterIncomingRTCPCallback(NULL);
    rtp_rtcp->RegisterSendTransport(NULL);
    module_process_thread_.DeRegisterModule(rtp_rtcp);
    RtpRtcp::DestroyRtpRtcp(rtp_rtcp);
    simulcast_rtp_rtcp_.erase(it);
  }
  if (decode_thread_) {
    StopDecodeThread();
  }

  delete &vie_receiver_;
  delete &vie_sender_;
  delete &vie_sync_;

  delete &callbackCritsect_;

  // Release modules.
  RtpRtcp::DestroyRtpRtcp(&rtp_rtcp_);
#ifndef WEBRTC_EXTERNAL_TRANSPORT
  UdpTransport::Destroy(&socket_transport_);
#endif
  VideoCodingModule::Destroy(&vcm_);
}

WebRtc_Word32 ViEChannel::SetSendCodec(const VideoCodec& video_codec,
                                       bool new_stream) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s: codec_type: %d", __FUNCTION__, video_codec.codecType);

  if (video_codec.codecType == kVideoCodecRED ||
      video_codec.codecType == kVideoCodecULPFEC) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: codec_type: %d is not a valid send codec.", __FUNCTION__,
                 video_codec.codecType);
    return -1;
  }
  if (kMaxSimulcastStreams < video_codec.numberOfSimulcastStreams) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: Too many simulcast streams", __FUNCTION__);
    return -1;
  }
  // Update the RTP module with the settings.
  // Stop and Start the RTP module -> trigger new SSRC, if an SSRC hasn't been
  // set explicitly.
  bool restart_rtp = false;
  if (rtp_rtcp_.Sending() && new_stream) {
    restart_rtp = true;
    rtp_rtcp_.SetSendingStatus(false);
  }
  if (video_codec.numberOfSimulcastStreams > 0) {
    WebRtc_UWord32 start_bitrate = video_codec.startBitrate * 1000;
    WebRtc_UWord32 stream_bitrate =
        std::min(start_bitrate, video_codec.simulcastStream[0].maxBitrate);
    start_bitrate -= stream_bitrate;
    // Set correct bitrate to base layer.
    if (rtp_rtcp_.SetSendBitrate(
          stream_bitrate, video_codec.minBitrate,
          video_codec.simulcastStream[0].maxBitrate) != 0) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                   "%s: could not set send bitrates", __FUNCTION__);
      return -1;
    }
    // Create our simulcast RTP modules.
    for (int i = simulcast_rtp_rtcp_.size();
         i < video_codec.numberOfSimulcastStreams - 1;
         i++) {
      RtpRtcp* rtp_rtcp = RtpRtcp::CreateRtpRtcp(
          ViEModuleId(engine_id_, channel_id_), false);
      if (rtp_rtcp->RegisterDefaultModule(default_rtp_rtcp_)) {
        WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                     "%s: could not register default module", __FUNCTION__);
        return -1;
      }
      simulcast_rtp_rtcp_.push_back(rtp_rtcp);
    }
    // Remove last in list if we have too many.
    for (int j = simulcast_rtp_rtcp_.size();
         j > (video_codec.numberOfSimulcastStreams - 1);
         j--) {
      RtpRtcp* rtp_rtcp = simulcast_rtp_rtcp_.back();
      rtp_rtcp->RegisterIncomingRTCPCallback(NULL);
      rtp_rtcp->RegisterSendTransport(NULL);
      module_process_thread_.DeRegisterModule(rtp_rtcp);
      RtpRtcp::DestroyRtpRtcp(rtp_rtcp);
      simulcast_rtp_rtcp_.pop_back();
    }
    VideoCodec video_codec;
    if (vcm_.Codec(kVideoCodecVP8, &video_codec) != VCM_OK) {
      WEBRTC_TRACE(kTraceWarning, kTraceVideo, ViEId(engine_id_, channel_id_),
                   "%s: VCM: failure geting default VP8 pl_type", __FUNCTION__);
      return -1;
    }
    WebRtc_UWord8 idx = 0;
    // Configure all simulcast modules.
    for (std::list<RtpRtcp*>::iterator it = simulcast_rtp_rtcp_.begin();
         it != simulcast_rtp_rtcp_.end();
         it++) {
      idx++;
      RtpRtcp* rtp_rtcp = *it;
      if (rtp_rtcp->InitSender() != 0) {
        WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                     "%s: RTP::InitSender failure", __FUNCTION__);
        return -1;
      }
      if (rtp_rtcp->InitReceiver() != 0) {
        WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                     "%s: RTP::InitReceiver failure", __FUNCTION__);
        return -1;
      }
      if (rtp_rtcp->RegisterSendTransport(
          static_cast<Transport*>(&vie_sender_)) != 0) {
        WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                     "%s: RTP::RegisterSendTransport failure", __FUNCTION__);
        return -1;
      }
      if (module_process_thread_.RegisterModule(rtp_rtcp) != 0) {
        WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                     "%s: RTP::RegisterModule failure", __FUNCTION__);
        return -1;
      }
      if (rtp_rtcp->SetRTCPStatus(rtp_rtcp_.RTCP()) != 0) {
        WEBRTC_TRACE(kTraceWarning, kTraceVideo, ViEId(engine_id_, channel_id_),
                     "%s: RTP::SetRTCPStatus failure", __FUNCTION__);
      }
      rtp_rtcp->DeRegisterSendPayload(video_codec.plType);
      if (rtp_rtcp->RegisterSendPayload(video_codec) != 0) {
        WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                     "%s: could not register payload type", __FUNCTION__);
        return -1;
      }
      if (restart_rtp) {
        rtp_rtcp->SetSendingStatus(true);
      }
      // Configure all simulcast streams min and max bitrates
      const WebRtc_UWord32 stream_bitrate =
          std::min(start_bitrate, video_codec.simulcastStream[idx].maxBitrate);
      start_bitrate -= stream_bitrate;
      if (rtp_rtcp->SetSendBitrate(
            stream_bitrate, video_codec.minBitrate,
            video_codec.simulcastStream[idx].maxBitrate) != 0) {
        WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                     "%s: could not set send bitrates", __FUNCTION__);
        return -1;
      }
    }
    vie_receiver_.RegisterSimulcastRtpRtcpModules(simulcast_rtp_rtcp_);
  } else {
    if (!simulcast_rtp_rtcp_.empty()) {
      // Delete all simulcast rtp modules.
      while (!simulcast_rtp_rtcp_.empty()) {
        RtpRtcp* rtp_rtcp = simulcast_rtp_rtcp_.back();
        rtp_rtcp->RegisterIncomingRTCPCallback(NULL);
        rtp_rtcp->RegisterSendTransport(NULL);
        module_process_thread_.DeRegisterModule(rtp_rtcp);
        RtpRtcp::DestroyRtpRtcp(rtp_rtcp);
        simulcast_rtp_rtcp_.pop_back();
      }
    }
    // Clear any previous modules.
    vie_receiver_.RegisterSimulcastRtpRtcpModules(simulcast_rtp_rtcp_);

    if (rtp_rtcp_.SetSendBitrate(video_codec.startBitrate * 1000,
                                 video_codec.minBitrate,
                                 video_codec.maxBitrate) != 0) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                   "%s: could not set send bitrates", __FUNCTION__);
      return -1;
    }
  }
  // Enable this if H264 is available.
  // This sets the wanted packetization mode.
  // if (video_codec.plType == kVideoCodecH264) {
  //   if (video_codec.codecSpecific.H264.packetization ==  kH264SingleMode) {
  //     rtp_rtcp_.SetH264PacketizationMode(H264_SINGLE_NAL_MODE);
  //   } else {
  //     rtp_rtcp_.SetH264PacketizationMode(H264_NON_INTERLEAVED_MODE);
  //   }
  //   if (video_codec.codecSpecific.H264.configParametersSize > 0) {
  //     rtp_rtcp_.SetH264SendModeNALU_PPS_SPS(true);
  //   }
  // }

  // Don't log this error, no way to check in advance if this pl_type is
  // registered or not...
  rtp_rtcp_.DeRegisterSendPayload(video_codec.plType);
  if (rtp_rtcp_.RegisterSendPayload(video_codec) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: could not register payload type", __FUNCTION__);
    return -1;
  }
  if (restart_rtp) {
    rtp_rtcp_.SetSendingStatus(true);
  }
  return 0;
}

WebRtc_Word32 ViEChannel::SetReceiveCodec(const VideoCodec& video_codec) {
  // We will not receive simulcast streams, so no need to handle that use case.
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s", __FUNCTION__);

  WebRtc_Word8 old_pltype = -1;
  if (rtp_rtcp_.ReceivePayloadType(video_codec, &old_pltype) != -1) {
    rtp_rtcp_.DeRegisterReceivePayload(old_pltype);
  }

  if (rtp_rtcp_.RegisterReceivePayload(video_codec) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: Could not register receive payload type", __FUNCTION__);
    return -1;
  }

  if (video_codec.codecType != kVideoCodecRED &&
      video_codec.codecType != kVideoCodecULPFEC) {
    // Register codec type with VCM, but do not register RED or ULPFEC.
    if (vcm_.RegisterReceiveCodec(&video_codec, number_of_cores_,
                                  wait_for_key_frame_) != VCM_OK) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                   "%s: Could not register decoder", __FUNCTION__);
      return -1;
    }
  }
  return 0;
}

WebRtc_Word32 ViEChannel::GetReceiveCodec(VideoCodec& video_codec) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s", __FUNCTION__);

  if (vcm_.ReceiveCodec(&video_codec) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: Could not get receive codec", __FUNCTION__);
    return -1;
  }
  return 0;
}

WebRtc_Word32 ViEChannel::RegisterCodecObserver(ViEDecoderObserver* observer) {
  CriticalSectionScoped cs(callbackCritsect_);
  if (observer) {
    if (codec_observer_) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                   "%s: already added", __FUNCTION__);
      return -1;
    }
    WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: observer added", __FUNCTION__);
    codec_observer_ = observer;
  } else {
    if (!codec_observer_) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                   "%s: no observer added", __FUNCTION__);
      return -1;
    }
    WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: observer removed", __FUNCTION__);
    codec_observer_ = NULL;
  }
  return 0;
}

WebRtc_Word32 ViEChannel::RegisterExternalDecoder(const WebRtc_UWord8 pl_type,
                                                  VideoDecoder* decoder,
                                                  bool decoder_render,
                                                  WebRtc_Word32 render_delay) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s", __FUNCTION__);

  WebRtc_Word32 result = 0;
  result = vcm_.RegisterExternalDecoder(decoder, pl_type, decoder_render);
  if (decoder_render && result == 0) {
    // Let VCM know how long before the actual render time the decoder needs
    // to get a frame for decoding.
    result = vcm_.SetRenderDelay(render_delay);
  }
  return result;
}

WebRtc_Word32 ViEChannel::DeRegisterExternalDecoder(
    const WebRtc_UWord8 pl_type) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s pl_type", __FUNCTION__, pl_type);

  VideoCodec current_receive_codec;
  WebRtc_Word32 result = 0;
  result = vcm_.ReceiveCodec(&current_receive_codec);
  if (vcm_.RegisterExternalDecoder(NULL, pl_type, false) != VCM_OK) {
    return -1;
  }

  if (result == 0 && current_receive_codec.plType == pl_type) {
    result = vcm_.RegisterReceiveCodec(&current_receive_codec, number_of_cores_,
                                       wait_for_key_frame_);
  }
  return result;
}

WebRtc_Word32 ViEChannel::ReceiveCodecStatistics(
    WebRtc_UWord32& num_key_frames, WebRtc_UWord32& num_delta_frames) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s", __FUNCTION__);

  VCMFrameCount received_frames;
  if (vcm_.ReceivedFrameCount(received_frames) != VCM_OK) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: Could not get received frame information", __FUNCTION__);
    return -1;
  }
  num_key_frames = received_frames.numKeyFrames;
  num_delta_frames = received_frames.numDeltaFrames;
  return 0;
}

WebRtc_UWord32 ViEChannel::DiscardedPackets() const {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);
  return vcm_.DiscardedPackets();
}

WebRtc_Word32 ViEChannel::WaitForKeyFrame(bool wait) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s(wait: %d)", __FUNCTION__, wait);
  wait_for_key_frame_ = wait;
  return 0;
}

WebRtc_Word32 ViEChannel::SetSignalPacketLossStatus(bool enable,
                                                    bool only_key_frames) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s(enable: %d)", __FUNCTION__, enable);
  if (enable) {
    if (only_key_frames) {
      vcm_.SetVideoProtection(kProtectionKeyOnLoss, false);
      if (vcm_.SetVideoProtection(kProtectionKeyOnKeyLoss, true) != VCM_OK) {
        WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                     "%s failed %d", __FUNCTION__, enable);
        return -1;
      }
    } else {
      vcm_.SetVideoProtection(kProtectionKeyOnKeyLoss, false);
      if (vcm_.SetVideoProtection(kProtectionKeyOnLoss, true) != VCM_OK) {
        WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                     "%s failed %d", __FUNCTION__, enable);
        return -1;
      }
    }
  } else {
    vcm_.SetVideoProtection(kProtectionKeyOnLoss, false);
    vcm_.SetVideoProtection(kProtectionKeyOnKeyLoss, false);
  }
  return 0;
}

WebRtc_Word32 ViEChannel::SetRTCPMode(const RTCPMethod rtcp_mode) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s: %d", __FUNCTION__, rtcp_mode);

  for (std::list<RtpRtcp*>::iterator it = simulcast_rtp_rtcp_.begin();
       it != simulcast_rtp_rtcp_.end();
       it++) {
    RtpRtcp* rtp_rtcp = *it;
    rtp_rtcp->SetRTCPStatus(rtcp_mode);
  }
  return rtp_rtcp_.SetRTCPStatus(rtcp_mode);
}

WebRtc_Word32 ViEChannel::GetRTCPMode(RTCPMethod& rtcp_mode) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s", __FUNCTION__);
  rtcp_mode = rtp_rtcp_.RTCP();
  return 0;
}

WebRtc_Word32 ViEChannel::SetNACKStatus(const bool enable) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s(enable: %d)", __FUNCTION__, enable);

  // Update the decoding VCM.
  if (vcm_.SetVideoProtection(kProtectionNack, enable) != VCM_OK) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: Could not set VCM NACK protection: %d", __FUNCTION__,
                 enable);
    return -1;
  }
  if (enable) {
    // Disable possible FEC.
    SetFECStatus(false, 0, 0);
  }
  // Update the decoding VCM.
  if (vcm_.SetVideoProtection(kProtectionNack, enable) != VCM_OK) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: Could not set VCM NACK protection: %d", __FUNCTION__,
                 enable);
    return -1;
  }
  return ProcessNACKRequest(enable);
}

WebRtc_Word32 ViEChannel::ProcessNACKRequest(const bool enable) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s(enable: %d)", __FUNCTION__, enable);

  if (enable) {
    // Turn on NACK.
    NACKMethod nackMethod = kNackRtcp;
    if (rtp_rtcp_.RTCP() == kRtcpOff) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                   "%s: Could not enable NACK, RTPC not on ", __FUNCTION__);
      return -1;
    }
    if (rtp_rtcp_.SetNACKStatus(nackMethod) != 0) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                   "%s: Could not set NACK method %d", __FUNCTION__,
                   nackMethod);
      return -1;
    }
    WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: Using NACK method %d", __FUNCTION__, nackMethod);
    rtp_rtcp_.SetStorePacketsStatus(true, kNackHistorySize);

    vcm_.RegisterPacketRequestCallback(this);

    for (std::list<RtpRtcp*>::iterator it = simulcast_rtp_rtcp_.begin();
         it != simulcast_rtp_rtcp_.end();
         it++) {
      RtpRtcp* rtp_rtcp = *it;
      rtp_rtcp->SetStorePacketsStatus(true, kNackHistorySize);
    }
  } else {
    for (std::list<RtpRtcp*>::iterator it = simulcast_rtp_rtcp_.begin();
         it != simulcast_rtp_rtcp_.end();
         it++) {
      RtpRtcp* rtp_rtcp = *it;
      rtp_rtcp->SetStorePacketsStatus(false);
    }
    rtp_rtcp_.SetStorePacketsStatus(false);
    vcm_.RegisterPacketRequestCallback(NULL);
    if (rtp_rtcp_.SetNACKStatus(kNackOff) != 0) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                   "%s: Could not turn off NACK", __FUNCTION__);
      return -1;
    }
  }
  return 0;
}

WebRtc_Word32 ViEChannel::SetFECStatus(const bool enable,
                                       const unsigned char payload_typeRED,
                                       const unsigned char payload_typeFEC) {
  // Disable possible NACK.
  if (enable) {
    SetNACKStatus(false);
  }

  return ProcessFECRequest(enable, payload_typeRED, payload_typeFEC);
}

WebRtc_Word32 ViEChannel::ProcessFECRequest(
    const bool enable,
    const unsigned char payload_typeRED,
    const unsigned char payload_typeFEC) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s(enable: %d, payload_typeRED: %u, payload_typeFEC: %u)",
               __FUNCTION__, enable, payload_typeRED, payload_typeFEC);

  if (rtp_rtcp_.SetGenericFECStatus(enable, payload_typeRED,
                                    payload_typeFEC) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: Could not change FEC status to %d", __FUNCTION__,
                 enable);
    return -1;
  }
  for (std::list<RtpRtcp*>::iterator it = simulcast_rtp_rtcp_.begin();
       it != simulcast_rtp_rtcp_.end();
       it++) {
    RtpRtcp* rtp_rtcp = *it;
    rtp_rtcp->SetGenericFECStatus(enable, payload_typeRED, payload_typeFEC);
  }
  return 0;
}

WebRtc_Word32 ViEChannel::SetHybridNACKFECStatus(
    const bool enable,
    const unsigned char payload_typeRED,
    const unsigned char payload_typeFEC) {
  // Update the decoding VCM with hybrid mode.
  if (vcm_.SetVideoProtection(kProtectionNackFEC, enable) != VCM_OK) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: Could not set VCM NACK protection: %d", __FUNCTION__,
                 enable);
    return -1;
  }

  WebRtc_Word32 ret_val = 0;
  ret_val = ProcessNACKRequest(enable);
  if (ret_val < 0) {
    return ret_val;
  }
  return ProcessFECRequest(enable, payload_typeRED, payload_typeFEC);
}

WebRtc_Word32 ViEChannel::SetKeyFrameRequestMethod(
    const KeyFrameRequestMethod method) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s: %d", __FUNCTION__, method);
  return rtp_rtcp_.SetKeyFrameRequestMethod(method);
}

WebRtc_Word32 ViEChannel::EnableTMMBR(const bool enable) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s: %d", __FUNCTION__, enable);
  return rtp_rtcp_.SetTMMBRStatus(enable);
}

WebRtc_Word32 ViEChannel::EnableKeyFrameRequestCallback(const bool enable) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s: %d", __FUNCTION__, enable);

  CriticalSectionScoped cs(callbackCritsect_);
  if (enable && !codec_observer_) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: No ViECodecObserver set", __FUNCTION__, enable);
    return -1;
  }
  do_key_frame_callbackRequest_ = enable;
  return 0;
}

WebRtc_Word32 ViEChannel::SetSSRC(const WebRtc_UWord32 SSRC,
                                  const StreamType /*usage*/,
                                  const unsigned char simulcast_idx) {
  // TODO(pwestin) add support for stream_type when we add RTX.
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s(SSRC: %u, idx:%u)", __FUNCTION__, SSRC, simulcast_idx);

  if (simulcast_idx == 0) {
    return rtp_rtcp_.SetSSRC(SSRC);
  }
  std::list<RtpRtcp*>::const_iterator it = simulcast_rtp_rtcp_.begin();
  for (int i = 1; i < simulcast_idx; i++) {
    it++;
    if (it == simulcast_rtp_rtcp_.end()) {
      return -1;
    }
  }
  RtpRtcp* rtp_rtcp = *it;
  return rtp_rtcp->SetSSRC(SSRC);
}

WebRtc_Word32 ViEChannel::GetLocalSSRC(WebRtc_UWord32& SSRC) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s", __FUNCTION__);
  SSRC = rtp_rtcp_.SSRC();
  return 0;
}

WebRtc_Word32 ViEChannel::GetRemoteSSRC(WebRtc_UWord32& SSRC) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);

  SSRC = rtp_rtcp_.RemoteSSRC();
  return 0;
}

WebRtc_Word32 ViEChannel::GetRemoteCSRC(unsigned int CSRCs[kRtpCsrcSize]) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);

  WebRtc_UWord32 arrayCSRC[kRtpCsrcSize];
  memset(arrayCSRC, 0, sizeof(arrayCSRC));

  WebRtc_Word32 num_csrcs = rtp_rtcp_.RemoteCSRCs(arrayCSRC);
  if (num_csrcs > 0) {
    memcpy(CSRCs, arrayCSRC, num_csrcs * sizeof(WebRtc_UWord32));
    for (int idx = 0; idx < num_csrcs; idx++) {
      WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
                   "\tCSRC[%d] = %lu", idx, CSRCs[idx]);
    }
  } else {
    WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: CSRC list is empty", __FUNCTION__);
  }
  return 0;
}

WebRtc_Word32 ViEChannel::SetStartSequenceNumber(
    WebRtc_UWord16 sequence_number) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);

  if (rtp_rtcp_.Sending()) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: already sending", __FUNCTION__);
    return -1;
  }
  return rtp_rtcp_.SetSequenceNumber(sequence_number);
}

WebRtc_Word32 ViEChannel::SetRTCPCName(const WebRtc_Word8 rtcp_cname[]) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s", __FUNCTION__);
  if (rtp_rtcp_.Sending()) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: already sending", __FUNCTION__);
    return -1;
  }
  return rtp_rtcp_.SetCNAME(rtcp_cname);
}

WebRtc_Word32 ViEChannel::GetRTCPCName(WebRtc_Word8 rtcp_cname[]) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s", __FUNCTION__);
  return rtp_rtcp_.CNAME(rtcp_cname);
}

WebRtc_Word32 ViEChannel::GetRemoteRTCPCName(WebRtc_Word8 rtcp_cname[]) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);

  WebRtc_UWord32 remoteSSRC = rtp_rtcp_.RemoteSSRC();
  return rtp_rtcp_.RemoteCNAME(remoteSSRC, rtcp_cname);
}

WebRtc_Word32 ViEChannel::RegisterRtpObserver(ViERTPObserver* observer) {
  CriticalSectionScoped cs(callbackCritsect_);
  if (observer) {
    if (rtp_observer_) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                   "%s: observer alread added", __FUNCTION__);
      return -1;
    }
    WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: observer added", __FUNCTION__);
    rtp_observer_ = observer;
  } else {
    if (!rtp_observer_) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                   "%s: no observer added", __FUNCTION__);
      return -1;
    }
    WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: observer removed", __FUNCTION__);
    rtp_observer_ = NULL;
  }
  return 0;
}

WebRtc_Word32 ViEChannel::RegisterRtcpObserver(ViERTCPObserver* observer) {
  CriticalSectionScoped cs(callbackCritsect_);
  if (observer) {
    if (rtcp_observer_) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                   "%s: observer alread added", __FUNCTION__);
      return -1;
    }
    WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: observer added", __FUNCTION__);
    rtcp_observer_ = observer;
  } else {
    if (!rtcp_observer_) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                   "%s: no observer added", __FUNCTION__);
      return -1;
    }
    WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: observer removed", __FUNCTION__);
    rtcp_observer_ = NULL;
  }
  return 0;
}

WebRtc_Word32 ViEChannel::SendApplicationDefinedRTCPPacket(
    const WebRtc_UWord8 sub_type,
    WebRtc_UWord32 name,
    const WebRtc_UWord8* data,
    WebRtc_UWord16 data_length_in_bytes) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);
  if (!rtp_rtcp_.Sending()) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: not sending", __FUNCTION__);
    return -1;
  }
  if (!data) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: no input argument", __FUNCTION__);
    return -1;
  }
  if (data_length_in_bytes % 4 != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: input length error", __FUNCTION__);
    return -1;
  }
  RTCPMethod rtcp_method = rtp_rtcp_.RTCP();
  if (rtcp_method == kRtcpOff) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: RTCP not enabled", __FUNCTION__);
    return -1;
  }
  // Create and send packet.
  if (rtp_rtcp_.SetRTCPApplicationSpecificData(sub_type, name, data,
                                               data_length_in_bytes) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: Could not send RTCP application data", __FUNCTION__);
    return -1;
  }
  return 0;
}

WebRtc_Word32 ViEChannel::GetSendRtcpStatistics(WebRtc_UWord16& fraction_lost,
                                                WebRtc_UWord32& cumulative_lost,
                                                WebRtc_UWord32& extended_max,
                                                WebRtc_UWord32& jitter_samples,
                                                WebRtc_Word32& rtt_ms) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);

  // TODO(pwestin) how do we do this for simulcast ? average for all
  // except cumulative_lost that is the sum ?
  // for (std::list<RtpRtcp*>::const_iterator it = simulcast_rtp_rtcp_.begin();
  //      it != simulcast_rtp_rtcp_.end();
  //      it++) {
  //   RtpRtcp* rtp_rtcp = *it;
  // }
  WebRtc_UWord32 remoteSSRC = rtp_rtcp_.RemoteSSRC();
  RTCPReportBlock remote_stat;
  if (rtp_rtcp_.RemoteRTCPStat(remoteSSRC, &remote_stat) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: Could not get remote stats", __FUNCTION__);
    return -1;
  }
  fraction_lost = remote_stat.fractionLost;
  cumulative_lost = remote_stat.cumulativeLost;
  extended_max = remote_stat.extendedHighSeqNum;
  jitter_samples = remote_stat.jitter;

  WebRtc_UWord16 dummy;
  WebRtc_UWord16 rtt = 0;
  if (rtp_rtcp_.RTT(remoteSSRC, &rtt, &dummy, &dummy, &dummy) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: Could not get RTT", __FUNCTION__);
    return -1;
  }
  rtt_ms = rtt;
  return 0;
}

WebRtc_Word32 ViEChannel::GetReceivedRtcpStatistics(
    WebRtc_UWord16& fraction_lost,
    WebRtc_UWord32& cumulative_lost,
    WebRtc_UWord32& extended_max,
    WebRtc_UWord32& jitter_samples,
    WebRtc_Word32& rtt_ms) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s", __FUNCTION__);

  WebRtc_UWord8 frac_lost = 0;
  if (rtp_rtcp_.StatisticsRTP(&frac_lost, &cumulative_lost, &extended_max,
                              &jitter_samples) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: Could not get received RTP statistics", __FUNCTION__);
    return -1;
  }
  fraction_lost = frac_lost;

  WebRtc_UWord32 remoteSSRC = rtp_rtcp_.RemoteSSRC();
  WebRtc_UWord16 dummy = 0;
  WebRtc_UWord16 rtt = 0;
  if (rtp_rtcp_.RTT(remoteSSRC, &rtt, &dummy, &dummy, &dummy) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: Could not get RTT", __FUNCTION__);
    return -1;
  }
  rtt_ms = rtt;
  return 0;
}

WebRtc_Word32 ViEChannel::GetRtpStatistics(
    WebRtc_UWord32& bytes_sent,
    WebRtc_UWord32& packets_sent,
    WebRtc_UWord32& bytes_received,
    WebRtc_UWord32& packets_received) const {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);

  if (rtp_rtcp_.DataCountersRTP(&bytes_sent,
                                &packets_sent,
                                &bytes_received,
                                &packets_received) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: Could not get counters", __FUNCTION__);
    return -1;
  }
  for (std::list<RtpRtcp*>::const_iterator it = simulcast_rtp_rtcp_.begin();
       it != simulcast_rtp_rtcp_.end();
       it++) {
    WebRtc_UWord32 bytes_sent_temp = 0;
    WebRtc_UWord32 packets_sent_temp = 0;
    RtpRtcp* rtp_rtcp = *it;
    rtp_rtcp->DataCountersRTP(&bytes_sent_temp, &packets_sent_temp, NULL, NULL);
    bytes_sent += bytes_sent_temp;
    packets_sent += packets_sent_temp;
  }
  return 0;
}

void ViEChannel::GetBandwidthUsage(WebRtc_UWord32& total_bitrate_sent,
                                   WebRtc_UWord32& video_bitrate_sent,
                                   WebRtc_UWord32& fec_bitrate_sent,
                                   WebRtc_UWord32& nackBitrateSent) const {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);

  rtp_rtcp_.BitrateSent(&total_bitrate_sent,
                        &video_bitrate_sent,
                        &fec_bitrate_sent,
                        &nackBitrateSent);
  for (std::list<RtpRtcp*>::const_iterator it = simulcast_rtp_rtcp_.begin();
       it != simulcast_rtp_rtcp_.end(); it++) {
    WebRtc_UWord32 stream_rate = 0;
    WebRtc_UWord32 video_rate = 0;
    WebRtc_UWord32 fec_rate = 0;
    WebRtc_UWord32 nackRate = 0;
    RtpRtcp* rtp_rtcp = *it;
    rtp_rtcp->BitrateSent(&stream_rate, &video_rate, &fec_rate, &nackRate);
    total_bitrate_sent += stream_rate;
    fec_bitrate_sent += fec_rate;
    nackBitrateSent += nackRate;
  }
}

WebRtc_Word32 ViEChannel::SetKeepAliveStatus(
    const bool enable,
    const WebRtc_Word8 unknown_payload_type,
    const WebRtc_UWord16 delta_transmit_timeMS) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s", __FUNCTION__);

  if (enable && rtp_rtcp_.RTPKeepalive()) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: RTP keepalive already enabled", __FUNCTION__);
    return -1;
  } else if (!enable && !rtp_rtcp_.RTPKeepalive()) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: RTP keepalive already disabled", __FUNCTION__);
    return -1;
  }

  if (rtp_rtcp_.SetRTPKeepaliveStatus(enable, unknown_payload_type,
                                      delta_transmit_timeMS) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: Could not set RTP keepalive status %d", __FUNCTION__,
                 enable);
    if (enable == false && !rtp_rtcp_.DefaultModuleRegistered()) {
      // Not sending media and we try to disable keep alive
      rtp_rtcp_.ResetSendDataCountersRTP();
      rtp_rtcp_.SetSendingStatus(false);
    }
    return -1;
  }

  if (enable && !rtp_rtcp_.Sending()) {
    // Enable sending to start sending Sender reports instead of receive
    // reports.
    if (rtp_rtcp_.SetSendingStatus(true) != 0) {
      rtp_rtcp_.SetRTPKeepaliveStatus(false, 0, 0);
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                   "%s: Could not start sending", __FUNCTION__);
      return -1;
    }
  } else if (!enable && !rtp_rtcp_.SendingMedia()) {
    // Not sending media and we're disabling keep alive.
    rtp_rtcp_.ResetSendDataCountersRTP();
    if (rtp_rtcp_.SetSendingStatus(false) != 0) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                   "%s: Could not stop sending", __FUNCTION__);
      return -1;
    }
  }
  return 0;
}

WebRtc_Word32 ViEChannel::GetKeepAliveStatus(
    bool& enabled,
    WebRtc_Word8& unknown_payload_type,
    WebRtc_UWord16& delta_transmit_time_ms) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);
  if (rtp_rtcp_.RTPKeepaliveStatus(&enabled, &unknown_payload_type,
                                   &delta_transmit_time_ms) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: Could not get RTP keepalive status", __FUNCTION__);
    return -1;
  }
  WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s: enabled = %d, unknown_payload_type = %d, "
               "delta_transmit_time_ms = %ul",
               __FUNCTION__, enabled, (WebRtc_Word32) unknown_payload_type,
    delta_transmit_time_ms);

  return 0;
}

WebRtc_Word32 ViEChannel::StartRTPDump(const char file_nameUTF8[1024],
                                       RTPDirections direction) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);

  if (direction != kRtpIncoming && direction != kRtpOutgoing) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: invalid input", __FUNCTION__);
    return -1;
  }

  if (direction == kRtpIncoming) {
    return vie_receiver_.StartRTPDump(file_nameUTF8);
  } else {
    return vie_sender_.StartRTPDump(file_nameUTF8);
  }
}

WebRtc_Word32 ViEChannel::StopRTPDump(RTPDirections direction) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s", __FUNCTION__);

  if (direction != kRtpIncoming && direction != kRtpOutgoing) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: invalid input", __FUNCTION__);
    return -1;
  }

  if (direction == kRtpIncoming) {
    return vie_receiver_.StopRTPDump();
  } else {
    return vie_sender_.StopRTPDump();
  }
}

WebRtc_Word32 ViEChannel::SetLocalReceiver(const WebRtc_UWord16 rtp_port,
                                           const WebRtc_UWord16 rtcp_port,
                                           const WebRtc_Word8* ip_address) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);

  callbackCritsect_.Enter();
  if (external_transport_) {
    callbackCritsect_.Leave();
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: external transport registered", __FUNCTION__);
    return -1;
  }
  callbackCritsect_.Leave();

#ifndef WEBRTC_EXTERNAL_TRANSPORT
  if (socket_transport_.Receiving()) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: already receiving", __FUNCTION__);
    return -1;
  }

  const WebRtc_Word8* multicast_ip_address = NULL;
  if (socket_transport_.InitializeReceiveSockets(&vie_receiver_, rtp_port,
                                                 ip_address,
                                                 multicast_ip_address,
                                                 rtcp_port) != 0) {
    WebRtc_Word32 socket_error = socket_transport_.LastError();
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: could not initialize receive sockets. Socket error: %d",
                 __FUNCTION__, socket_error);
    return -1;
  }
  return 0;
#else
  WEBRTC_TRACE(kTraceStateInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s: not available for external transport", __FUNCTION__);
  return -1;
#endif
}

WebRtc_Word32 ViEChannel::GetLocalReceiver(WebRtc_UWord16& rtp_port,
                                           WebRtc_UWord16& rtcp_port,
                                           WebRtc_Word8* ip_address) const {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);

  callbackCritsect_.Enter();
  if (external_transport_) {
    callbackCritsect_.Leave();
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: external transport registered", __FUNCTION__);
    return -1;
  }
  callbackCritsect_.Leave();

#ifndef WEBRTC_EXTERNAL_TRANSPORT
  if (socket_transport_.ReceiveSocketsInitialized() == false) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: receive sockets not initialized", __FUNCTION__);
    return -1;
  }

  WebRtc_Word8 multicast_ip_address[UdpTransport::kIpAddressVersion6Length];
  if (socket_transport_.ReceiveSocketInformation(ip_address, rtp_port,
                                                 rtcp_port,
                                                 multicast_ip_address) != 0) {
    WebRtc_Word32 socket_error = socket_transport_.LastError();
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
      "%s: could not get receive socket information. Socket error: %d",
      __FUNCTION__, socket_error);
    return -1;
  }
  return 0;
#else
  WEBRTC_TRACE(kTraceStateInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s: not available for external transport", __FUNCTION__);
  return -1;
#endif
}

WebRtc_Word32 ViEChannel::SetSendDestination(
    const WebRtc_Word8* ip_address,
    const WebRtc_UWord16 rtp_port,
    const WebRtc_UWord16 rtcp_port,
    const WebRtc_UWord16 source_rtp_port,
    const WebRtc_UWord16 source_rtcp_port) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);

  callbackCritsect_.Enter();
  if (external_transport_) {
    callbackCritsect_.Leave();
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: external transport registered", __FUNCTION__);
    return -1;
  }
  callbackCritsect_.Leave();

#ifndef WEBRTC_EXTERNAL_TRANSPORT
  const bool is_ipv6 = socket_transport_.IpV6Enabled();
  if (UdpTransport::IsIpAddressValid(ip_address, is_ipv6) == false) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: Not a valid IP address: %s", __FUNCTION__, ip_address);
    return -1;
  }
  if (socket_transport_.InitializeSendSockets(ip_address, rtp_port,
                                              rtcp_port)!= 0) {
    WebRtc_Word32 socket_error = socket_transport_.LastError();
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: could not initialize send socket. Socket error: %d",
                 __FUNCTION__, socket_error);
    return -1;
  }

  if (source_rtp_port != 0) {
    WebRtc_UWord16 receive_rtp_port = 0;
    WebRtc_UWord16 receive_rtcp_port = 0;
    if (socket_transport_.ReceiveSocketInformation(NULL, receive_rtp_port,
                                                   receive_rtcp_port,
                                                   NULL) != 0) {
      WebRtc_Word32 socket_error = socket_transport_.LastError();
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
        "%s: could not get receive port information. Socket error: %d",
        __FUNCTION__, socket_error);
      return -1;
    }
    // Initialize an extra socket only if send port differs from receive
    // port.
    if (source_rtp_port != receive_rtp_port) {
      if (socket_transport_.InitializeSourcePorts(source_rtp_port,
                                                  source_rtcp_port) != 0) {
        WebRtc_Word32 socket_error = socket_transport_.LastError();
        WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                     "%s: could not set source ports. Socket error: %d",
                     __FUNCTION__, socket_error);
        return -1;
      }
    }
  }
  vie_sender_.RegisterSendTransport(&socket_transport_);

  // Workaround to avoid SSRC colision detection in loppback tests.
  if (!is_ipv6) {
    WebRtc_UWord32 local_host_address = 0;
    const WebRtc_UWord32 current_ip_address =
        UdpTransport::InetAddrIPV4(ip_address);

    if ((UdpTransport::LocalHostAddress(local_host_address) == 0 &&
        local_host_address == current_ip_address) ||
        strncmp("127.0.0.1", ip_address, 9) == 0) {
      rtp_rtcp_.SetSSRC(0xFFFFFFFF);
      WEBRTC_TRACE(kTraceStateInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
                   "Running in loopback. Forcing fixed SSRC");
    }
  } else {
    WebRtc_UWord8 local_host_address[16];
    WebRtc_UWord8 current_ip_address[16];

    WebRtc_Word32 conv_result =
      UdpTransport::LocalHostAddressIPV6(local_host_address);
    conv_result += socket_transport_.InetPresentationToNumeric(
        23, ip_address, current_ip_address);
    if (conv_result == 0) {
      bool local_host = true;
      for (WebRtc_Word32 i = 0; i < 16; i++) {
        if (local_host_address[i] != current_ip_address[i]) {
          local_host = false;
          break;
        }
      }
      if (!local_host) {
        local_host = true;
        for (WebRtc_Word32 i = 0; i < 15; i++) {
          if (current_ip_address[i] != 0) {
            local_host = false;
            break;
          }
        }
        if (local_host == true && current_ip_address[15] != 1) {
          local_host = false;
        }
      }
      if (local_host) {
        rtp_rtcp_.SetSSRC(0xFFFFFFFF);
        WEBRTC_TRACE(kTraceStateInfo, kTraceVideo,
                     ViEId(engine_id_, channel_id_),
                     "Running in loopback. Forcing fixed SSRC");
      }
    }
  }
  return 0;
#else
  WEBRTC_TRACE(kTraceStateInfo, kTraceVideo,
               ViEId(engine_id_, channel_id_),
               "%s: not available for external transport", __FUNCTION__);
  return -1;
#endif
}

WebRtc_Word32 ViEChannel::GetSendDestination(
    WebRtc_Word8* ip_address,
    WebRtc_UWord16& rtp_port,
    WebRtc_UWord16& rtcp_port,
    WebRtc_UWord16& source_rtp_port,
    WebRtc_UWord16& source_rtcp_port) const {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);

  callbackCritsect_.Enter();
  if (external_transport_) {
    callbackCritsect_.Leave();
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: external transport registered", __FUNCTION__);
    return -1;
  }
  callbackCritsect_.Leave();

#ifndef WEBRTC_EXTERNAL_TRANSPORT
  if (socket_transport_.SendSocketsInitialized() == false) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: send sockets not initialized", __FUNCTION__);
    return -1;
  }
  if (socket_transport_.SendSocketInformation(ip_address, rtp_port, rtcp_port)
      != 0) {
    WebRtc_Word32 socket_error = socket_transport_.LastError();
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
      "%s: could not get send socket information. Socket error: %d",
      __FUNCTION__, socket_error);
    return -1;
  }
  source_rtp_port = 0;
  source_rtcp_port = 0;
  if (socket_transport_.SourcePortsInitialized()) {
    socket_transport_.SourcePorts(source_rtp_port, source_rtcp_port);
  }
  return 0;
#else
  WEBRTC_TRACE(kTraceStateInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
      "%s: not available for external transport", __FUNCTION__);
  return -1;
#endif
}

WebRtc_Word32 ViEChannel::StartSend() {
  CriticalSectionScoped cs(callbackCritsect_);
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s", __FUNCTION__);

#ifndef WEBRTC_EXTERNAL_TRANSPORT
  if (!external_transport_) {
    if (socket_transport_.SendSocketsInitialized() == false) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                   "%s: send sockets not initialized", __FUNCTION__);
      return -1;
    }
  }
#endif
  rtp_rtcp_.SetSendingMediaStatus(true);

  if (rtp_rtcp_.Sending() && !rtp_rtcp_.RTPKeepalive()) {
    if (rtp_rtcp_.RTPKeepalive()) {
      // Sending Keep alive, don't trigger an error.
      return 0;
    }
    // Already sending.
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: Already sending", __FUNCTION__);
    return kViEBaseAlreadySending;
  }
  if (rtp_rtcp_.SetSendingStatus(true) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: Could not start sending RTP", __FUNCTION__);
    return -1;
  }
  for (std::list<RtpRtcp*>::const_iterator it = simulcast_rtp_rtcp_.begin();
       it != simulcast_rtp_rtcp_.end();
       it++) {
    RtpRtcp* rtp_rtcp = *it;
    rtp_rtcp->SetSendingMediaStatus(true);
    rtp_rtcp->SetSendingStatus(true);
  }
  return 0;
}

WebRtc_Word32 ViEChannel::StopSend() {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);

  rtp_rtcp_.SetSendingMediaStatus(false);
  for (std::list<RtpRtcp*>::iterator it = simulcast_rtp_rtcp_.begin();
       it != simulcast_rtp_rtcp_.end();
       it++) {
    RtpRtcp* rtp_rtcp = *it;
    rtp_rtcp->SetSendingMediaStatus(false);
  }
  if (rtp_rtcp_.RTPKeepalive()) {
    // Don't turn off sending since we'll send keep alive packets.
    return 0;
  }
  if (!rtp_rtcp_.Sending()) {
    WEBRTC_TRACE(kTraceWarning, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: Not sending", __FUNCTION__);
    return kViEBaseNotSending;
  }

  // Reset.
  rtp_rtcp_.ResetSendDataCountersRTP();
  if (rtp_rtcp_.SetSendingStatus(false) != 0) {
    WEBRTC_TRACE(kTraceWarning, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: could not stop RTP sending", __FUNCTION__);
    return -1;
  }
  for (std::list<RtpRtcp*>::iterator it = simulcast_rtp_rtcp_.begin();
       it != simulcast_rtp_rtcp_.end();
       it++) {
    RtpRtcp* rtp_rtcp = *it;
    rtp_rtcp->ResetSendDataCountersRTP();
    rtp_rtcp->SetSendingStatus(false);
  }
  return 0;
}

bool ViEChannel::Sending() {
  return rtp_rtcp_.Sending();
}

WebRtc_Word32 ViEChannel::StartReceive() {
  CriticalSectionScoped cs(callbackCritsect_);
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);

#ifndef WEBRTC_EXTERNAL_TRANSPORT
  if (!external_transport_) {
    if (socket_transport_.Receiving()) {
      // Warning, don't return error.
      WEBRTC_TRACE(kTraceWarning, kTraceVideo, ViEId(engine_id_, channel_id_),
                   "%s: already receiving", __FUNCTION__);
      return 0;
    }
    if (socket_transport_.ReceiveSocketsInitialized() == false) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                   "%s: receive sockets not initialized", __FUNCTION__);
      return -1;
    }
    if (socket_transport_.StartReceiving(kViENumReceiveSocketBuffers) != 0) {
      WebRtc_Word32 socket_error = socket_transport_.LastError();
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
        "%s: could not get receive socket information. Socket error:%d",
        __FUNCTION__, socket_error);
      return -1;
    }
  }
#endif
  if (StartDecodeThread() != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: could not start decoder thread", __FUNCTION__);

#ifndef WEBRTC_EXTERNAL_TRANSPORT
    socket_transport_.StopReceiving();
#endif
    vie_receiver_.StopReceive();
    return -1;
  }
  vie_receiver_.StartReceive();

  return 0;
}

WebRtc_Word32 ViEChannel::StopReceive() {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);

  vie_receiver_.StopReceive();
  StopDecodeThread();
  vcm_.ResetDecoder();
  {
    CriticalSectionScoped cs(callbackCritsect_);
    if (external_transport_) {
      return 0;
    }
  }

#ifndef WEBRTC_EXTERNAL_TRANSPORT
  if (socket_transport_.Receiving() == false) {
    // Warning, don't return error
    WEBRTC_TRACE(kTraceWarning, kTraceVideo,
                 ViEId(engine_id_, channel_id_), "%s: not receiving",
                 __FUNCTION__);
    return 0;
  }
  if (socket_transport_.StopReceiving() != 0) {
    WebRtc_Word32 socket_error = socket_transport_.LastError();
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: Socket error: %d", __FUNCTION__, socket_error);
    return -1;
  }
#endif

  return 0;
}

bool ViEChannel::Receiving() {
#ifndef WEBRTC_EXTERNAL_TRANSPORT
  return socket_transport_.Receiving();
#else
  return false;
#endif
}

WebRtc_Word32 ViEChannel::GetSourceInfo(WebRtc_UWord16& rtp_port,
                                        WebRtc_UWord16& rtcp_port,
                                        WebRtc_Word8* ip_address,
                                        WebRtc_UWord32 ip_address_length) {
  {
    CriticalSectionScoped cs(callbackCritsect_);
    WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
                 __FUNCTION__);
    if (external_transport_) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                   "%s: external transport registered", __FUNCTION__);
      return -1;
    }
  }
#ifndef WEBRTC_EXTERNAL_TRANSPORT
  if (socket_transport_.IpV6Enabled() &&
      ip_address_length < UdpTransport::kIpAddressVersion6Length) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: IP address length is too small for IPv6", __FUNCTION__);
    return -1;
  } else if (ip_address_length < UdpTransport::kIpAddressVersion4Length) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: IP address length is too small for IPv4", __FUNCTION__);
    return -1;
  }

  if (socket_transport_.RemoteSocketInformation(ip_address, rtp_port, rtcp_port)
      != 0) {
    WebRtc_Word32 socket_error = socket_transport_.LastError();
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: Error getting source ports. Socket error: %d",
                 __FUNCTION__, socket_error);
    return -1;
  }
  return 0;
#else
  WEBRTC_TRACE(kTraceStateInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s: not available for external transport", __FUNCTION__);
  return -1;
#endif
}
WebRtc_Word32 ViEChannel::RegisterSendTransport(Transport& transport) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);

#ifndef WEBRTC_EXTERNAL_TRANSPORT
  if (socket_transport_.SendSocketsInitialized() ||
      socket_transport_.ReceiveSocketsInitialized()) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s:  socket transport already initialized", __FUNCTION__);
    return -1;
  }
#endif
  if (rtp_rtcp_.Sending()) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: Sending", __FUNCTION__);
    return -1;
  }

  CriticalSectionScoped cs(callbackCritsect_);
  if (external_transport_) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: transport already registered", __FUNCTION__);
    return -1;
  }
  external_transport_ = &transport;
  vie_sender_.RegisterSendTransport(&transport);
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s: Transport registered: 0x%p", __FUNCTION__,
               &external_transport_);

  return 0;
}

WebRtc_Word32 ViEChannel::DeregisterSendTransport() {
  CriticalSectionScoped cs(callbackCritsect_);
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);

  if (!external_transport_) {
    WEBRTC_TRACE(kTraceWarning, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: no transport registered", __FUNCTION__);
    return -1;
  }
  if (rtp_rtcp_.Sending()) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: Sending", __FUNCTION__);
    return -1;
  }
  external_transport_ = NULL;
  vie_sender_.DeregisterSendTransport();
  return 0;
}

WebRtc_Word32 ViEChannel::ReceivedRTPPacket(
    const void* rtp_packet, const WebRtc_Word32 rtp_packet_length) {
  {
    CriticalSectionScoped cs(callbackCritsect_);
    if (!external_transport_) {
      return -1;
    }
  }
  return vie_receiver_.ReceivedRTPPacket(rtp_packet, rtp_packet_length);
}

WebRtc_Word32 ViEChannel::ReceivedRTCPPacket(
  const void* rtcp_packet, const WebRtc_Word32 rtcp_packet_length) {
  {
    CriticalSectionScoped cs(callbackCritsect_);
    if (!external_transport_) {
      return -1;
    }
  }
  return vie_receiver_.ReceivedRTCPPacket(rtcp_packet, rtcp_packet_length);
}

WebRtc_Word32 ViEChannel::EnableIPv6() {
  callbackCritsect_.Enter();
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s", __FUNCTION__);

  if (external_transport_) {
    callbackCritsect_.Leave();
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(engine_id_, channel_id_),
                 "%s: External transport registered", __FUNCTION__);
    return -1;
  }
  callbackCritsect_.Leave();

#ifndef WEBRTC_EXTERNAL_TRANSPORT
  if (socket_transport_.IpV6Enabled()) {
    WEBRTC_TRACE(kTraceWarning, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: IPv6 already enabled", __FUNCTION__);
    return -1;
  }

  if (socket_transport_.EnableIpV6() != 0) {
    WebRtc_Word32 socket_error = socket_transport_.LastError();
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: could not enable IPv6. Socket error: %d", __FUNCTION__,
                 socket_error);
    return -1;
  }
  return 0;
#else
  WEBRTC_TRACE(kTraceStateInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s: not available for external transport", __FUNCTION__);
  return -1;
#endif
}

bool ViEChannel::IsIPv6Enabled() {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);
  {
    CriticalSectionScoped cs(callbackCritsect_);
    if (external_transport_) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                   "%s: External transport registered", __FUNCTION__);
      return false;
    }
  }
#ifndef WEBRTC_EXTERNAL_TRANSPORT
  return socket_transport_.IpV6Enabled();
#else
  WEBRTC_TRACE(kTraceStateInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s: not available for external transport", __FUNCTION__);
  return false;
#endif
}

WebRtc_Word32 ViEChannel::SetSourceFilter(const WebRtc_UWord16 rtp_port,
                                          const WebRtc_UWord16 rtcp_port,
                                          const WebRtc_Word8* ip_address) {
  callbackCritsect_.Enter();
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);

  if (external_transport_) {
    callbackCritsect_.Leave();
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: External transport registered", __FUNCTION__);
    return -1;
  }
  callbackCritsect_.Leave();

#ifndef WEBRTC_EXTERNAL_TRANSPORT
  if (socket_transport_.SetFilterIP(ip_address) != 0) {
    // Logging done in module.
    return -1;
  }
  if (socket_transport_.SetFilterPorts(rtp_port, rtcp_port) != 0) {
    // Logging done.
    return -1;
  }
  return 0;
#else
  WEBRTC_TRACE(kTraceStateInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s: not available for external transport", __FUNCTION__);
  return -1;
#endif
}

WebRtc_Word32 ViEChannel::GetSourceFilter(WebRtc_UWord16& rtp_port,
                                          WebRtc_UWord16& rtcp_port,
                                          WebRtc_Word8* ip_address) const {
  callbackCritsect_.Enter();
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);

  if (external_transport_) {
    callbackCritsect_.Leave();
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: External transport registered", __FUNCTION__);
    return -1;
  }
  callbackCritsect_.Leave();

#ifndef WEBRTC_EXTERNAL_TRANSPORT
  if (socket_transport_.FilterIP(ip_address) != 0) {
    // Logging done in module.
    return -1;
  }
  if (socket_transport_.FilterPorts(rtp_port, rtcp_port) != 0) {
    // Logging done in module.
    return -1;
  }
  return 0;
#else
  WEBRTC_TRACE(kTraceStateInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s: not available for external transport", __FUNCTION__);
  return -1;
#endif
}

WebRtc_Word32 ViEChannel::SetToS(const WebRtc_Word32 DSCP,
                                 const bool use_set_sockOpt) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);
  {
    CriticalSectionScoped cs(callbackCritsect_);
    if (external_transport_) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                   "%s: External transport registered", __FUNCTION__);
      return -1;
    }
  }
#ifndef WEBRTC_EXTERNAL_TRANSPORT
  if (socket_transport_.SetToS(DSCP, use_set_sockOpt) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: Socket error: %d", __FUNCTION__,
                 socket_transport_.LastError());
    return -1;
  }
  return 0;
#else
  WEBRTC_TRACE(kTraceStateInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s: not available for external transport", __FUNCTION__);
  return -1;
#endif
}

WebRtc_Word32 ViEChannel::GetToS(WebRtc_Word32& DSCP,
                                 bool& use_set_sockOpt) const {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);
  {
    CriticalSectionScoped cs(callbackCritsect_);
    if (external_transport_) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                   "%s: External transport registered", __FUNCTION__);
      return -1;
    }
  }
#ifndef WEBRTC_EXTERNAL_TRANSPORT
  if (socket_transport_.ToS(DSCP, use_set_sockOpt) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: Socket error: %d", __FUNCTION__,
                 socket_transport_.LastError());
    return -1;
  }
  return 0;
#else
  WEBRTC_TRACE(kTraceStateInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s: not available for external transport", __FUNCTION__);
  return -1;
#endif
}

WebRtc_Word32 ViEChannel::SetSendGQoS(const bool enable,
                                      const WebRtc_Word32 service_type,
                                      const WebRtc_UWord32 max_bitrate,
                                      const WebRtc_Word32 overrideDSCP) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);
  {
    CriticalSectionScoped cs(callbackCritsect_);
    if (external_transport_) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                   "%s: External transport registered", __FUNCTION__);
      return -1;
    }
  }
#ifndef WEBRTC_EXTERNAL_TRANSPORT
  if (socket_transport_.SetQoS(enable, service_type, max_bitrate, overrideDSCP,
                               false) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: Socket error: %d", __FUNCTION__,
                 socket_transport_.LastError());
    return -1;
  }
  return 0;
#else
  WEBRTC_TRACE(kTraceStateInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s: not available for external transport", __FUNCTION__);
  return -1;
#endif
}

WebRtc_Word32 ViEChannel::GetSendGQoS(bool& enabled,
                                      WebRtc_Word32& service_type,
                                      WebRtc_Word32& overrideDSCP) const {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);
  {
    CriticalSectionScoped cs(callbackCritsect_);
    if (external_transport_) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                   "%s: External transport registered", __FUNCTION__);
      return -1;
    }
  }
#ifndef WEBRTC_EXTERNAL_TRANSPORT
  if (socket_transport_.QoS(enabled, service_type, overrideDSCP) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: Socket error: %d", __FUNCTION__,
                 socket_transport_.LastError());
    return -1;
  }
  return 0;
#else
  WEBRTC_TRACE(kTraceStateInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s: not available for external transport", __FUNCTION__);
  return -1;
#endif
}

WebRtc_Word32 ViEChannel::SetMTU(WebRtc_UWord16 mtu) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);
  if (rtp_rtcp_.SetMaxTransferUnit(mtu) != 0) {
    // Logging done.
    return -1;
  }
  for (std::list<RtpRtcp*>::iterator it = simulcast_rtp_rtcp_.begin();
       it != simulcast_rtp_rtcp_.end();
       it++) {
    RtpRtcp* rtp_rtcp = *it;
    rtp_rtcp->SetMaxTransferUnit(mtu);
  }
  return 0;
}

WebRtc_UWord16 ViEChannel::MaxDataPayloadLength() const {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s", __FUNCTION__);
  return rtp_rtcp_.MaxDataPayloadLength();
}

WebRtc_Word32 ViEChannel::SetPacketTimeoutNotification(
    bool enable, WebRtc_UWord32 timeout_seconds) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);
  if (enable) {
    WebRtc_UWord32 timeout_ms = 1000 * timeout_seconds;
    if (rtp_rtcp_.SetPacketTimeout(timeout_ms, 0) != 0) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                   "%s", __FUNCTION__);
      return -1;
    }
  } else {
    if (rtp_rtcp_.SetPacketTimeout(0, 0) != 0) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                   "%s", __FUNCTION__);
      return -1;
    }
  }
  return 0;
}

WebRtc_Word32 ViEChannel::RegisterNetworkObserver(
    ViENetworkObserver* observer) {
  CriticalSectionScoped cs(callbackCritsect_);
  if (observer) {
    if (networkObserver_) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                   "%s: observer alread added", __FUNCTION__);
      return -1;
    }
    WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: observer added", __FUNCTION__);
    networkObserver_ = observer;
  } else {
    if (!networkObserver_) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                   "%s: no observer added", __FUNCTION__);
      return -1;
    }
    WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: observer removed", __FUNCTION__);
    networkObserver_ = NULL;
  }
  return 0;
}

bool ViEChannel::NetworkObserverRegistered() {
  CriticalSectionScoped cs(callbackCritsect_);
  return networkObserver_ != NULL;
}

WebRtc_Word32 ViEChannel::SetPeriodicDeadOrAliveStatus(
  const bool enable, const WebRtc_UWord32 sample_time_seconds) {
  WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);

  CriticalSectionScoped cs(callbackCritsect_);
  if (!networkObserver_) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: no observer added", __FUNCTION__);
    return -1;
  }

  bool enabled = false;
  WebRtc_UWord8 current_sampletime_seconds = 0;

  // Get old settings.
  rtp_rtcp_.PeriodicDeadOrAliveStatus(enabled, current_sampletime_seconds);
  // Set new settings.
  if (rtp_rtcp_.SetPeriodicDeadOrAliveStatus(
        enable, static_cast<WebRtc_UWord8>(sample_time_seconds)) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: Could not set periodic dead-or-alive status",
                 __FUNCTION__);
    return -1;
  }
  if (!enable) {
    // Restore last utilized sample time.
    // Without this trick, the sample time would always be reset to default
    // (2 sec), each time dead-or-alive was disabled without sample-time
    // parameter.
    rtp_rtcp_.SetPeriodicDeadOrAliveStatus(enable, current_sampletime_seconds);
  }
  return 0;
}

WebRtc_Word32 ViEChannel::SendUDPPacket(const WebRtc_Word8* data,
                                        const WebRtc_UWord32 length,
                                        WebRtc_Word32& transmitted_bytes,
                                        bool use_rtcp_socket) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);
  {
    CriticalSectionScoped cs(callbackCritsect_);
    if (external_transport_) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                   "%s: External transport registered", __FUNCTION__);
      return -1;
    }
  }
#ifndef WEBRTC_EXTERNAL_TRANSPORT
  transmitted_bytes = socket_transport_.SendRaw(data, length, use_rtcp_socket);
  if (transmitted_bytes == -1) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
                 __FUNCTION__);
    return -1;
  }
  return 0;
#else
  WEBRTC_TRACE(kTraceStateInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s: not available for external transport", __FUNCTION__);
  return -1;
#endif
}

WebRtc_Word32 ViEChannel::EnableColorEnhancement(bool enable) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s(enable: %d)", __FUNCTION__, enable);

  CriticalSectionScoped cs(callbackCritsect_);
  if (enable && color_enhancement_) {
    WEBRTC_TRACE(kTraceWarning, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: Already enabled", __FUNCTION__);
    return -1;
  } else if (!enable && !color_enhancement_) {
    WEBRTC_TRACE(kTraceWarning, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: not enabled", __FUNCTION__);
    return -1;
  }
  color_enhancement_ = enable;
  return 0;
}

WebRtc_Word32 ViEChannel::RegisterSendRtpRtcpModule(
    RtpRtcp& send_rtp_rtcp_module) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);

  WebRtc_Word32 ret_val = rtp_rtcp_.RegisterDefaultModule(
      &send_rtp_rtcp_module);
  if (ret_val == 0) {
    // We need to store this for the SetSendCodec call.
    default_rtp_rtcp_ = &send_rtp_rtcp_module;
  }
  return ret_val;
}

WebRtc_Word32 ViEChannel::DeregisterSendRtpRtcpModule() {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);
  default_rtp_rtcp_ = NULL;

  for (std::list<RtpRtcp*>::const_iterator it = simulcast_rtp_rtcp_.begin();
       it != simulcast_rtp_rtcp_.end();
       it++) {
    RtpRtcp* rtp_rtcp = *it;
    rtp_rtcp->DeRegisterDefaultModule();
  }
  return rtp_rtcp_.DeRegisterDefaultModule();
}

WebRtc_Word32 ViEChannel::FrameToRender(VideoFrame& video_frame) {
  CriticalSectionScoped cs(callbackCritsect_);

  if (decoder_reset_) {
    // Trigger a callback to the user if the incoming codec has changed.
    if (codec_observer_) {
      VideoCodec decoder;
      memset(&decoder, 0, sizeof(decoder));
      if (vcm_.ReceiveCodec(&decoder) == VCM_OK) {
        // VCM::ReceiveCodec returns the codec set by
        // RegisterReceiveCodec, which might not be the size we're
        // actually decoding.
        decoder.width = static_cast<unsigned short>(video_frame.Width());
        decoder.height = static_cast<unsigned short>(video_frame.Height());
        codec_observer_->IncomingCodecChanged(channel_id_, decoder);
      } else {
        assert(false);
        WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
                     "%s: Could not get receive codec", __FUNCTION__);
      }
    }
    decoder_reset_ = false;
  }
  if (effect_filter_) {
    effect_filter_->Transform(video_frame.Length(), video_frame.Buffer(),
                              video_frame.TimeStamp(), video_frame.Width(),
                              video_frame.Height());
  }
  if (color_enhancement_) {
    VideoProcessingModule::ColorEnhancement(video_frame);
  }

  // Record videoframe.
  file_recorder_.RecordVideoFrame(video_frame);

  WebRtc_UWord32 arr_ofCSRC[kRtpCsrcSize];
  WebRtc_Word32 no_of_csrcs = rtp_rtcp_.RemoteCSRCs(arr_ofCSRC);
  if (no_of_csrcs <= 0) {
    arr_ofCSRC[0] = rtp_rtcp_.RemoteSSRC();
    no_of_csrcs = 1;
  }
  DeliverFrame(video_frame, no_of_csrcs, arr_ofCSRC);
  return 0;
}

WebRtc_Word32 ViEChannel::ReceivedDecodedReferenceFrame(
  const WebRtc_UWord64 picture_id) {
  return rtp_rtcp_.SendRTCPReferencePictureSelection(picture_id);
}

WebRtc_Word32 ViEChannel::StoreReceivedFrame(
  const EncodedVideoData& frame_to_store) {
  return 0;
}

WebRtc_Word32 ViEChannel::ReceiveStatistics(const WebRtc_UWord32 bit_rate,
                                            const WebRtc_UWord32 frame_rate) {
  CriticalSectionScoped cs(callbackCritsect_);
  if (codec_observer_) {
    WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: bitrate %u, framerate %u", __FUNCTION__, bit_rate,
                 frame_rate);
    codec_observer_->IncomingRate(channel_id_, frame_rate, bit_rate);
  }
  return 0;
}

WebRtc_Word32 ViEChannel::FrameTypeRequest(const FrameType frame_type) {
  WEBRTC_TRACE(kTraceStream, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s(frame_type: %d)", __FUNCTION__, frame_type);
  {
    CriticalSectionScoped cs(callbackCritsect_);
    if (codec_observer_ && do_key_frame_callbackRequest_) {
      codec_observer_->RequestNewKeyFrame(channel_id_);
    }
  }
  return rtp_rtcp_.RequestKeyFrame(frame_type);
}

WebRtc_Word32 ViEChannel::SliceLossIndicationRequest(
  const WebRtc_UWord64 picture_id) {
  return rtp_rtcp_.SendRTCPSliceLossIndication((WebRtc_UWord8) picture_id);
}

WebRtc_Word32 ViEChannel::ResendPackets(const WebRtc_UWord16* sequence_numbers,
                                        WebRtc_UWord16 length) {
  WEBRTC_TRACE(kTraceStream, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s(length: %d)", __FUNCTION__, length);
  return rtp_rtcp_.SendNACK(sequence_numbers, length);
}

bool ViEChannel::ChannelDecodeThreadFunction(void* obj) {
  return static_cast<ViEChannel*>(obj)->ChannelDecodeProcess();
}

bool ViEChannel::ChannelDecodeProcess() {
  // Decode is blocking, but sleep some time anyway to not get a spin.
  vcm_.Decode(kMaxDecodeWaitTimeMs);

  if ((TickTime::Now() - vcm_rttreported_).Milliseconds() > 1000) {
    WebRtc_UWord16 RTT;
    WebRtc_UWord16 avgRTT;
    WebRtc_UWord16 minRTT;
    WebRtc_UWord16 maxRTT;

    if (rtp_rtcp_.RTT(rtp_rtcp_.RemoteSSRC(), &RTT, &avgRTT, &minRTT, &maxRTT)
        == 0) {
      vcm_.SetReceiveChannelParameters(RTT);
    }
    vcm_rttreported_ = TickTime::Now();
  }
  return true;
}

WebRtc_Word32 ViEChannel::StartDecodeThread() {
  // Start the decode thread
  if (decode_thread_) {
    // Already started.
    return 0;
  }
  decode_thread_ = ThreadWrapper::CreateThread(ChannelDecodeThreadFunction,
                                                   this, kHighestPriority,
                                                   "DecodingThread");
  if (!decode_thread_) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: could not create decode thread", __FUNCTION__);
    return -1;
  }

  unsigned int thread_id;
  if (decode_thread_->Start(thread_id) == false) {
    delete decode_thread_;
    decode_thread_ = NULL;
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: could not start decode thread", __FUNCTION__);
    return -1;
  }

  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s: decode thread with id %u started", __FUNCTION__);
  return 0;
}

WebRtc_Word32 ViEChannel::StopDecodeThread() {
  if (!decode_thread_) {
    WEBRTC_TRACE(kTraceWarning, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: decode thread not running", __FUNCTION__);
    return 0;
  }

  decode_thread_->SetNotAlive();
  if (decode_thread_->Stop()) {
    delete decode_thread_;
  } else {
    // Couldn't stop the thread, leak instead of crash.
    WEBRTC_TRACE(kTraceWarning, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: could not stop decode thread", __FUNCTION__);
    assert(!"could not stop decode thread");
  }
  decode_thread_ = NULL;
  return 0;
}

WebRtc_Word32 ViEChannel::RegisterExternalEncryption(Encryption* encryption) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);

  CriticalSectionScoped cs(callbackCritsect_);
  if (external_encryption_) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: external encryption already registered", __FUNCTION__);
    return -1;
  }

  external_encryption_ = encryption;

  vie_receiver_.RegisterExternalDecryption(encryption);
  vie_sender_.RegisterExternalEncryption(encryption);

  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s", "external encryption object registerd with channel=%d",
               channel_id_);
  return 0;
}

WebRtc_Word32 ViEChannel::DeRegisterExternalEncryption() {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);

  CriticalSectionScoped cs(callbackCritsect_);
  if (!external_encryption_) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: external encryption is not registered", __FUNCTION__);
    return -1;
  }

  external_transport_ = NULL;
  vie_receiver_.DeregisterExternalDecryption();
  vie_sender_.DeregisterExternalEncryption();
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s external encryption object de-registerd with channel=%d",
               __FUNCTION__, channel_id_);
  return 0;
}

WebRtc_Word32 ViEChannel::SetVoiceChannel(WebRtc_Word32 ve_channel_id,
                                          VoEVideoSync* ve_sync_interface) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s, audio channel %d, video channel %d", __FUNCTION__,
               ve_channel_id, channel_id_);

  if (ve_sync_interface) {
    // Register lip sync
    module_process_thread_.RegisterModule(&vie_sync_);
  } else {
    module_process_thread_.DeRegisterModule(&vie_sync_);
  }
  return vie_sync_.SetVoiceChannel(ve_channel_id, ve_sync_interface);
}

WebRtc_Word32 ViEChannel::VoiceChannel() {
  return vie_sync_.VoiceChannel();
}

WebRtc_Word32 ViEChannel::RegisterEffectFilter(ViEEffectFilter* effect_filter) {
  CriticalSectionScoped cs(callbackCritsect_);
  if (!effect_filter) {
    if (!effect_filter_) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                   "%s: no effect filter added for channel %d",
                   __FUNCTION__, channel_id_);
      return -1;
    }
    WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: deregister effect filter for device %d", __FUNCTION__,
                 channel_id_);
  } else {
    WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: register effect filter for device %d", __FUNCTION__,
                 channel_id_);
    if (effect_filter_) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                   "%s: effect filter already added for channel %d",
                   __FUNCTION__, channel_id_);
      return -1;
    }
  }
  effect_filter_ = effect_filter;
  return 0;
}

ViEFileRecorder& ViEChannel::GetIncomingFileRecorder() {
  // Start getting callback of all frames before they are decoded.
  vcm_.RegisterFrameStorageCallback(this);
  return file_recorder_;
}

void ViEChannel::ReleaseIncomingFileRecorder() {
  // Stop getting callback of all frames before they are decoded.
  vcm_.RegisterFrameStorageCallback(NULL);
}

void ViEChannel::OnLipSyncUpdate(const WebRtc_Word32 id,
                                 const WebRtc_Word32 audio_video_offset) {
  if (channel_id_ != ChannelId(id)) {
    WEBRTC_TRACE(kTraceStream, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s, incorrect id", __FUNCTION__, id);
    return;
  }
  vie_sync_.SetNetworkDelay(audio_video_offset);
}

void ViEChannel::OnApplicationDataReceived(const WebRtc_Word32 id,
                                           const WebRtc_UWord8 sub_type,
                                           const WebRtc_UWord32 name,
                                           const WebRtc_UWord16 length,
                                           const WebRtc_UWord8* data) {
  if (channel_id_ != ChannelId(id)) {
    WEBRTC_TRACE(kTraceStream, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s, incorrect id", __FUNCTION__, id);
    return;
  }
  CriticalSectionScoped cs(callbackCritsect_);
  {
    if (rtcp_observer_) {
      rtcp_observer_->OnApplicationDataReceived(
          channel_id_, sub_type, name, reinterpret_cast<const char*>(data),
          length);
    }
  }
}

WebRtc_Word32 ViEChannel::OnInitializeDecoder(
    const WebRtc_Word32 id,
    const WebRtc_Word8 payload_type,
    const WebRtc_Word8 payload_name[RTP_PAYLOAD_NAME_SIZE],
    const int frequency,
    const WebRtc_UWord8 channels,
    const WebRtc_UWord32 rate) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s: payload_type %d, payload_name %s", __FUNCTION__,
               payload_type, payload_name);
  vcm_.ResetDecoder();

  callbackCritsect_.Enter();
  decoder_reset_ = true;
  callbackCritsect_.Leave();
  return 0;
}

void ViEChannel::OnPacketTimeout(const WebRtc_Word32 id) {
  assert(ChannelId(id) == channel_id_);
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);

  CriticalSectionScoped cs(callbackCritsect_);
  if (networkObserver_) {
#ifndef WEBRTC_EXTERNAL_TRANSPORT
    if (socket_transport_.Receiving() || external_transport_) {
#else
    if (external_transport_) {
#endif
      networkObserver_->PacketTimeout(channel_id_, NoPacket);
      rtp_packet_timeout_ = true;
    }
  }
}

void ViEChannel::OnReceivedPacket(const WebRtc_Word32 id,
                                  const RtpRtcpPacketType packet_type) {
  assert(ChannelId(id) == channel_id_);
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);
  if (rtp_packet_timeout_ && packet_type == kPacketRtp) {
    CriticalSectionScoped cs(callbackCritsect_);
    if (networkObserver_) {
      networkObserver_->PacketTimeout(channel_id_, PacketReceived);
    }

    // Reset even if no observer set, might have been removed during timeout.
    rtp_packet_timeout_ = false;
  }
}

void ViEChannel::OnPeriodicDeadOrAlive(const WebRtc_Word32 id,
                                       const RTPAliveType alive) {
  assert(ChannelId(id) == channel_id_);
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s(id=%d, alive=%d)", __FUNCTION__, id, alive);

  CriticalSectionScoped cs(callbackCritsect_);
  if (!networkObserver_) {
    return;
  }
  bool is_alive = true;
  if (alive == kRtpDead) {
    is_alive = false;
  }
  networkObserver_->OnPeriodicDeadOrAlive(channel_id_, is_alive);
  return;
}

void ViEChannel::OnIncomingSSRCChanged(const WebRtc_Word32 id,
                                       const WebRtc_UWord32 SSRC) {
  if (channel_id_ != ChannelId(id)) {
    assert(false);
    WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s, incorrect id", __FUNCTION__, id);
    return;
  }

  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s: %u", __FUNCTION__, SSRC);

  CriticalSectionScoped cs(callbackCritsect_);
  {
    if (rtp_observer_) {
      rtp_observer_->IncomingSSRCChanged(channel_id_, SSRC);
    }
  }
}

void ViEChannel::OnIncomingCSRCChanged(const WebRtc_Word32 id,
                                       const WebRtc_UWord32 CSRC,
                                       const bool added) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s: %u added: %d", __FUNCTION__, CSRC, added);

  if (channel_id_ != ChannelId(id)) {
    assert(false);
    WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s, incorrect id", __FUNCTION__, id);
    return;
  }

  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s: %u", __FUNCTION__, CSRC);

  CriticalSectionScoped cs(callbackCritsect_);
  {
    if (rtp_observer_) {
      rtp_observer_->IncomingCSRCChanged(channel_id_, CSRC, added);
    }
  }
}

WebRtc_Word32 ViEChannel::SetInverseH263Logic(const bool enable) {
  return rtp_rtcp_.SetH263InverseLogic(enable);
}

}  // namespace webrtc
