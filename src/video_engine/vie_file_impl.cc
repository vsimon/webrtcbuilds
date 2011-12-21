/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "engine_configurations.h"

#include "video_engine/vie_file_impl.h"

#ifdef WEBRTC_VIDEO_ENGINE_FILE_API
#include "common_video/jpeg/main/interface/jpeg.h"
#include "system_wrappers/interface/condition_variable_wrapper.h"
#include "system_wrappers/interface/critical_section_wrapper.h"
#include "system_wrappers/interface/trace.h"
#include "video_engine/main/interface/vie_errors.h"
#include "video_engine/vie_capturer.h"
#include "video_engine/vie_channel.h"
#include "video_engine/vie_channel_manager.h"
#include "video_engine/vie_defines.h"
#include "video_engine/vie_encoder.h"
#include "video_engine/vie_file_image.h"
#include "video_engine/vie_file_player.h"
#include "video_engine/vie_file_recorder.h"
#include "video_engine/vie_impl.h"
#include "video_engine/vie_input_manager.h"
#include "video_engine/vie_render_manager.h"
#include "video_engine/vie_renderer.h"
#endif

namespace webrtc {

ViEFile* ViEFile::GetInterface(VideoEngine* video_engine) {
#ifdef WEBRTC_VIDEO_ENGINE_FILE_API
  if (!video_engine) {
    return NULL;
  }
  VideoEngineImpl* vie_impl = reinterpret_cast<VideoEngineImpl*>(video_engine);
  ViEFileImpl* vie_file_impl = vie_impl;
  // Increase ref count.
  (*vie_file_impl)++;
  return vie_file_impl;
#else
  return NULL;
#endif
}

#ifdef WEBRTC_VIDEO_ENGINE_FILE_API

int ViEFileImpl::Release() {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, instance_id_,
               "ViEFile::Release()");
  // Decrease ref count.
  (*this)--;
  WebRtc_Word32 ref_count = GetCount();
  if (ref_count < 0) {
    WEBRTC_TRACE(kTraceWarning, kTraceVideo, instance_id_,
                 "ViEFile release too many times");
    SetLastError(kViEAPIDoesNotExist);
    return -1;
  }
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, instance_id_,
               "ViEFile reference count: %d", ref_count);
  return ref_count;
}

ViEFileImpl::ViEFileImpl() {
  WEBRTC_TRACE(kTraceMemory, kTraceVideo, instance_id_,
               "ViEFileImpl::ViEFileImpl() Ctor");
}

ViEFileImpl::~ViEFileImpl() {
  WEBRTC_TRACE(kTraceMemory, kTraceVideo, instance_id_,
               "ViEFileImpl::~ViEFileImpl() Dtor");
}

int ViEFileImpl::StartPlayFile(const char* file_nameUTF8,
                               int& file_id,
                               const bool loop,
                               const FileFormats file_format) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(instance_id_), "%s",
               __FUNCTION__);

  if (!Initialized()) {
    SetLastError(kViENotInitialized);
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_),
                 "%s - ViE instance %d not initialized", __FUNCTION__,
                 instance_id_);
    return -1;
  }

  VoiceEngine* voice = channel_manager_.GetVoiceEngine();
  const WebRtc_Word32 result = input_manager_.CreateFilePlayer(file_nameUTF8,
                                                               loop,
                                                               file_format,
                                                               voice, file_id);
  if (result != 0) {
    SetLastError(result);
    return -1;
  }
  return 0;
}

int ViEFileImpl::StopPlayFile(const int file_id) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(instance_id_),
               "%s(file_id: %d)", __FUNCTION__, file_id);
  {
    ViEInputManagerScoped is(input_manager_);
    ViEFilePlayer* vie_file_player = is.FilePlayer(file_id);
    if (!vie_file_player) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_),
                   "%s: File with id %d is not playing.", __FUNCTION__,
                   file_id);
      SetLastError(kViEFileNotPlaying);
      return -1;
    }
  }
  // Destroy the capture device.
  return input_manager_.DestroyFilePlayer(file_id);
}

