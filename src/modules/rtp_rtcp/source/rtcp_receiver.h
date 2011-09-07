/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_RECEIVER_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_RECEIVER_H_

#include "typedefs.h"
#include "map_wrapper.h"
#include "rtp_utility.h"
#include "rtcp_utility.h"
#include "rtp_rtcp_defines.h"
#include "rtp_rtcp_private.h"
#include "rtcp_receiver_help.h"

namespace webrtc {
class RTCPReceiver
{
public:
    RTCPReceiver(const WebRtc_Word32 id, ModuleRtpRtcpPrivate& callback);
    virtual ~RTCPReceiver();

    void ChangeUniqueId(const WebRtc_Word32 id);

    RTCPMethod Status() const;
    WebRtc_Word32 SetRTCPStatus(const RTCPMethod method);

    WebRtc_UWord32 LastReceived();

    void SetSSRC( const WebRtc_UWord32 ssrc);
    void SetRelaySSRC( const WebRtc_UWord32 ssrc);
    WebRtc_Word32 SetRemoteSSRC( const WebRtc_UWord32 ssrc);

    WebRtc_UWord32 RelaySSRC() const;

    WebRtc_Word32 RegisterIncomingRTCPCallback(RtcpFeedback* incomingMessagesCallback);

    WebRtc_Word32 RegisterIncomingVideoCallback(RtpVideoFeedback* incomingMessagesCallback);

    WebRtc_Word32 IncomingRTCPPacket(RTCPHelp::RTCPPacketInformation& rtcpPacketInformation,
                                   RTCPUtility::RTCPParserV2 *rtcpParser);

    void TriggerCallbacksFromRTCPPacket(RTCPHelp::RTCPPacketInformation& rtcpPacketInformation);

    // get received cname
    WebRtc_Word32 CNAME(const WebRtc_UWord32 remoteSSRC,
                      WebRtc_Word8 cName[RTCP_CNAME_SIZE]) const;

    // get received NTP
    WebRtc_Word32 NTP(WebRtc_UWord32 *ReceivedNTPsecs,
                    WebRtc_UWord32 *ReceivedNTPfrac,
                    WebRtc_UWord32 *RTCPArrivalTimeSecs,
                    WebRtc_UWord32 *RTCPArrivalTimeFrac) const;

    // get rtt
    WebRtc_Word32 RTT(const WebRtc_UWord32 remoteSSRC,
                    WebRtc_UWord16* RTT,
                    WebRtc_UWord16* avgRTT,
                    WebRtc_UWord16* minRTT,
                    WebRtc_UWord16* maxRTT) const;

    WebRtc_Word32 ResetRTT(const WebRtc_UWord32 remoteSSRC);

    void UpdateLipSync(const WebRtc_Word32 audioVideoOffset) const;

    WebRtc_Word32 SenderInfoReceived(RTCPSenderInfo* senderInfo) const;

    void OnReceivedIntraFrameRequest(const WebRtc_UWord8 message) const;
    void OnReceivedSliceLossIndication(const WebRtc_UWord8 pitureID) const;
    void OnReceivedReferencePictureSelectionIndication(const WebRtc_UWord64 pitureID) const;

    // get statistics
    WebRtc_Word32 StatisticsReceived(const WebRtc_UWord32 remoteSSRC,
                                   RTCPReportBlock* receiveBlock) const;
    // Get TMMBR
    WebRtc_Word32 TMMBRReceived(const WebRtc_UWord32 size,
                              const WebRtc_UWord32 accNumCandidates,
                              TMMBRSet* candidateSet) const;

    bool UpdateRTCPReceiveInformationTimers();

    void UpdateBandwidthEstimate(const WebRtc_UWord16 bwEstimateKbit);

    WebRtc_Word32 BoundingSet(bool &tmmbrOwner,
                            TMMBRSet*& boundingSetRec);

    WebRtc_Word32 SetPacketTimeout(const WebRtc_UWord32 timeoutMS);
    void PacketTimeout();

protected:
    RTCPHelp::RTCPReportBlockInformation* CreateReportBlockInformation(const WebRtc_UWord32 remoteSSRC);
    RTCPHelp::RTCPReportBlockInformation* GetReportBlockInformation(const WebRtc_UWord32 remoteSSRC) const;

    RTCPUtility::RTCPCnameInformation* CreateCnameInformation(const WebRtc_UWord32 remoteSSRC);
    RTCPUtility::RTCPCnameInformation* GetCnameInformation(const WebRtc_UWord32 remoteSSRC) const;

    RTCPHelp::RTCPReceiveInformation* CreateReceiveInformation(const WebRtc_UWord32 remoteSSRC);
    RTCPHelp::RTCPReceiveInformation* GetReceiveInformation(const WebRtc_UWord32 remoteSSRC);

    void UpdateReceiveInformation( RTCPHelp::RTCPReceiveInformation& receiveInformation);

