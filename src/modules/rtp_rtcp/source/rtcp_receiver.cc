/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtcp_receiver.h"

#include <string.h> //memset
#include <cassert> //assert

#include "trace.h"
#include "critical_section_wrapper.h"
#include "rtcp_utility.h"
#include "rtp_rtcp_impl.h"

namespace
{
    const float FRAC = 4.294967296E9;
}

namespace webrtc {
using namespace RTCPUtility;
using namespace RTCPHelp;

RTCPReceiver::RTCPReceiver(const WebRtc_Word32 id,
                           RtpRtcpClock* clock,
                           ModuleRtpRtcpImpl* owner) :
    _id(id),
    _clock(*clock),
    _method(kRtcpOff),
    _lastReceived(0),
    _rtpRtcp(*owner),
    _criticalSectionFeedbacks(CriticalSectionWrapper::CreateCriticalSection()),
    _cbRtcpFeedback(NULL),
    _cbVideoFeedback(NULL),
    _criticalSectionRTCPReceiver(
        CriticalSectionWrapper::CreateCriticalSection()),
    _SSRC(0),
    _remoteSSRC(0),
    _remoteSenderInfo(),
    _lastReceivedSRNTPsecs(0),
    _lastReceivedSRNTPfrac(0),
    _receivedInfoMap(),
    _packetTimeOutMS(0)
{
    memset(&_remoteSenderInfo, 0, sizeof(_remoteSenderInfo));
    WEBRTC_TRACE(kTraceMemory, kTraceRtpRtcp, id, "%s created", __FUNCTION__);
}

RTCPReceiver::~RTCPReceiver()
{
    delete _criticalSectionRTCPReceiver;
    delete _criticalSectionFeedbacks;

    bool loop = true;
    do
    {
        MapItem* item = _receivedReportBlockMap.First();
        if(item)
        {
            // delete
            RTCPReportBlockInformation* block= ((RTCPReportBlockInformation*)item->GetItem());
            delete block;

            // remove from map and delete Item
            _receivedReportBlockMap.Erase(item);
        } else
        {
            loop = false;
        }
    } while (loop);

    loop = true;
    do
    {
        MapItem* item = _receivedInfoMap.First();
        if(item)
        {
            // delete
            RTCPReceiveInformation* block= ((RTCPReceiveInformation*)item->GetItem());
            delete block;

            // remove from map and delete Item
            _receivedInfoMap.Erase(item);
        } else
        {
            loop = false;
        }
    } while (loop);

    loop = true;
    do
    {
        MapItem* item = _receivedCnameMap.First();
        if(item)
        {
            // delete
            RTCPCnameInformation* block= ((RTCPCnameInformation*)item->GetItem());
            delete block;

            // remove from map and delete Item
            _receivedCnameMap.Erase(item);
        } else
        {
            loop = false;
        }
    } while (loop);

    WEBRTC_TRACE(kTraceMemory, kTraceRtpRtcp, _id, "%s deleted", __FUNCTION__);
}

void
RTCPReceiver::ChangeUniqueId(const WebRtc_Word32 id)
{
    _id = id;
}

RTCPMethod
RTCPReceiver::Status() const
{
    CriticalSectionScoped lock(_criticalSectionRTCPReceiver);
    return _method;
}

WebRtc_Word32
RTCPReceiver::SetRTCPStatus(const RTCPMethod method)
{
    CriticalSectionScoped lock(_criticalSectionRTCPReceiver);
    _method = method;
    return 0;
}

WebRtc_UWord32
RTCPReceiver::LastReceived()
{
    CriticalSectionScoped lock(_criticalSectionRTCPReceiver);
    return _lastReceived;
}

WebRtc_Word32
RTCPReceiver::SetRemoteSSRC( const WebRtc_UWord32 ssrc)
{
    CriticalSectionScoped lock(_criticalSectionRTCPReceiver);

    // new SSRC reset old reports
    memset(&_remoteSenderInfo, 0, sizeof(_remoteSenderInfo));
    _lastReceivedSRNTPsecs = 0;
    _lastReceivedSRNTPfrac = 0;

    _remoteSSRC = ssrc;
    return 0;
}

WebRtc_Word32
RTCPReceiver::RegisterIncomingRTCPCallback(RtcpFeedback* incomingMessagesCallback)
{
    CriticalSectionScoped lock(_criticalSectionFeedbacks);
    _cbRtcpFeedback = incomingMessagesCallback;
    return 0;
}

WebRtc_Word32
RTCPReceiver::RegisterIncomingVideoCallback(RtpVideoFeedback* incomingMessagesCallback)
{
    CriticalSectionScoped lock(_criticalSectionFeedbacks);
    _cbVideoFeedback = incomingMessagesCallback;
    return 0;
}

void
RTCPReceiver::SetSSRC( const WebRtc_UWord32 ssrc)
{
    CriticalSectionScoped lock(_criticalSectionRTCPReceiver);
    _SSRC = ssrc;
}

WebRtc_Word32
RTCPReceiver::ResetRTT(const WebRtc_UWord32 remoteSSRC)
{
    CriticalSectionScoped lock(_criticalSectionRTCPReceiver);
    RTCPReportBlockInformation* reportBlock = GetReportBlockInformation(remoteSSRC);
    if(reportBlock == NULL)
    {
        WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,  "\tfailed to GetReportBlockInformation(%d)", remoteSSRC);
        return -1;
    }
    reportBlock->RTT = 0;
    reportBlock->avgRTT = 0;
    reportBlock->minRTT = 0;
    reportBlock->maxRTT = 0;

    return 0;
}

WebRtc_Word32
RTCPReceiver::RTT(const WebRtc_UWord32 remoteSSRC,
                  WebRtc_UWord16* RTT,
                  WebRtc_UWord16* avgRTT,
                  WebRtc_UWord16* minRTT,
                  WebRtc_UWord16* maxRTT) const

{
    CriticalSectionScoped lock(_criticalSectionRTCPReceiver);
    RTCPReportBlockInformation* reportBlock = GetReportBlockInformation(remoteSSRC);
    if(reportBlock == NULL)
    {
        WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,  "\tfailed to GetReportBlockInformation(%d)", remoteSSRC);
        return -1;
    }
    if(RTT)
    {
        *RTT = reportBlock->RTT;
    }
    if(avgRTT)
    {
        *avgRTT = reportBlock->avgRTT;
    }
    if(minRTT)
    {
        *minRTT = reportBlock->minRTT;
    }
    if(maxRTT)
    {
        *maxRTT = reportBlock->maxRTT;
    }
    return 0;
}

void
RTCPReceiver::UpdateLipSync(const WebRtc_Word32 audioVideoOffset) const
{
    CriticalSectionScoped lock(_criticalSectionFeedbacks);
    if(_cbRtcpFeedback)
    {
        _cbRtcpFeedback->OnLipSyncUpdate(_id,audioVideoOffset);
    }
};

WebRtc_Word32
RTCPReceiver::NTP(WebRtc_UWord32 *ReceivedNTPsecs,
                  WebRtc_UWord32 *ReceivedNTPfrac,
                  WebRtc_UWord32 *RTCPArrivalTimeSecs,
                  WebRtc_UWord32 *RTCPArrivalTimeFrac) const
{
    CriticalSectionScoped lock(_criticalSectionRTCPReceiver);
    if(ReceivedNTPsecs)
    {
        *ReceivedNTPsecs = _remoteSenderInfo.NTPseconds; // NTP from incoming SendReport
    }
    if(ReceivedNTPfrac)
    {
        *ReceivedNTPfrac = _remoteSenderInfo.NTPfraction;
    }
    if(RTCPArrivalTimeFrac)
    {
        *RTCPArrivalTimeFrac = _lastReceivedSRNTPfrac; // local NTP time when we received a RTCP packet with a send block
    }
    if(RTCPArrivalTimeSecs)
    {
        *RTCPArrivalTimeSecs = _lastReceivedSRNTPsecs;
    }
    return 0;
}

WebRtc_Word32
RTCPReceiver::SenderInfoReceived(RTCPSenderInfo* senderInfo) const
{
    if(senderInfo == NULL)
    {
        WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id, "%s invalid argument", __FUNCTION__);
        return -1;
    }
    CriticalSectionScoped lock(_criticalSectionRTCPReceiver);
    if(_lastReceivedSRNTPsecs == 0)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, _id, "%s No received SR", __FUNCTION__);
        return -1;
    }
    memcpy(senderInfo, &(_remoteSenderInfo), sizeof(RTCPSenderInfo));
    return 0;
}

