/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "voe_file_impl.h"

#include "channel.h"
#include "critical_section_wrapper.h"
#include "file_wrapper.h"
#include "media_file.h"
#include "output_mixer.h"
#include "trace.h"
#include "transmit_mixer.h"
#include "voe_errors.h"
#include "voice_engine_impl.h"

namespace webrtc {

VoEFile* VoEFile::GetInterface(VoiceEngine* voiceEngine)
{
#ifndef WEBRTC_VOICE_ENGINE_FILE_API
    return NULL;
#else
    if (NULL == voiceEngine)
    {
        return NULL;
    }
    VoiceEngineImpl* s =
        reinterpret_cast<VoiceEngineImpl*> (voiceEngine);
    VoEFileImpl* d = s;
    (*d)++;
    return (d);
#endif
}

#ifdef WEBRTC_VOICE_ENGINE_FILE_API

VoEFileImpl::VoEFileImpl()
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_instanceId,-1),
                 "VoEFileImpl::VoEFileImpl() - ctor");
}

VoEFileImpl::~VoEFileImpl()
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_instanceId,-1),
                 "VoEFileImpl::~VoEFileImpl() - dtor");
}

int VoEFileImpl::Release()
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "VoEFile::Release()");
    (*this)--;
    int refCount = GetCount();
    if (refCount < 0)
    {
        Reset();
        _engineStatistics.SetLastError(VE_INTERFACE_NOT_FOUND,
                                       kTraceWarning);
        return (-1);
    }
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId,-1),
                 "VoEFile reference counter = %d", refCount);
    return (refCount);
}

int VoEFileImpl::StartPlayingFileLocally(
    int channel,
    const char fileNameUTF8[1024],
    bool loop, FileFormats format,
    float volumeScaling,
    int startPointMs,
    int stopPointMs)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "StartPlayingFileLocally(channel=%d, fileNameUTF8[]=%s, "
                 "loop=%d, format=%d, volumeScaling=%5.3f, startPointMs=%d,"
                 " stopPointMs=%d)",
                 channel, fileNameUTF8, loop, format, volumeScaling,
                 startPointMs, stopPointMs);
    assert(1024 == FileWrapper::kMaxFileNameSize);
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_channelManager, channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _engineStatistics.SetLastError(
            VE_CHANNEL_NOT_VALID, kTraceError,
            "StartPlayingFileLocally() failed to locate channel");
        return -1;
    }

    return channelPtr->StartPlayingFileLocally(fileNameUTF8,
                                               loop,
                                               format,
                                               startPointMs,
                                               volumeScaling,
                                               stopPointMs,
                                               NULL);
}

int VoEFileImpl::StartPlayingFileLocally(int channel,
                                         InStream* stream,
                                         FileFormats format,
                                         float volumeScaling,
                                         int startPointMs,
                                         int stopPointMs)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "StartPlayingFileLocally(channel=%d, stream, format=%d, "
                 "volumeScaling=%5.3f, startPointMs=%d, stopPointMs=%d)",
                 channel, format, volumeScaling, startPointMs, stopPointMs);

    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }

    voe::ScopedChannel sc(_channelManager, channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _engineStatistics.SetLastError(
            VE_CHANNEL_NOT_VALID, kTraceError,
            "StartPlayingFileLocally() failed to locate channel");
        return -1;
    }

    return channelPtr->StartPlayingFileLocally(stream,
                                               format,
                                               startPointMs,
                                               volumeScaling,
                                               stopPointMs,
                                               NULL);
}

int VoEFileImpl::StopPlayingFileLocally(int channel)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "StopPlayingFileLocally()");
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_channelManager, channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _engineStatistics.SetLastError(
            VE_CHANNEL_NOT_VALID, kTraceError,
            "StopPlayingFileLocally() failed to locate channel");
        return -1;
    }
    return channelPtr->StopPlayingFileLocally();
}

int VoEFileImpl::IsPlayingFileLocally(int channel)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "IsPlayingFileLocally(channel=%d)", channel);
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_channelManager, channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _engineStatistics.SetLastError(
            VE_CHANNEL_NOT_VALID, kTraceError,
            "StopPlayingFileLocally() failed to locate channel");
        return -1;
    }
    return channelPtr->IsPlayingFileLocally();
}

int VoEFileImpl::ScaleLocalFilePlayout(int channel, float scale)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "ScaleLocalFilePlayout(channel=%d, scale=%5.3f)",
                 channel, scale);
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_channelManager, channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _engineStatistics.SetLastError(
            VE_CHANNEL_NOT_VALID, kTraceError,
            "StopPlayingFileLocally() failed to locate channel");
        return -1;
    }
    return channelPtr->ScaleLocalFilePlayout(scale);
}

