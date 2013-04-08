/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/main/test/rtp_player.h"

#include <cstdlib>
#ifdef WIN32
#include <windows.h>
#include <Winsock2.h>
#else
#include <arpa/inet.h>
#endif

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp.h"
#include "webrtc/modules/video_coding/main/source/internal_defines.h"
#include "webrtc/modules/video_coding/main/test/test_util.h"
#include "webrtc/system_wrappers/interface/clock.h"

using namespace webrtc;

RawRtpPacket::RawRtpPacket(uint8_t* rtp_data, uint16_t rtp_length)
    : data(rtp_data),
      length(rtp_length),
      resend_time_ms(-1) {
  data = new uint8_t[length];
  memcpy(data, rtp_data, length);
}

RawRtpPacket::~RawRtpPacket() {
  delete [] data;
}

LostPackets::LostPackets()
    : crit_sect_(CriticalSectionWrapper::CreateCriticalSection()),
      loss_count_(0),
      debug_file_(NULL),
      packets_() {
  debug_file_ = fopen("PacketLossDebug.txt", "w");
}

LostPackets::~LostPackets() {
  if (debug_file_) {
      fclose(debug_file_);
  }
  while (!packets_.empty()) {
    delete packets_.front();
    packets_.pop_front();
  }
  delete crit_sect_;
}

void LostPackets::AddPacket(RawRtpPacket* packet) {
  CriticalSectionScoped cs(crit_sect_);
  packets_.push_back(packet);
  uint16_t seq_num = (packet->data[2] << 8) + packet->data[3];
  if (debug_file_ != NULL) {
    fprintf(debug_file_, "%u Lost packet: %u\n", loss_count_, seq_num);
  }
  ++loss_count_;
}

void LostPackets::SetResendTime(uint16_t resend_seq_num,
                                int64_t resend_time_ms,
                                int64_t now_ms) {
  CriticalSectionScoped cs(crit_sect_);
  for (RtpPacketIterator it = packets_.begin(); it != packets_.end(); ++it) {
    const uint16_t seq_num = ((*it)->data[2] << 8) +
        (*it)->data[3];
    if (resend_seq_num == seq_num) {
      if ((*it)->resend_time_ms + 10 < now_ms) {
        if (debug_file_ != NULL) {
          fprintf(debug_file_, "Resend %u at %u\n", seq_num,
                  MaskWord64ToUWord32(resend_time_ms));
        }
        (*it)->resend_time_ms = resend_time_ms;
      }
      return;
    }
  }
  assert(false);
}

RawRtpPacket* LostPackets::NextPacketToResend(int64_t timeNow) {
  CriticalSectionScoped cs(crit_sect_);
  for (RtpPacketIterator it = packets_.begin(); it != packets_.end(); ++it) {
    if (timeNow >= (*it)->resend_time_ms && (*it)->resend_time_ms != -1) {
      RawRtpPacket* packet = *it;
      it = packets_.erase(it);
      return packet;
    }
  }
  return NULL;
}

int LostPackets::NumberOfPacketsToResend() const {
  CriticalSectionScoped cs(crit_sect_);
  int count = 0;
  for (ConstRtpPacketIterator it = packets_.begin(); it != packets_.end();
      ++it) {
    if ((*it)->resend_time_ms >= 0) {
        count++;
    }
  }
  return count;
}

void LostPackets::SetPacketResent(uint16_t seq_num, int64_t now_ms) {
  CriticalSectionScoped cs(crit_sect_);
  if (debug_file_ != NULL) {
    fprintf(debug_file_, "Resent %u at %u\n", seq_num,
            MaskWord64ToUWord32(now_ms));
  }
}

void LostPackets::Print() const {
  CriticalSectionScoped cs(crit_sect_);
  printf("Lost packets: %u\n", loss_count_);
  printf("Packets waiting to be resent: %u\n",
         NumberOfPacketsToResend());
  printf("Packets still lost: %u\n",
         static_cast<unsigned int>(packets_.size()));
  printf("Sequence numbers:\n");
  for (ConstRtpPacketIterator it = packets_.begin(); it != packets_.end();
      ++it) {
    uint16_t seq_num = ((*it)->data[2] << 8) + (*it)->data[3];
    printf("%u, ", seq_num);
  }
  printf("\n");
}

RTPPlayer::RTPPlayer(const char* filename,
                     RtpData* callback,
                     Clock* clock)