// statistics
// we can get multiple receive reports when we receive the report from a CE
WebRtc_Word32
RTCPReceiver::StatisticsReceived(const WebRtc_UWord32 remoteSSRC,
                                 RTCPReportBlock* receiveBlock) const
{
    if(receiveBlock == NULL)
    {
        WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id, "%s invalid argument", __FUNCTION__);
        return -1;
    }
    CriticalSectionScoped lock(_criticalSectionRTCPReceiver);
    RTCPReportBlockInformation* reportBlockInfo = GetReportBlockInformation(remoteSSRC);
    if(reportBlockInfo == NULL)
    {
        WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,  "\tfailed to GetReportBlockInformation(%d)", remoteSSRC);
        return -1;
    }
    memcpy(receiveBlock, &(reportBlockInfo->remoteReceiveBlock), sizeof(RTCPReportBlock));
    return 0;
}

WebRtc_Word32
RTCPReceiver::IncomingRTCPPacket(RTCPPacketInformation& rtcpPacketInformation,
                                 RTCPUtility::RTCPParserV2* rtcpParser)
{
    CriticalSectionScoped lock(_criticalSectionRTCPReceiver);

    _lastReceived = _clock.GetTimeInMS();

    RTCPUtility::RTCPPacketTypes pktType = rtcpParser->Begin();
    while (pktType != RTCPUtility::kRtcpNotValidCode)
    {
        // Each "case" is responsible for iterate the parser to the
        // next top level packet.
        switch (pktType)
        {
        case RTCPUtility::kRtcpSrCode:
        case RTCPUtility::kRtcpRrCode:
            HandleSenderReceiverReport(*rtcpParser, rtcpPacketInformation);
            break;
        case RTCPUtility::kRtcpSdesCode:
            HandleSDES(*rtcpParser);
            break;
        case RTCPUtility::kRtcpXrVoipMetricCode:
            HandleXRVOIPMetric(*rtcpParser, rtcpPacketInformation);
            break;
        case RTCPUtility::kRtcpByeCode:
            HandleBYE(*rtcpParser);
            break;
        case RTCPUtility::kRtcpRtpfbNackCode:
            HandleNACK(*rtcpParser, rtcpPacketInformation);
            break;
        case RTCPUtility::kRtcpRtpfbTmmbrCode:
            HandleTMMBR(*rtcpParser, rtcpPacketInformation);
            break;
        case RTCPUtility::kRtcpRtpfbTmmbnCode:
            HandleTMMBN(*rtcpParser);
            break;
        case RTCPUtility::kRtcpRtpfbSrReqCode:
            HandleSR_REQ(*rtcpParser, rtcpPacketInformation);
            break;
        case RTCPUtility::kRtcpPsfbPliCode:
            HandlePLI(*rtcpParser, rtcpPacketInformation);
            break;
        case RTCPUtility::kRtcpPsfbSliCode:
            HandleSLI(*rtcpParser, rtcpPacketInformation);
            break;
        case RTCPUtility::kRtcpPsfbRpsiCode:
            HandleRPSI(*rtcpParser, rtcpPacketInformation);
            break;
        case RTCPUtility::kRtcpExtendedIjCode:
            HandleIJ(*rtcpParser, rtcpPacketInformation);
            break;
        case RTCPUtility::kRtcpPsfbFirCode:
            HandleFIR(*rtcpParser, rtcpPacketInformation);
            break;
        case RTCPUtility::kRtcpPsfbAppCode:
            HandlePsfbApp(*rtcpParser, rtcpPacketInformation);
            break;
        case RTCPUtility::kRtcpAppCode:
            // generic application messages
            HandleAPP(*rtcpParser, rtcpPacketInformation);
            break;
        case RTCPUtility::kRtcpAppItemCode:
            // generic application messages
            HandleAPPItem(*rtcpParser, rtcpPacketInformation);
            break;
        default:
            rtcpParser->Iterate();
            break;
        }
        pktType = rtcpParser->PacketType();
    }
    return 0;
}

// no need for critsect we have _criticalSectionRTCPReceiver
void
RTCPReceiver::HandleSenderReceiverReport(RTCPUtility::RTCPParserV2& rtcpParser,
                                         RTCPPacketInformation& rtcpPacketInformation)
{
    RTCPUtility::RTCPPacketTypes rtcpPacketType = rtcpParser.PacketType();
    const RTCPUtility::RTCPPacket& rtcpPacket   = rtcpParser.Packet();

    assert((rtcpPacketType == RTCPUtility::kRtcpRrCode) || (rtcpPacketType == RTCPUtility::kRtcpSrCode));

    // SR.SenderSSRC
    // The synchronization source identifier for the originator of this SR packet

    // rtcpPacket.RR.SenderSSRC
    // The source of the packet sender, same as of SR? or is this a CE?

    const WebRtc_UWord32 remoteSSRC = (rtcpPacketType == RTCPUtility::kRtcpRrCode) ? rtcpPacket.RR.SenderSSRC:rtcpPacket.SR.SenderSSRC;
    const WebRtc_UWord8  numberOfReportBlocks = (rtcpPacketType == RTCPUtility::kRtcpRrCode) ? rtcpPacket.RR.NumberOfReportBlocks:rtcpPacket.SR.NumberOfReportBlocks;

    rtcpPacketInformation.remoteSSRC = remoteSSRC;

    RTCPReceiveInformation* ptrReceiveInfo = CreateReceiveInformation(remoteSSRC);
    if (!ptrReceiveInfo)
    {
        rtcpParser.Iterate();
        return;
    }

    if (rtcpPacketType == RTCPUtility::kRtcpSrCode)
    {
        WEBRTC_TRACE(kTraceDebug, kTraceRtpRtcp, _id,
            "Received SR(%d). SSRC:0x%x, from SSRC:0x%x, to us %d.", _id, _SSRC, remoteSSRC, (_remoteSSRC == remoteSSRC)?1:0);

        if (_remoteSSRC == remoteSSRC) // have I received RTP packets from this party
        {
            // only signal that we have received a SR when we accept one
            rtcpPacketInformation.rtcpPacketTypeFlags |= kRtcpSr;

            // We will only store the send report from one source, but
            // we will store all the receive block

            // Save the NTP time of this report
            _remoteSenderInfo.NTPseconds = rtcpPacket.SR.NTPMostSignificant;
            _remoteSenderInfo.NTPfraction = rtcpPacket.SR.NTPLeastSignificant;
            _remoteSenderInfo.RTPtimeStamp = rtcpPacket.SR.RTPTimestamp;
            _remoteSenderInfo.sendPacketCount = rtcpPacket.SR.SenderPacketCount;
            _remoteSenderInfo.sendOctetCount = rtcpPacket.SR.SenderOctetCount;

            _clock.CurrentNTP(_lastReceivedSRNTPsecs, _lastReceivedSRNTPfrac);
        }
        else
        {
            rtcpPacketInformation.rtcpPacketTypeFlags |= kRtcpRr;
        }
    } else
    {
        WEBRTC_TRACE(kTraceDebug, kTraceRtpRtcp, _id,
            "Received RR(%d). SSRC:0x%x, from SSRC:0x%x", _id, _SSRC, remoteSSRC);

        rtcpPacketInformation.rtcpPacketTypeFlags |= kRtcpRr;
    }
    UpdateReceiveInformation(*ptrReceiveInfo);

    rtcpPacketType = rtcpParser.Iterate();

    while (rtcpPacketType == RTCPUtility::kRtcpReportBlockItemCode)
    {
        HandleReportBlock(rtcpPacket, rtcpPacketInformation, remoteSSRC, numberOfReportBlocks);
        rtcpPacketType = rtcpParser.Iterate();
    }
}

