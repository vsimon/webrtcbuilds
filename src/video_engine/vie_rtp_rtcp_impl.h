/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_VIE_RTP_RTCP_IMPL_H_
#define WEBRTC_VIDEO_ENGINE_VIE_RTP_RTCP_IMPL_H_

#include "modules/rtp_rtcp/interface/rtp_rtcp_defines.h"
#include "typedefs.h"
#include "video_engine/include/vie_rtp_rtcp.h"
#include "video_engine/vie_ref_count.h"
#include "video_engine/vie_shared_data.h"

namespace webrtc {

class ViERTP_RTCPImpl
    : public virtual ViESharedData,
      public ViERTP_RTCP,
      public ViERefCount {
 public:
  // Implements ViERTP_RTCP
  virtual int Release();
  virtual int SetLocalSSRC(const int video_channel,
                           const unsigned int SSRC,
                           const StreamType usage,
                           const unsigned char simulcast_idx);
  virtual int GetLocalSSRC(const int video_channel, unsigned int& SSRC) const;
  virtual int SetRemoteSSRCType(const int video_channel,
                                const StreamType usage,
                                const unsigned int SSRC) const;
  virtual int GetRemoteSSRC(const int video_channel, unsigned int& SSRC) const;
  virtual int GetRemoteCSRCs(const int video_channel,
                             unsigned int CSRCs[kRtpCsrcSize]) const;
  virtual int SetStartSequenceNumber(const int video_channel,
                                     unsigned short sequence_number);
  virtual int SetRTCPStatus(const int video_channel,
                            const ViERTCPMode rtcp_mode);
  virtual int GetRTCPStatus(const int video_channel,
                            ViERTCPMode& rtcp_mode) const;
  virtual int SetRTCPCName(const int video_channel,
                           const char rtcp_cname[KMaxRTCPCNameLength]);
  virtual int GetRTCPCName(const int video_channel,
                           char rtcp_cname[KMaxRTCPCNameLength]) const;
  virtual int GetRemoteRTCPCName(const int video_channel,
                                 char rtcp_cname[KMaxRTCPCNameLength]) const;
  virtual int SendApplicationDefinedRTCPPacket(
      const int video_channel,
      const unsigned char sub_type,
      unsigned int name,
      const char* data,
      unsigned short data_length_in_bytes);
  virtual int SetNACKStatus(const int video_channel, const bool enable);
  virtual int SetFECStatus(const int video_channel, const bool enable,
                           const unsigned char payload_typeRED,
                           const unsigned char payload_typeFEC);
  virtual int SetHybridNACKFECStatus(const int video_channel, const bool enable,
                                     const unsigned char payload_typeRED,
                                     const unsigned char payload_typeFEC);
  virtual int SetKeyFrameRequestMethod(const int video_channel,
                                       const ViEKeyFrameRequestMethod method);
  virtual int SetTMMBRStatus(const int video_channel, const bool enable);
  virtual bool SetRembStatus(int video_channel, bool sender, bool receiver);
  virtual int GetReceivedRTCPStatistics(const int video_channel,
                                        unsigned short& fraction_lost,
                                        unsigned int& cumulative_lost,
                                        unsigned int& extended_max,
                                        unsigned int& jitter,
                                        int& rtt_ms) const;
  virtual int GetSentRTCPStatistics(const int video_channel,
                                    unsigned short& fraction_lost,
                                    unsigned int& cumulative_lost,
                                    unsigned int& extended_max,
                                    unsigned int& jitter, int& rtt_ms) const;
  virtual int GetRTPStatistics(const int video_channel,
                               unsigned int& bytes_sent,
                               unsigned int& packets_sent,
                               unsigned int& bytes_received,
                               unsigned int& packets_received) const;
  virtual int GetBandwidthUsage(const int video_channel,
                                unsigned int& total_bitrate_sent,
                                unsigned int& video_bitrate_sent,
                                unsigned int& fec_bitrate_sent,
                                unsigned int& nackBitrateSent) const;
  virtual int SetRTPKeepAliveStatus(
      const int video_channel, bool enable, const char unknown_payload_type,
      const unsigned int delta_transmit_time_seconds);
  virtual int GetRTPKeepAliveStatus(
      const int video_channel,
      bool& enabled,
      char& unkown_payload_type,
      unsigned int& delta_transmit_time_seconds) const;
  virtual int StartRTPDump(const int video_channel,
                           const char file_nameUTF8[1024],
                           RTPDirections direction);
  virtual int StopRTPDump(const int video_channel, RTPDirections direction);
  virtual int RegisterRTPObserver(const int video_channel,
                                  ViERTPObserver& observer);
  virtual int DeregisterRTPObserver(const int video_channel);
  virtual int RegisterRTCPObserver(const int video_channel,
                                   ViERTCPObserver& observer);
  virtual int DeregisterRTCPObserver(const int video_channel);

 protected:
  ViERTP_RTCPImpl();
  virtual ~ViERTP_RTCPImpl();
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_VIE_RTP_RTCP_IMPL_H_