:
_clock(clock),
_rtpModule(NULL),
_nextRtpTime(0),
_dataCallback(callback),
_firstPacket(true),
_lossRate(0.0f),
_nackEnabled(false),
_resendPacketCount(0),
_noLossStartup(100),
_endOfFile(false),
_rttMs(0),
_firstPacketRtpTime(0),
_firstPacketTimeMs(0),
_reorderBuffer(NULL),
_reordering(false),
_nextPacket(),
_nextPacketLength(0),
_randVec(),
_randVecPos(0)
{
    _rtpFile = fopen(filename, "rb");
    memset(_nextPacket, 0, sizeof(_nextPacket));
}

RTPPlayer::~RTPPlayer()
{
    delete _rtpModule;
    if (_rtpFile != NULL)
    {
        fclose(_rtpFile);
    }
    if (_reorderBuffer != NULL)
    {
        delete _reorderBuffer;
        _reorderBuffer = NULL;
    }
}

int32_t RTPPlayer::Initialize(const PayloadTypeList* payloadList)
{
    RtpRtcp::Configuration configuration;
    configuration.id = 1;
    configuration.audio = false;
    configuration.incoming_data = _dataCallback;
    _rtpModule = RtpRtcp::CreateRtpRtcp(configuration);

    std::srand(321);
    for (int i=0; i < RAND_VEC_LENGTH; i++)
    {
        _randVec[i] = rand();
    }
    _randVecPos = 0;
    int32_t ret = _rtpModule->SetNACKStatus(kNackOff,
                                                  kMaxPacketAgeToNack);
    if (ret < 0)
    {
        return -1;
    }
    _rtpModule->SetRTCPStatus(kRtcpNonCompound);
    _rtpModule->SetTMMBRStatus(true);

    if (ret < 0)
    {
        return -1;
    }
    // Register payload types
    for (PayloadTypeList::const_iterator it = payloadList->begin();
        it != payloadList->end(); ++it) {
        PayloadCodecTuple* payloadType = *it;
        if (payloadType != NULL)
        {
            VideoCodec videoCodec;
            strncpy(videoCodec.plName, payloadType->name.c_str(), 32);
            videoCodec.plType = payloadType->payloadType;
            if (_rtpModule->RegisterReceivePayload(videoCodec) < 0)
            {
                return -1;
            }
        }
    }
    if (ReadHeader() < 0)
    {
        return -1;
    }
    memset(_nextPacket, 0, sizeof(_nextPacket));
    _nextPacketLength = ReadPacket(_nextPacket, &_nextRtpTime);
    return 0;
}

int32_t RTPPlayer::ReadHeader()
{
    char firstline[FIRSTLINELEN];
    if (_rtpFile == NULL)
    {
        return -1;
    }
    EXPECT_TRUE(fgets(firstline, FIRSTLINELEN, _rtpFile) != NULL);
    if(strncmp(firstline,"#!rtpplay",9) == 0) {
        if(strncmp(firstline,"#!rtpplay1.0",12) != 0){
            printf("ERROR: wrong rtpplay version, must be 1.0\n");
            return -1;
        }
    }
    else if (strncmp(firstline,"#!RTPencode",11) == 0) {
        if(strncmp(firstline,"#!RTPencode1.0",14) != 0){
            printf("ERROR: wrong RTPencode version, must be 1.0\n");
            return -1;
        }
    }
    else {
        printf("ERROR: wrong file format of input file\n");
        return -1;
    }

    uint32_t start_sec;
    uint32_t start_usec;
    uint32_t source;
    uint16_t port;
    uint16_t padding;

    EXPECT_GT(fread(&start_sec, 4, 1, _rtpFile), 0u);
    start_sec=ntohl(start_sec);
    EXPECT_GT(fread(&start_usec, 4, 1, _rtpFile), 0u);
    start_usec=ntohl(start_usec);
    EXPECT_GT(fread(&source, 4, 1, _rtpFile), 0u);
    source=ntohl(source);
    EXPECT_GT(fread(&port, 2, 1, _rtpFile), 0u);
    port=ntohs(port);
    EXPECT_GT(fread(&padding, 2, 1, _rtpFile), 0u);
    padding=ntohs(padding);
    return 0;
}

uint32_t RTPPlayer::TimeUntilNextPacket() const
{
    int64_t timeLeft = (_nextRtpTime - _firstPacketRtpTime) -
        (_clock->TimeInMilliseconds() - _firstPacketTimeMs);
    if (timeLeft < 0)
    {
        return 0;
    }
    return static_cast<uint32_t>(timeLeft);
}