int VoEFileImpl::StartPlayingFileAsMicrophone(int channel,
                                              const char fileNameUTF8[1024],
                                              bool loop,
                                              bool mixWithMicrophone,
                                              FileFormats format,
                                              float volumeScaling)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "StartPlayingFileAsMicrophone(channel=%d, fileNameUTF8=%s, "
                 "loop=%d, mixWithMicrophone=%d, format=%d, "
                 "volumeScaling=%5.3f)",
                 channel, fileNameUTF8, loop, mixWithMicrophone, format,
                 volumeScaling);
    assert(1024 == FileWrapper::kMaxFileNameSize);
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }

    const WebRtc_UWord32 startPointMs(0);
    const WebRtc_UWord32 stopPointMs(0);

    if (channel == -1)
    {
        int res = _transmitMixerPtr->StartPlayingFileAsMicrophone(
            fileNameUTF8,
            loop,
            format,
            startPointMs,
            volumeScaling,
            stopPointMs,
            NULL);
        if (res)
        {
            WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId,-1),
                         "StartPlayingFileAsMicrophone() failed to start"
                         " playing file");
            return(-1);
        }
        else
        {
            _transmitMixerPtr->SetMixWithMicStatus(mixWithMicrophone);
            return(0);
        }
    }
    else
    {
        // Add file after demultiplexing <=> affects one channel only
        voe::ScopedChannel sc(_channelManager, channel);
        voe::Channel* channelPtr = sc.ChannelPtr();
        if (channelPtr == NULL)
        {
            _engineStatistics.SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                "StartPlayingFileAsMicrophone() failed to locate channel");
            return -1;
        }

        int res = channelPtr->StartPlayingFileAsMicrophone(fileNameUTF8,
                                                           loop,
                                                           format,
                                                           startPointMs,
                                                           volumeScaling,
                                                           stopPointMs,
                                                           NULL);
        if (res)
        {
            WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId,-1),
                         "StartPlayingFileAsMicrophone() failed to start "
                         "playing file");
            return -1;
        }
        else
        {
            channelPtr->SetMixWithMicStatus(mixWithMicrophone);
            return 0;
        }
    }
}

int VoEFileImpl::StartPlayingFileAsMicrophone(int channel,
                                              InStream* stream,
                                              bool mixWithMicrophone,
                                              FileFormats format,
                                              float volumeScaling)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "StartPlayingFileAsMicrophone(channel=%d, stream,"
                 " mixWithMicrophone=%d, format=%d, volumeScaling=%5.3f)",
                 channel, mixWithMicrophone, format, volumeScaling);

    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }

    const WebRtc_UWord32 startPointMs(0);
    const WebRtc_UWord32 stopPointMs(0);

    if (channel == -1)
    {
        int res = _transmitMixerPtr->StartPlayingFileAsMicrophone(
            stream,
            format,
            startPointMs,
            volumeScaling,
            stopPointMs,
            NULL);
        if (res)
        {
            WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId,-1),
                         "StartPlayingFileAsMicrophone() failed to start"
                         " playing stream");
            return(-1);
        }
        else
        {
            _transmitMixerPtr->SetMixWithMicStatus(mixWithMicrophone);
            return(0);
        }
    }
    else
    {
        // Add file after demultiplexing <=> affects one channel only
        voe::ScopedChannel sc(_channelManager, channel);
        voe::Channel* channelPtr = sc.ChannelPtr();
        if (channelPtr == NULL)
        {
            _engineStatistics.SetLastError(
                VE_CHANNEL_NOT_VALID, kTraceError,
                "StartPlayingFileAsMicrophone() failed to locate channel");
            return -1;
        }

        int res = channelPtr->StartPlayingFileAsMicrophone(
            stream, format, startPointMs, volumeScaling, stopPointMs, NULL);
        if (res)
        {
            WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId,-1),
                         "StartPlayingFileAsMicrophone() failed to start"
                         " playing stream");
            return -1;
        }
        else
        {
            channelPtr->SetMixWithMicStatus(mixWithMicrophone);
            return 0;
        }
    }
}

int VoEFileImpl::StopPlayingFileAsMicrophone(int channel)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "StopPlayingFileAsMicrophone(channel=%d)", channel);
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    if (channel == -1)
    {
        // Stop adding file before demultiplexing <=> affects all channels
        return _transmitMixerPtr->StopPlayingFileAsMicrophone();
    }
    else
    {
        // Stop adding file after demultiplexing <=> affects one channel only
        voe::ScopedChannel sc(_channelManager, channel);
        voe::Channel* channelPtr = sc.ChannelPtr();
        if (channelPtr == NULL)
        {
            _engineStatistics.SetLastError(
                VE_CHANNEL_NOT_VALID, kTraceError,
                "StopPlayingFileAsMicrophone() failed to locate channel");
            return -1;
        }
        return channelPtr->StopPlayingFileAsMicrophone();
    }
}

int VoEFileImpl::IsPlayingFileAsMicrophone(int channel)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "IsPlayingFileAsMicrophone(channel=%d)", channel);
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    if (channel == -1)
    {
        return _transmitMixerPtr->IsPlayingFileAsMicrophone();
    }
    else
    {
        // Stop adding file after demultiplexing <=> affects one channel only
        voe::ScopedChannel sc(_channelManager, channel);
        voe::Channel* channelPtr = sc.ChannelPtr();
        if (channelPtr == NULL)
        {
            _engineStatistics.SetLastError(
                VE_CHANNEL_NOT_VALID, kTraceError,
                "IsPlayingFileAsMicrophone() failed to locate channel");
            return -1;
        }
        return channelPtr->IsPlayingFileAsMicrophone();
    }
}