// no need for critsect we have _criticalSectionRTCPReceiver
void
RTCPReceiver::HandleReportBlock(const RTCPUtility::RTCPPacket& rtcpPacket,
                                RTCPPacketInformation& rtcpPacketInformation,
                                const WebRtc_UWord32 remoteSSRC,
                                const WebRtc_UWord8 numberOfReportBlocks)
{
    // this will be called once per report block in the RTCP packet
    // we store all incoming reports
    // each packet has max 31 RR blocks
    //
    // we can calc RTT if we send a send report and get a report block back

    /*
        rtcpPacket.ReportBlockItem.SSRC
        The SSRC identifier of the source to which the information in this
        reception report block pertains.
    */

    // if we receive a RTCP packet with multiple reportBlocks only store the ones to us
    if( _SSRC &&
        numberOfReportBlocks > 1)
    {
        // we have more than one reportBlock in the RTCP packet
        if(rtcpPacket.ReportBlockItem.SSRC != _SSRC)
        {
            // this block is not for us ignore it
            return;
        }
    }

    _criticalSectionRTCPReceiver->Leave();
     // to avoid problem with accuireing _criticalSectionRTCPSender while holding _criticalSectionRTCPReceiver

    WebRtc_UWord32 sendTimeMS = 
        _rtpRtcp.SendTimeOfSendReport(rtcpPacket.ReportBlockItem.LastSR);

    _criticalSectionRTCPReceiver->Enter();

    // ReportBlockItem.SSRC is who it's to
    // we store all incoming reports, used in conference relay

    RTCPReportBlockInformation* reportBlock = CreateReportBlockInformation(remoteSSRC);
    if(reportBlock == NULL)
    {
        return;
    }

    reportBlock->remoteReceiveBlock.fractionLost      = rtcpPacket.ReportBlockItem.FractionLost;
    reportBlock->remoteReceiveBlock.cumulativeLost    = rtcpPacket.ReportBlockItem.CumulativeNumOfPacketsLost;
    reportBlock->remoteReceiveBlock.extendedHighSeqNum= rtcpPacket.ReportBlockItem.ExtendedHighestSequenceNumber;
    reportBlock->remoteReceiveBlock.jitter            = rtcpPacket.ReportBlockItem.Jitter;
    reportBlock->remoteReceiveBlock.delaySinceLastSR  = rtcpPacket.ReportBlockItem.DelayLastSR;
    reportBlock->remoteReceiveBlock.lastSR            = rtcpPacket.ReportBlockItem.LastSR;

    if(rtcpPacket.ReportBlockItem.Jitter > reportBlock->remoteMaxJitter)
    {
        reportBlock->remoteMaxJitter = rtcpPacket.ReportBlockItem.Jitter;
    }

    WebRtc_UWord32 delaySinceLastSendReport   = rtcpPacket.ReportBlockItem.DelayLastSR;

    // do we have a local SSRC
    // keep track of our relayed SSRC too
    if(_SSRC)
    {
        // we filter rtcpPacket.ReportBlockItem.SSRC to our SSRC
        // hence only reports to us
        if( rtcpPacket.ReportBlockItem.SSRC == _SSRC)
        {
            // local NTP time when we received this
            WebRtc_UWord32 lastReceivedRRNTPsecs = 0;
            WebRtc_UWord32 lastReceivedRRNTPfrac = 0;

            _clock.CurrentNTP(lastReceivedRRNTPsecs, lastReceivedRRNTPfrac);

            // time when we received this in MS
            WebRtc_UWord32 receiveTimeMS = ModuleRTPUtility::ConvertNTPTimeToMS(lastReceivedRRNTPsecs, lastReceivedRRNTPfrac);

            // Estimate RTT
            WebRtc_UWord32 d =(delaySinceLastSendReport&0x0000ffff)*1000;
            d /= 65536;
            d+=((delaySinceLastSendReport&0xffff0000)>>16)*1000;

            WebRtc_Word32 RTT = 0;

            if(sendTimeMS > 0)
            {
                RTT = receiveTimeMS - d - sendTimeMS;
                if( RTT <= 0)
                {
                    RTT = 1;
                }
                if (RTT > reportBlock->maxRTT)
                {
                    // store max RTT
                    reportBlock->maxRTT = (WebRtc_UWord16)RTT;
                }
                if(reportBlock->minRTT == 0)
                {
                    // first RTT
                    reportBlock->minRTT = (WebRtc_UWord16)RTT;
                }else if (RTT < reportBlock->minRTT)
                {
                    // Store min RTT
                    reportBlock->minRTT = (WebRtc_UWord16)RTT;
                }
                // store last RTT
                reportBlock->RTT = (WebRtc_UWord16)RTT;

                // store average RTT
                if(reportBlock->numAverageCalcs != 0)
                {
                    float ac = static_cast<float>(reportBlock->numAverageCalcs);
                    float newAverage = ((ac / (ac + 1)) * reportBlock->avgRTT) + ((1 / (ac + 1)) * RTT);
                    reportBlock->avgRTT = static_cast<int>(newAverage + 0.5f);
                }else
                {
                    // first RTT
                    reportBlock->avgRTT = (WebRtc_UWord16)RTT;
                }
                reportBlock->numAverageCalcs++;
            }

            WEBRTC_TRACE(kTraceDebug, kTraceRtpRtcp, _id,
                " -> Received report block(%d), from SSRC:0x%x, RTT:%d, loss:%d", _id, remoteSSRC, RTT, rtcpPacket.ReportBlockItem.FractionLost);

            // rtcpPacketInformation
            rtcpPacketInformation.AddReportInfo(reportBlock->remoteReceiveBlock.fractionLost,
                                                (WebRtc_UWord16)RTT,
                                                reportBlock->remoteReceiveBlock.extendedHighSeqNum,
                                                reportBlock->remoteReceiveBlock.jitter);
        }
    }
}

RTCPReportBlockInformation*
RTCPReceiver::CreateReportBlockInformation(WebRtc_UWord32 remoteSSRC)
{
    CriticalSectionScoped lock(_criticalSectionRTCPReceiver);

    RTCPReportBlockInformation* ptrReportBlockInfo = NULL;
    MapItem* ptrReportBlockInfoItem = _receivedReportBlockMap.Find(remoteSSRC);
    if (ptrReportBlockInfoItem == NULL)
    {
        ptrReportBlockInfo = new RTCPReportBlockInformation;
        _receivedReportBlockMap.Insert(remoteSSRC, ptrReportBlockInfo);
    } else
    {
        ptrReportBlockInfo = static_cast<RTCPReportBlockInformation*>(ptrReportBlockInfoItem->GetItem());
    }
    return ptrReportBlockInfo;

}

RTCPReportBlockInformation*
RTCPReceiver::GetReportBlockInformation(WebRtc_UWord32 remoteSSRC) const
{
    CriticalSectionScoped lock(_criticalSectionRTCPReceiver);

    MapItem* ptrReportBlockInfoItem = _receivedReportBlockMap.Find(remoteSSRC);
    if (ptrReportBlockInfoItem == NULL)
    {
        return NULL;
    }
    return static_cast<RTCPReportBlockInformation*>(ptrReportBlockInfoItem->GetItem());
}

RTCPCnameInformation*
RTCPReceiver::CreateCnameInformation(WebRtc_UWord32 remoteSSRC)
{
    CriticalSectionScoped lock(_criticalSectionRTCPReceiver);

    RTCPCnameInformation* ptrCnameInfo = NULL;
    MapItem* ptrCnameInfoItem = _receivedCnameMap.Find(remoteSSRC);
    if (ptrCnameInfoItem == NULL)
    {
        ptrCnameInfo = new RTCPCnameInformation;
        _receivedCnameMap.Insert(remoteSSRC, ptrCnameInfo);
    } else
    {
        ptrCnameInfo = static_cast<RTCPCnameInformation*>(ptrCnameInfoItem->GetItem());
    }
    return ptrCnameInfo;
}

RTCPCnameInformation*
RTCPReceiver::GetCnameInformation(WebRtc_UWord32 remoteSSRC) const
{
    CriticalSectionScoped lock(_criticalSectionRTCPReceiver);

    MapItem* ptrCnameInfoItem = _receivedCnameMap.Find(remoteSSRC);
    if (ptrCnameInfoItem == NULL)
    {
        return NULL;
    }
    return static_cast<RTCPCnameInformation*>(ptrCnameInfoItem->GetItem());
}