int32_t RTPPlayer::NextPacket(const int64_t timeNow)
{
    // Send any packets ready to be resent,
    RawRtpPacket* resend_packet = _lostPackets.NextPacketToResend(timeNow);
    while (resend_packet != NULL) {
      const uint16_t seqNo = (resend_packet->data[2] << 8) +
          resend_packet->data[3];
      printf("Resend: %u\n", seqNo);
      int ret = SendPacket(resend_packet->data, resend_packet->length);
      delete resend_packet;
      _resendPacketCount++;
      if (ret > 0) {
        _lostPackets.SetPacketResent(seqNo, _clock->TimeInMilliseconds());
      } else if (ret < 0) {
        return ret;
      }
      resend_packet = _lostPackets.NextPacketToResend(timeNow);
    }

    // Send any packets from rtp file
    if (!_endOfFile && (TimeUntilNextPacket() == 0 || _firstPacket))
    {
        _rtpModule->Process();
        if (_firstPacket)
        {
            _firstPacketRtpTime = static_cast<int64_t>(_nextRtpTime);
            _firstPacketTimeMs = _clock->TimeInMilliseconds();
        }
        if (_reordering && _reorderBuffer == NULL)
        {
            _reorderBuffer = new RawRtpPacket(reinterpret_cast<uint8_t*>(_nextPacket), static_cast<uint16_t>(_nextPacketLength));
            return 0;
        }
        int32_t ret = SendPacket(reinterpret_cast<uint8_t*>(_nextPacket), static_cast<uint16_t>(_nextPacketLength));
        if (_reordering && _reorderBuffer != NULL)
        {
            RawRtpPacket* rtpPacket = _reorderBuffer;
            _reorderBuffer = NULL;
            SendPacket(rtpPacket->data, rtpPacket->length);
            delete rtpPacket;
        }
        _firstPacket = false;
        if (ret < 0)
        {
            return ret;
        }
        _nextPacketLength = ReadPacket(_nextPacket, &_nextRtpTime);
        if (_nextPacketLength < 0)
        {
            _endOfFile = true;
            return 0;
        }
        else if (_nextPacketLength == 0)
        {
            return 0;
        }
    }
    if (_endOfFile && _lostPackets.NumberOfPacketsToResend() == 0)
    {
        return 1;
    }
    return 0;
}

int32_t RTPPlayer::SendPacket(uint8_t* rtpData, uint16_t rtpLen)
{
    if ((_randVec[(_randVecPos++) % RAND_VEC_LENGTH] + 1.0)/(RAND_MAX + 1.0) < _lossRate &&
        _noLossStartup < 0)
    {
        if (_nackEnabled)
        {
            const uint16_t seqNo = (rtpData[2] << 8) + rtpData[3];
            printf("Throw: %u\n", seqNo);
            _lostPackets.AddPacket(new RawRtpPacket(rtpData, rtpLen));
            return 0;
        }
    }
    else if (rtpLen > 0)
    {
        int32_t ret = _rtpModule->IncomingPacket(rtpData, rtpLen);
        if (ret < 0)
        {
            return -1;
        }
    }
    if (_noLossStartup >= 0)
    {
        _noLossStartup--;
    }
    return 1;
}

int32_t RTPPlayer::ReadPacket(int16_t* rtpdata, uint32_t* offset)
{
    uint16_t length, plen;

    if (fread(&length,2,1,_rtpFile)==0)
        return(-1);
    length=ntohs(length);

    if (fread(&plen,2,1,_rtpFile)==0)
        return(-1);
    plen=ntohs(plen);

    if (fread(offset,4,1,_rtpFile)==0)
        return(-1);
    *offset=ntohl(*offset);

    // Use length here because a plen of 0 specifies rtcp
    length = (uint16_t) (length - HDR_SIZE);
    if (fread((unsigned short *) rtpdata,1,length,_rtpFile) != length)
        return(-1);

#ifdef JUNK_DATA
    // destroy the RTP payload with random data
    if (plen > 12) { // ensure that we have more than just a header
        for ( int ix = 12; ix < plen; ix=ix+2 ) {
            rtpdata[ix>>1] = (short) (rtpdata[ix>>1] + (short) rand());
        }
    }
#endif
    return plen;
}

int32_t RTPPlayer::SimulatePacketLoss(float lossRate, bool enableNack, uint32_t rttMs)
{
    _nackEnabled = enableNack;
    _lossRate = lossRate;
    _rttMs = rttMs;
    return 0;
}

int32_t RTPPlayer::SetReordering(bool enabled)
{
    _reordering = enabled;
    return 0;
}

int32_t RTPPlayer::ResendPackets(const uint16_t* sequenceNumbers, uint16_t length)
{
    if (sequenceNumbers == NULL)
    {
        return 0;
    }
    for (int i=0; i < length; i++)
    {
        _lostPackets.SetResendTime(sequenceNumbers[i],
                                   _clock->TimeInMilliseconds() + _rttMs,
                                   _clock->TimeInMilliseconds());
    }
    return 0;
}

void RTPPlayer::Print() const
{
    printf("Resent packets: %u\n", _resendPacketCount);
    _lostPackets.Print();
}
