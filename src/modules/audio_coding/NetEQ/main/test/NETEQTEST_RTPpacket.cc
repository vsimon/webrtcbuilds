/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "NETEQTEST_RTPpacket.h"

#include <string.h>

#ifdef WIN32
#include <winsock2.h>
#else
#include <netinet/in.h> // for htons, htonl, etc
#endif

#define HDR_SIZE 8 // rtpplay packet header size in bytes


NETEQTEST_RTPpacket::NETEQTEST_RTPpacket()
:
_datagram(NULL),
_payloadPtr(NULL),
_memSize(0),
_datagramLen(-1),
_payloadLen(0),
_rtpParsed(false),
_receiveTime(0),
_lost(false)
{
    memset(&_rtpInfo, 0, sizeof(_rtpInfo));
    _blockList.clear();
}

NETEQTEST_RTPpacket::NETEQTEST_RTPpacket(const NETEQTEST_RTPpacket& copyFromMe)
{

    memcpy(this, &copyFromMe, sizeof(NETEQTEST_RTPpacket));

    _datagram = NULL;
    _payloadPtr = NULL;

    if(copyFromMe._datagram)
    {
        _datagram = new WebRtc_UWord8[_memSize];
        
        if(_datagram)
        {
            memcpy(_datagram, copyFromMe._datagram, _memSize);
        }
    }

    if(copyFromMe._payloadPtr)
    {
        _payloadPtr = _datagram + (copyFromMe._payloadPtr - copyFromMe._datagram);
    }

    _blockList = copyFromMe._blockList;

}

    
NETEQTEST_RTPpacket & NETEQTEST_RTPpacket::operator = (const NETEQTEST_RTPpacket & other)
{
    if (this != &other) // protect against invalid self-assignment
    {

        // deallocate datagram memory if allocated
        if(_datagram)
        {
            delete[] _datagram;
        }

        // do shallow copy
        memcpy(this, &other, sizeof(NETEQTEST_RTPpacket));

        // reset pointers
        _datagram = NULL;
        _payloadPtr = NULL;

        if(other._datagram)
        {
            _datagram = new WebRtc_UWord8[other._memSize];
            _memSize = other._memSize;

            if(_datagram)
            {
                memcpy(_datagram, other._datagram, _memSize);
            }
        }

        if(other._payloadPtr)
        {
            _payloadPtr = _datagram + (other._payloadPtr - other._datagram);
        }

        // copy the blocking list (map)
        _blockList = other._blockList;

    }

    // by convention, always return *this
    return *this;
}



NETEQTEST_RTPpacket::~NETEQTEST_RTPpacket()
{
    if(_datagram) 
    {
        delete _datagram;
    }
}


void NETEQTEST_RTPpacket::reset()
{
    if(_datagram) {
        delete _datagram;
    }
    _datagram = NULL;
    _memSize = 0;
    _datagramLen = -1;
    _payloadLen = 0;
    _payloadPtr = NULL;
    _receiveTime = 0;
    memset(&_rtpInfo, 0, sizeof(_rtpInfo));
    _rtpParsed = false;

}


int NETEQTEST_RTPpacket::readFromFile(FILE *fp)
{
    if(!fp)
    {
        return(-1);
    }

	WebRtc_UWord16 length, plen;
    WebRtc_UWord32 offset;

    if (fread(&length,2,1,fp)==0)
    {
        reset();
        return(-2);
    }
    length = ntohs(length);

    if (fread(&plen,2,1,fp)==0)
    {
        reset();
        return(-1);
    }
    int packetLen = ntohs(plen);

    if (fread(&offset,4,1,fp)==0)
    {
        reset();
        return(-1);
    }
    WebRtc_UWord32 receiveTime = ntohl(offset); // store in local variable until we have passed the reset below
	
	// Use length here because a plen of 0 specifies rtcp
	length = (WebRtc_UWord16) (length - HDR_SIZE);

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

	if (fread((unsigned short *) _datagram,1,length,fp) != length)
    {
        reset();
		return(-1);
    }

    _datagramLen = length;
    _receiveTime = receiveTime;

    if (!_blockList.empty() && _blockList.count(payloadType()) > 0)
    {
        // discard this payload
        return(readFromFile(fp));
    }

	return(packetLen);

}