    void HandleSenderReceiverReport(RTCPUtility::RTCPParserV2& rtcpParser,
                                    RTCPHelp::RTCPPacketInformation& rtcpPacketInformation);

    void HandleReportBlock(const RTCPUtility::RTCPPacket& rtcpPacket,
                           RTCPHelp::RTCPPacketInformation& rtcpPacketInformation,
                           const WebRtc_UWord32 remoteSSRC,
                           const WebRtc_UWord8 numberOfReportBlocks);

    void HandleSDES(RTCPUtility::RTCPParserV2& rtcpParser);

    void HandleSDESChunk(RTCPUtility::RTCPParserV2& rtcpParser);

    void HandleXRVOIPMetric(RTCPUtility::RTCPParserV2& rtcpParser,
                            RTCPHelp::RTCPPacketInformation& rtcpPacketInformation);

    void HandleNACK(RTCPUtility::RTCPParserV2& rtcpParser,
                    RTCPHelp::RTCPPacketInformation& rtcpPacketInformation);

    void HandleNACKItem(const RTCPUtility::RTCPPacket& rtcpPacket,
                        RTCPHelp::RTCPPacketInformation& rtcpPacketInformation);

    void HandleBYE(RTCPUtility::RTCPParserV2& rtcpParser);

    void HandlePLI(RTCPUtility::RTCPParserV2& rtcpParser,
                   RTCPHelp::RTCPPacketInformation& rtcpPacketInformation);

    void HandleSLI(RTCPUtility::RTCPParserV2& rtcpParser,
                   RTCPHelp::RTCPPacketInformation& rtcpPacketInformation);

    void HandleSLIItem(const RTCPUtility::RTCPPacket& rtcpPacket,
                       RTCPHelp::RTCPPacketInformation& rtcpPacketInformation);

    void HandleRPSI(RTCPUtility::RTCPParserV2& rtcpParser,
                    RTCPHelp::RTCPPacketInformation& rtcpPacketInformation);

    void HandleTMMBR(RTCPUtility::RTCPParserV2& rtcpParser,
                     RTCPHelp::RTCPPacketInformation& rtcpPacketInformation);

    void HandleTMMBRItem(RTCPHelp::RTCPReceiveInformation& receiveInfo,
                         const RTCPUtility::RTCPPacket& rtcpPacket,
                         RTCPHelp::RTCPPacketInformation& rtcpPacketInformation,
                         const WebRtc_UWord32 senderSSRC);

    void HandleTMMBN(RTCPUtility::RTCPParserV2& rtcpParser);

    void HandleSR_REQ(RTCPUtility::RTCPParserV2& rtcpParser,
                      RTCPHelp::RTCPPacketInformation& rtcpPacketInformation);

    void HandleTMMBNItem(RTCPHelp::RTCPReceiveInformation& receiveInfo,
                         const RTCPUtility::RTCPPacket& rtcpPacket);

    void HandleFIR(RTCPUtility::RTCPParserV2& rtcpParser,
                   RTCPHelp::RTCPPacketInformation& rtcpPacketInformation);

    void HandleFIRItem(RTCPHelp::RTCPReceiveInformation& receiveInfo,
                       const RTCPUtility::RTCPPacket& rtcpPacket,
                       RTCPHelp::RTCPPacketInformation& rtcpPacketInformation);

    void HandleAPP(RTCPUtility::RTCPParserV2& rtcpParser,
                   RTCPHelp::RTCPPacketInformation& rtcpPacketInformation);

    void HandleAPPItem(RTCPUtility::RTCPParserV2& rtcpParser,
                       RTCPHelp::RTCPPacketInformation& rtcpPacketInformation);

private:
    WebRtc_Word32             _id;
    RTCPMethod          _method;
    WebRtc_UWord32            _lastReceived;
    ModuleRtpRtcpPrivate&   _cbRtcpPrivate;

    CriticalSectionWrapper&    _criticalSectionFeedbacks;
    RtcpFeedback*       _cbRtcpFeedback;
    RtpVideoFeedback*   _cbVideoFeedback;

    CriticalSectionWrapper&    _criticalSectionRTCPReceiver;
    WebRtc_UWord32            _SSRC;
    WebRtc_UWord32            _remoteSSRC;

    // Received send report
    RTCPSenderInfo      _remoteSenderInfo;
    WebRtc_UWord32            _lastReceivedSRNTPsecs;     // when did we receive the last send report
    WebRtc_UWord32            _lastReceivedSRNTPfrac;

    // Received report block
    MapWrapper                 _receivedReportBlockMap;    // pair SSRC to report block
    MapWrapper                 _receivedInfoMap;           // pair SSRC of sender to might not be a SSRC that have any data (i.e. a conference)
    MapWrapper                 _receivedCnameMap;          // pair SSRC to Cname

    // timeout
    WebRtc_UWord32            _packetTimeOutMS;
};
} // namespace webrtc
#endif // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_RECEIVER_H_
