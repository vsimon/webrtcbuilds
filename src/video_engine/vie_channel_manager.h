/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_VIE_CHANNEL_MANAGER_H_
#define WEBRTC_VIDEO_ENGINE_VIE_CHANNEL_MANAGER_H_

#include "engine_configurations.h"
#include "system_wrappers/interface/map_wrapper.h"
#include "typedefs.h"
#include "video_engine/vie_defines.h"
#include "video_engine/vie_manager_base.h"

namespace webrtc {

class CriticalSectionWrapper;
class ProcessThread;
class ViEChannel;
class ViEEncoder;
class ViEPerformanceMonitor;
class VoEVideoSync;
class VoiceEngine;

class ViEChannelManager: private ViEManagerBase {
  friend class ViEChannelManagerScoped;
 public:
  ViEChannelManager(int engine_id,
                    int number_of_cores,
                    ViEPerformanceMonitor& vie_performance_monitor);
  ~ViEChannelManager();

  void SetModuleProcessThread(ProcessThread& module_process_thread);

  // Creates a new channel. 'channelId' will be the id of the created channel.
  int CreateChannel(int& channel_id);

  // Creates a channel and attaches to an already existing ViEEncoder.
  int CreateChannel(int& channel_id, int original_channel);

  // Deletes a channel.
  int DeleteChannel(int channel_id);

  // Set the voice engine instance to be used by all video channels.
  int SetVoiceEngine(VoiceEngine* voice_engine);

  // Enables lip sync of the channel.
  int ConnectVoiceChannel(int channel_id, int audio_channel_id);

  // Disables lip sync of the channel.
  int DisconnectVoiceChannel(int channel_id);

  VoiceEngine* GetVoiceEngine();

 private:
  // Used by ViEChannelScoped, forcing a manager user to use scoped.
  // Returns a pointer to the channel with id 'channelId'.
  ViEChannel* ViEChannelPtr(int channel_id) const;

  // Adds all channels to channel_map.
  void GetViEChannels(MapWrapper& channel_map);

  // Methods used by ViECaptureScoped and ViEEncoderScoped.
  // Gets the ViEEncoder used as input for video_channel_id
  ViEEncoder* ViEEncoderPtr(int video_channel_id) const;

  // Returns true if we found a new channel id, free_channel_id, false
  // otherwise.
  bool GetFreeChannelId(int& free_channel_id);

  // Returns a previously allocated channel id.
  void ReturnChannelId(int channel_id);

  // Returns true if at least one other channels uses the same ViEEncoder as
  // channel_id.
  bool ChannelUsingViEEncoder(int channel_id) const;

  // Protects channel_map_ and free_channel_ids_.
  CriticalSectionWrapper* channel_id_critsect_;
  int engine_id_;
  int number_of_cores_;
  ViEPerformanceMonitor& vie_performance_monitor_;
  MapWrapper channel_map_;
  bool* free_channel_ids_;
  int free_channel_ids_size_;

  // Maps Channel id -> ViEEncoder.
  MapWrapper vie_encoder_map_;
  VoEVideoSync* voice_sync_interface_;
  VoiceEngine* voice_engine_;
  ProcessThread* module_process_thread_;
};

class ViEChannelManagerScoped: private ViEManagerScopedBase {
 public:
  explicit ViEChannelManagerScoped(
      const ViEChannelManager& vie_channel_manager);
  ViEChannel* Channel(int vie_channel_id) const;
  ViEEncoder* Encoder(int vie_channel_id) const;

  // Returns true if at lease one other channels uses the same ViEEncoder as
  // channel_id.
  bool ChannelUsingViEEncoder(int channel_id) const;
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_VIE_CHANNEL_MANAGER_H_