int NETEQTEST_RTPpacket::readFixedFromFile(FILE *fp, int length)
{
    if(!fp)
    {
        return(-1);
    }

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

	if (fread((unsigned short *) _datagram,1,length,fp) != length)
    {
        reset();
		return(-1);
    }

    _datagramLen = length;
    _receiveTime = 0;

    if (!_blockList.empty() && _blockList.count(payloadType()) > 0)
    {
        // discard this payload
        return(readFromFile(fp));
    }

	return(length);

}


int NETEQTEST_RTPpacket::writeToFile(FILE *fp)
{
    if(!fp)
    {
        return(-1);
    }

	WebRtc_UWord16 length, plen;
    WebRtc_UWord32 offset;

    // length including RTPplay header
    length = htons(_datagramLen + HDR_SIZE);
    if (fwrite(&length, 2, 1, fp) != 1)
    {
        return(-1);
    }

    // payload length
    plen = htons(_datagramLen);
    if (fwrite(&plen, 2, 1, fp) != 1)
    {
        return(-1);
    }
    
    // offset (=receive time)
    offset = htonl(_receiveTime);
    if (fwrite(&offset, 4, 1, fp) != 1)
    {
        return(-1);
    }


    // write packet data
    if (fwrite((unsigned short *) _datagram, 1, _datagramLen, fp) != _datagramLen)
    {
        return(-1);
    }

	return(_datagramLen + HDR_SIZE); // total number of bytes written

}


void NETEQTEST_RTPpacket::blockPT(WebRtc_UWord8 pt)
{
    _blockList[pt] = true;
}


void NETEQTEST_RTPpacket::parseHeader()
{
    if (_rtpParsed)
    {
        // nothing to do
        return;
    }

    if (_datagramLen < 12)
    {
        // corrupt packet?
        return;
    }

    _payloadLen = parseRTPheader(_datagram, _datagramLen, &_rtpInfo, &_payloadPtr);

    _rtpParsed = true;

    return;

}

void NETEQTEST_RTPpacket::parseHeader(WebRtcNetEQ_RTPInfo & rtpInfo)
{
    if (!_rtpParsed)
    {
        // parse the header
        parseHeader();
    }

    memcpy(&rtpInfo, &_rtpInfo, sizeof(WebRtcNetEQ_RTPInfo));
}

WebRtcNetEQ_RTPInfo const * NETEQTEST_RTPpacket::RTPinfo() const
{
    if (_rtpParsed)
    {
        return &_rtpInfo;
    }
    else
    {
        return NULL;
    }
}

WebRtc_UWord8 * NETEQTEST_RTPpacket::datagram() const
{
    if (_datagramLen > 0)
    {
        return _datagram;
    }
    else
    {
        return NULL;
    }
}

WebRtc_UWord8 * NETEQTEST_RTPpacket::payload() const
{
    if (_payloadLen > 0)
    {
        return _payloadPtr;
    }
    else
    {
        return NULL;
    }
}

WebRtc_Word16 NETEQTEST_RTPpacket::payloadLen() const
{
    return _payloadLen;
}

WebRtc_Word16 NETEQTEST_RTPpacket::dataLen() const
{
    return _datagramLen;
}

bool NETEQTEST_RTPpacket::isParsed() const
{
    return _rtpParsed;
}

bool NETEQTEST_RTPpacket::isLost() const
{
    return _lost;
}

WebRtc_UWord8  NETEQTEST_RTPpacket::payloadType() const
{
    WebRtcNetEQ_RTPInfo tempRTPinfo;
    
    if(_datagram)
    {
        parseRTPheader(_datagram, _datagramLen, &tempRTPinfo);
    }
    else
    {
        return 0;
    }

    return tempRTPinfo.payloadType;
}