int VoEFileImpl::ScaleFileAsMicrophonePlayout(int channel, float scale)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "ScaleFileAsMicrophonePlayout(channel=%d, scale=%5.3f)",
                 channel, scale);

    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    if (channel == -1)
    {
        return _transmitMixerPtr->ScaleFileAsMicrophonePlayout(scale);
    }
    else
    {
        // Stop adding file after demultiplexing <=> affects one channel only
        voe::ScopedChannel sc(_channelManager, channel);
        voe::Channel* channelPtr = sc.ChannelPtr();
        if (channelPtr == NULL)
        {
            _engineStatistics.SetLastError(
                VE_CHANNEL_NOT_VALID, kTraceError,
                "IsPlayingFileAsMicrophone() failed to locate channel");
            return -1;
        }
        return channelPtr->ScaleFileAsMicrophonePlayout(scale);
    }
}

int VoEFileImpl::StartRecordingPlayout(
    int channel, const char* fileNameUTF8, CodecInst* compression,
    int maxSizeBytes)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "StartRecordingPlayout(channel=%d, fileNameUTF8=%s, "
                 "compression, maxSizeBytes=%d)",
                 channel, fileNameUTF8, maxSizeBytes);
    assert(1024 == FileWrapper::kMaxFileNameSize);

    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    if (channel == -1)
    {
        _outputMixerPtr->StartRecordingPlayout(fileNameUTF8, compression);
        return 0;
    }
    else
    {
        // Add file after demultiplexing <=> affects one channel only
        voe::ScopedChannel sc(_channelManager, channel);
        voe::Channel* channelPtr = sc.ChannelPtr();
        if (channelPtr == NULL)
        {
            _engineStatistics.SetLastError(
                VE_CHANNEL_NOT_VALID, kTraceError,
                "StartRecordingPlayout() failed to locate channel");
            return -1;
        }
        return channelPtr->StartRecordingPlayout(fileNameUTF8, compression);
    }
}

int VoEFileImpl::StartRecordingPlayout(
    int channel, OutStream* stream, CodecInst* compression)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "StartRecordingPlayout(channel=%d, stream, compression)",
                 channel);
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    if (channel == -1)
    {
        return _outputMixerPtr->StartRecordingPlayout(stream, compression);
    }
    else
    {
        voe::ScopedChannel sc(_channelManager, channel);
        voe::Channel* channelPtr = sc.ChannelPtr();
        if (channelPtr == NULL)
        {
            _engineStatistics.SetLastError(
                VE_CHANNEL_NOT_VALID, kTraceError,
                "StartRecordingPlayout() failed to locate channel");
            return -1;
        }
        return channelPtr->StartRecordingPlayout(stream, compression);
    }
}

int VoEFileImpl::StopRecordingPlayout(int channel)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "StopRecordingPlayout(channel=%d)", channel);
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    if (channel == -1)
    {
        return _outputMixerPtr->StopRecordingPlayout();
    }
    else
    {
        voe::ScopedChannel sc(_channelManager, channel);
        voe::Channel* channelPtr = sc.ChannelPtr();
        if (channelPtr == NULL)
        {
            _engineStatistics.SetLastError(
                VE_CHANNEL_NOT_VALID, kTraceError,
                "StopRecordingPlayout() failed to locate channel");
            return -1;
        }
        return channelPtr->StopRecordingPlayout();
    }
}

int VoEFileImpl::StartRecordingMicrophone(
    const char* fileNameUTF8, CodecInst* compression, int maxSizeBytes)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "StartRecordingMicrophone(fileNameUTF8=%s, compression, "
                 "maxSizeBytes=%d)", fileNameUTF8, maxSizeBytes);
    assert(1024 == FileWrapper::kMaxFileNameSize);

    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    if (_transmitMixerPtr->StartRecordingMicrophone(fileNameUTF8, compression))
    {
        WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId,-1),
                     "StartRecordingMicrophone() failed to start recording");
        return -1;
    }
    if (_audioDevicePtr->Recording())
    {
        return 0;
    }
    if (!_externalRecording)
    {
        if (_audioDevicePtr->InitRecording() != 0)
        {
            WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId,-1),
                         "StartRecordingMicrophone() failed to initialize"
                         " recording");
            return -1;
        }
        if (_audioDevicePtr->StartRecording() != 0)
        {
            WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId,-1),
                "StartRecordingMicrophone() failed to start recording");
            return -1;
        }
    }
    return 0;
}

int VoEFileImpl::StartRecordingMicrophone(
    OutStream* stream, CodecInst* compression)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "StartRecordingMicrophone(stream, compression)");

    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    if (_transmitMixerPtr->StartRecordingMicrophone(stream, compression) == -1)
    {
        WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId,-1),
                     "StartRecordingMicrophone() failed to start recording");
        return -1;
    }
    if (_audioDevicePtr->Recording())
    {
        return 0;
    }
    if (!_externalRecording)
    {
        if (_audioDevicePtr->InitRecording() != 0)
        {
            WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId,-1),
                         "StartRecordingMicrophone() failed to initialize "
                         "recording");
            return -1;
        }
        if (_audioDevicePtr->StartRecording() != 0)
        {
            WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId,-1),
                         "StartRecordingMicrophone() failed to start"
                         " recording");
            return -1;
        }
    }
    return 0;
}

