/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "RTPFile.h"

#include <stdlib.h>

#ifdef WIN32
#   include <Winsock2.h>
#else
#   include <arpa/inet.h>
#endif

#include "audio_coding_module.h"
#include "engine_configurations.h"
#include "gtest/gtest.h" // TODO (tlegrand): Consider removing usage of gtest.
#include "rw_lock_wrapper.h"

namespace webrtc {

void RTPStream::ParseRTPHeader(WebRtcRTPHeader* rtpInfo, const WebRtc_UWord8* rtpHeader)
{
    rtpInfo->header.payloadType = rtpHeader[1];
    rtpInfo->header.sequenceNumber = (static_cast<WebRtc_UWord16>(rtpHeader[2])<<8) | rtpHeader[3];
    rtpInfo->header.timestamp = (static_cast<WebRtc_UWord32>(rtpHeader[4])<<24) |
                         (static_cast<WebRtc_UWord32>(rtpHeader[5])<<16) |
                         (static_cast<WebRtc_UWord32>(rtpHeader[6])<<8) |
                         rtpHeader[7];
    rtpInfo->header.ssrc = (static_cast<WebRtc_UWord32>(rtpHeader[8])<<24) |
                    (static_cast<WebRtc_UWord32>(rtpHeader[9])<<16) |
                    (static_cast<WebRtc_UWord32>(rtpHeader[10])<<8) |
                    rtpHeader[11];
}

void RTPStream::MakeRTPheader(WebRtc_UWord8* rtpHeader, 
                              WebRtc_UWord8 payloadType, WebRtc_Word16 seqNo,
                              WebRtc_UWord32 timeStamp, WebRtc_UWord32 ssrc)
{
    rtpHeader[0]=(unsigned char)0x80;
    rtpHeader[1]=(unsigned char)(payloadType & 0xFF);
    rtpHeader[2]=(unsigned char)((seqNo>>8)&0xFF);
    rtpHeader[3]=(unsigned char)((seqNo)&0xFF);
    rtpHeader[4]=(unsigned char)((timeStamp>>24)&0xFF);
    rtpHeader[5]=(unsigned char)((timeStamp>>16)&0xFF);

    rtpHeader[6]=(unsigned char)((timeStamp>>8)&0xFF); 
    rtpHeader[7]=(unsigned char)(timeStamp & 0xFF);

    rtpHeader[8]=(unsigned char)((ssrc>>24)&0xFF);
    rtpHeader[9]=(unsigned char)((ssrc>>16)&0xFF);

    rtpHeader[10]=(unsigned char)((ssrc>>8)&0xFF);
    rtpHeader[11]=(unsigned char)(ssrc & 0xFF);
}


RTPPacket::RTPPacket(WebRtc_UWord8 payloadType, WebRtc_UWord32 timeStamp,
                                    WebRtc_Word16 seqNo, const WebRtc_UWord8* payloadData,
                                    WebRtc_UWord16 payloadSize, WebRtc_UWord32 frequency)
                                    :
payloadType(payloadType),
timeStamp(timeStamp),
seqNo(seqNo),
payloadSize(payloadSize),
frequency(frequency)
{
    if (payloadSize > 0)
    {
        this->payloadData = new WebRtc_UWord8[payloadSize];
        memcpy(this->payloadData, payloadData, payloadSize);
    }
}

RTPPacket::~RTPPacket()
{
    delete [] payloadData;
}

RTPBuffer::RTPBuffer()
{
    _queueRWLock = RWLockWrapper::CreateRWLock();
}

RTPBuffer::~RTPBuffer()
{
    delete _queueRWLock;
}

void
RTPBuffer::Write(const WebRtc_UWord8 payloadType, const WebRtc_UWord32 timeStamp,
                                    const WebRtc_Word16 seqNo, const WebRtc_UWord8* payloadData,
                                    const WebRtc_UWord16 payloadSize, WebRtc_UWord32 frequency)
{
    RTPPacket *packet = new RTPPacket(payloadType, timeStamp, seqNo, payloadData, payloadSize, frequency);
    _queueRWLock->AcquireLockExclusive();
    _rtpQueue.push(packet);
    _queueRWLock->ReleaseLockExclusive();
}

WebRtc_UWord16
RTPBuffer::Read(WebRtcRTPHeader* rtpInfo,
                WebRtc_UWord8* payloadData,
                WebRtc_UWord16 payloadSize,
                WebRtc_UWord32* offset)
{
    _queueRWLock->AcquireLockShared();
    RTPPacket *packet = _rtpQueue.front();
    _rtpQueue.pop();
    _queueRWLock->ReleaseLockShared();
    rtpInfo->header.markerBit = 1;
    rtpInfo->header.payloadType = packet->payloadType;
    rtpInfo->header.sequenceNumber = packet->seqNo;
    rtpInfo->header.ssrc = 0;
    rtpInfo->header.timestamp = packet->timeStamp;
    if (packet->payloadSize > 0 && payloadSize >= packet->payloadSize)
    {
        memcpy(payloadData, packet->payloadData, packet->payloadSize);
    }
    else
    {
        return 0;
    }
    *offset = (packet->timeStamp/(packet->frequency/1000));

    return packet->payloadSize;
}

bool
RTPBuffer::EndOfFile() const
{
    _queueRWLock->AcquireLockShared();
    bool eof = _rtpQueue.empty();
    _queueRWLock->ReleaseLockShared();
    return eof;
}

void RTPFile::Open(const char *filename, const char *mode)
{
    if ((_rtpFile = fopen(filename, mode)) == NULL)
    {
        printf("Cannot write file %s.\n", filename);
        ADD_FAILURE() << "Unable to write file";
        exit(1);
    }
}

void RTPFile::Close()
{
    if (_rtpFile != NULL)
    {
        fclose(_rtpFile);
        _rtpFile = NULL;
    }
}


void RTPFile::WriteHeader()
{
    // Write data in a format that NetEQ and RTP Play can parse
    fprintf(_rtpFile, "#!RTPencode%s\n", "1.0");
    WebRtc_UWord32 dummy_variable = 0;
    // should be converted to network endian format, but does not matter when 0
    if (fwrite(&dummy_variable, 4, 1, _rtpFile) != 1) {
      return;
    }
    if (fwrite(&dummy_variable, 4, 1, _rtpFile) != 1) {
      return;
    }
    if (fwrite(&dummy_variable, 4, 1, _rtpFile) != 1) {
      return;
    }
    if (fwrite(&dummy_variable, 2, 1, _rtpFile) != 1) {
      return;
    }
    if (fwrite(&dummy_variable, 2, 1, _rtpFile) != 1) {
      return;
    }
    fflush(_rtpFile);
}

void RTPFile::ReadHeader()
{
    WebRtc_UWord32 start_sec, start_usec, source;
    WebRtc_UWord16 port, padding;
    char fileHeader[40];
    EXPECT_TRUE(fgets(fileHeader, 40, _rtpFile) != 0);
    EXPECT_EQ(1u, fread(&start_sec, 4, 1, _rtpFile));
    start_sec=ntohl(start_sec);
    EXPECT_EQ(1u, fread(&start_usec, 4, 1, _rtpFile));
    start_usec=ntohl(start_usec);
    EXPECT_EQ(1u, fread(&source, 4, 1, _rtpFile));
    source=ntohl(source);
    EXPECT_EQ(1u, fread(&port, 2, 1, _rtpFile));
    port=ntohs(port);
    EXPECT_EQ(1u, fread(&padding, 2, 1, _rtpFile));
    padding=ntohs(padding);
}

void RTPFile::Write(const WebRtc_UWord8 payloadType, const WebRtc_UWord32 timeStamp,
                    const WebRtc_Word16 seqNo, const WebRtc_UWord8* payloadData,
                    const WebRtc_UWord16 payloadSize, WebRtc_UWord32 frequency)
{
    /* write RTP packet to file */
    WebRtc_UWord8 rtpHeader[12];
    MakeRTPheader(rtpHeader, payloadType, seqNo, timeStamp, 0);
    WebRtc_UWord16 lengthBytes = htons(12 + payloadSize + 8);
    WebRtc_UWord16 plen = htons(12 + payloadSize);
    WebRtc_UWord32 offsetMs;

    offsetMs = (timeStamp/(frequency/1000));
    offsetMs = htonl(offsetMs);
    if (fwrite(&lengthBytes, 2, 1, _rtpFile) != 1) {
      return;
    }
    if (fwrite(&plen, 2, 1, _rtpFile) != 1) {
      return;
    }
    if (fwrite(&offsetMs, 4, 1, _rtpFile) != 1) {
      return;
    }
    if (fwrite(rtpHeader, 12, 1, _rtpFile) != 1) {
      return;
    }
    if (fwrite(payloadData, 1, payloadSize, _rtpFile) != payloadSize) {
      return;
    }
}

WebRtc_UWord16 RTPFile::Read(WebRtcRTPHeader* rtpInfo,
                   WebRtc_UWord8* payloadData,
                   WebRtc_UWord16 payloadSize,
                   WebRtc_UWord32* offset)
{
    WebRtc_UWord16 lengthBytes;
    WebRtc_UWord16 plen;
    WebRtc_UWord8 rtpHeader[12];
    size_t read_len = fread(&lengthBytes, 2, 1, _rtpFile);
    /* Check if we have reached end of file. */
    if ((read_len == 0) && feof(_rtpFile))
    {
        _rtpEOF = true;
        return 0;
    }
    EXPECT_EQ(1u, fread(&plen, 2, 1, _rtpFile));
    EXPECT_EQ(1u, fread(offset, 4, 1, _rtpFile));
    lengthBytes = ntohs(lengthBytes);
    plen = ntohs(plen);
    *offset = ntohl(*offset);
    EXPECT_GT(plen, 11);

    EXPECT_EQ(1u, fread(rtpHeader, 12, 1, _rtpFile));
    ParseRTPHeader(rtpInfo, rtpHeader);
    rtpInfo->type.Audio.isCNG = false;
    rtpInfo->type.Audio.channel = 1;
    EXPECT_EQ(lengthBytes, plen + 8);

    if (plen == 0)
    {
        return 0;
    }
    if (payloadSize < (lengthBytes - 20))
    {
      return -1;
    }
    if (lengthBytes < 20)
    {
      return -1;
    }
    lengthBytes -= 20;
    EXPECT_EQ(lengthBytes, fread(payloadData, 1, lengthBytes, _rtpFile));
    return lengthBytes;
}

} // namespace webrtc
