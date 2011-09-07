/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "bandwidth_management.h"
#include "trace.h"
#include "rtp_utility.h"
#include "rtp_rtcp_config.h"

#include <math.h>   // sqrt()

namespace webrtc {
BandwidthManagement::BandwidthManagement(const WebRtc_Word32 id) :
    _id(id),

    _critsect(*CriticalSectionWrapper::CreateCriticalSection()),

    _lastPacketLossExtendedHighSeqNum(0),
    _lastReportAllLost(false),
    _lastLoss(0),
    _accumulateLostPacketsQ8(0),
    _accumulateExpectedPackets(0),

    _bitRate(0),
    _minBitRateConfigured(0),
    _maxBitRateConfigured(0),

    _last_fraction_loss(0),
    _last_round_trip_time(0),

    // bandwidth estimate
    _bwEstimateIncoming(0),
    _bwEstimateIncomingMax(0),
    _smoothedFractionLostQ4(-1), // indicate uninitialized
    _sFLFactorQ4(14)             // 0.875 in Q4
{
}

BandwidthManagement::~BandwidthManagement()
{
    delete &_critsect;
}

WebRtc_Word32
BandwidthManagement::SetSendBitrate(const WebRtc_UWord32 startBitrate,
                                    const WebRtc_UWord16 minBitrateKbit,
                                    const WebRtc_UWord16 maxBitrateKbit)
{
    CriticalSectionScoped cs(_critsect);

    _bitRate = startBitrate;
    _minBitRateConfigured = minBitrateKbit*1000;
    if(maxBitrateKbit == 0)
    {
        // no max configured use 1Gbit/s
        _maxBitRateConfigured = 1000000000;
    } else
    {
        _maxBitRateConfigured = maxBitrateKbit*1000;
    }
    return 0;
}

WebRtc_Word32
BandwidthManagement::MaxConfiguredBitrate(WebRtc_UWord16& maxBitrateKbit)
{
    CriticalSectionScoped cs(_critsect);

    if(_maxBitRateConfigured == 0)
    {
        return -1;
    }
    maxBitrateKbit = (WebRtc_UWord16)(_maxBitRateConfigured/1000);
    return 0;
}

WebRtc_Word32
BandwidthManagement::UpdateBandwidthEstimate(const WebRtc_UWord16 bandWidthMinKbit,
                                             const WebRtc_UWord16 bandWidthMaxKbit,
                                             WebRtc_UWord32& newBitrate,
                                             WebRtc_UWord8& fractionLost,
                                             WebRtc_UWord16& roundTripTime)
{
    newBitrate = 0;
    CriticalSectionScoped cs(_critsect);

    _bwEstimateIncoming = bandWidthMinKbit*1000;
    _bwEstimateIncomingMax = bandWidthMaxKbit*1000;

    if(_bitRate == 0)
    {
        // BandwidthManagement off
        return -1;
    }

    if (_bwEstimateIncoming > 0 && _bitRate > _bwEstimateIncoming)
    {
        _bitRate   = _bwEstimateIncoming;
    }
    else
    {
        return -1;
    }
    newBitrate = _bitRate;
    fractionLost = _last_fraction_loss;
    roundTripTime = _last_round_trip_time;
    return 0;
}

WebRtc_Word32
BandwidthManagement::UpdatePacketLoss(const WebRtc_UWord32 lastReceivedExtendedHighSeqNum,
                                      const bool defaultCodec,
                                      const WebRtc_UWord8 lossInput,
                                      const WebRtc_UWord16 rtt,
                                      WebRtc_UWord32& newBitrate,
                                      WebRtc_UWord16& bwEstimateKbitMin,
                                      WebRtc_UWord16& bwEstimateKbitMax)
{
    CriticalSectionScoped cs(_critsect);

    WebRtc_UWord8 loss = lossInput; // Local copy to modify.

    _last_fraction_loss = loss;
    _last_round_trip_time = rtt;

    if(_bitRate == 0)
    {
        // BandwidthManagement off
        return -1;
    }

    // Check sequence number diff and weight loss report
    if (_lastPacketLossExtendedHighSeqNum > 0 &&
        (lastReceivedExtendedHighSeqNum >= _lastPacketLossExtendedHighSeqNum))
    {
        // This is not the first loss report and the sequence number is
        // non-decreasing. Calculate sequence number diff.
        WebRtc_UWord32 seqNumDiff = lastReceivedExtendedHighSeqNum
            - _lastPacketLossExtendedHighSeqNum;

        // Check if this report and the last was 100% loss, then report
        // 100% loss even though seqNumDiff is small.
        // If not, go on with the checks.
        if (!(_lastReportAllLost && loss == 255))
        {
            _lastReportAllLost = (loss == 255);

            // Calculate number of lost packets.
            // loss = 256 * numLostPackets / expectedPackets.
            const int numLostPacketsQ8 = loss * seqNumDiff;

            // Accumulate reports.
            _accumulateLostPacketsQ8 += numLostPacketsQ8;
            _accumulateExpectedPackets += seqNumDiff;

            // Report loss if the total report is based on sufficiently
            // many packets.
            const int limitNumPackets = 10;
            if (_accumulateExpectedPackets >= limitNumPackets)
            {
                loss = _accumulateLostPacketsQ8 / _accumulateExpectedPackets;

                // Reset accumulators
                _accumulateLostPacketsQ8 = 0;
                _accumulateExpectedPackets = 0;
            }
            else
            {
                // Report same loss as before and keep the accumulators until
                // the next report.
                loss = _lastLoss;
            }
        }
    }

    // Keep for next time.
    _lastLoss = loss;

    // Remember the sequence number until next time
    _lastPacketLossExtendedHighSeqNum = lastReceivedExtendedHighSeqNum;

    bwEstimateKbitMax = static_cast<WebRtc_UWord16>(_bwEstimateIncomingMax / 1000);
    bwEstimateKbitMin = static_cast<WebRtc_UWord16>(_bwEstimateIncoming / 1000);

    newBitrate = 0;

    if (defaultCodec)
    {
        return 0;
    }
    WebRtc_UWord32 bitRate = ShapeSimple(loss, rtt);
    if(bitRate == 0)
    {
        // no change
        return -1;
    }
    _bitRate = bitRate;
    newBitrate = bitRate;
    return 0;
}

/* Calculate the rate that TCP-Friendly Rate Control (TFRC) would apply.
 * The formula in RFC 3448, Section 3.1, is used.
 */

// protected
WebRtc_Word32
BandwidthManagement::CalcTFRCbps(WebRtc_Word16 avgPackSizeBytes, WebRtc_Word32 rttMs, WebRtc_Word32 packetLoss)
{
    if (avgPackSizeBytes <= 0 || rttMs <= 0 || packetLoss <= 0)
    {
        // input variables out of range; return -1
        return -1;
    }

    double R = static_cast<double>(rttMs)/1000; // RTT in seconds
    int b = 1; // number of packets acknowledged by a single TCP acknowledgement; recommended = 1
    double t_RTO = 4.0 * R; // TCP retransmission timeout value in seconds; recommended = 4*R
    double p = static_cast<double>(packetLoss)/255; // packet loss rate in [0, 1)
    double s = static_cast<double>(avgPackSizeBytes);

    // calculate send rate in bytes/second
    double X = s / (R * sqrt(2 * b * p / 3) + (t_RTO * (3 * sqrt( 3 * b * p / 8) * p * (1 + 32 * p * p))));

    return (static_cast<WebRtc_Word32>(X*8)); // bits/second
}

/*
*  Simple bandwidth estimation. Depends a lot on bwEstimateIncoming and packetLoss.
*/
// protected
WebRtc_UWord32
BandwidthManagement::ShapeSimple(WebRtc_Word32 packetLoss, WebRtc_Word32 rtt)
{
    WebRtc_UWord32 newBitRate = 0;
    bool reducing = false;

    if (packetLoss > 5 && packetLoss <= 26)
    {
        // 2% - 10%
        newBitRate = _bitRate;
    }
    else if (packetLoss > 26)
    {
        // 26/256 ~= 10%
        // reduce rate: newRate = rate * (1 - 0.5*lossRate)
        // packetLoss = 256*lossRate
        newBitRate = (_bitRate * (512 - packetLoss)) / 512;
        reducing = true;
    }
    else
    {
        // increase rate by 5%
        newBitRate = static_cast<WebRtc_UWord32>(_bitRate * 1.05 + 0.5);

        // add 1 kbps extra, just to make sure that we do not get stuck
        // (gives a little extra increase at low rates, negligible at higher rates)
        newBitRate += 1000;
    }

    // Calculate smoothed loss number
    if (_smoothedFractionLostQ4 < 0)
    {
        // startup
        _smoothedFractionLostQ4 = static_cast<WebRtc_UWord16>(packetLoss);
    }
    else
    {
        _smoothedFractionLostQ4 = ((_sFLFactorQ4 * _smoothedFractionLostQ4 + 8) >> 4) // Q4*Q4 = Q8; down to Q4 again with proper rounding
            + (16 - _sFLFactorQ4) * static_cast<WebRtc_UWord16>(packetLoss);  // Q4 * Q0 = Q4
    }

    // Calculate what rate TFRC would apply in this situation
    //WebRtc_Word32 tfrcRate = CalcTFRCbps(1000, rtt, _smoothedFractionLostQ4 >> 4); // scale loss to Q0 (back to [0, 255])
    WebRtc_Word32 tfrcRate = CalcTFRCbps(1000, rtt, packetLoss); // scale loss to Q0 (back to [0, 255])

    if (reducing &&
        tfrcRate > 0 &&
        static_cast<WebRtc_UWord32>(tfrcRate) > newBitRate)
    {
        // do not reduce further if rate is below TFRC rate
        newBitRate = _bitRate;
    }

    if (_bwEstimateIncoming > 0 && newBitRate > _bwEstimateIncoming)
    {
        newBitRate = _bwEstimateIncoming;
    }

    if (newBitRate > _maxBitRateConfigured)
    {
        newBitRate = _maxBitRateConfigured;
    }

    if (newBitRate < _minBitRateConfigured)
    {
        newBitRate = _minBitRateConfigured;
    }

    return newBitRate;
}

} // namespace webrtc