RTCPReceiveInformation*
RTCPReceiver::CreateReceiveInformation(WebRtc_UWord32 remoteSSRC)
{
    CriticalSectionScoped lock(_criticalSectionRTCPReceiver);

    RTCPReceiveInformation* ptrReceiveInfo = NULL;
    MapItem* ptrReceiveInfoItem = _receivedInfoMap.Find(remoteSSRC);
    if (ptrReceiveInfoItem == NULL)
    {
        ptrReceiveInfo = new RTCPReceiveInformation;
        _receivedInfoMap.Insert(remoteSSRC, ptrReceiveInfo);
    } else
    {
        ptrReceiveInfo = static_cast<RTCPReceiveInformation*>(ptrReceiveInfoItem->GetItem());
    }
    return ptrReceiveInfo;
}

RTCPReceiveInformation*
RTCPReceiver::GetReceiveInformation(WebRtc_UWord32 remoteSSRC)
{
    CriticalSectionScoped lock(_criticalSectionRTCPReceiver);

    MapItem* ptrReceiveInfoItem = _receivedInfoMap.Find(remoteSSRC);
    if (ptrReceiveInfoItem == NULL)
    {
        return NULL;
    }
    return  static_cast<RTCPReceiveInformation*>(ptrReceiveInfoItem->GetItem());
}

void
RTCPReceiver::UpdateReceiveInformation( RTCPReceiveInformation& receiveInformation)
{
    // Update that this remote is alive
    receiveInformation.lastTimeReceived = _clock.GetTimeInMS();
}

bool
RTCPReceiver::UpdateRTCPReceiveInformationTimers()
{
    CriticalSectionScoped lock(_criticalSectionRTCPReceiver);

    bool updateBoundingSet = false;
    WebRtc_UWord32 timeNow = _clock.GetTimeInMS();
    MapItem* receiveInfoItem=_receivedInfoMap.First();

    while(receiveInfoItem)
    {
        RTCPReceiveInformation* receiveInfo = (RTCPReceiveInformation*)receiveInfoItem->GetItem();
        if(receiveInfo == NULL)
        {
            return updateBoundingSet;
        }
        // time since last received rtcp packet
        // when we dont have a lastTimeReceived and the object is marked readyForDelete
        // it's removed from the map
        if( receiveInfo->lastTimeReceived)
        {
            if((timeNow - receiveInfo->lastTimeReceived) > 5*RTCP_INTERVAL_AUDIO_MS)  // use audio define since we don't know what interval the remote peer is using
            {
                // no rtcp packet for the last five regular intervals, reset limitations
                receiveInfo->TmmbrSet.lengthOfSet = 0;
                receiveInfo->lastTimeReceived = 0; // prevent that we call this over and over again
                updateBoundingSet = true;  // send new TMMBN to all channels using the default codec
            }
            receiveInfoItem = _receivedInfoMap.Next(receiveInfoItem);
        }else
        {
            if(receiveInfo->readyForDelete)
            {
                // store our current receiveInfoItem
                MapItem* receiveInfoItemToBeErased = receiveInfoItem;

                // iterate
                receiveInfoItem = _receivedInfoMap.Next(receiveInfoItem);

                // delete current
                delete receiveInfo;
                _receivedInfoMap.Erase(receiveInfoItemToBeErased);
            }else
            {
                receiveInfoItem = _receivedInfoMap.Next(receiveInfoItem);
            }
        }

    }
    return updateBoundingSet;
}

WebRtc_Word32
RTCPReceiver::BoundingSet(bool &tmmbrOwner, TMMBRSet*& boundingSetRec)
{
    CriticalSectionScoped lock(_criticalSectionRTCPReceiver);

    MapItem* receiveInfoItem=_receivedInfoMap.Find(_remoteSSRC);
    if(receiveInfoItem )
    {
        RTCPReceiveInformation* receiveInfo = (RTCPReceiveInformation*)receiveInfoItem->GetItem();
        if(receiveInfo == NULL)
        {
            WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id, "%s failed to get RTCPReceiveInformation", __FUNCTION__);
            return -1;
        }
        if(receiveInfo->TmmbnBoundingSet.lengthOfSet > 0)
        {
            boundingSetRec->VerifyAndAllocateSet(receiveInfo->TmmbnBoundingSet.lengthOfSet + 1);
            for(WebRtc_UWord32 i=0; i< receiveInfo->TmmbnBoundingSet.lengthOfSet; i++)
            {
                if(receiveInfo->TmmbnBoundingSet.ptrSsrcSet[i] == _SSRC)
                {
                    // owner of bounding set
                    tmmbrOwner = true;
                }
                boundingSetRec->ptrTmmbrSet[i]    = receiveInfo->TmmbnBoundingSet.ptrTmmbrSet[i];
                boundingSetRec->ptrPacketOHSet[i] = receiveInfo->TmmbnBoundingSet.ptrPacketOHSet[i];
                boundingSetRec->ptrSsrcSet[i]     = receiveInfo->TmmbnBoundingSet.ptrSsrcSet[i];
            }
            return receiveInfo->TmmbnBoundingSet.lengthOfSet;
        }
    }
    return -1;
}

// no need for critsect we have _criticalSectionRTCPReceiver
void
RTCPReceiver::HandleSDES(RTCPUtility::RTCPParserV2& rtcpParser)
{
    RTCPUtility::RTCPPacketTypes pktType = rtcpParser.Iterate();
    while (pktType == RTCPUtility::kRtcpSdesChunkCode)
    {
        HandleSDESChunk(rtcpParser);
        pktType = rtcpParser.Iterate();
    }
}

// no need for critsect we have _criticalSectionRTCPReceiver
void
RTCPReceiver::HandleSDESChunk(RTCPUtility::RTCPParserV2& rtcpParser)
{
    const RTCPUtility::RTCPPacket& rtcpPacket = rtcpParser.Packet();

    RTCPCnameInformation* cnameInfo = CreateCnameInformation(rtcpPacket.CName.SenderSSRC);
    if (cnameInfo)
    {
        memcpy(cnameInfo->name, rtcpPacket.CName.CName, rtcpPacket.CName.CNameLength);
        cnameInfo->length = rtcpPacket.CName.CNameLength;
    }
}

// no need for critsect we have _criticalSectionRTCPReceiver
void
RTCPReceiver::HandleNACK(RTCPUtility::RTCPParserV2& rtcpParser,
                         RTCPPacketInformation& rtcpPacketInformation)
{
    const RTCPUtility::RTCPPacket& rtcpPacket = rtcpParser.Packet();

    RTCPReceiveInformation* ptrReceiveInfo = GetReceiveInformation(rtcpPacket.NACK.SenderSSRC);
    if (ptrReceiveInfo == NULL)
    {
        // This remote SSRC must be saved before.
        rtcpParser.Iterate();
        return;
    }
    if (_SSRC != rtcpPacket.NACK.MediaSSRC)
    {
        // Not to us.
        rtcpParser.Iterate();
        return;
    }

    rtcpPacketInformation.ResetNACKPacketIdArray();

    RTCPUtility::RTCPPacketTypes pktType = rtcpParser.Iterate();
    while (pktType == RTCPUtility::kRtcpRtpfbNackItemCode)
    {
        HandleNACKItem(rtcpPacket, rtcpPacketInformation);
        pktType = rtcpParser.Iterate();
    }
}

// no need for critsect we have _criticalSectionRTCPReceiver
void
RTCPReceiver::HandleNACKItem(const RTCPUtility::RTCPPacket& rtcpPacket,
                             RTCPPacketInformation& rtcpPacketInformation)
{
    rtcpPacketInformation.AddNACKPacket(rtcpPacket.NACKItem.PacketID);

    WebRtc_UWord16 bitMask = rtcpPacket.NACKItem.BitMask;
    if(bitMask)
    {
        for(int i=1; i <= 16; ++i)
        {
            if(bitMask & 0x01)
            {
                rtcpPacketInformation.AddNACKPacket(rtcpPacket.NACKItem.PacketID + i);
            }
            bitMask = bitMask >>1;
        }
    }

    rtcpPacketInformation.rtcpPacketTypeFlags |= kRtcpNack;
}

