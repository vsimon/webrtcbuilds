#include "../testing/gmock/include/gmock/gmock.h"

#include "modules/interface/module.h"
#include "modules/rtp_rtcp/interface/rtp_rtcp.h"
#include "modules/rtp_rtcp/interface/rtp_rtcp_defines.h"

namespace webrtc {

class MockRtpRtcp : public RtpRtcp {
 public:
  MOCK_METHOD1(ChangeUniqueId,
      WebRtc_Word32(const WebRtc_Word32 id));
  MOCK_METHOD1(RegisterDefaultModule,
      WebRtc_Word32(RtpRtcp* module));
  MOCK_METHOD0(DeRegisterDefaultModule,
      WebRtc_Word32());
  MOCK_METHOD0(DefaultModuleRegistered,
      bool());
  MOCK_METHOD0(NumberChildModules,
      WebRtc_UWord32());
  MOCK_METHOD1(RegisterSyncModule,
      WebRtc_Word32(RtpRtcp* module));
  MOCK_METHOD0(DeRegisterSyncModule,
      WebRtc_Word32());
  MOCK_METHOD0(InitReceiver,
      WebRtc_Word32());
  MOCK_METHOD1(RegisterIncomingDataCallback,
      WebRtc_Word32(RtpData* incomingDataCallback));
  MOCK_METHOD1(RegisterIncomingRTPCallback,
      WebRtc_Word32(RtpFeedback* incomingMessagesCallback));
  MOCK_METHOD2(SetPacketTimeout,
      WebRtc_Word32(const WebRtc_UWord32 RTPtimeoutMS, const WebRtc_UWord32 RTCPtimeoutMS));
  MOCK_METHOD2(SetPeriodicDeadOrAliveStatus,
      WebRtc_Word32(const bool enable, const WebRtc_UWord8 sampleTimeSeconds));
  MOCK_METHOD2(PeriodicDeadOrAliveStatus,
      WebRtc_Word32(bool &enable, WebRtc_UWord8 &sampleTimeSeconds));
  MOCK_METHOD1(RegisterReceivePayload,
      WebRtc_Word32(const CodecInst& voiceCodec));
  MOCK_METHOD1(RegisterReceivePayload,
      WebRtc_Word32(const VideoCodec& videoCodec));
  MOCK_METHOD2(ReceivePayloadType,
      WebRtc_Word32(const CodecInst& voiceCodec, WebRtc_Word8* plType));
  MOCK_METHOD2(ReceivePayloadType,
      WebRtc_Word32(const VideoCodec& videoCodec, WebRtc_Word8* plType));
  MOCK_METHOD1(DeRegisterReceivePayload,
      WebRtc_Word32(const WebRtc_Word8 payloadType));
  MOCK_METHOD2(RegisterReceiveRtpHeaderExtension,
      WebRtc_Word32(const RTPExtensionType type, const WebRtc_UWord8 id));
  MOCK_METHOD1(DeregisterReceiveRtpHeaderExtension,
               WebRtc_Word32(const RTPExtensionType type));
  MOCK_CONST_METHOD0(RemoteTimestamp,
      WebRtc_UWord32());
  MOCK_CONST_METHOD1(EstimatedRemoteTimeStamp,
      WebRtc_Word32(WebRtc_UWord32& timestamp));
  MOCK_CONST_METHOD0(RemoteSSRC,
      WebRtc_UWord32());
  MOCK_CONST_METHOD1(RemoteCSRCs,
      WebRtc_Word32(WebRtc_UWord32 arrOfCSRC[kRtpCsrcSize]));
  MOCK_CONST_METHOD1(SSRCFilter,
      WebRtc_Word32(WebRtc_UWord32& allowedSSRC));
  MOCK_METHOD2(SetSSRCFilter,
      WebRtc_Word32(const bool enable, const WebRtc_UWord32 allowedSSRC));
  MOCK_METHOD2(IncomingPacket,
      WebRtc_Word32(const WebRtc_UWord8* incomingPacket, const WebRtc_UWord16 packetLength));
  MOCK_METHOD4(IncomingAudioNTP,
      WebRtc_Word32(const WebRtc_UWord32 audioReceivedNTPsecs, const WebRtc_UWord32 audioReceivedNTPfrac, const WebRtc_UWord32 audioRTCPArrivalTimeSecs, const WebRtc_UWord32 audioRTCPArrivalTimeFrac));
  MOCK_METHOD0(InitSender,
      WebRtc_Word32());
  MOCK_METHOD1(RegisterSendTransport,
      WebRtc_Word32(Transport* outgoingTransport));
  MOCK_METHOD1(SetMaxTransferUnit,
      WebRtc_Word32(const WebRtc_UWord16 size));
  MOCK_METHOD3(SetTransportOverhead,
      WebRtc_Word32(const bool TCP, const bool IPV6, const WebRtc_UWord8 authenticationOverhead));
  MOCK_CONST_METHOD0(MaxPayloadLength,
      WebRtc_UWord16());
  MOCK_CONST_METHOD0(MaxDataPayloadLength,
      WebRtc_UWord16());
  MOCK_METHOD3(SetRTPKeepaliveStatus,
      WebRtc_Word32(const bool enable, const WebRtc_Word8 unknownPayloadType, const WebRtc_UWord16 deltaTransmitTimeMS));
  MOCK_CONST_METHOD3(RTPKeepaliveStatus,
      WebRtc_Word32(bool* enable, WebRtc_Word8* unknownPayloadType, WebRtc_UWord16* deltaTransmitTimeMS));
  MOCK_CONST_METHOD0(RTPKeepalive,
      bool());
  MOCK_METHOD1(RegisterSendPayload,
      WebRtc_Word32(const CodecInst& voiceCodec));
  MOCK_METHOD1(RegisterSendPayload,
      WebRtc_Word32(const VideoCodec& videoCodec));
  MOCK_METHOD1(DeRegisterSendPayload,
      WebRtc_Word32(const WebRtc_Word8 payloadType));
  MOCK_METHOD2(RegisterSendRtpHeaderExtension,
      WebRtc_Word32(const RTPExtensionType type, const WebRtc_UWord8 id));
  MOCK_METHOD1(DeregisterSendRtpHeaderExtension,
      WebRtc_Word32(const RTPExtensionType type));
  MOCK_CONST_METHOD0(StartTimestamp,
      WebRtc_UWord32());
  MOCK_METHOD1(SetStartTimestamp,
      WebRtc_Word32(const WebRtc_UWord32 timestamp));
  MOCK_CONST_METHOD0(SequenceNumber,
      WebRtc_UWord16());
  MOCK_METHOD1(SetSequenceNumber,
      WebRtc_Word32(const WebRtc_UWord16 seq));
  MOCK_CONST_METHOD0(SSRC,
      WebRtc_UWord32());
  MOCK_METHOD1(SetSSRC,
      WebRtc_Word32(const WebRtc_UWord32 ssrc));
  MOCK_CONST_METHOD1(CSRCs,
      WebRtc_Word32(WebRtc_UWord32 arrOfCSRC[kRtpCsrcSize]));
  MOCK_METHOD2(SetCSRCs,
      WebRtc_Word32(const WebRtc_UWord32 arrOfCSRC[kRtpCsrcSize], const WebRtc_UWord8 arrLength));
  MOCK_METHOD1(SetCSRCStatus,
      WebRtc_Word32(const bool include));
  MOCK_METHOD1(SetSendingStatus,
      WebRtc_Word32(const bool sending));
  MOCK_CONST_METHOD0(Sending,
      bool());
  MOCK_METHOD1(SetSendingMediaStatus,
      WebRtc_Word32(const bool sending));
  MOCK_CONST_METHOD0(SendingMedia,
      bool());
  MOCK_CONST_METHOD4(BitrateSent,
      void(WebRtc_UWord32* totalRate, WebRtc_UWord32* videoRate, WebRtc_UWord32* fecRate, WebRtc_UWord32* nackRate));
  MOCK_METHOD7(SendOutgoingData,
      WebRtc_Word32(const FrameType frameType, const WebRtc_Word8 payloadType, const WebRtc_UWord32 timeStamp, const WebRtc_UWord8* payloadData, const WebRtc_UWord32 payloadSize, const RTPFragmentationHeader* fragmentation, const RTPVideoHeader* rtpVideoHdr));
  MOCK_METHOD1(RegisterIncomingRTCPCallback,
      WebRtc_Word32(RtcpFeedback* incomingMessagesCallback));
  MOCK_CONST_METHOD0(RTCP,
      RTCPMethod());
  MOCK_METHOD1(SetRTCPStatus,
      WebRtc_Word32(const RTCPMethod method));
  MOCK_METHOD1(SetCNAME,
      WebRtc_Word32(const WebRtc_Word8 cName[RTCP_CNAME_SIZE]));
  MOCK_METHOD1(CNAME,
      WebRtc_Word32(WebRtc_Word8 cName[RTCP_CNAME_SIZE]));
  MOCK_CONST_METHOD2(RemoteCNAME,
      WebRtc_Word32(const WebRtc_UWord32 remoteSSRC, WebRtc_Word8 cName[RTCP_CNAME_SIZE]));
  MOCK_CONST_METHOD4(RemoteNTP,
      WebRtc_Word32(WebRtc_UWord32 *ReceivedNTPsecs, WebRtc_UWord32 *ReceivedNTPfrac, WebRtc_UWord32 *RTCPArrivalTimeSecs, WebRtc_UWord32 *RTCPArrivalTimeFrac));
  MOCK_METHOD2(AddMixedCNAME,
      WebRtc_Word32(const WebRtc_UWord32 SSRC, const WebRtc_Word8 cName[RTCP_CNAME_SIZE]));
  MOCK_METHOD1(RemoveMixedCNAME,
      WebRtc_Word32(const WebRtc_UWord32 SSRC));
  MOCK_CONST_METHOD5(RTT,
      WebRtc_Word32(const WebRtc_UWord32 remoteSSRC, WebRtc_UWord16* RTT, WebRtc_UWord16* avgRTT, WebRtc_UWord16* minRTT, WebRtc_UWord16* maxRTT));
  MOCK_METHOD1(ResetRTT,
      WebRtc_Word32(const WebRtc_UWord32 remoteSSRC));
  MOCK_METHOD1(SendRTCP,
      WebRtc_Word32(WebRtc_UWord32 rtcpPacketType));
  MOCK_METHOD1(SendRTCPReferencePictureSelection,
      WebRtc_Word32(const WebRtc_UWord64 pictureID));
  MOCK_METHOD1(SendRTCPSliceLossIndication,
      WebRtc_Word32(const WebRtc_UWord8 pictureID));
  MOCK_METHOD0(ResetStatisticsRTP,
      WebRtc_Word32());
  MOCK_CONST_METHOD5(StatisticsRTP,
      WebRtc_Word32(WebRtc_UWord8 *fraction_lost, WebRtc_UWord32 *cum_lost, WebRtc_UWord32 *ext_max, WebRtc_UWord32 *jitter, WebRtc_UWord32 *max_jitter));
  MOCK_METHOD0(ResetReceiveDataCountersRTP,
      WebRtc_Word32());
  MOCK_METHOD0(ResetSendDataCountersRTP,
      WebRtc_Word32());
  MOCK_CONST_METHOD4(DataCountersRTP,
      WebRtc_Word32(WebRtc_UWord32 *bytesSent, WebRtc_UWord32 *packetsSent, WebRtc_UWord32 *bytesReceived, WebRtc_UWord32 *packetsReceived));
  MOCK_METHOD1(RemoteRTCPStat,
      WebRtc_Word32(RTCPSenderInfo* senderInfo));
  MOCK_METHOD2(RemoteRTCPStat,
      WebRtc_Word32(const WebRtc_UWord32 remoteSSRC, RTCPReportBlock* receiveBlock));
  MOCK_METHOD2(AddRTCPReportBlock,
      WebRtc_Word32(const WebRtc_UWord32 SSRC, const RTCPReportBlock* receiveBlock));
  MOCK_METHOD1(RemoveRTCPReportBlock,
      WebRtc_Word32(const WebRtc_UWord32 SSRC));
  MOCK_METHOD4(SetRTCPApplicationSpecificData,
      WebRtc_Word32(const WebRtc_UWord8 subType, const WebRtc_UWord32 name, const WebRtc_UWord8* data, const WebRtc_UWord16 length));
  MOCK_METHOD1(SetRTCPVoIPMetrics,
      WebRtc_Word32(const RTCPVoIPMetric* VoIPMetric));
  MOCK_CONST_METHOD0(REMB,
      bool());
  MOCK_METHOD1(SetREMBStatus,
      WebRtc_Word32(const bool enable));
  MOCK_METHOD3(SetREMBData,
      WebRtc_Word32(const WebRtc_UWord32 bitrate, const WebRtc_UWord8 numberOfSSRC, const WebRtc_UWord32* SSRC));
  MOCK_METHOD1(SetRemoteBitrateObserver,
      bool(RtpRemoteBitrateObserver*));
  MOCK_CONST_METHOD0(IJ,
      bool());
  MOCK_METHOD1(SetIJStatus,
      WebRtc_Word32(const bool));
  MOCK_CONST_METHOD0(TMMBR,
      bool());
  MOCK_METHOD1(SetTMMBRStatus,
      WebRtc_Word32(const bool enable));
  MOCK_METHOD1(OnBandwidthEstimateUpdate,
      void(WebRtc_UWord16 bandWidthKbit));
  MOCK_CONST_METHOD0(NACK,
      NACKMethod());
  MOCK_METHOD1(SetNACKStatus,
      WebRtc_Word32(const NACKMethod method));
  MOCK_CONST_METHOD0(SelectiveRetransmissions,
      int());
  MOCK_METHOD1(SetSelectiveRetransmissions,
      int(uint8_t settings));
  MOCK_METHOD2(SendNACK,
      WebRtc_Word32(const WebRtc_UWord16* nackList, const WebRtc_UWord16 size));
  MOCK_METHOD2(SetStorePacketsStatus,
      WebRtc_Word32(const bool enable, const WebRtc_UWord16 numberToStore));
  MOCK_METHOD1(RegisterAudioCallback,
      WebRtc_Word32(RtpAudioFeedback* messagesCallback));
  MOCK_METHOD1(SetAudioPacketSize,
      WebRtc_Word32(const WebRtc_UWord16 packetSizeSamples));
  MOCK_METHOD3(SetTelephoneEventStatus,
      WebRtc_Word32(const bool enable, const bool forwardToDecoder, const bool detectEndOfTone));
  MOCK_CONST_METHOD0(TelephoneEvent,
      bool());
  MOCK_CONST_METHOD0(TelephoneEventForwardToDecoder,
      bool());
  MOCK_CONST_METHOD1(SendTelephoneEventActive,
      bool(WebRtc_Word8& telephoneEvent));
  MOCK_METHOD3(SendTelephoneEventOutband,
      WebRtc_Word32(const WebRtc_UWord8 key, const WebRtc_UWord16 time_ms, const WebRtc_UWord8 level));
  MOCK_METHOD1(SetSendREDPayloadType,
      WebRtc_Word32(const WebRtc_Word8 payloadType));
  MOCK_CONST_METHOD1(SendREDPayloadType,
      WebRtc_Word32(WebRtc_Word8& payloadType));
  MOCK_METHOD2(SetRTPAudioLevelIndicationStatus,
      WebRtc_Word32(const bool enable, const WebRtc_UWord8 ID));
  MOCK_CONST_METHOD2(GetRTPAudioLevelIndicationStatus,
      WebRtc_Word32(bool& enable, WebRtc_UWord8& ID));
  MOCK_METHOD1(SetAudioLevel,
      WebRtc_Word32(const WebRtc_UWord8 level_dBov));
  MOCK_METHOD1(RegisterIncomingVideoCallback,
      WebRtc_Word32(RtpVideoFeedback* incomingMessagesCallback));
  MOCK_METHOD1(SetCameraDelay,
      WebRtc_Word32(const WebRtc_Word32 delayMS));
  MOCK_METHOD3(SetSendBitrate,
      WebRtc_Word32(const WebRtc_UWord32 startBitrate, const WebRtc_UWord16 minBitrateKbit, const WebRtc_UWord16 maxBitrateKbit));
  MOCK_METHOD3(SetGenericFECStatus,
      WebRtc_Word32(const bool enable, const WebRtc_UWord8 payloadTypeRED, const WebRtc_UWord8 payloadTypeFEC));
  MOCK_METHOD3(GenericFECStatus,
      WebRtc_Word32(bool& enable, WebRtc_UWord8& payloadTypeRED, WebRtc_UWord8& payloadTypeFEC));
  MOCK_METHOD2(SetFECCodeRate,
      WebRtc_Word32(const WebRtc_UWord8 keyFrameCodeRate, const WebRtc_UWord8 deltaFrameCodeRate));
  MOCK_METHOD2(SetFECUepProtection,
      WebRtc_Word32(const bool keyUseUepProtection, const bool deltaUseUepProtection));
  MOCK_METHOD1(SetKeyFrameRequestMethod,
      WebRtc_Word32(const KeyFrameRequestMethod method));
  MOCK_METHOD1(RequestKeyFrame,
      WebRtc_Word32(const FrameType frameType));
  MOCK_METHOD1(SetH263InverseLogic,
      WebRtc_Word32(const bool enable));

  MOCK_CONST_METHOD3(Version,
      int32_t(char* version, uint32_t& remaining_buffer_in_bytes, uint32_t& position));
  MOCK_METHOD0(TimeUntilNextProcess,
        int32_t());
  MOCK_METHOD0(Process,
        int32_t());

  // Members.
  unsigned int remote_ssrc_;
};

}  // namespace webrtc