WebRtc_UWord16 NETEQTEST_RTPpacket::sequenceNumber() const
{
    WebRtcNetEQ_RTPInfo tempRTPinfo;
    
    if(_datagram)
    {
        parseRTPheader(_datagram, _datagramLen, &tempRTPinfo);
    }
    else
    {
        return 0;
    }

    return tempRTPinfo.sequenceNumber;
}

WebRtc_UWord32 NETEQTEST_RTPpacket::timeStamp() const
{
    WebRtcNetEQ_RTPInfo tempRTPinfo;
    
    if(_datagram)
    {
        parseRTPheader(_datagram, _datagramLen, &tempRTPinfo);
    }
    else
    {
        return 0;
    }

    return tempRTPinfo.timeStamp;
}

WebRtc_UWord32 NETEQTEST_RTPpacket::SSRC() const
{
    WebRtcNetEQ_RTPInfo tempRTPinfo;
    
    if(_datagram)
    {
        parseRTPheader(_datagram, _datagramLen, &tempRTPinfo);
    }
    else
    {
        return 0;
    }

    return tempRTPinfo.SSRC;
}

WebRtc_UWord8  NETEQTEST_RTPpacket::markerBit() const
{
    WebRtcNetEQ_RTPInfo tempRTPinfo;
    
    if(_datagram)
    {
        parseRTPheader(_datagram, _datagramLen, &tempRTPinfo);
    }
    else
    {
        return 0;
    }

    return tempRTPinfo.markerBit;
}



int NETEQTEST_RTPpacket::setPayloadType(WebRtc_UWord8 pt)
{
    
    if (_datagramLen < 12)
    {
        return -1;
    }

    if (!_rtpParsed)
    {
        _rtpInfo.payloadType = pt;
    }

    _datagram[1]=(unsigned char)(pt & 0xFF);

    return 0;

}

int NETEQTEST_RTPpacket::setSequenceNumber(WebRtc_UWord16 sn)
{
    
    if (_datagramLen < 12)
    {
        return -1;
    }

    if (!_rtpParsed)
    {
        _rtpInfo.sequenceNumber = sn;
    }

    _datagram[2]=(unsigned char)((sn>>8)&0xFF);
    _datagram[3]=(unsigned char)((sn)&0xFF);

    return 0;

}

int NETEQTEST_RTPpacket::setTimeStamp(WebRtc_UWord32 ts)
{
    
    if (_datagramLen < 12)
    {
        return -1;
    }

    if (!_rtpParsed)
    {
        _rtpInfo.timeStamp = ts;
    }

    _datagram[4]=(unsigned char)((ts>>24)&0xFF);
    _datagram[5]=(unsigned char)((ts>>16)&0xFF);
    _datagram[6]=(unsigned char)((ts>>8)&0xFF); 
    _datagram[7]=(unsigned char)(ts & 0xFF);

    return 0;

}

int NETEQTEST_RTPpacket::setSSRC(WebRtc_UWord32 ssrc)
{
    
    if (_datagramLen < 12)
    {
        return -1;
    }

    if (!_rtpParsed)
    {
        _rtpInfo.SSRC = ssrc;
    }

    _datagram[8]=(unsigned char)((ssrc>>24)&0xFF);
    _datagram[9]=(unsigned char)((ssrc>>16)&0xFF);
    _datagram[10]=(unsigned char)((ssrc>>8)&0xFF);
    _datagram[11]=(unsigned char)(ssrc & 0xFF);

    return 0;

}

int NETEQTEST_RTPpacket::setMarkerBit(WebRtc_UWord8 mb)
{
    
    if (_datagramLen < 12)
    {
        return -1;
    }

    if (_rtpParsed)
    {
        _rtpInfo.markerBit = mb;
    }

    if (mb)
    {
        _datagram[0] |= 0x01;
    }
    else
    {
        _datagram[0] &= 0xFE;
    }

    return 0;

}

int NETEQTEST_RTPpacket::setRTPheader(const WebRtcNetEQ_RTPInfo *RTPinfo)
{
    if (_datagramLen < 12)
    {
        // this packet is not ok
        return -1;
    }

    makeRTPheader(_datagram, 
        RTPinfo->payloadType, 
        RTPinfo->sequenceNumber, 
        RTPinfo->timeStamp, 
        RTPinfo->SSRC,
        RTPinfo->markerBit);

    return 0;
}


