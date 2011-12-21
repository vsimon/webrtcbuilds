/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// ViESharedData contains data and instances common to all interface
// implementations.

#ifndef WEBRTC_VIDEO_ENGINE_VIE_SHARED_DATA_H_
#define WEBRTC_VIDEO_ENGINE_VIE_SHARED_DATA_H_

#include "vie_defines.h"
#include "vie_performance_monitor.h"

namespace webrtc {

class CriticalSectionWrapper;
class ProcessThread;
class ViEChannelManager;
class ViEInputManager;
class ViERenderManager;

class ViESharedData {
 protected:
  ViESharedData();
  ~ViESharedData();

  bool Initialized() const;
  int SetInitialized();
  int SetUnInitialized();
  void SetLastError(const int error) const;
  int LastErrorInternal() const;

  int NumberOfCores() const;

  static int instance_counter_;
  const int instance_id_;
  CriticalSectionWrapper& api_critsect_;
  bool initialized_;
  const int number_cores_;

  ViEPerformanceMonitor vie_performance_monitor_;
  ViEChannelManager& channel_manager_;
  ViEInputManager& input_manager_;
  ViERenderManager& render_manager_;
  ProcessThread* module_process_thread_;

 private:
  mutable int last_error_;
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_VIE_SHARED_DATA_H_