int ViEFileImpl::RegisterObserver(int file_id, ViEFileObserver& observer) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(instance_id_),
               "%s(file_id: %d)", __FUNCTION__, file_id);

  ViEInputManagerScoped is(input_manager_);
  ViEFilePlayer* vie_file_player = is.FilePlayer(file_id);
  if (!vie_file_player) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_),
                 "%s: File with id %d is not playing.", __FUNCTION__,
                 file_id);
    SetLastError(kViEFileNotPlaying);
    return -1;
  }
  if (vie_file_player->IsObserverRegistered()) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_, file_id),
                 "%s: Observer already registered", __FUNCTION__);
    SetLastError(kViEFileObserverAlreadyRegistered);
    return -1;
  }
  if (vie_file_player->RegisterObserver(observer) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_, file_id),
                 "%s: Failed to register observer", __FUNCTION__, file_id);
    SetLastError(kViEFileUnknownError);
    return -1;
  }
  return 0;
}

int ViEFileImpl::DeregisterObserver(int file_id, ViEFileObserver& observer) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(instance_id_),
               "%s(file_id: %d)", __FUNCTION__, file_id);

  ViEInputManagerScoped is(input_manager_);
  ViEFilePlayer* vie_file_player = is.FilePlayer(file_id);
  if (!vie_file_player) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_),
                 "%s: File with id %d is not playing.", __FUNCTION__,
                 file_id);
    SetLastError(kViEFileNotPlaying);
    return -1;
  }
  if (!vie_file_player->IsObserverRegistered()) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(instance_id_, file_id), "%s: No Observer registered",
                 __FUNCTION__);
    SetLastError(kViEFileObserverNotRegistered);
    return -1;
  }
  if (vie_file_player->DeRegisterObserver() != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_, file_id),
                 "%s: Failed to deregister observer", __FUNCTION__, file_id);
    SetLastError(kViEFileUnknownError);
    return -1;
  }
  return 0;
}

int ViEFileImpl::SendFileOnChannel(const int file_id, const int video_channel) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(instance_id_),
               "%s(file_id: %d)", __FUNCTION__, file_id);

  ViEChannelManagerScoped cs(channel_manager_);
  ViEEncoder* vie_encoder = cs.Encoder(video_channel);
  if (!vie_encoder) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_, video_channel),
                 "%s: Channel %d doesn't exist", __FUNCTION__, video_channel);
    SetLastError(kViEFileInvalidChannelId);
    return -1;
  }

  ViEInputManagerScoped is(input_manager_);
  if (is.FrameProvider(vie_encoder) != NULL) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_, video_channel),
                 "%s: Channel %d already connected to a capture device or "
                 "file.", __FUNCTION__, video_channel);
    SetLastError(kViEFileInputAlreadyConnected);
    return -1;
  }

  ViEFilePlayer* vie_file_player = is.FilePlayer(file_id);
  if (!vie_file_player) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_),
                 "%s: File with id %d is not playing.", __FUNCTION__,
                 file_id);
    SetLastError(kViEFileNotPlaying);
    return -1;
  }

  if (vie_file_player->RegisterFrameCallback(video_channel, vie_encoder)
      != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_),
                 "%s: Failed to register frame callback.", __FUNCTION__,
                 file_id);
    SetLastError(kViEFileUnknownError);
    return -1;
  }
  return 0;
}

int ViEFileImpl::StopSendFileOnChannel(const int video_channel) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(instance_id_),
               "%s(video_channel: %d)", __FUNCTION__, video_channel);

  ViEChannelManagerScoped cs(channel_manager_);
  ViEEncoder* vie_encoder = cs.Encoder(video_channel);
  if (!vie_encoder) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_, video_channel),
                 "%s: Channel %d doesn't exist", __FUNCTION__, video_channel);
    SetLastError(kViEFileInvalidChannelId);
    return -1;
  }

  ViEInputManagerScoped is(input_manager_);
  ViEFrameProviderBase* frame_provider = is.FrameProvider(vie_encoder);
  if (!frame_provider ||
      frame_provider->Id() < kViEFileIdBase ||
      frame_provider->Id() > kViEFileIdMax) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_, video_channel),
                 "%s: No file connected to Channel %d", __FUNCTION__,
                 video_channel);
    SetLastError(kViEFileNotConnected);
    return -1;
  }
  if (frame_provider->DeregisterFrameCallback(vie_encoder) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_, video_channel),
                 "%s: Failed to deregister file from channel %d",
                 __FUNCTION__, video_channel);
    SetLastError(kViEFileUnknownError);
  }
  return 0;
}