// no need for critsect we have _criticalSectionRTCPReceiver
void
RTCPReceiver::HandleBYE(RTCPUtility::RTCPParserV2& rtcpParser)
{
    const RTCPUtility::RTCPPacket& rtcpPacket = rtcpParser.Packet();

    // clear our lists
    CriticalSectionScoped lock(_criticalSectionRTCPReceiver);

    MapItem* ptrReportBlockInfoItem = _receivedReportBlockMap.Find(rtcpPacket.BYE.SenderSSRC);
    if (ptrReportBlockInfoItem != NULL)
    {
        delete static_cast<RTCPReportBlockInformation*>(ptrReportBlockInfoItem->GetItem());
        _receivedReportBlockMap.Erase(ptrReportBlockInfoItem);
    }
    //  we can't delete it due to TMMBR
    MapItem* ptrReceiveInfoItem = _receivedInfoMap.Find(rtcpPacket.BYE.SenderSSRC);
    if (ptrReceiveInfoItem != NULL)
    {
        static_cast<RTCPReceiveInformation*>(ptrReceiveInfoItem->GetItem())->readyForDelete = true;
    }

    MapItem* ptrCnameInfoItem = _receivedCnameMap.Find(rtcpPacket.BYE.SenderSSRC);
    if (ptrCnameInfoItem != NULL)
    {
        delete static_cast<RTCPCnameInformation*>(ptrCnameInfoItem->GetItem());
        _receivedCnameMap.Erase(ptrCnameInfoItem);
    }
    rtcpParser.Iterate();
}

// no need for critsect we have _criticalSectionRTCPReceiver
void
RTCPReceiver::HandleXRVOIPMetric(RTCPUtility::RTCPParserV2& rtcpParser,
                                 RTCPPacketInformation& rtcpPacketInformation)
{
    const RTCPUtility::RTCPPacket& rtcpPacket = rtcpParser.Packet();

    CriticalSectionScoped lock(_criticalSectionRTCPReceiver);

    if(rtcpPacket.XRVOIPMetricItem.SSRC == _SSRC)
    {
        // Store VoIP metrics block if it's about me
        // from OriginatorSSRC do we filter it?
        // rtcpPacket.XR.OriginatorSSRC;

        RTCPVoIPMetric receivedVoIPMetrics;
        receivedVoIPMetrics.burstDensity = rtcpPacket.XRVOIPMetricItem.burstDensity;
        receivedVoIPMetrics.burstDuration = rtcpPacket.XRVOIPMetricItem.burstDuration;
        receivedVoIPMetrics.discardRate = rtcpPacket.XRVOIPMetricItem.discardRate;
        receivedVoIPMetrics.endSystemDelay = rtcpPacket.XRVOIPMetricItem.endSystemDelay;
        receivedVoIPMetrics.extRfactor = rtcpPacket.XRVOIPMetricItem.extRfactor;
        receivedVoIPMetrics.gapDensity = rtcpPacket.XRVOIPMetricItem.gapDensity;
        receivedVoIPMetrics.gapDuration = rtcpPacket.XRVOIPMetricItem.gapDuration;
        receivedVoIPMetrics.Gmin = rtcpPacket.XRVOIPMetricItem.Gmin;
        receivedVoIPMetrics.JBabsMax = rtcpPacket.XRVOIPMetricItem.JBabsMax;
        receivedVoIPMetrics.JBmax = rtcpPacket.XRVOIPMetricItem.JBmax;
        receivedVoIPMetrics.JBnominal = rtcpPacket.XRVOIPMetricItem.JBnominal;
        receivedVoIPMetrics.lossRate = rtcpPacket.XRVOIPMetricItem.lossRate;
        receivedVoIPMetrics.MOSCQ = rtcpPacket.XRVOIPMetricItem.MOSCQ;
        receivedVoIPMetrics.MOSLQ = rtcpPacket.XRVOIPMetricItem.MOSLQ;
        receivedVoIPMetrics.noiseLevel = rtcpPacket.XRVOIPMetricItem.noiseLevel;
        receivedVoIPMetrics.RERL = rtcpPacket.XRVOIPMetricItem.RERL;
        receivedVoIPMetrics.Rfactor = rtcpPacket.XRVOIPMetricItem.Rfactor;
        receivedVoIPMetrics.roundTripDelay = rtcpPacket.XRVOIPMetricItem.roundTripDelay;
        receivedVoIPMetrics.RXconfig = rtcpPacket.XRVOIPMetricItem.RXconfig;
        receivedVoIPMetrics.signalLevel = rtcpPacket.XRVOIPMetricItem.signalLevel;

        rtcpPacketInformation.AddVoIPMetric(&receivedVoIPMetrics);

        rtcpPacketInformation.rtcpPacketTypeFlags |= kRtcpXrVoipMetric; // received signal
    }
    rtcpParser.Iterate();
}

// no need for critsect we have _criticalSectionRTCPReceiver
void
RTCPReceiver::HandlePLI(RTCPUtility::RTCPParserV2& rtcpParser,
                        RTCPPacketInformation& rtcpPacketInformation)
{
    const RTCPUtility::RTCPPacket& rtcpPacket = rtcpParser.Packet();

    RTCPReceiveInformation* ptrReceiveInfo = GetReceiveInformation(rtcpPacket.PLI.SenderSSRC);
    if (ptrReceiveInfo == NULL)
    {
        // This remote SSRC must be saved before.
        rtcpParser.Iterate();
        return;
    }
    if (_SSRC != rtcpPacket.PLI.MediaSSRC)
    {
        // Not to us.
        rtcpParser.Iterate();
        return;
    }
    rtcpPacketInformation.rtcpPacketTypeFlags |= kRtcpPli; // received signal that we need to send a new key frame
    rtcpParser.Iterate();
}

// no need for critsect we have _criticalSectionRTCPReceiver
void
RTCPReceiver::HandleTMMBR(RTCPUtility::RTCPParserV2& rtcpParser,
                          RTCPPacketInformation& rtcpPacketInformation)
{
    const RTCPUtility::RTCPPacket& rtcpPacket = rtcpParser.Packet();

    WebRtc_UWord32 senderSSRC = rtcpPacket.TMMBR.SenderSSRC;
    RTCPReceiveInformation* ptrReceiveInfo = GetReceiveInformation(senderSSRC);
    if (ptrReceiveInfo == NULL)
    {
        // This remote SSRC must be saved before.
        rtcpParser.Iterate();
        return;
    }
    if(rtcpPacket.TMMBR.MediaSSRC)
    {
        // rtcpPacket.TMMBR.MediaSSRC SHOULD be 0 if same as SenderSSRC
        // in relay mode this is a valid number
        senderSSRC = rtcpPacket.TMMBR.MediaSSRC;
    }

    // Use packet length to calc max number of TMMBR blocks
    // each TMMBR block is 8 bytes
    ptrdiff_t maxNumOfTMMBRBlocks = rtcpParser.LengthLeft() / 8;

    // sanity
    if(maxNumOfTMMBRBlocks > 200) // we can't have more than what's in one packet
    {
        assert(false);
        rtcpParser.Iterate();
        return;
    }
    ptrReceiveInfo->VerifyAndAllocateTMMBRSet((WebRtc_UWord32)maxNumOfTMMBRBlocks);

    RTCPUtility::RTCPPacketTypes pktType = rtcpParser.Iterate();
    while (pktType == RTCPUtility::kRtcpRtpfbTmmbrItemCode)
    {
        HandleTMMBRItem(*ptrReceiveInfo, rtcpPacket, rtcpPacketInformation, senderSSRC);
        pktType = rtcpParser.Iterate();
    }
}

// no need for critsect we have _criticalSectionRTCPReceiver
void
RTCPReceiver::HandleTMMBRItem(RTCPReceiveInformation& receiveInfo,
                              const RTCPUtility::RTCPPacket& rtcpPacket,
                              RTCPPacketInformation& rtcpPacketInformation,
                              const WebRtc_UWord32 senderSSRC)
{
    if (_SSRC == rtcpPacket.TMMBRItem.SSRC &&
        rtcpPacket.TMMBRItem.MaxTotalMediaBitRate > 0)
    {
        receiveInfo.InsertTMMBRItem(senderSSRC, rtcpPacket.TMMBRItem,
                                    _clock.GetTimeInMS());
        rtcpPacketInformation.rtcpPacketTypeFlags |= kRtcpTmmbr;
    }
}