int NETEQTEST_RTPpacket::splitStereo(NETEQTEST_RTPpacket& slaveRtp, enum stereoModes mode)
{
    // if mono, do nothing
    if (mode == stereoModeMono)
    {
        return 0;
    }

    // check that the RTP header info is parsed
    parseHeader();

    // start by copying the main rtp packet
    slaveRtp = *this;

    if(_payloadLen == 0)
    {
        // do no more
        return 0;
    }

    if(_payloadLen%2 != 0)
    {
        // length must be a factor of 2
        return -1;
    }

    switch(mode)
    {
    case stereoModeSample1:
        {
            // sample based codec with 1-byte samples
            splitStereoSample(slaveRtp, 1 /* 1 byte/sample */);
            break;
        }
    case stereoModeSample2:
        {
            // sample based codec with 2-byte samples
            splitStereoSample(slaveRtp, 2 /* 2 bytes/sample */);
            break;
        }
    case stereoModeFrame:
        {
            // frame based codec
            splitStereoFrame(slaveRtp);
            break;
        }
    }

    return 0;
}


void NETEQTEST_RTPpacket::makeRTPheader(unsigned char* rtp_data, WebRtc_UWord8 payloadType, WebRtc_UWord16 seqNo, WebRtc_UWord32 timestamp, WebRtc_UWord32 ssrc, WebRtc_UWord8 markerBit) const
{
    rtp_data[0]=(unsigned char)0x80;
    if (markerBit)
    {
        rtp_data[0] |= 0x01;
    }
    else
    {
        rtp_data[0] &= 0xFE;
    }
    rtp_data[1]=(unsigned char)(payloadType & 0xFF);
    rtp_data[2]=(unsigned char)((seqNo>>8)&0xFF);
    rtp_data[3]=(unsigned char)((seqNo)&0xFF);
    rtp_data[4]=(unsigned char)((timestamp>>24)&0xFF);
    rtp_data[5]=(unsigned char)((timestamp>>16)&0xFF);

    rtp_data[6]=(unsigned char)((timestamp>>8)&0xFF); 
    rtp_data[7]=(unsigned char)(timestamp & 0xFF);

    rtp_data[8]=(unsigned char)((ssrc>>24)&0xFF);
    rtp_data[9]=(unsigned char)((ssrc>>16)&0xFF);

    rtp_data[10]=(unsigned char)((ssrc>>8)&0xFF);
    rtp_data[11]=(unsigned char)(ssrc & 0xFF);
}