int ViEFileImpl::StartPlayFileAsMicrophone(const int file_id,
                                           const int audio_channel,
                                           bool mix_microphone,
                                           float volume_scaling) {
  ViEInputManagerScoped is(input_manager_);

  ViEFilePlayer* vie_file_player = is.FilePlayer(file_id);
  if (!vie_file_player) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_),
                 "%s: File with id %d is not playing.", __FUNCTION__,
                 file_id);
    SetLastError(kViEFileNotPlaying);
    return -1;
  }
  if (vie_file_player->SendAudioOnChannel(audio_channel, mix_microphone,
  volume_scaling) != 0) {
    SetLastError(kViEFileVoEFailure);
    return -1;
  }
  return 0;
}

int ViEFileImpl::StopPlayFileAsMicrophone(const int file_id,
const int audio_channel) {
  ViEInputManagerScoped is(input_manager_);

  ViEFilePlayer* vie_file_player = is.FilePlayer(file_id);
  if (!vie_file_player) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_),
                 "%s: File with id %d is not playing.", __FUNCTION__,
                 file_id);
    SetLastError(kViEFileNotPlaying);
    return -1;
  }

  if (vie_file_player->StopSendAudioOnChannel(audio_channel) != 0) {
    SetLastError(kViEFileVoEFailure);
    return -1;
  }
  return 0;
}

int ViEFileImpl::StartPlayAudioLocally(const int file_id,
                                       const int audio_channel,
                                       float volume_scaling) {
  ViEInputManagerScoped is(input_manager_);

  ViEFilePlayer* vie_file_player = is.FilePlayer(file_id);
  if (!vie_file_player) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_),
                 "%s: File with id %d is not playing.", __FUNCTION__,
                 file_id);
    SetLastError(kViEFileNotPlaying);
    return -1;
  }
  if (vie_file_player->PlayAudioLocally(audio_channel, volume_scaling) != 0) {
    SetLastError(kViEFileVoEFailure);
    return -1;
  }
  return 0;
}

int ViEFileImpl::StopPlayAudioLocally(const int file_id,
                                      const int audio_channel) {
  ViEInputManagerScoped is(input_manager_);

  ViEFilePlayer* vie_file_player = is.FilePlayer(file_id);
  if (!vie_file_player) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_),
                 "%s: File with id %d is not playing.", __FUNCTION__,
                 file_id);
    SetLastError(kViEFileNotPlaying);
    return -1;
  }
  if (vie_file_player->StopPlayAudioLocally(audio_channel) != 0) {
    SetLastError(kViEFileVoEFailure);
    return -1;
  }
  return 0;
}

int ViEFileImpl::StartRecordOutgoingVideo(const int video_channel,
                                          const char* file_nameUTF8,
                                          AudioSource audio_source,
                                          const CodecInst& audio_codec,
                                          const VideoCodec& video_codec,
                                          const FileFormats file_format) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(instance_id_, video_channel),
               "%s video_channel: %d)", __FUNCTION__, video_channel);

  ViEChannelManagerScoped cs(channel_manager_);
  ViEEncoder* vie_encoder = cs.Encoder(video_channel);
  if (!vie_encoder) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_, video_channel),
                 "%s: Channel %d doesn't exist", __FUNCTION__, video_channel);
    SetLastError(kViEFileInvalidChannelId);
    return -1;
  }
  ViEFileRecorder& file_recorder = vie_encoder->GetOutgoingFileRecorder();
  if (file_recorder.RecordingStarted()) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_, video_channel),
                 "%s: Already recording outgoing video on channel %d",
                 __FUNCTION__, video_channel);
    SetLastError(kViEFileAlreadyRecording);
    return -1;
  }

  WebRtc_Word32 ve_channel_id = -1;
  VoiceEngine* ve_ptr = NULL;
  if (audio_source != NO_AUDIO) {
    ViEChannel* vie_channel = cs.Channel(video_channel);
    ve_channel_id = vie_channel->VoiceChannel();
    ve_ptr = channel_manager_.GetVoiceEngine();
    if (!ve_ptr) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_, video_channel),
                   "%s: Can't access voice engine. Have SetVoiceEngine "
                   "been called?", __FUNCTION__);
      SetLastError(kViEFileVoENotSet);
      return -1;
    }
  }
  if (file_recorder.StartRecording(file_nameUTF8, video_codec, audio_source,
                                   ve_channel_id, audio_codec, ve_ptr,
                                   file_format) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_, video_channel),
                 "%s: Failed to start recording. Check arguments.",
                 __FUNCTION__);
    SetLastError(kViEFileUnknownError);
    return -1;
  }

  return 0;
}

