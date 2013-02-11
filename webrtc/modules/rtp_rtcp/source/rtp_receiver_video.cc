/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtp_receiver_video.h"

#include <math.h>

#include <cassert>  // assert
#include <cstring>  // memcpy()

#include "webrtc/modules/rtp_rtcp/source/receiver_fec.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_payload_registry.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_rtcp_impl.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/trace.h"

namespace webrtc {
WebRtc_UWord32 BitRateBPS(WebRtc_UWord16 x) {
  return (x & 0x3fff) * WebRtc_UWord32(pow(10.0f, (2 + (x >> 14))));
}

RTPReceiverVideo::RTPReceiverVideo(
    const WebRtc_Word32 id,
    const RTPPayloadRegistry* rtp_rtp_payload_registry,
    RtpData* data_callback)
    : RTPReceiverStrategy(data_callback),
      id_(id),
      rtp_rtp_payload_registry_(rtp_rtp_payload_registry),
      critical_section_receiver_video_(
          CriticalSectionWrapper::CreateCriticalSection()),
      current_fec_frame_decoded_(false),
      receive_fec_(NULL) {
}

RTPReceiverVideo::~RTPReceiverVideo() {
  delete critical_section_receiver_video_;
  delete receive_fec_;
}

bool RTPReceiverVideo::ShouldReportCsrcChanges(
    WebRtc_UWord8 payload_type) const {
  // Always do this for video packets.
  return true;
}

WebRtc_Word32 RTPReceiverVideo::OnNewPayloadTypeCreated(
    const char payload_name[RTP_PAYLOAD_NAME_SIZE],
    const WebRtc_Word8 payload_type,
    const WebRtc_UWord32 frequency) {
  if (ModuleRTPUtility::StringCompare(payload_name, "ULPFEC", 6)) {
    // Enable FEC if not enabled.
    if (receive_fec_ == NULL) {
      receive_fec_ = new ReceiverFEC(id_, this);
    }
    receive_fec_->SetPayloadTypeFEC(payload_type);
  }
  return 0;
}

WebRtc_Word32 RTPReceiverVideo::ParseRtpPacket(
    WebRtcRTPHeader* rtp_header,
    const ModuleRTPUtility::PayloadUnion& specific_payload,
    const bool is_red,
    const WebRtc_UWord8* packet,
    const WebRtc_UWord16 packet_length,
    const WebRtc_Word64 timestamp_ms,
    const bool is_first_packet) {
  const WebRtc_UWord8* payload_data =
      ModuleRTPUtility::GetPayloadData(rtp_header, packet);
  const WebRtc_UWord16 payload_data_length =
      ModuleRTPUtility::GetPayloadDataLength(rtp_header, packet_length);
  return ParseVideoCodecSpecific(rtp_header,
                                 payload_data,
                                 payload_data_length,
                                 specific_payload.Video.videoCodecType,
                                 is_red,
                                 packet,
                                 packet_length,
                                 timestamp_ms,
                                 is_first_packet);
}

WebRtc_Word32 RTPReceiverVideo::GetFrequencyHz() const {
  return kDefaultVideoFrequency;
}

RTPAliveType RTPReceiverVideo::ProcessDeadOrAlive(
    WebRtc_UWord16 last_payload_length) const {
  return kRtpDead;
}

WebRtc_Word32 RTPReceiverVideo::InvokeOnInitializeDecoder(
    RtpFeedback* callback,
    const WebRtc_Word32 id,
    const WebRtc_Word8 payload_type,
    const char payload_name[RTP_PAYLOAD_NAME_SIZE],
    const ModuleRTPUtility::PayloadUnion& specific_payload) const {
  // For video we just go with default values.
  if (-1 == callback->OnInitializeDecoder(
      id, payload_type, payload_name, kDefaultVideoFrequency, 1, 0)) {
    WEBRTC_TRACE(kTraceError,
                 kTraceRtpRtcp,
                 id,
                 "Failed to create video decoder for payload type:%d",
                 payload_type);
    return -1;
  }
  return 0;
}

// we have no critext when calling this
// we are not allowed to have any critsects when calling
// CallbackOfReceivedPayloadData
WebRtc_Word32 RTPReceiverVideo::ParseVideoCodecSpecific(
    WebRtcRTPHeader* rtp_header,
    const WebRtc_UWord8* payload_data,
    const WebRtc_UWord16 payload_data_length,
    const RtpVideoCodecTypes video_type,
    const bool is_red,
    const WebRtc_UWord8* incoming_rtp_packet,
    const WebRtc_UWord16 incoming_rtp_packet_size,
    const WebRtc_Word64 now_ms,
    const bool is_first_packet) {
  WebRtc_Word32 ret_val = 0;

  critical_section_receiver_video_->Enter();

  if (is_red) {
    if (receive_fec_ == NULL) {
      critical_section_receiver_video_->Leave();
      return -1;
    }
    bool FECpacket = false;
    ret_val = receive_fec_->AddReceivedFECPacket(
        rtp_header, incoming_rtp_packet, payload_data_length, FECpacket);
    if (ret_val != -1) {
      ret_val = receive_fec_->ProcessReceivedFEC();
    }
    critical_section_receiver_video_->Leave();

    if (ret_val == 0 && FECpacket) {
      // Callback with the received FEC packet.
      // The normal packets are delivered after parsing.
      // This contains the original RTP packet header but with
      // empty payload and data length.
      rtp_header->frameType = kFrameEmpty;
      // We need this for the routing.
      WebRtc_Word32 ret_val = SetCodecType(video_type, rtp_header);
      if (ret_val != 0) {
        return ret_val;
      }
      // Pass the length of FEC packets so that they can be accounted for in
      // the bandwidth estimator.
      ret_val = data_callback_->OnReceivedPayloadData(
          NULL, payload_data_length, rtp_header);
    }
  } else {
    // will leave the critical_section_receiver_video_ critsect
    ret_val = ParseVideoCodecSpecificSwitch(rtp_header,
                                            payload_data,
                                            payload_data_length,
                                            video_type,
                                            is_first_packet);
  }
  return ret_val;
}

WebRtc_Word32 RTPReceiverVideo::BuildRTPheader(
    const WebRtcRTPHeader* rtp_header,
    WebRtc_UWord8* data_buffer) const {
  data_buffer[0] = static_cast<WebRtc_UWord8>(0x80);  // version 2
  data_buffer[1] = static_cast<WebRtc_UWord8>(rtp_header->header.payloadType);
  if (rtp_header->header.markerBit) {
    data_buffer[1] |= kRtpMarkerBitMask;  // MarkerBit is 1
  }
  ModuleRTPUtility::AssignUWord16ToBuffer(data_buffer + 2,
                                          rtp_header->header.sequenceNumber);
  ModuleRTPUtility::AssignUWord32ToBuffer(data_buffer + 4,
                                          rtp_header->header.timestamp);
  ModuleRTPUtility::AssignUWord32ToBuffer(data_buffer + 8,
                                          rtp_header->header.ssrc);

  WebRtc_Word32 rtp_header_length = 12;

  // Add the CSRCs if any
  if (rtp_header->header.numCSRCs > 0) {
    if (rtp_header->header.numCSRCs > 16) {
      // error
      assert(false);
    }
    WebRtc_UWord8* ptr = &data_buffer[rtp_header_length];
    for (WebRtc_UWord32 i = 0; i < rtp_header->header.numCSRCs; ++i) {
      ModuleRTPUtility::AssignUWord32ToBuffer(ptr,
                                              rtp_header->header.arrOfCSRCs[i]);
      ptr += 4;
    }
    data_buffer[0] = (data_buffer[0] & 0xf0) | rtp_header->header.numCSRCs;
    // Update length of header
    rtp_header_length += sizeof(WebRtc_UWord32) * rtp_header->header.numCSRCs;
  }
  return rtp_header_length;
}

WebRtc_Word32 RTPReceiverVideo::ReceiveRecoveredPacketCallback(
    WebRtcRTPHeader* rtp_header,
    const WebRtc_UWord8* payload_data,
    const WebRtc_UWord16 payload_data_length) {
  // TODO(pwestin) Re-factor this to avoid the messy critsect handling.
  critical_section_receiver_video_->Enter();

  current_fec_frame_decoded_ = true;

  ModuleRTPUtility::Payload* payload = NULL;
  if (rtp_rtp_payload_registry_->PayloadTypeToPayload(
          rtp_header->header.payloadType, payload) != 0) {
    critical_section_receiver_video_->Leave();
    return -1;
  }
  // here we can re-create the original lost packet so that we can use it for
  // the relay we need to re-create the RED header too
  WebRtc_UWord8 recovered_packet[IP_PACKET_SIZE];
  WebRtc_UWord16 rtp_header_length =
      (WebRtc_UWord16) BuildRTPheader(rtp_header, recovered_packet);

  const WebRtc_UWord8 kREDForFECHeaderLength = 1;

  // replace pltype
  recovered_packet[1] &= 0x80;  // Reset.
  recovered_packet[1] += rtp_rtp_payload_registry_->red_payload_type();

  // add RED header
  recovered_packet[rtp_header_length] = rtp_header->header.payloadType;
  // f-bit always 0

  memcpy(recovered_packet + rtp_header_length + kREDForFECHeaderLength,
         payload_data,
         payload_data_length);

  // A recovered packet can be the first packet, but we lack the ability to
  // detect it at the moment since we do not store the history of recently
  // received packets. Most codecs like VP8 deal with this in other ways.
  bool is_first_packet = false;

  return ParseVideoCodecSpecificSwitch(
      rtp_header,
      payload_data,
      payload_data_length,
      payload->typeSpecific.Video.videoCodecType,
      is_first_packet);
}

WebRtc_Word32 RTPReceiverVideo::SetCodecType(
    const RtpVideoCodecTypes video_type,
    WebRtcRTPHeader* rtp_header) const {
  switch (video_type) {
    case kRtpNoVideo:
      rtp_header->type.Video.codec = kRTPVideoGeneric;
      break;
    case kRtpVp8Video:
      rtp_header->type.Video.codec = kRTPVideoVP8;
      break;
    case kRtpFecVideo:
      rtp_header->type.Video.codec = kRTPVideoFEC;
      break;
  }
  return 0;
}

WebRtc_Word32 RTPReceiverVideo::ParseVideoCodecSpecificSwitch(
    WebRtcRTPHeader* rtp_header,
    const WebRtc_UWord8* payload_data,
    const WebRtc_UWord16 payload_data_length,
    const RtpVideoCodecTypes video_type,
    const bool is_first_packet) {
  WebRtc_Word32 ret_val = SetCodecType(video_type, rtp_header);
  if (ret_val != 0) {
    critical_section_receiver_video_->Leave();
    return ret_val;
  }
  WEBRTC_TRACE(kTraceStream,
               kTraceRtpRtcp,
               id_,
               "%s(timestamp:%u)",
               __FUNCTION__,
               rtp_header->header.timestamp);

  // All receive functions release critical_section_receiver_video_ before
  // returning.
  switch (video_type) {
    case kRtpNoVideo:
      rtp_header->type.Video.isFirstPacket = is_first_packet;
      return ReceiveGenericCodec(rtp_header, payload_data, payload_data_length);
    case kRtpVp8Video:
      return ReceiveVp8Codec(rtp_header, payload_data, payload_data_length);
    case kRtpFecVideo:
      break;
  }
  critical_section_receiver_video_->Leave();
  return -1;
}

WebRtc_Word32 RTPReceiverVideo::ReceiveVp8Codec(
    WebRtcRTPHeader* rtp_header,
    const WebRtc_UWord8* payload_data,
    const WebRtc_UWord16 payload_data_length) {
  bool success;
  ModuleRTPUtility::RTPPayload parsed_packet;
  if (payload_data_length == 0) {
    success = true;
    parsed_packet.info.VP8.dataLength = 0;
  } else {
    ModuleRTPUtility::RTPPayloadParser rtp_payload_parser(
        kRtpVp8Video, payload_data, payload_data_length, id_);

    success = rtp_payload_parser.Parse(parsed_packet);
  }
  // from here down we only work on local data
  critical_section_receiver_video_->Leave();

  if (!success) {
    return -1;
  }
  if (parsed_packet.info.VP8.dataLength == 0) {
    // we have an "empty" VP8 packet, it's ok, could be one way video
    // Inform the jitter buffer about this packet.
    rtp_header->frameType = kFrameEmpty;
    if (data_callback_->OnReceivedPayloadData(NULL, 0, rtp_header) != 0) {
      return -1;
    }
    return 0;
  }
  rtp_header->frameType = (parsed_packet.frameType == ModuleRTPUtility::kIFrame)
      ? kVideoFrameKey : kVideoFrameDelta;

  RTPVideoHeaderVP8* to_header = &rtp_header->type.Video.codecHeader.VP8;
  ModuleRTPUtility::RTPPayloadVP8* from_header = &parsed_packet.info.VP8;

  rtp_header->type.Video.isFirstPacket =
      from_header->beginningOfPartition && (from_header->partitionID == 0);
  to_header->nonReference = from_header->nonReferenceFrame;
  to_header->pictureId =
      from_header->hasPictureID ? from_header->pictureID : kNoPictureId;
  to_header->tl0PicIdx =
      from_header->hasTl0PicIdx ? from_header->tl0PicIdx : kNoTl0PicIdx;
  if (from_header->hasTID) {
    to_header->temporalIdx = from_header->tID;
    to_header->layerSync = from_header->layerSync;
  } else {
    to_header->temporalIdx = kNoTemporalIdx;
    to_header->layerSync = false;
  }
  to_header->keyIdx = from_header->hasKeyIdx ? from_header->keyIdx : kNoKeyIdx;

  to_header->frameWidth = from_header->frameWidth;
  to_header->frameHeight = from_header->frameHeight;

  to_header->partitionId = from_header->partitionID;
  to_header->beginningOfPartition = from_header->beginningOfPartition;

  if (data_callback_->OnReceivedPayloadData(parsed_packet.info.VP8.data,
                                            parsed_packet.info.VP8.dataLength,
                                            rtp_header) != 0) {
    return -1;
  }
  return 0;
}

WebRtc_Word32 RTPReceiverVideo::ReceiveGenericCodec(
    WebRtcRTPHeader* rtp_header,
    const WebRtc_UWord8* payload_data,
    const WebRtc_UWord16 payload_data_length) {
  rtp_header->frameType = kVideoFrameKey;

  critical_section_receiver_video_->Leave();

  if (data_callback_->OnReceivedPayloadData(
          payload_data, payload_data_length, rtp_header) != 0) {
    return -1;
  }
  return 0;
}
}  // namespace webrtc