int VoEFileImpl::StopRecordingMicrophone()
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "StopRecordingMicrophone()");
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    if ((NumOfSendingChannels() == 0)&&!_transmitMixerPtr->IsRecordingMic())
    {
        // Stop audio-device recording if no channel is recording
        if (_audioDevicePtr->StopRecording() != 0)
        {
            _engineStatistics.SetLastError(
                VE_CANNOT_STOP_RECORDING, kTraceError,
                "StopRecordingMicrophone() failed to stop recording");
            return -1;
        }
    }
    return _transmitMixerPtr->StopRecordingMicrophone();
}

int VoEFileImpl::ConvertPCMToWAV(const char* fileNameInUTF8,
                                 const char* fileNameOutUTF8)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "ConvertPCMToWAV(fileNameInUTF8=%s, fileNameOutUTF8=%s)",
                 fileNameInUTF8, fileNameOutUTF8);

    // Create file player object
    FilePlayer& playerObj(*FilePlayer::CreateFilePlayer(
        -1,
        kFileFormatPcm16kHzFile));

    int res=playerObj.StartPlayingFile(fileNameInUTF8,false,0,1.0,0,0, NULL);
    if (res)
    {
        _engineStatistics.SetLastError(
            VE_BAD_FILE, kTraceError,
            "ConvertPCMToWAV failed to create player object");
        playerObj.StopPlayingFile();
        FilePlayer::DestroyFilePlayer(&playerObj);
        return -1;
    }

    // Create file recorder object
    FileRecorder& recObj(*FileRecorder::CreateFileRecorder(
        -1, kFileFormatWavFile));

    CodecInst codecInst;
    strncpy(codecInst.plname,"L16",32);
            codecInst.channels = 1;
            codecInst.rate     = 256000;
            codecInst.plfreq   = 16000;
            codecInst.pltype   = 94;
            codecInst.pacsize  = 160;

    res = recObj.StartRecordingAudioFile(fileNameOutUTF8,codecInst,0);
    if (res)
    {
        _engineStatistics.SetLastError(
            VE_BAD_FILE, kTraceError,
            "ConvertPCMToWAV failed to create recorder object");
        playerObj.StopPlayingFile();
        FilePlayer::DestroyFilePlayer(&playerObj);
        recObj.StopRecording();
        FileRecorder::DestroyFileRecorder(&recObj);
        return -1;
    }

    // Run throught the file
    AudioFrame audioFrame;
    WebRtc_Word16 decodedData[160];
    WebRtc_UWord32 decLength=0;
    const WebRtc_UWord32 frequency = 16000;

    while(!playerObj.Get10msAudioFromFile(decodedData,decLength,frequency))
    {
        if(decLength!=frequency/100)
        {
            // This is an OK way to end
            break;
        }

        res=audioFrame.UpdateFrame(-1, 0, decodedData,
                                  (WebRtc_UWord16)decLength,
                                   frequency, AudioFrame::kNormalSpeech,
                                   AudioFrame::kVadActive);
        if(res)
        {
            WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId,-1),
                         "ConvertPCMToWAV failed during conversion "
                         "(audio frame)");
            break;
        }

        res=recObj.RecordAudioToFile(audioFrame);
        if(res)
        {
            WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId,-1),
                         "ConvertPCMToWAV failed during converstion "
                         "(write frame)");
        }
    }

    playerObj.StopPlayingFile();
    recObj.StopRecording();
    FilePlayer::DestroyFilePlayer(&playerObj);
    FileRecorder::DestroyFileRecorder(&recObj);

    return res;
}