int ViEFileImpl::StopRecordOutgoingVideo(const int video_channel) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(instance_id_, video_channel), "%s video_channel: %d)",
               __FUNCTION__, video_channel);

  ViEChannelManagerScoped cs(channel_manager_);
  ViEEncoder* vie_encoder = cs.Encoder(video_channel);
  if (!vie_encoder) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_, video_channel),
                 "%s: Channel %d doesn't exist", __FUNCTION__, video_channel);
    SetLastError(kViEFileInvalidChannelId);
    return -1;
  }
  ViEFileRecorder& file_recorder = vie_encoder->GetOutgoingFileRecorder();
  if (!file_recorder.RecordingStarted()) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_, video_channel),
                 "%s: Channel %d is not recording.", __FUNCTION__,
                 video_channel);
    SetLastError(kViEFileNotRecording);
    return -1;
  }
  if (file_recorder.StopRecording() != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_, video_channel),
                 "%s: Failed to stop recording of channel %d.", __FUNCTION__,
                 video_channel);
    SetLastError(kViEFileUnknownError);
    return -1;
  }
  return 0;
}

int ViEFileImpl::StopRecordIncomingVideo(const int video_channel) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(instance_id_, video_channel),
               "%s video_channel: %d)", __FUNCTION__, video_channel);

  ViEChannelManagerScoped cs(channel_manager_);
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_, video_channel),
                 "%s: Channel %d doesn't exist", __FUNCTION__, video_channel);
    SetLastError(kViEFileInvalidChannelId);
    return -1;
  }
  ViEFileRecorder& file_recorder = vie_channel->GetIncomingFileRecorder();
  if (!file_recorder.RecordingStarted()) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_, video_channel),
                 "%s: Channel %d is not recording.", __FUNCTION__,
                 video_channel);
    SetLastError(kViEFileNotRecording);
    vie_channel->ReleaseIncomingFileRecorder();
    return -1;
  }
  if (file_recorder.StopRecording() != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_, video_channel),
                 "%s: Failed to stop recording of channel %d.",
                 __FUNCTION__, video_channel);
    SetLastError(kViEFileUnknownError);
    vie_channel->ReleaseIncomingFileRecorder();
    return -1;
  }
  // Let the channel know we are no longer recording.
  vie_channel->ReleaseIncomingFileRecorder();
  return 0;
}

int ViEFileImpl::StartRecordIncomingVideo(const int video_channel,
                                          const char* file_nameUTF8,
                                          AudioSource audio_source,
                                          const CodecInst& audio_codec,
                                          const VideoCodec& video_codec,
                                          const FileFormats file_format) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(instance_id_, video_channel),
               "%s video_channel: %d)", __FUNCTION__, video_channel);

  ViEChannelManagerScoped cs(channel_manager_);
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_, video_channel),
                 "%s: Channel %d doesn't exist", __FUNCTION__,
                 video_channel);
    SetLastError(kViEFileInvalidChannelId);
    return -1;
  }
  ViEFileRecorder& file_recorder = vie_channel->GetIncomingFileRecorder();
  if (file_recorder.RecordingStarted()) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_, video_channel),
                 "%s: Already recording outgoing video on channel %d",
                 __FUNCTION__, video_channel);
    SetLastError(kViEFileAlreadyRecording);
    return -1;
  }

  WebRtc_Word32 ve_channel_id = -1;
  VoiceEngine* ve_ptr = NULL;
  if (audio_source != NO_AUDIO) {
    ve_channel_id = vie_channel->VoiceChannel();
    ve_ptr = channel_manager_.GetVoiceEngine();

    if (!ve_ptr) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_, video_channel),
                   "%s: Can't access voice engine. Have SetVoiceEngine "
                   "been called?", __FUNCTION__);
      SetLastError(kViEFileVoENotSet);
      return -1;
    }
  }
  if (file_recorder.StartRecording(file_nameUTF8, video_codec, audio_source,
                                   ve_channel_id, audio_codec, ve_ptr,
                                   file_format) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_, video_channel),
                 "%s: Failed to start recording. Check arguments.",
                 __FUNCTION__);
    SetLastError(kViEFileUnknownError);
    return -1;
  }
  return 0;
}

