/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_SENDER_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_SENDER_H_

#include "rtp_rtcp_config.h"           // misc. defines (e.g. MAX_PACKET_LENGTH)
#include "common_types.h"          // Encryption
#include "ssrc_database.h"
#include "list_wrapper.h"
#include "map_wrapper.h"
#include "Bitrate.h"
#include "video_codec_information.h"

#include <cassert>
#include <cmath>

#define MAX_INIT_RTP_SEQ_NUMBER 32767 // 2^15 -1

namespace webrtc {
class CriticalSectionWrapper;
class RTPSenderAudio;
class RTPSenderVideo;

class RTPSenderInterface
{
public:
    RTPSenderInterface() {}
    virtual ~RTPSenderInterface() {}

    virtual WebRtc_UWord32 SSRC() const = 0;
    virtual WebRtc_UWord32 Timestamp() const = 0;

    virtual WebRtc_Word32 BuildRTPheader(WebRtc_UWord8* dataBuffer,
                                       const WebRtc_Word8 payloadType,
                                       const bool markerBit,
                                       const WebRtc_UWord32 captureTimeStamp,
                                       const bool timeStampProvided = true,
                                       const bool incSequenceNumber = true) = 0;

    virtual WebRtc_UWord16 RTPHeaderLength() const = 0;
    virtual WebRtc_UWord16 IncrementSequenceNumber() = 0;
    virtual WebRtc_UWord16 SequenceNumber()   const = 0;
    virtual WebRtc_UWord16 MaxPayloadLength() const = 0;
    virtual WebRtc_UWord16 PacketOverHead() const = 0;
    virtual WebRtc_UWord16 TargetSendBitrateKbit() const = 0;
    virtual WebRtc_UWord16 ActualSendBitrateKbit() const = 0;

    virtual WebRtc_Word32 SendToNetwork(const WebRtc_UWord8* dataBuffer,
                                      const WebRtc_UWord16 payloadLength,
                                      const WebRtc_UWord16 rtpHeaderLength,
                                      const bool dontStore = false) = 0;
};

class RTPSender : public Bitrate, public RTPSenderInterface
{
public:
    RTPSender(const WebRtc_Word32 id, const bool audio);
    virtual ~RTPSender();

    WebRtc_Word32 Init(const WebRtc_UWord32 remoteSSRC);
    void ChangeUniqueId(const WebRtc_Word32 id);

    void ProcessBitrate();

    WebRtc_UWord16 TargetSendBitrateKbit() const;
    WebRtc_UWord16 ActualSendBitrateKbit() const;

    WebRtc_Word32 SetTargetSendBitrate(const WebRtc_UWord32 bits);

    WebRtc_UWord16 MaxDataPayloadLength() const; // with RTP and FEC headers

    // callback
    WebRtc_Word32 RegisterSendTransport(Transport* outgoingTransport);

    WebRtc_Word32 RegisterPayload(const WebRtc_Word8 payloadName[RTP_PAYLOAD_NAME_SIZE],
                                const WebRtc_Word8 payloadType,
                                const WebRtc_UWord32 frequency,
                                 const WebRtc_UWord8 channels,
                                const WebRtc_UWord32 rate);

    WebRtc_Word32 DeRegisterSendPayload(const WebRtc_Word8 payloadType);

    WebRtc_Word8 SendPayloadType() const;

    int SendPayloadFrequency() const;

    void SetSendingStatus(const bool enabled);

    void SetSendingMediaStatus(const bool enabled);
    bool SendingMedia() const;

    // number of sent RTP packets
    WebRtc_UWord32 Packets() const;

    // number of sent RTP bytes
    WebRtc_UWord32 Bytes() const;

    WebRtc_Word32 ResetDataCounters();

    WebRtc_UWord32 StartTimestamp() const;
    WebRtc_Word32 SetStartTimestamp( const WebRtc_UWord32 timestamp, const bool force = false);

    WebRtc_UWord32 GenerateNewSSRC();
    WebRtc_Word32 SetSSRC( const WebRtc_UWord32 ssrc);

    WebRtc_UWord16 SequenceNumber() const;
    WebRtc_Word32 SetSequenceNumber( WebRtc_UWord16 seq);