int VoEFileImpl::ConvertPCMToWAV(InStream* streamIn, OutStream* streamOut)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "ConvertPCMToWAV(streamIn, streamOut)");

    if ((streamIn == NULL) || (streamOut == NULL))
    {
        WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId,-1),
            "invalid stream handles");
        return (-1);
    }

    // Create file player object
    FilePlayer& playerObj(*FilePlayer::CreateFilePlayer(-1,
        kFileFormatPcm16kHzFile));
    int res = playerObj.StartPlayingFile(*streamIn,0,1.0,0,0,NULL);
    if (res)
    {
        _engineStatistics.SetLastError(VE_BAD_FILE, kTraceError,
            "ConvertPCMToWAV failed to create player object");
        playerObj.StopPlayingFile();
        FilePlayer::DestroyFilePlayer(&playerObj);
        return -1;
    }

    // Create file recorder object
    FileRecorder& recObj(*FileRecorder::CreateFileRecorder(-1,
        kFileFormatWavFile));
    CodecInst codecInst;
    strncpy(codecInst.plname, "L16", 32);
            codecInst.channels = 1;
            codecInst.rate     = 256000;
            codecInst.plfreq   = 16000;
            codecInst.pltype   = 94;
            codecInst.pacsize  = 160;
    res = recObj.StartRecordingAudioFile(*streamOut,codecInst,0);
    if (res)
    {
        _engineStatistics.SetLastError(VE_BAD_FILE, kTraceError,
            "ConvertPCMToWAV failed to create recorder object");
        playerObj.StopPlayingFile();
        FilePlayer::DestroyFilePlayer(&playerObj);
        recObj.StopRecording();
        FileRecorder::DestroyFileRecorder(&recObj);
        return -1;
    }

    // Run throught the file
    AudioFrame audioFrame;
    WebRtc_Word16 decodedData[160];
    WebRtc_UWord32 decLength=0;
    const WebRtc_UWord32 frequency = 16000;

    while(!playerObj.Get10msAudioFromFile(decodedData,decLength,frequency))
    {
        if(decLength!=frequency/100)
        {
            // This is an OK way to end
            break;
        }

        res=audioFrame.UpdateFrame(-1, 0, decodedData,
                                  (WebRtc_UWord16)decLength, frequency,
                                   AudioFrame::kNormalSpeech,
                                   AudioFrame::kVadActive);
        if(res)
        {
            WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId,-1),
                         "ConvertPCMToWAV failed during conversion "
                         "(create audio frame)");
            break;
        }

        res=recObj.RecordAudioToFile(audioFrame);
        if(res)
        {
            WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId,-1),
                         "ConvertPCMToWAV failed during converstion "
                         "(write frame)");
        }
    }

    playerObj.StopPlayingFile();
    recObj.StopRecording();
    FilePlayer::DestroyFilePlayer(&playerObj);
    FileRecorder::DestroyFileRecorder(&recObj);

    return res;
}

int VoEFileImpl::ConvertWAVToPCM(const char* fileNameInUTF8,
                                 const char* fileNameOutUTF8)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "ConvertWAVToPCM(fileNameInUTF8=%s, fileNameOutUTF8=%s)",
                 fileNameInUTF8, fileNameOutUTF8);

    // Create file player object
    FilePlayer& playerObj(*FilePlayer::CreateFilePlayer(-1,
                                                        kFileFormatWavFile));
    int res = playerObj.StartPlayingFile(fileNameInUTF8,false,0,1.0,0,0,NULL);
    if (res)
    {
        _engineStatistics.SetLastError(
            VE_BAD_FILE, kTraceError,
            "ConvertWAVToPCM failed to create player object");
        playerObj.StopPlayingFile();
        FilePlayer::DestroyFilePlayer(&playerObj);
        return -1;
    }

    // Create file recorder object
    FileRecorder& recObj(*FileRecorder::CreateFileRecorder(
        -1, kFileFormatPcm16kHzFile));

    CodecInst codecInst;
    strncpy(codecInst.plname,"L16",32);
            codecInst.channels = 1;
            codecInst.rate     = 256000;
            codecInst.plfreq   = 16000;
            codecInst.pltype   = 94;
            codecInst.pacsize  = 160;

    res = recObj.StartRecordingAudioFile(fileNameOutUTF8,codecInst,0);
    if (res)
    {
        _engineStatistics.SetLastError(
            VE_BAD_FILE, kTraceError,
            "ConvertWAVToPCM failed to create recorder object");
        playerObj.StopPlayingFile();
        FilePlayer::DestroyFilePlayer(&playerObj);
        recObj.StopRecording();
        FileRecorder::DestroyFileRecorder(&recObj);
        return -1;
    }

    // Run throught the file
    AudioFrame audioFrame;
    WebRtc_Word16 decodedData[160];
    WebRtc_UWord32 decLength=0;
    const WebRtc_UWord32 frequency = 16000;

    while(!playerObj.Get10msAudioFromFile(decodedData,decLength,frequency))
    {
        if(decLength!=frequency/100)
        {
            // This is an OK way to end
            break;
        }

        res=audioFrame.UpdateFrame(-1, 0, decodedData,
                                   (WebRtc_UWord16)decLength,
                                   frequency, AudioFrame::kNormalSpeech,
                                   AudioFrame::kVadActive);
        if(res)
        {
            WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId,-1),
                         "ConvertWAVToPCM failed during conversion "
                         "(audio frame)");
            break;
        }

        res=recObj.RecordAudioToFile(audioFrame);
        if(res)
        {
            WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId,-1),
                         "ConvertWAVToPCM failed during converstion "
                         "(write frame)");
        }
    }

    playerObj.StopPlayingFile();
    recObj.StopRecording();
    FilePlayer::DestroyFilePlayer(&playerObj);
    FileRecorder::DestroyFileRecorder(&recObj);

    return res;
}