int ViEFileImpl::GetFileInformation(const char* file_name,
                                    VideoCodec& video_codec,
                                    CodecInst& audio_codec,
                                    const FileFormats file_format) {
  return ViEFilePlayer::GetFileInformation(
           instance_id_, static_cast<const WebRtc_Word8*>(file_name),
           video_codec, audio_codec, file_format);
}

int ViEFileImpl::GetRenderSnapshot(const int video_channel,
                                   const char* file_nameUTF8) {
  // Gain access to the renderer for the specified channel and get it's
  // current frame.
  ViERenderManagerScoped rs(render_manager_);
  ViERenderer* renderer = rs.Renderer(video_channel);
  if (!renderer) {
    return -1;
  }

  VideoFrame video_frame;
  if (renderer->GetLastRenderedFrame(video_channel, video_frame) == -1) {
    return -1;
  }

  const int JPEG_FORMAT = 0;
  int format = JPEG_FORMAT;
  switch (format) {
    case JPEG_FORMAT: {
      // JPEGEncoder writes the jpeg file for you (no control over it) and does
      // not return you the buffer. Thus, we are not going to be writing to the
      // disk here.
      JpegEncoder jpeg_encoder;
      RawImage input_image;
      if (jpeg_encoder.SetFileName(file_nameUTF8) == -1) {
        WEBRTC_TRACE(kTraceError, kTraceVideo, instance_id_,
                     "\tCould not open output file '%s' for writing!",
                     file_nameUTF8);
        return -1;
      }

      input_image._width = video_frame.Width();
      input_image._height = video_frame.Height();
      video_frame.Swap(input_image._buffer, input_image._length,
                       input_image._size);

      if (jpeg_encoder.Encode(input_image) == -1) {
        WEBRTC_TRACE(kTraceError, kTraceVideo, instance_id_,
                     "\tCould not encode i420 -> jpeg file '%s' for writing!",
                     file_nameUTF8);
        if (input_image._buffer) {
          delete [] input_image._buffer;
        }
        return -1;
      }
      delete [] input_image._buffer;
      input_image._buffer = NULL;
      break;
    }
    default: {
      WEBRTC_TRACE(kTraceError, kTraceFile, instance_id_,
                   "\tUnsupported file format for %s", __FUNCTION__);
      return -1;
      break;
    }
  }
  return 0;
}

int ViEFileImpl::GetRenderSnapshot(const int video_channel,
                                   ViEPicture& picture) {
  // Gain access to the renderer for the specified channel and get it's
  // current frame.
  ViERenderManagerScoped rs(render_manager_);
  ViERenderer* renderer = rs.Renderer(video_channel);
  if (!renderer) {
    return -1;
  }

  VideoFrame video_frame;
  if (renderer->GetLastRenderedFrame(video_channel, video_frame) == -1) {
    return -1;
  }

  // Copy from VideoFrame class to ViEPicture struct.
  int buffer_length =
      static_cast<int>(video_frame.Width() * video_frame.Height() * 1.5);
  picture.data =  static_cast<WebRtc_UWord8*>(malloc(
      buffer_length * sizeof(WebRtc_UWord8)));
  memcpy(picture.data, video_frame.Buffer(), buffer_length);
  picture.size = buffer_length;
  picture.width = video_frame.Width();
  picture.height = video_frame.Height();
  picture.type = kVideoI420;
  return 0;
}

int ViEFileImpl::GetCaptureDeviceSnapshot(const int capture_id,
                                          const char* file_nameUTF8) {
  ViEInputManagerScoped is(input_manager_);
  ViECapturer* capturer = is.Capture(capture_id);
  if (!capturer) {
    return -1;
  }

  VideoFrame video_frame;
  if (GetNextCapturedFrame(capture_id, video_frame) == -1) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, instance_id_,
                 "Could not gain acces to capture device %d video frame "
                 "%s:%d", capture_id, __FUNCTION__);
    return -1;
  }

  const int JPEG_FORMAT = 0;
  int format = JPEG_FORMAT;
  switch (format) {
    case JPEG_FORMAT: {
      // JPEGEncoder writes the jpeg file for you (no control over it) and does
      // not return you the buffer Thusly, we are not going to be writing to the
      // disk here.
      JpegEncoder jpeg_encoder;
      RawImage input_image;
      input_image._width = video_frame.Width();
      input_image._height = video_frame.Height();
      video_frame.Swap(input_image._buffer, input_image._length,
                       input_image._size);

      if (jpeg_encoder.SetFileName(file_nameUTF8) == -1) {
        WEBRTC_TRACE(kTraceError, kTraceVideo, instance_id_,
                     "\tCould not open output file '%s' for writing!",
                     file_nameUTF8);

        if (input_image._buffer) {
          delete [] input_image._buffer;
        }
        return -1;
      }
      if (jpeg_encoder.Encode(input_image) == -1) {
        WEBRTC_TRACE(kTraceError, kTraceVideo, instance_id_,
                     "\tCould not encode i420 -> jpeg file '%s' for "
                     "writing!", file_nameUTF8);
        if (input_image._buffer) {
          delete [] input_image._buffer;
        }
        return -1;
      }
      delete [] input_image._buffer;
      input_image._buffer = NULL;
      break;
    }
    default: {
      WEBRTC_TRACE(kTraceError, kTraceFile, instance_id_,
                   "\tUnsupported file format for %s", __FUNCTION__);
      return -1;
      break;
    }
  }
  return 0;
}