// no need for critsect we have _criticalSectionRTCPReceiver
void
RTCPReceiver::HandleTMMBN(RTCPUtility::RTCPParserV2& rtcpParser)
{
    const RTCPUtility::RTCPPacket& rtcpPacket = rtcpParser.Packet();
    RTCPReceiveInformation* ptrReceiveInfo = GetReceiveInformation(rtcpPacket.TMMBN.SenderSSRC);
    if (ptrReceiveInfo == NULL)
    {
        // This remote SSRC must be saved before.
        rtcpParser.Iterate();
        return;
    }
    // Use packet length to calc max number of TMMBN blocks
    // each TMMBN block is 8 bytes
    ptrdiff_t maxNumOfTMMBNBlocks = rtcpParser.LengthLeft() / 8;

    // sanity
    if(maxNumOfTMMBNBlocks > 200) // we cant have more than what's in one packet
    {
        assert(false);
        rtcpParser.Iterate();
        return;
    }

    ptrReceiveInfo->VerifyAndAllocateBoundingSet((WebRtc_UWord32)maxNumOfTMMBNBlocks);

    RTCPUtility::RTCPPacketTypes pktType = rtcpParser.Iterate();
    while (pktType == RTCPUtility::kRtcpRtpfbTmmbnItemCode)
    {
        HandleTMMBNItem(*ptrReceiveInfo, rtcpPacket);
        pktType = rtcpParser.Iterate();
    }
}

// no need for critsect we have _criticalSectionRTCPReceiver
void
RTCPReceiver::HandleSR_REQ(RTCPUtility::RTCPParserV2& rtcpParser,
                           RTCPPacketInformation& rtcpPacketInformation)
{
    rtcpPacketInformation.rtcpPacketTypeFlags |= kRtcpSrReq;
    rtcpParser.Iterate();
}

// no need for critsect we have _criticalSectionRTCPReceiver
void
RTCPReceiver::HandleTMMBNItem(RTCPReceiveInformation& receiveInfo,
                              const RTCPUtility::RTCPPacket& rtcpPacket)
{
    const unsigned int idx = receiveInfo.TmmbnBoundingSet.lengthOfSet;

    receiveInfo.TmmbnBoundingSet.ptrTmmbrSet[idx]    = rtcpPacket.TMMBNItem.MaxTotalMediaBitRate;
    receiveInfo.TmmbnBoundingSet.ptrPacketOHSet[idx] = rtcpPacket.TMMBNItem.MeasuredOverhead;
    receiveInfo.TmmbnBoundingSet.ptrSsrcSet[idx]     = rtcpPacket.TMMBNItem.SSRC;

    ++receiveInfo.TmmbnBoundingSet.lengthOfSet;
}

// no need for critsect we have _criticalSectionRTCPReceiver
void
RTCPReceiver::HandleSLI(RTCPUtility::RTCPParserV2& rtcpParser,
                        RTCPPacketInformation& rtcpPacketInformation)
{
    const RTCPUtility::RTCPPacket& rtcpPacket = rtcpParser.Packet();

    RTCPReceiveInformation* ptrReceiveInfo = GetReceiveInformation(rtcpPacket.SLI.SenderSSRC);
    if (ptrReceiveInfo == NULL)
    {
        // This remote SSRC must be saved before.
        rtcpParser.Iterate();
        return;
    }

    RTCPUtility::RTCPPacketTypes pktType = rtcpParser.Iterate();
    while (pktType == RTCPUtility::kRtcpPsfbSliItemCode)
    {
        HandleSLIItem(rtcpPacket, rtcpPacketInformation);
        pktType = rtcpParser.Iterate();
    }
}

// no need for critsect we have _criticalSectionRTCPReceiver
void
RTCPReceiver::HandleSLIItem(const RTCPUtility::RTCPPacket& rtcpPacket,
                            RTCPPacketInformation& rtcpPacketInformation)
{
    // in theory there could be multiple slices lost
    rtcpPacketInformation.rtcpPacketTypeFlags |= kRtcpSli; // received signal that we need to refresh a slice
    rtcpPacketInformation.sliPictureId = rtcpPacket.SLIItem.PictureId;
}

void
RTCPReceiver::HandleRPSI(RTCPUtility::RTCPParserV2& rtcpParser,
                         RTCPHelp::RTCPPacketInformation& rtcpPacketInformation)
{
    const RTCPUtility::RTCPPacket& rtcpPacket = rtcpParser.Packet();

    RTCPReceiveInformation* ptrReceiveInfo = GetReceiveInformation(rtcpPacket.RPSI.SenderSSRC);
    if (ptrReceiveInfo == NULL)
    {
        // This remote SSRC must be saved before.
        rtcpParser.Iterate();
        return;
    }
    RTCPUtility::RTCPPacketTypes pktType = rtcpParser.Iterate();
    if(pktType == RTCPUtility::kRtcpPsfbRpsiCode)
    {
        rtcpPacketInformation.rtcpPacketTypeFlags |= kRtcpRpsi; // received signal that we have a confirmed reference picture
        if(rtcpPacket.RPSI.NumberOfValidBits%8 != 0)
        {
            // to us unknown
            // continue
            rtcpParser.Iterate();
            return;
        }
        rtcpPacketInformation.rpsiPictureId = 0;

        // convert NativeBitString to rpsiPictureId
        WebRtc_UWord8 numberOfBytes = rtcpPacket.RPSI.NumberOfValidBits /8;
        for(WebRtc_UWord8 n = 0; n < (numberOfBytes-1); n++)
        {
            rtcpPacketInformation.rpsiPictureId += (rtcpPacket.RPSI.NativeBitString[n] & 0x7f);
            rtcpPacketInformation.rpsiPictureId <<= 7; // prepare next
        }
        rtcpPacketInformation.rpsiPictureId += (rtcpPacket.RPSI.NativeBitString[numberOfBytes-1] & 0x7f);
    }
}

// no need for critsect we have _criticalSectionRTCPReceiver
void
RTCPReceiver::HandlePsfbApp(RTCPUtility::RTCPParserV2& rtcpParser,
                            RTCPPacketInformation& rtcpPacketInformation)
{
    RTCPUtility::RTCPPacketTypes pktType = rtcpParser.Iterate();
    if (pktType == RTCPUtility::kRtcpPsfbRembItemCode)
    {
        HandleREMBItem(rtcpParser, rtcpPacketInformation);
    }
}

// no need for critsect we have _criticalSectionRTCPReceiver
void
RTCPReceiver::HandleIJ(RTCPUtility::RTCPParserV2& rtcpParser,
                       RTCPPacketInformation& rtcpPacketInformation)
{
    const RTCPUtility::RTCPPacket& rtcpPacket = rtcpParser.Packet();

    RTCPUtility::RTCPPacketTypes pktType = rtcpParser.Iterate();
    while (pktType == RTCPUtility::kRtcpExtendedIjItemCode)
    {
        HandleIJItem(rtcpPacket, rtcpPacketInformation);
        pktType = rtcpParser.Iterate();
    }
}

void
RTCPReceiver::HandleIJItem(const RTCPUtility::RTCPPacket& rtcpPacket,
                           RTCPPacketInformation& rtcpPacketInformation)
{
    rtcpPacketInformation.rtcpPacketTypeFlags |= kRtcpTransmissionTimeOffset;
    rtcpPacketInformation.interArrivalJitter =
    rtcpPacket.ExtendedJitterReportItem.Jitter;
}

void
RTCPReceiver::HandleREMBItem(RTCPUtility::RTCPParserV2& rtcpParser,
                             RTCPPacketInformation& rtcpPacketInformation)
{
    rtcpParser.Iterate();
    const RTCPUtility::RTCPPacket& rtcpPacket = rtcpParser.Packet();

    rtcpPacketInformation.rtcpPacketTypeFlags |= kRtcpRemb;
    rtcpPacketInformation.receiverEstimatedMaxBitrate = rtcpPacket.REMB.BitRate;
    // TODO(pwestin) send up SSRCs and do a sanity check
}

// no need for critsect we have _criticalSectionRTCPReceiver
void
RTCPReceiver::HandleFIR(RTCPUtility::RTCPParserV2& rtcpParser,
                        RTCPPacketInformation& rtcpPacketInformation)
{
    const RTCPUtility::RTCPPacket& rtcpPacket = rtcpParser.Packet();

    RTCPReceiveInformation* ptrReceiveInfo = GetReceiveInformation(rtcpPacket.FIR.SenderSSRC);
    if (ptrReceiveInfo == NULL)
    {
        // This remote SSRC must be saved before.
        rtcpParser.Iterate();
        return;
    }

    RTCPUtility::RTCPPacketTypes pktType = rtcpParser.Iterate();
    while (pktType == RTCPUtility::kRtcpPsfbFirItemCode)
    {
        HandleFIRItem(*ptrReceiveInfo, rtcpPacket, rtcpPacketInformation);
        pktType = rtcpParser.Iterate();
    }
}

