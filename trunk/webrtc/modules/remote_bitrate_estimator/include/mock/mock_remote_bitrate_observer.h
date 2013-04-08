/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_INCLUDE_MOCK_MOCK_REMOTE_BITRATE_ESTIMATOR_H_
#define WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_INCLUDE_MOCK_MOCK_REMOTE_BITRATE_ESTIMATOR_H_

#include <gmock/gmock.h>

#include <vector>

#include "modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"

namespace webrtc {

class MockRemoteBitrateObserver : public RemoteBitrateObserver {
 public:
  MOCK_METHOD2(OnReceiveBitrateChanged,
      void(std::vector<unsigned int>* ssrcs, unsigned int bitrate));
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_INCLUDE_MOCK_MOCK_REMOTE_BITRATE_ESTIMATOR_H_