int ViEFileImpl::GetCaptureDeviceSnapshot(const int capture_id,
                                          ViEPicture& picture) {
  VideoFrame video_frame;
  ViEInputManagerScoped is(input_manager_);
  ViECapturer* capturer = is.Capture(capture_id);
  if (!capturer) {
    return -1;
  }
  if (GetNextCapturedFrame(capture_id, video_frame) == -1) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, instance_id_,
                 "Could not gain acces to capture device %d video frame "
                 "%s:%d", capture_id, __FUNCTION__);
    return -1;
  }

  // Copy from VideoFrame class to ViEPicture struct.
  int buffer_length =
      static_cast<int>(video_frame.Width() * video_frame.Height() * 1.5);
  picture.data = static_cast<WebRtc_UWord8*>(malloc(
      buffer_length * sizeof(WebRtc_UWord8)));
  memcpy(picture.data, video_frame.Buffer(), buffer_length);
  picture.size = buffer_length;
  picture.width = video_frame.Width();
  picture.height = video_frame.Height();
  picture.type = kVideoI420;
  return 0;
}

int ViEFileImpl::FreePicture(ViEPicture& picture) {
  if (picture.data) {
    free(picture.data);
  }

  picture.data = NULL;
  picture.size = 0;
  picture.width = 0;
  picture.height = 0;
  picture.type = kVideoUnknown;
  return 0;
}
int ViEFileImpl::SetCaptureDeviceImage(const int capture_id,
                                       const char* file_nameUTF8) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, instance_id_,
               "%s(capture_id: %d)", __FUNCTION__, capture_id);

  ViEInputManagerScoped is(input_manager_);
  ViECapturer* capturer = is.Capture(capture_id);
  if (!capturer) {
    SetLastError(kViEFileInvalidCaptureId);
    return -1;
  }

  VideoFrame capture_image;
  if (ViEFileImage::ConvertJPEGToVideoFrame(
  ViEId(instance_id_, capture_id), file_nameUTF8, capture_image) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(instance_id_, capture_id),
                 "%s(capture_id: %d) Failed to open file.", __FUNCTION__,
                 capture_id);
    SetLastError(kViEFileInvalidFile);
    return -1;
  }
  if (capturer->SetCaptureDeviceImage(capture_image)) {
    SetLastError(kViEFileSetCaptureImageError);
    return -1;
  }
  return 0;
}

int ViEFileImpl::SetCaptureDeviceImage(const int capture_id,
const ViEPicture& picture) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, instance_id_,
               "%s(capture_id: %d)", __FUNCTION__, capture_id);

  if (picture.type != kVideoI420) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_, capture_id),
                 "%s(capture_id: %d) Not a valid picture type.",
                 __FUNCTION__, capture_id);
    SetLastError(kViEFileInvalidArgument);
    return -1;
  }
  ViEInputManagerScoped is(input_manager_);
  ViECapturer* capturer = is.Capture(capture_id);
  if (!capturer) {
    SetLastError(kViEFileSetCaptureImageError);
    return -1;
  }

  VideoFrame capture_image;
  if (ViEFileImage::ConvertPictureToVideoFrame(
  ViEId(instance_id_, capture_id), picture, capture_image) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_, capture_id),
                 "%s(capture_id: %d) Failed to use picture.", __FUNCTION__,
                 capture_id);
    SetLastError(kViEFileInvalidFile);
    return -1;
  }
  if (capturer->SetCaptureDeviceImage(capture_image)) {
    SetLastError(kViEFileInvalidCapture);
    return -1;
  }
  return 0;
}