// no need for critsect we have _criticalSectionRTCPReceiver
void
RTCPReceiver::HandleFIRItem(RTCPReceiveInformation& receiveInfo,
                            const RTCPUtility::RTCPPacket& rtcpPacket,
                            RTCPPacketInformation& rtcpPacketInformation)
{
    if (_SSRC == rtcpPacket.FIRItem.SSRC) // is it our sender that is requested to generate a new keyframe
    {
        // rtcpPacket.FIR.MediaSSRC SHOULD be 0 but we ignore to check it
        // we don't know who this originate from

        // check if we have reported this FIRSequenceNumber before
        if (rtcpPacket.FIRItem.CommandSequenceNumber != receiveInfo.lastFIRSequenceNumber)
        {
            //
            WebRtc_UWord32 now = _clock.GetTimeInMS();

            // extra sanity don't go crazy with the callbacks
            if( (now - receiveInfo.lastFIRRequest) > RTCP_MIN_FRAME_LENGTH_MS)
            {
                receiveInfo.lastFIRRequest = now;
                receiveInfo.lastFIRSequenceNumber = rtcpPacket.FIRItem.CommandSequenceNumber;

                rtcpPacketInformation.rtcpPacketTypeFlags |= kRtcpFir; // received signal that we need to send a new key frame
            }
        }
    }
}

void
RTCPReceiver::HandleAPP(RTCPUtility::RTCPParserV2& rtcpParser,
                        RTCPPacketInformation& rtcpPacketInformation)
{
    const RTCPUtility::RTCPPacket& rtcpPacket = rtcpParser.Packet();

    rtcpPacketInformation.rtcpPacketTypeFlags |= kRtcpApp;
    rtcpPacketInformation.applicationSubType = rtcpPacket.APP.SubType;
    rtcpPacketInformation.applicationName = rtcpPacket.APP.Name;

    rtcpParser.Iterate();
}

void
RTCPReceiver::HandleAPPItem(RTCPUtility::RTCPParserV2& rtcpParser,
                           RTCPPacketInformation& rtcpPacketInformation)
{
    const RTCPUtility::RTCPPacket& rtcpPacket = rtcpParser.Packet();

    rtcpPacketInformation.AddApplicationData(rtcpPacket.APP.Data, rtcpPacket.APP.Size);

    rtcpParser.Iterate();
}

void
RTCPReceiver::OnReceivedIntraFrameRequest(const FrameType frameType,
                                          const WebRtc_UWord8 streamIdx) const
{
    CriticalSectionScoped lock(_criticalSectionFeedbacks);

    if(_cbVideoFeedback)
    {
        _cbVideoFeedback->OnReceivedIntraFrameRequest(_id, frameType, streamIdx);
    }
}

void
RTCPReceiver::OnReceivedSliceLossIndication(const WebRtc_UWord8 pitureID) const
{
    CriticalSectionScoped lock(_criticalSectionFeedbacks);

    if(_cbRtcpFeedback)
    {
        _cbRtcpFeedback->OnSLIReceived(_id, pitureID);
    }
}

void
RTCPReceiver::OnReceivedReferencePictureSelectionIndication(const WebRtc_UWord64 pitureID) const
{
    CriticalSectionScoped lock(_criticalSectionFeedbacks);

    if(_cbRtcpFeedback)
    {
        _cbRtcpFeedback->OnRPSIReceived(_id, pitureID);
    }
}

// Holding no Critical section
void
RTCPReceiver::TriggerCallbacksFromRTCPPacket(RTCPPacketInformation& rtcpPacketInformation)
{
    // callback if SR or RR
    if (rtcpPacketInformation.rtcpPacketTypeFlags & kRtcpSr ||
        rtcpPacketInformation.rtcpPacketTypeFlags & kRtcpRr)
    {
        if(rtcpPacketInformation.reportBlock)
        {
            // We only want to trigger one OnNetworkChanged callback per RTCP
            // packet. The callback is triggered by a SR, RR and TMMBR, so we
            // don't want to trigger one from here if the packet also contains a
            // TMMBR block.
            bool triggerCallback =
                !(rtcpPacketInformation.rtcpPacketTypeFlags & kRtcpTmmbr);
            _rtpRtcp.OnPacketLossStatisticsUpdate(
                rtcpPacketInformation.fractionLost,
                rtcpPacketInformation.roundTripTime,
                rtcpPacketInformation.lastReceivedExtendedHighSeqNum,
                triggerCallback);
        }
    }
    if (rtcpPacketInformation.rtcpPacketTypeFlags & kRtcpSr)
    {
        _rtpRtcp.OnReceivedNTP();
    }
    if (rtcpPacketInformation.rtcpPacketTypeFlags & kRtcpSrReq)
    {
        _rtpRtcp.OnRequestSendReport();
    }
    if (rtcpPacketInformation.rtcpPacketTypeFlags & kRtcpNack)
    {
        if (rtcpPacketInformation.nackSequenceNumbersLength > 0)
        {
            WEBRTC_TRACE(kTraceStateInfo, kTraceRtpRtcp, _id, "SIG [RTCP] Incoming NACK to id:%d", _id);
            _rtpRtcp.OnReceivedNACK(rtcpPacketInformation.nackSequenceNumbersLength,
                                          rtcpPacketInformation.nackSequenceNumbers);
        }
    }
    if (rtcpPacketInformation.rtcpPacketTypeFlags & kRtcpTmmbr)
    {
        WEBRTC_TRACE(kTraceStateInfo, kTraceRtpRtcp, _id, "SIG [RTCP] Incoming TMMBR to id:%d", _id);

        // might trigger a OnReceivedBandwidthEstimateUpdate
        _rtpRtcp.OnReceivedTMMBR();
    }
    if ((rtcpPacketInformation.rtcpPacketTypeFlags & kRtcpPli) ||
        (rtcpPacketInformation.rtcpPacketTypeFlags & kRtcpFir))
    {
        if (rtcpPacketInformation.rtcpPacketTypeFlags & kRtcpPli)
        {
            WEBRTC_TRACE(kTraceStateInfo, kTraceRtpRtcp, _id, "SIG [RTCP] Incoming PLI to id:%d", _id);
        }else
        {
            WEBRTC_TRACE(kTraceStateInfo, kTraceRtpRtcp, _id, "SIG [RTCP] Incoming FIR to id:%d", _id);
        }
        _rtpRtcp.OnReceivedIntraFrameRequest(&_rtpRtcp);
    }
    if (rtcpPacketInformation.rtcpPacketTypeFlags & kRtcpSli)
    {
         // we need use a bounce it up to handle default channel
        _rtpRtcp.OnReceivedSliceLossIndication(
            rtcpPacketInformation.sliPictureId);
    }
    if (rtcpPacketInformation.rtcpPacketTypeFlags & kRtcpRemb)
    {
       // We need to bounce this to the default channel
        _rtpRtcp.OnReceivedEstimatedMaxBitrate(
            rtcpPacketInformation.receiverEstimatedMaxBitrate);
    }
    if (rtcpPacketInformation.rtcpPacketTypeFlags & kRtcpRpsi)
    {
         // we need use a bounce it up to handle default channel
        _rtpRtcp.OnReceivedReferencePictureSelectionIndication(
            rtcpPacketInformation.rpsiPictureId);
    }
    {
        CriticalSectionScoped lock(_criticalSectionFeedbacks);

        // we need a feedback that we have received a report block(s) so that we can generate a new packet
        // in a conference relay scenario, one received report can generate several RTCP packets, based
        // on number relayed/mixed
        // a send report block should go out to all receivers
        if(_cbRtcpFeedback)
        {
            if(rtcpPacketInformation.rtcpPacketTypeFlags & kRtcpSr)
            {
                _cbRtcpFeedback->OnSendReportReceived(_id, rtcpPacketInformation.remoteSSRC);
            } else
            {
                _cbRtcpFeedback->OnReceiveReportReceived(_id, rtcpPacketInformation.remoteSSRC);
            }
            if (rtcpPacketInformation.rtcpPacketTypeFlags & kRtcpRemb)
            {
                _cbRtcpFeedback->OnReceiverEstimatedMaxBitrateReceived(_id,
                    rtcpPacketInformation.receiverEstimatedMaxBitrate);
            }
            if(rtcpPacketInformation.rtcpPacketTypeFlags & kRtcpXrVoipMetric)
            {
                WebRtc_Word8 VoIPmetricBuffer[7*4];
                VoIPmetricBuffer[0] = rtcpPacketInformation.VoIPMetric->lossRate;
                VoIPmetricBuffer[1] = rtcpPacketInformation.VoIPMetric->discardRate;
                VoIPmetricBuffer[2] = rtcpPacketInformation.VoIPMetric->burstDensity;
                VoIPmetricBuffer[3] = rtcpPacketInformation.VoIPMetric->gapDensity;

                VoIPmetricBuffer[4] = (WebRtc_UWord8)(rtcpPacketInformation.VoIPMetric->burstDuration >> 8);
                VoIPmetricBuffer[5] = (WebRtc_UWord8)(rtcpPacketInformation.VoIPMetric->burstDuration);
                VoIPmetricBuffer[6] = (WebRtc_UWord8)(rtcpPacketInformation.VoIPMetric->gapDuration >> 8);
                VoIPmetricBuffer[7] = (WebRtc_UWord8)(rtcpPacketInformation.VoIPMetric->gapDuration);

                VoIPmetricBuffer[8] = (WebRtc_UWord8)(rtcpPacketInformation.VoIPMetric->roundTripDelay >> 8);
                VoIPmetricBuffer[9] = (WebRtc_UWord8)(rtcpPacketInformation.VoIPMetric->roundTripDelay);
                VoIPmetricBuffer[10] = (WebRtc_UWord8)(rtcpPacketInformation.VoIPMetric->endSystemDelay >> 8);
                VoIPmetricBuffer[11] = (WebRtc_UWord8)(rtcpPacketInformation.VoIPMetric->endSystemDelay);

                VoIPmetricBuffer[12] = rtcpPacketInformation.VoIPMetric->signalLevel;
                VoIPmetricBuffer[13] = rtcpPacketInformation.VoIPMetric->noiseLevel;
                VoIPmetricBuffer[14] = rtcpPacketInformation.VoIPMetric->RERL;
                VoIPmetricBuffer[15] = rtcpPacketInformation.VoIPMetric->Gmin;

                VoIPmetricBuffer[16] = rtcpPacketInformation.VoIPMetric->Rfactor;
                VoIPmetricBuffer[17] = rtcpPacketInformation.VoIPMetric->extRfactor;
                VoIPmetricBuffer[18] = rtcpPacketInformation.VoIPMetric->MOSLQ;
                VoIPmetricBuffer[19] = rtcpPacketInformation.VoIPMetric->MOSCQ;

                VoIPmetricBuffer[20] = rtcpPacketInformation.VoIPMetric->RXconfig;
                VoIPmetricBuffer[21] = 0; // reserved
                VoIPmetricBuffer[22] = (WebRtc_UWord8)(rtcpPacketInformation.VoIPMetric->JBnominal >> 8);
                VoIPmetricBuffer[23] = (WebRtc_UWord8)(rtcpPacketInformation.VoIPMetric->JBnominal);

                VoIPmetricBuffer[24] = (WebRtc_UWord8)(rtcpPacketInformation.VoIPMetric->JBmax >> 8);
                VoIPmetricBuffer[25] = (WebRtc_UWord8)(rtcpPacketInformation.VoIPMetric->JBmax);
                VoIPmetricBuffer[26] = (WebRtc_UWord8)(rtcpPacketInformation.VoIPMetric->JBabsMax >> 8);
                VoIPmetricBuffer[27] = (WebRtc_UWord8)(rtcpPacketInformation.VoIPMetric->JBabsMax);

                _cbRtcpFeedback->OnXRVoIPMetricReceived(_id, rtcpPacketInformation.VoIPMetric, VoIPmetricBuffer);
            }
            if(rtcpPacketInformation.rtcpPacketTypeFlags & kRtcpApp)
            {
                _cbRtcpFeedback->OnApplicationDataReceived(_id,
                                                           rtcpPacketInformation.applicationSubType,
                                                           rtcpPacketInformation.applicationName,
                                                           rtcpPacketInformation.applicationLength,
                                                           rtcpPacketInformation.applicationData);
            }
        }
    }
}