    WebRtc_Word32 CSRCs(WebRtc_UWord32 arrOfCSRC[kRtpCsrcSize]) const;

    WebRtc_Word32 SetCSRCStatus(const bool include);

    WebRtc_Word32 SetCSRCs(const WebRtc_UWord32 arrOfCSRC[kRtpCsrcSize],
                         const WebRtc_UWord8 arrLength);

    WebRtc_Word32 SetMaxPayloadLength(const WebRtc_UWord16 length,
                                    const WebRtc_UWord16 packetOverHead);

    WebRtc_Word32
    SendOutgoingData(const FrameType frameType,
                     const WebRtc_Word8 payloadType,
                     const WebRtc_UWord32 timeStamp,
                     const WebRtc_UWord8* payloadData,
                     const WebRtc_UWord32 payloadSize,
                     const RTPFragmentationHeader* fragmentation,
                     VideoCodecInformation* codecInfo = NULL,
                     const RTPVideoTypeHeader* rtpTypeHdr = NULL);

    /*
    *    NACK
    */
    void OnReceivedNACK(const WebRtc_UWord16 nackSequenceNumbersLength,
                        const WebRtc_UWord16* nackSequenceNumbers,
                        const WebRtc_UWord16 avgRTT);

    WebRtc_Word32 SetStorePacketsStatus(const bool enable, const WebRtc_UWord16 numberToStore);

    bool StorePackets() const;

    WebRtc_Word32 ReSendToNetwork(WebRtc_UWord16 packetID,
                                WebRtc_UWord32 minResendTime=0);

    bool ProcessNACKBitRate(const WebRtc_UWord32 now);

    void UpdateNACKBitRate( const WebRtc_UWord32 bytes,
                            const WebRtc_UWord32 now);

    /*
    *    Keep alive
    */
    WebRtc_Word32 EnableRTPKeepalive( const WebRtc_Word8 unknownPayloadType,
                                    const WebRtc_UWord16 deltaTransmitTimeMS);

    WebRtc_Word32 RTPKeepaliveStatus(bool* enable,
                                   WebRtc_Word8* unknownPayloadType,
                                   WebRtc_UWord16* deltaTransmitTimeMS) const;

    WebRtc_Word32 DisableRTPKeepalive();

    bool RTPKeepalive() const;

    bool TimeToSendRTPKeepalive() const;

    WebRtc_Word32 SendRTPKeepalivePacket();

    /*
    * Functions wrapping RTPSenderInterface
    */
    virtual WebRtc_Word32 BuildRTPheader(WebRtc_UWord8* dataBuffer,
                                       const WebRtc_Word8 payloadType,
                                       const bool markerBit,
                                       const WebRtc_UWord32 captureTimeStamp,
                                       const bool timeStampProvided = true,
                                       const bool incSequenceNumber = true);

    virtual WebRtc_UWord16 RTPHeaderLength() const ;
    virtual WebRtc_UWord16 IncrementSequenceNumber();
    virtual WebRtc_UWord16 MaxPayloadLength() const;
    virtual WebRtc_UWord16 PacketOverHead() const;

    // current timestamp
    virtual WebRtc_UWord32 Timestamp() const;
    virtual WebRtc_UWord32 SSRC() const;

    virtual WebRtc_Word32 SendToNetwork(const WebRtc_UWord8* dataBuffer,
                                      const WebRtc_UWord16 payloadLength,
                                      const WebRtc_UWord16 rtpHeaderLength,
                                      const bool dontStore = false);

    /*
    *    Audio
    */
    WebRtc_Word32 RegisterAudioCallback(RtpAudioFeedback* messagesCallback);

    // Send a DTMF tone using RFC 2833 (4733)
      WebRtc_Word32 SendTelephoneEvent(const WebRtc_UWord8 key,
                                   const WebRtc_UWord16 time_ms,
                                   const WebRtc_UWord8 level);

    bool SendTelephoneEventActive(WebRtc_Word8& telephoneEvent) const;

    // set audio packet size, used to determine when it's time to send a DTMF packet in silence (CNG)
    WebRtc_Word32 SetAudioPacketSize(const WebRtc_UWord16 packetSizeSamples);

    // Set status and ID for header-extension-for-audio-level-indication.
    WebRtc_Word32 SetAudioLevelIndicationStatus(const bool enable,
                                              const WebRtc_UWord8 ID);