int ViEFileImpl::SetRenderStartImage(const int video_channel,
const char* file_nameUTF8) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(instance_id_, video_channel),
               "%s(video_channel: %d)", __FUNCTION__, video_channel);

  ViERenderManagerScoped rs(render_manager_);
  ViERenderer* renderer = rs.Renderer(video_channel);
  if (!renderer) {
    SetLastError(kViEFileInvalidRenderId);
    return -1;
  }

  VideoFrame start_image;
  if (ViEFileImage::ConvertJPEGToVideoFrame(
  ViEId(instance_id_, video_channel), file_nameUTF8, start_image) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_, video_channel),
                 "%s(video_channel: %d) Failed to open file.", __FUNCTION__,
                 video_channel);
    SetLastError(kViEFileInvalidFile);
    return -1;
  }
  if (renderer->SetRenderStartImage(start_image) != 0) {
    SetLastError(kViEFileSetStartImageError);
    return -1;
  }
  return 0;
}

int ViEFileImpl::SetRenderStartImage(const int video_channel,
                                     const ViEPicture& picture) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(instance_id_, video_channel), "%s(video_channel: %d)",
               __FUNCTION__, video_channel);
  if (picture.type != kVideoI420) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_, video_channel),
                 "%s(video_channel: %d) Not a valid picture type.",
                 __FUNCTION__, video_channel);
    SetLastError(kViEFileInvalidArgument);
    return -1;
  }

  ViERenderManagerScoped rs(render_manager_);
  ViERenderer* renderer = rs.Renderer(video_channel);
  if (!renderer) {
    SetLastError(kViEFileInvalidRenderId);
    return -1;
  }

  VideoFrame start_image;
  if (ViEFileImage::ConvertPictureToVideoFrame(
  ViEId(instance_id_, video_channel), picture, start_image) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_, video_channel),
                 "%s(video_channel: %d) Failed to use picture.",
                 __FUNCTION__, video_channel);
    SetLastError(kViEFileInvalidCapture);
    return -1;
  }
  if (renderer->SetRenderStartImage(start_image) != 0) {
    SetLastError(kViEFileSetStartImageError);
    return -1;
  }
  return 0;
}
int ViEFileImpl::SetRenderTimeoutImage(const int video_channel,
                                       const char* file_nameUTF8,
                                       const unsigned int timeout_ms) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(instance_id_, video_channel),
               "%s(video_channel: %d)", __FUNCTION__, video_channel);

  ViERenderManagerScoped rs(render_manager_);
  ViERenderer* renderer = rs.Renderer(video_channel);
  if (!renderer) {
    SetLastError(kViEFileInvalidRenderId);
    return -1;
  }
  VideoFrame timeout_image;
  if (ViEFileImage::ConvertJPEGToVideoFrame(
  ViEId(instance_id_, video_channel), file_nameUTF8, timeout_image) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_, video_channel),
                 "%s(video_channel: %d) Failed to open file.", __FUNCTION__,
                 video_channel);
    SetLastError(kViEFileInvalidFile);
    return -1;
  }
  WebRtc_Word32 timeout_time = timeout_ms;
  if (timeout_ms < kViEMinRenderTimeoutTimeMs) {
    WEBRTC_TRACE(kTraceWarning, kTraceVideo, ViEId(instance_id_, video_channel),
                 "%s(video_channel: %d) Invalid timeout_ms, using %d.",
                 __FUNCTION__, video_channel, kViEMinRenderTimeoutTimeMs);
    timeout_time = kViEMinRenderTimeoutTimeMs;
  }
  if (timeout_ms > kViEMaxRenderTimeoutTimeMs) {
    WEBRTC_TRACE(kTraceWarning, kTraceVideo, ViEId(instance_id_, video_channel),
                 "%s(video_channel: %d) Invalid timeout_ms, using %d.",
                 __FUNCTION__, video_channel, kViEMaxRenderTimeoutTimeMs);
    timeout_time = kViEMaxRenderTimeoutTimeMs;
  }
  if (renderer->SetTimeoutImage(timeout_image, timeout_time) != 0) {
    SetLastError(kViEFileSetRenderTimeoutError);
    return -1;
  }
  return 0;
}