void
RTCPReceiver::UpdateBandwidthEstimate(const WebRtc_UWord16 bwEstimateKbit)
{
    CriticalSectionScoped lock(_criticalSectionFeedbacks);

    if(_cbRtcpFeedback)
    {
        _cbRtcpFeedback->OnTMMBRReceived(_id, bwEstimateKbit);
    }

}

WebRtc_Word32
RTCPReceiver::CNAME(const WebRtc_UWord32 remoteSSRC,
                    WebRtc_Word8 cName[RTCP_CNAME_SIZE]) const
{
    if(cName == NULL)
    {
        WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id, "%s invalid argument", __FUNCTION__);
        return -1;
    }

    CriticalSectionScoped lock(_criticalSectionRTCPReceiver);

    RTCPCnameInformation* cnameInfo = GetCnameInformation(remoteSSRC);
    if(cnameInfo == NULL)
    {
        WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,  "\tfailed to GetCnameInformation(%d)", remoteSSRC);
        return -1;
    }
    memcpy(cName, cnameInfo->name, cnameInfo->length);
    cName[cnameInfo->length] = 0;
    return 0;
}

// no callbacks allowed inside this function
WebRtc_Word32
RTCPReceiver::TMMBRReceived(const WebRtc_UWord32 size,
                            const WebRtc_UWord32 accNumCandidates,
                            TMMBRSet* candidateSet) const
{
    CriticalSectionScoped lock(_criticalSectionRTCPReceiver);

    MapItem* receiveInfoItem=_receivedInfoMap.First();
    if(receiveInfoItem == NULL)
    {
        return -1;
    }
    WebRtc_UWord32 num = accNumCandidates;
    if(candidateSet)
    {
        while( num < size && receiveInfoItem)
        {
            RTCPReceiveInformation* receiveInfo = (RTCPReceiveInformation*)receiveInfoItem->GetItem();
            if(receiveInfo == NULL)
            {
                return 0;
            }
            for (WebRtc_UWord32 i = 0; (num < size) && (i < receiveInfo->TmmbrSet.lengthOfSet); i++)
            {
                if(receiveInfo->GetTMMBRSet(i, num, candidateSet,
                                            _clock.GetTimeInMS()) == 0)
                {
                    num++;
                }
            }
            receiveInfoItem = _receivedInfoMap.Next(receiveInfoItem);
        }
    } else
    {
        while(receiveInfoItem)
        {
            RTCPReceiveInformation* receiveInfo = (RTCPReceiveInformation*)receiveInfoItem->GetItem();
            if(receiveInfo == NULL)
            {
                WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id, "%s failed to get RTCPReceiveInformation", __FUNCTION__);
                return -1;
            }
            num += receiveInfo->TmmbrSet.lengthOfSet;

            receiveInfoItem = _receivedInfoMap.Next(receiveInfoItem);
        }
    }
    return num;
}

WebRtc_Word32
RTCPReceiver::SetPacketTimeout(const WebRtc_UWord32 timeoutMS)
{
    CriticalSectionScoped lock(_criticalSectionRTCPReceiver);
    _packetTimeOutMS = timeoutMS;
    return 0;
}

void RTCPReceiver::PacketTimeout()
{
    if(_packetTimeOutMS == 0)
    {
        // not configured
        return;
    }

    bool packetTimeOut = false;
    {
        CriticalSectionScoped lock(_criticalSectionRTCPReceiver);
        if(_lastReceived == 0)
        {
            // not active
            return;
        }

        WebRtc_UWord32 now = _clock.GetTimeInMS();

        if(now - _lastReceived > _packetTimeOutMS)
        {
            packetTimeOut = true;
            _lastReceived = 0;  // only one callback
        }
    }
    CriticalSectionScoped lock(_criticalSectionFeedbacks);
    if(packetTimeOut && _cbRtcpFeedback)
    {
        _cbRtcpFeedback->OnRTCPPacketTimeout(_id);
    }
}
} // namespace webrtc
