/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "NETEQTEST_DummyRTPpacket.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#ifdef WIN32
#include <winsock2.h>
#else
#include <netinet/in.h> // for htons, htonl, etc
#endif

int NETEQTEST_DummyRTPpacket::readFromFile(FILE *fp)
{
    if (!fp)
    {
        return -1;
    }

    WebRtc_UWord16 length, plen;
    WebRtc_UWord32 offset;

    if (fread(&length, 2, 1, fp) == 0)
    {
        reset();
        return -2;
    }
    length = ntohs(length);

    if (fread(&plen, 2, 1, fp) == 0)
    {
        reset();
        return -1;
    }
    int packetLen = ntohs(plen);

    if (fread(&offset, 4, 1, fp) == 0)
    {
        reset();
        return -1;
    }
    // Store in local variable until we have passed the reset below.
    WebRtc_UWord32 receiveTime = ntohl(offset);

    // Use length here because a plen of 0 specifies rtcp.
    length = (WebRtc_UWord16) (length - _kRDHeaderLen);

    // check buffer size
    if (_datagram && _memSize < length)
    {
        reset();
    }

    if (!_datagram)
    {
        _datagram = new WebRtc_UWord8[length];
        _memSize = length;
    }
    memset(_datagram, 0, length);

    if (length == 0)
    {
        _datagramLen = 0;
        _rtpParsed = false;
        return packetLen;
    }

    // Read basic header
    if (fread((unsigned short *) _datagram, 1, _kBasicHeaderLen, fp)
        != (size_t)_kBasicHeaderLen)
    {
        reset();
        return -1;
    }
    _receiveTime = receiveTime;
    _datagramLen = _kBasicHeaderLen;

    // Parse the basic header
    webrtc::WebRtcRTPHeader tempRTPinfo;
    int P, X, CC;
    parseBasicHeader(&tempRTPinfo, &P, &X, &CC);

    // Check if we have to extend the header
    if (X != 0 || CC != 0)
    {
        int newLen = _kBasicHeaderLen + CC * 4 + X * 4;
        assert(_memSize >= newLen);

        // Read extension from file
        size_t readLen = newLen - _kBasicHeaderLen;
        if (fread((unsigned short *) _datagram + _kBasicHeaderLen, 1, readLen,
            fp) != readLen)
        {
            reset();
            return -1;
        }
        _datagramLen = newLen;

        if (X != 0)
        {
            int totHdrLen = calcHeaderLength(X, CC);
            assert(_memSize >= totHdrLen);

            // Read extension from file
            size_t readLen = totHdrLen - newLen;
            if (fread((unsigned short *) _datagram + newLen, 1, readLen, fp)
                != readLen)
            {
                reset();
                return -1;
            }
            _datagramLen = totHdrLen;
        }
    }
    _datagramLen = length;

    if (!_blockList.empty() && _blockList.count(payloadType()) > 0)
    {
        // discard this payload
        return readFromFile(fp);
    }

    _rtpParsed = false;
    return packetLen;

}

int NETEQTEST_DummyRTPpacket::writeToFile(FILE *fp)
{
    if (!fp)
    {
        return -1;
    }

    WebRtc_UWord16 length, plen;
    WebRtc_UWord32 offset;

    // length including RTPplay header
    length = htons(_datagramLen + _kRDHeaderLen);
    if (fwrite(&length, 2, 1, fp) != 1)
    {
        return -1;
    }

    // payload length
    plen = htons(_datagramLen);
    if (fwrite(&plen, 2, 1, fp) != 1)
    {
        return -1;
    }

    // offset (=receive time)
    offset = htonl(_receiveTime);
    if (fwrite(&offset, 4, 1, fp) != 1)
    {
        return -1;
    }

    // Figure out the length of the RTP header.
    int headerLen;
    if (_datagramLen == 0)
    {
        // No payload at all; we are done writing to file.
        headerLen = 0;
    }
    else
    {
        parseHeader();
        headerLen = _payloadPtr - _datagram;
        assert(headerLen >= 0);
    }

    // write RTP header
    if (fwrite((unsigned short *) _datagram, 1, headerLen, fp) !=
        static_cast<size_t>(headerLen))
    {
        return -1;
    }

    return (headerLen + _kRDHeaderLen); // total number of bytes written

}