    // Get status and ID for header-extension-for-audio-level-indication.
    WebRtc_Word32 AudioLevelIndicationStatus(bool& enable,
                                           WebRtc_UWord8& ID) const;

    // Store the audio level in dBov for header-extension-for-audio-level-indication.
    WebRtc_Word32 SetAudioLevel(const WebRtc_UWord8 level_dBov);

    // Set payload type for Redundant Audio Data RFC 2198
    WebRtc_Word32 SetRED(const WebRtc_Word8 payloadType);

    // Get payload type for Redundant Audio Data RFC 2198
    WebRtc_Word32 RED(WebRtc_Word8& payloadType) const;

    /*
    *    Video
    */
    VideoCodecInformation* CodecInformationVideo();

    RtpVideoCodecTypes VideoCodecType() const;

    WebRtc_UWord32 MaxConfiguredBitrateVideo() const;

    WebRtc_Word32 SendRTPIntraRequest();

    // FEC
    WebRtc_Word32 SetGenericFECStatus(const bool enable,
                                    const WebRtc_UWord8 payloadTypeRED,
                                    const WebRtc_UWord8 payloadTypeFEC);

    WebRtc_Word32 GenericFECStatus(bool& enable,
                                 WebRtc_UWord8& payloadTypeRED,
                                 WebRtc_UWord8& payloadTypeFEC) const;

    WebRtc_Word32 SetFECCodeRate(const WebRtc_UWord8 keyFrameCodeRate,
                                 const WebRtc_UWord8 deltaFrameCodeRate);

    WebRtc_Word32 SetFECUepProtection(const bool keyUseUepProtection,
                                      const bool deltaUseUepProtection);

protected:
    WebRtc_Word32 CheckPayloadType(const WebRtc_Word8 payloadType, RtpVideoCodecTypes& videoType);

private:
    WebRtc_Word32             _id;
    const bool              _audioConfigured;
    RTPSenderAudio*         _audio;
    RTPSenderVideo*         _video;

    CriticalSectionWrapper&    _sendCritsect;

    CriticalSectionWrapper&    _transportCritsect;
    Transport*         _transport;

    bool                      _sendingMedia;

    WebRtc_UWord16            _maxPayloadLength;
    WebRtc_UWord16            _targetSendBitrate;
    WebRtc_UWord16            _packetOverHead;

    WebRtc_Word8              _payloadType;
    MapWrapper                _payloadTypeMap;

    bool                      _keepAliveIsActive;
    WebRtc_Word8              _keepAlivePayloadType;
    WebRtc_UWord32            _keepAliveLastSent;
    WebRtc_UWord16            _keepAliveDeltaTimeSend;

    bool                      _storeSentPackets;
    WebRtc_UWord16            _storeSentPacketsNumber;
    CriticalSectionWrapper&    _prevSentPacketsCritsect;
    WebRtc_Word32             _prevSentPacketsIndex;
    WebRtc_Word8**            _ptrPrevSentPackets;
    WebRtc_UWord16*           _prevSentPacketsSeqNum;
    WebRtc_UWord16*           _prevSentPacketsLength;
    WebRtc_UWord32*           _prevSentPacketsResendTime;

    // NACK
    WebRtc_UWord32            _nackByteCountTimes[NACK_BYTECOUNT_SIZE];
    WebRtc_Word32             _nackByteCount[NACK_BYTECOUNT_SIZE];

    // statistics
    WebRtc_UWord32            _packetsSent;
    WebRtc_UWord32            _payloadBytesSent;

    // RTP variables
    bool                    _startTimeStampForced;
    WebRtc_UWord32            _startTimeStamp;
    SSRCDatabase&           _ssrcDB;
    WebRtc_UWord32            _remoteSSRC;
    bool                    _sequenceNumberForced;
    WebRtc_UWord16            _sequenceNumber;
    bool                    _ssrcForced;
    WebRtc_UWord32            _ssrc;
    WebRtc_UWord32            _timeStamp;
    WebRtc_UWord8             _CSRCs;
    WebRtc_UWord32            _CSRC[kRtpCsrcSize];
    bool                      _includeCSRCs;
};
} // namespace webrtc

#endif // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_SENDER_H_