int VoEFileImpl::ConvertWAVToPCM(InStream* streamIn, OutStream* streamOut)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "ConvertWAVToPCM(streamIn, streamOut)");

    if ((streamIn == NULL) || (streamOut == NULL))
    {
        WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId,-1),
                     "invalid stream handles");
        return (-1);
    }

    // Create file player object
    FilePlayer& playerObj(*FilePlayer::CreateFilePlayer(-1,
                                                        kFileFormatWavFile));
    int res = playerObj.StartPlayingFile(*streamIn,0,1.0,0,0,NULL);
    if (res)
    {
        _engineStatistics.SetLastError(VE_BAD_FILE, kTraceError,
            "ConvertWAVToPCM failed to create player object");
        playerObj.StopPlayingFile();
        FilePlayer::DestroyFilePlayer(&playerObj);
        return -1;
    }

    // Create file recorder object
    FileRecorder& recObj(*FileRecorder::CreateFileRecorder(
        -1, kFileFormatPcm16kHzFile));

    CodecInst codecInst;
    strncpy(codecInst.plname,"L16",32);
            codecInst.channels = 1;
            codecInst.rate     = 256000;
            codecInst.plfreq   = 16000;
            codecInst.pltype   = 94;
            codecInst.pacsize  = 160;

    res = recObj.StartRecordingAudioFile(*streamOut,codecInst,0);
    if (res)
    {
        _engineStatistics.SetLastError(VE_BAD_FILE, kTraceError,
            "ConvertWAVToPCM failed to create recorder object");
        playerObj.StopPlayingFile();
        FilePlayer::DestroyFilePlayer(&playerObj);
        recObj.StopRecording();
        FileRecorder::DestroyFileRecorder(&recObj);
        return -1;
    }

    // Run throught the file
    AudioFrame audioFrame;
    WebRtc_Word16 decodedData[160];
    WebRtc_UWord32 decLength=0;
    const WebRtc_UWord32 frequency = 16000;

    while(!playerObj.Get10msAudioFromFile(decodedData,decLength,frequency))
    {
        if(decLength!=frequency/100)
        {
            // This is an OK way to end
            break;
        }

        res=audioFrame.UpdateFrame(-1, 0, decodedData,
                                  (WebRtc_UWord16)decLength, frequency,
                                   AudioFrame::kNormalSpeech,
                                   AudioFrame::kVadActive);
        if(res)
        {
            WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId,-1),
                         "ConvertWAVToPCM failed during conversion "
                         "(audio frame)");
            break;
        }

        res=recObj.RecordAudioToFile(audioFrame);
        if(res)
        {
            WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId,-1),
                         "ConvertWAVToPCM failed during converstion"
                         " (write frame)");
        }
    }

    playerObj.StopPlayingFile();
    recObj.StopRecording();
    FilePlayer::DestroyFilePlayer(&playerObj);
    FileRecorder::DestroyFileRecorder(&recObj);

    return res;
}

int VoEFileImpl::ConvertPCMToCompressed(const char* fileNameInUTF8,
                                        const char* fileNameOutUTF8,
                                        CodecInst* compression)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "ConvertPCMToCompressed(fileNameInUTF8=%s, fileNameOutUTF8=%s"
                 ",  compression)", fileNameInUTF8, fileNameOutUTF8);
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,-1),
                 "  compression: plname=%s, plfreq=%d, pacsize=%d",
                 compression->plname, compression->plfreq,
                 compression->pacsize);

    // Create file player object
    FilePlayer& playerObj(*FilePlayer::CreateFilePlayer(
        -1,
        kFileFormatPcm16kHzFile));
    int res = playerObj.StartPlayingFile(fileNameInUTF8,false,0,1.0,0,0, NULL);
    if (res)
    {
        _engineStatistics.SetLastError(VE_BAD_FILE, kTraceError,
            "ConvertPCMToCompressed failed to create player object");
        // Clean up and shutdown the file player
        playerObj.StopPlayingFile();
        FilePlayer::DestroyFilePlayer(&playerObj);
        return -1;
    }

    // Create file recorder object
    FileRecorder& recObj(*FileRecorder::CreateFileRecorder(
        -1,
        kFileFormatCompressedFile));
    res = recObj.StartRecordingAudioFile(fileNameOutUTF8, *compression,0);
    if (res)
    {
        _engineStatistics.SetLastError(VE_BAD_FILE, kTraceError,
            "ConvertPCMToCompressed failed to create recorder object");
        playerObj.StopPlayingFile();
        FilePlayer::DestroyFilePlayer(&playerObj);
        recObj.StopRecording();
        FileRecorder::DestroyFileRecorder(&recObj);
        return -1;
    }

    // Run throught the file
    AudioFrame audioFrame;
    WebRtc_Word16 decodedData[160];
    WebRtc_UWord32 decLength=0;
    const WebRtc_UWord32 frequency = 16000;

    while(!playerObj.Get10msAudioFromFile(decodedData,decLength,frequency))
    {
        if(decLength!=frequency/100)
        {
            // This is an OK way to end
            break;
        }
        res=audioFrame.UpdateFrame(-1, 0, decodedData,
                                  (WebRtc_UWord16)decLength,
                                  frequency, AudioFrame::kNormalSpeech,
                                  AudioFrame::kVadActive);
        if(res)
        {
            WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId,-1),
                         "ConvertPCMToCompressed failed during conversion "
                         "(audio frame)");
            break;
        }

        res=recObj.RecordAudioToFile(audioFrame);
        if(res)
        {
            WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId,-1),
                         "ConvertPCMToCompressed failed during converstion "
                         "(write frame)");
        }
    }

    playerObj.StopPlayingFile();
    recObj.StopRecording();
    FilePlayer::DestroyFilePlayer(&playerObj);
    FileRecorder::DestroyFileRecorder(&recObj);

    return res;
}