int ViEFileImpl::SetRenderTimeoutImage(const int video_channel,
                                       const ViEPicture& picture,
const unsigned int timeout_ms) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(instance_id_, video_channel),
               "%s(video_channel: %d)", __FUNCTION__, video_channel);

  if (picture.type != kVideoI420) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_, video_channel),
                 "%s(video_channel: %d) Not a valid picture type.",
                 __FUNCTION__, video_channel);
    SetLastError(kViEFileInvalidArgument);
    return -1;
  }

  ViERenderManagerScoped rs(render_manager_);
  ViERenderer* renderer = rs.Renderer(video_channel);
  if (!renderer) {
    SetLastError(kViEFileSetRenderTimeoutError);
    return -1;
  }
  VideoFrame timeout_image;
  if (ViEFileImage::ConvertPictureToVideoFrame(
  ViEId(instance_id_, video_channel), picture, timeout_image) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_, video_channel),
                 "%s(video_channel: %d) Failed to use picture.",
                 __FUNCTION__, video_channel);
    SetLastError(kViEFileInvalidCapture);
    return -1;
  }
  WebRtc_Word32 timeout_time = timeout_ms;
  if (timeout_ms < kViEMinRenderTimeoutTimeMs) {
    WEBRTC_TRACE(kTraceWarning, kTraceVideo, ViEId(instance_id_, video_channel),
                 "%s(video_channel: %d) Invalid timeout_ms, using %d.",
                 __FUNCTION__, video_channel, kViEMinRenderTimeoutTimeMs);
    timeout_time = kViEMinRenderTimeoutTimeMs;
  }
  if (timeout_ms > kViEMaxRenderTimeoutTimeMs) {
    WEBRTC_TRACE(kTraceWarning, kTraceVideo, ViEId(instance_id_, video_channel),
                 "%s(video_channel: %d) Invalid timeout_ms, using %d.",
                 __FUNCTION__, video_channel, kViEMaxRenderTimeoutTimeMs);
    timeout_time = kViEMaxRenderTimeoutTimeMs;
  }
  if (renderer->SetTimeoutImage(timeout_image, timeout_time) != 0) {
    SetLastError(kViEFileSetRenderTimeoutError);
    return -1;
  }
  return 0;
}

WebRtc_Word32 ViEFileImpl::GetNextCapturedFrame(WebRtc_Word32 capture_id,
VideoFrame& video_frame) {
  ViEInputManagerScoped is(input_manager_);
  ViECapturer* capturer = is.Capture(capture_id);
  if (!capturer) {
    return -1;
  }

  ViECaptureSnapshot* snap_shot = new ViECaptureSnapshot();
  capturer->RegisterFrameCallback(-1, snap_shot);
  bool snapshot_taken = snap_shot->GetSnapshot(
      video_frame, kViECaptureMaxSnapshotWaitTimeMs);

  // Check once again if it has been destroyed.
  capturer->DeregisterFrameCallback(snap_shot);
  delete snap_shot;
  snap_shot = NULL;

  if (snapshot_taken) {
    return 0;
  }
  return -1;
}

ViECaptureSnapshot::ViECaptureSnapshot()
    : crit_(*CriticalSectionWrapper::CreateCriticalSection()),
      condition_varaible_(*ConditionVariableWrapper::CreateConditionVariable()),
      video_frame_(NULL) {
}

ViECaptureSnapshot::~ViECaptureSnapshot() {
  crit_.Enter();
  crit_.Leave();
  delete &crit_;
  if (video_frame_) {
    delete video_frame_;
    video_frame_ = NULL;
  }
}

bool ViECaptureSnapshot::GetSnapshot(VideoFrame& video_frame,
unsigned int max_wait_time) {
  crit_.Enter();
  video_frame_ = new VideoFrame();
  if (condition_varaible_.SleepCS(crit_, max_wait_time)) {
    // Snapshot taken
    video_frame.SwapFrame(*video_frame_);
    delete video_frame_;
    video_frame_ = NULL;
    crit_.Leave();
    return true;
  }
  return false;
}

void ViECaptureSnapshot::DeliverFrame(int id, VideoFrame& video_frame,
                                      int num_csrcs,
const WebRtc_UWord32 CSRC[kRtpCsrcSize]) {
  CriticalSectionScoped cs(crit_);
  if (!video_frame_) {
    return;
  }
  video_frame_->SwapFrame(video_frame);
  condition_varaible_.WakeAll();
  return;
}

#endif

}  // namespace webrtc