WebRtc_UWord16 NETEQTEST_RTPpacket::parseRTPheader(const WebRtc_UWord8 *datagram, int datagramLen, WebRtcNetEQ_RTPInfo *RTPinfo, WebRtc_UWord8 **payloadPtr) const
{
    WebRtc_Word16 *rtp_data = (WebRtc_Word16 *) datagram;
    int i_P, i_X, i_CC, i_extlength=-1, i_padlength=0, i_startPosition;

	i_P=(((WebRtc_UWord16)(rtp_data[0] & 0x20))>>5);				/* Extract the P bit		*/
	i_X=(((WebRtc_UWord16)(rtp_data[0] & 0x10))>>4);				/* Extract the X bit		*/
	i_CC=(WebRtc_UWord16)(rtp_data[0] & 0xF);						/* Get the CC number		*/
    RTPinfo->markerBit = (WebRtc_UWord8) ((rtp_data[0] >> 15) & 0x01);    /* Get the marker bit */
    RTPinfo->payloadType = (WebRtc_UWord8) ((rtp_data[0] >> 8) & 0x7F);	/* Get the coder type		*/
    RTPinfo->sequenceNumber = ((( ((WebRtc_UWord16)rtp_data[1]) >> 8) & 0xFF) | 
		( ((WebRtc_UWord16)(rtp_data[1] & 0xFF)) << 8));			/* Get the packet number	*/
	RTPinfo->timeStamp = ((((WebRtc_UWord16)rtp_data[2]) & 0xFF) << 24) | 
		((((WebRtc_UWord16)rtp_data[2]) & 0xFF00) << 8) | 
		((((WebRtc_UWord16)rtp_data[3]) >> 8) & 0xFF) |
		((((WebRtc_UWord16)rtp_data[3]) & 0xFF) << 8);			/* Get timestamp            */
	RTPinfo->SSRC=((((WebRtc_UWord16)rtp_data[4]) & 0xFF) << 24) | 
		((((WebRtc_UWord16)rtp_data[4]) & 0xFF00) << 8) | 
		((((WebRtc_UWord16)rtp_data[5]) >> 8) & 0xFF) |
		((((WebRtc_UWord16)rtp_data[5]) & 0xFF) << 8);			/* Get the SSRC				*/

	if (i_X==1) {
		/* Extention header exists. Find out how many WebRtc_Word32 it consists of */
		i_extlength=((( ((WebRtc_UWord16)rtp_data[7+2*i_CC]) >> 8) & 0xFF) |
				( ((WebRtc_UWord16)(rtp_data[7+2*i_CC]&0xFF)) << 8));
	}
	if (i_P==1) {
		/* Padding exists. Find out how many bytes the padding consists of */
		if (datagramLen & 0x1) {
			/* odd number of bytes => last byte in higher byte */
			i_padlength=(rtp_data[datagramLen>>1] & 0xFF);
		} else {
			/* even number of bytes => last byte in lower byte */
			i_padlength=(((WebRtc_UWord16)rtp_data[(datagramLen>>1)-1]) >> 8);
		}
	}

	i_startPosition=12+4*(i_extlength+1)+4*i_CC;

    if (payloadPtr) {
        *payloadPtr = (WebRtc_UWord8*) &rtp_data[i_startPosition>>1];
    }

	return (WebRtc_UWord16) (datagramLen-i_startPosition-i_padlength);
}

//void NETEQTEST_RTPpacket::splitStereoSample(WebRtc_UWord8 *data, WebRtc_UWord16 *lenBytes, WebRtc_UWord8 *slaveData, WebRtc_UWord16 *slaveLenBytes, int stride)
void NETEQTEST_RTPpacket::splitStereoSample(NETEQTEST_RTPpacket& slaveRtp, int stride)
{
    if(!_payloadPtr || !slaveRtp._payloadPtr 
        || _payloadLen <= 0 || slaveRtp._memSize < _memSize)
    {
        return;
    }

    WebRtc_UWord8 *readDataPtr = _payloadPtr;
    WebRtc_UWord8 *writeDataPtr = _payloadPtr;
    WebRtc_UWord8 *slaveData = slaveRtp._payloadPtr;

    while (readDataPtr - _payloadPtr < _payloadLen)
    {
        // master data
        for (int ix = 0; ix < stride; ix++) {
            *writeDataPtr = *readDataPtr;
            writeDataPtr++;
            readDataPtr++;
        }

        // slave data
        for (int ix = 0; ix < stride; ix++) {
            *slaveData = *readDataPtr;
            slaveData++;
            readDataPtr++;
        }
    }

    _payloadLen /= 2;
    slaveRtp._payloadLen = _payloadLen;
}


//void NETEQTEST_RTPpacket::splitStereoFrame(WebRtc_UWord8 *data, WebRtc_UWord16 *lenBytes, WebRtc_UWord8 *slaveData, WebRtc_UWord16 *slaveLenBytes)
void NETEQTEST_RTPpacket::splitStereoFrame(NETEQTEST_RTPpacket& slaveRtp)
{
    if(!_payloadPtr || !slaveRtp._payloadPtr 
        || _payloadLen <= 0 || slaveRtp._memSize < _memSize)
    {
        return;
    }

    memmove(slaveRtp._payloadPtr, _payloadPtr + _payloadLen/2, _payloadLen/2);

    _payloadLen /= 2;
    slaveRtp._payloadLen = _payloadLen;
}