int VoEFileImpl::ConvertPCMToCompressed(InStream* streamIn,
                                        OutStream* streamOut,
                                        CodecInst* compression)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "ConvertPCMToCompressed(streamIn, streamOut, compression)");

    if ((streamIn == NULL) || (streamOut == NULL))
    {
        WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId,-1),
                     "invalid stream handles");
        return (-1);
    }

    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,-1),
                 "  compression: plname=%s, plfreq=%d, pacsize=%d",
                 compression->plname, compression->plfreq,
                 compression->pacsize);

    // Create file player object
    FilePlayer& playerObj(*FilePlayer::CreateFilePlayer(
        -1, kFileFormatPcm16kHzFile));

    int res = playerObj.StartPlayingFile(*streamIn,0,1.0,0,0,NULL);
    if (res)
    {
        _engineStatistics.SetLastError(VE_BAD_FILE, kTraceError,
            "ConvertPCMToCompressed failed to create player object");
        playerObj.StopPlayingFile();
        FilePlayer::DestroyFilePlayer(&playerObj);
        return -1;
    }

    // Create file recorder object
    FileRecorder& recObj(*FileRecorder::CreateFileRecorder(
        -1, kFileFormatCompressedFile));
    res = recObj.StartRecordingAudioFile(*streamOut,*compression,0);
    if (res)
    {
        _engineStatistics.SetLastError(VE_BAD_FILE, kTraceError,
            "ConvertPCMToCompressed failed to create recorder object");
        playerObj.StopPlayingFile();
        FilePlayer::DestroyFilePlayer(&playerObj);
        recObj.StopRecording();
        FileRecorder::DestroyFileRecorder(&recObj);
        return -1;
    }

    // Run throught the file
    AudioFrame audioFrame;
    WebRtc_Word16 decodedData[160];
    WebRtc_UWord32 decLength=0;
    const WebRtc_UWord32 frequency = 16000;

    while(!playerObj.Get10msAudioFromFile(decodedData,decLength,frequency))
    {
        if(decLength!=frequency/100)
        {
            // This is an OK way to end
            break;
        }
        res=audioFrame.UpdateFrame(-1, 0, decodedData,
                                  (WebRtc_UWord16)decLength,
                                   frequency, AudioFrame::kNormalSpeech,
                                   AudioFrame::kVadActive);
        if(res)
        {
            WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId,-1),
                         "ConvertPCMToCompressed failed during conversion"
                         " (audio frame)");
            break;
        }

        res=recObj.RecordAudioToFile(audioFrame);
        if(res)
        {
            WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId,-1),
                         "ConvertPCMToCompressed failed during converstion "
                         "(write frame)");
        }
    }

    playerObj.StopPlayingFile();
    recObj.StopRecording();
    FilePlayer::DestroyFilePlayer(&playerObj);
    FileRecorder::DestroyFileRecorder(&recObj);

    return res;
}

int VoEFileImpl::ConvertCompressedToPCM(const char* fileNameInUTF8,
                                        const char* fileNameOutUTF8)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "ConvertCompressedToPCM(fileNameInUTF8=%s,"
                 " fileNameOutUTF8=%s)",
                 fileNameInUTF8, fileNameOutUTF8);

    // Create file player object
    FilePlayer& playerObj(*FilePlayer::CreateFilePlayer(
        -1, kFileFormatCompressedFile));

    int res = playerObj.StartPlayingFile(fileNameInUTF8,false,0,1.0,0,0,NULL);
    if (res)
    {
        _engineStatistics.SetLastError(VE_BAD_FILE, kTraceError,
            "ConvertCompressedToPCM failed to create player object");
        playerObj.StopPlayingFile();
        FilePlayer::DestroyFilePlayer(&playerObj);
        return -1;
    }

    // Create file recorder object
    FileRecorder& recObj(*FileRecorder::CreateFileRecorder(
        -1, kFileFormatPcm16kHzFile));

    CodecInst codecInst;
    strncpy(codecInst.plname,"L16",32);
            codecInst.channels = 1;
            codecInst.rate     = 256000;
            codecInst.plfreq   = 16000;
            codecInst.pltype   = 94;
            codecInst.pacsize  = 160;

    res = recObj.StartRecordingAudioFile(fileNameOutUTF8,codecInst,0);
    if (res)
    {
        _engineStatistics.SetLastError(VE_BAD_FILE, kTraceError,
            "ConvertCompressedToPCM failed to create recorder object");
        playerObj.StopPlayingFile();
        FilePlayer::DestroyFilePlayer(&playerObj);
        recObj.StopRecording();
        FileRecorder::DestroyFileRecorder(&recObj);
        return -1;
    }

    // Run throught the file
    AudioFrame audioFrame;
    WebRtc_Word16 decodedData[160];
    WebRtc_UWord32 decLength=0;
    const WebRtc_UWord32 frequency = 16000;

    while(!playerObj.Get10msAudioFromFile(decodedData,decLength,frequency))
    {
        if(decLength!=frequency/100)
        {
            // This is an OK way to end
            break;
        }
        res=audioFrame.UpdateFrame(-1, 0, decodedData,
                                  (WebRtc_UWord16)decLength,
                                   frequency,
                                   AudioFrame::kNormalSpeech,
                                   AudioFrame::kVadActive);
        if(res)
        {
            WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId,-1),
                         "ConvertCompressedToPCM failed during conversion "
                         "(create audio frame)");
            break;
        }

        res=recObj.RecordAudioToFile(audioFrame);
        if(res)
        {
            WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId,-1),
                         "ConvertCompressedToPCM failed during converstion "
                         "(write frame)");
        }
    }

    playerObj.StopPlayingFile();
    recObj.StopRecording();
    FilePlayer::DestroyFilePlayer(&playerObj);
    FileRecorder::DestroyFileRecorder(&recObj);

    return res;
}

int VoEFileImpl::ConvertCompressedToPCM(InStream* streamIn,
                                        OutStream* streamOut)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "ConvertCompressedToPCM(file, file);");

    if ((streamIn == NULL) || (streamOut == NULL))
    {
        WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId,-1),
            "invalid stream handles");
        return (-1);
    }

    // Create file player object
    FilePlayer& playerObj(*FilePlayer::CreateFilePlayer(
        -1, kFileFormatCompressedFile));
    int res;

    res = playerObj.StartPlayingFile(*streamIn,0,1.0,0,0,NULL);
    if (res)
    {
        _engineStatistics.SetLastError(VE_BAD_FILE, kTraceError,
            "ConvertCompressedToPCM failed to create player object");
        playerObj.StopPlayingFile();
        FilePlayer::DestroyFilePlayer(&playerObj);
        return -1;
    }

    // Create file recorder object
    FileRecorder& recObj(*FileRecorder::CreateFileRecorder(
        -1, kFileFormatPcm16kHzFile));

    CodecInst codecInst;
    strncpy(codecInst.plname,"L16",32);
            codecInst.channels = 1;
            codecInst.rate     = 256000;
            codecInst.plfreq   = 16000;
            codecInst.pltype   = 94;
            codecInst.pacsize  = 160;

    res = recObj.StartRecordingAudioFile(*streamOut,codecInst,0);
    if (res)
    {
        _engineStatistics.SetLastError(VE_BAD_FILE, kTraceError,
            "ConvertCompressedToPCM failed to create recorder object");
        playerObj.StopPlayingFile();
        FilePlayer::DestroyFilePlayer(&playerObj);
        recObj.StopRecording();
        FileRecorder::DestroyFileRecorder(&recObj);
        return -1;
    }

    // Run throught the file
    AudioFrame audioFrame;
    WebRtc_Word16 decodedData[160];
    WebRtc_UWord32 decLength=0;
    const WebRtc_UWord32 frequency = 16000;

    while(!playerObj.Get10msAudioFromFile(decodedData,decLength,frequency))
    {
        if(decLength!=frequency/100)
        {
            // This is an OK way to end
            break;
        }
        res=audioFrame.UpdateFrame(-1, 0, decodedData,
                                  (WebRtc_UWord16)decLength,
                                   frequency,
                                   AudioFrame::kNormalSpeech,
                                   AudioFrame::kVadActive);
        if(res)
        {
            WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId,-1),
                         "ConvertCompressedToPCM failed during conversion"
                         " (audio frame)");
            break;
        }

        res=recObj.RecordAudioToFile(audioFrame);
        if(res)
        {
            WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId,-1),
                         "ConvertCompressedToPCM failed during converstion"
                         " (write frame)");
        }
    }

    playerObj.StopPlayingFile();
    recObj.StopRecording();
    FilePlayer::DestroyFilePlayer(&playerObj);
    FileRecorder::DestroyFileRecorder(&recObj);

    return res;
}


int VoEFileImpl::GetFileDuration(const char* fileNameUTF8,
                                 int& durationMs,
                                 FileFormats format)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "GetFileDuration(fileNameUTF8=%s, format=%d)",
                 fileNameUTF8, format);

    // Create a dummy file module for this
    MediaFile * fileModule=MediaFile::CreateMediaFile(-1);

    // Temp container of the right format
    WebRtc_UWord32 duration;
    int res=fileModule->FileDurationMs(fileNameUTF8,duration,format);
    if (res)
    {
        _engineStatistics.SetLastError(VE_BAD_FILE, kTraceError,
            "GetFileDuration() failed measure file duration");
        return -1;
    }
    durationMs = duration;
    MediaFile::DestroyMediaFile(fileModule);
    fileModule = NULL;

    return(res);
}

int VoEFileImpl::GetPlaybackPosition(int channel, int& positionMs)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "GetPlaybackPosition(channel=%d)", channel);

    voe::ScopedChannel sc(_channelManager, channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _engineStatistics.SetLastError(
            VE_CHANNEL_NOT_VALID, kTraceError,
            "GetPlaybackPosition() failed to locate channel");
        return -1;
    }
    return channelPtr->GetLocalPlayoutPosition(positionMs);
}

}  // namespace webrtc

#endif  // #ifdef WEBRTC_VOICE_ENGINE_FILE_API
