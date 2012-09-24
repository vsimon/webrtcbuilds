/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_engine/encoder_state_feedback.h"

#include <assert.h>

#include "modules/rtp_rtcp/interface/rtp_rtcp_defines.h"
#include "system_wrappers/interface/critical_section_wrapper.h"
#include "video_engine/vie_encoder.h"

namespace webrtc {

// Helper class registered at the RTP module relaying callbacks to
// EncoderStatFeedback.
class EncoderStateFeedbackObserver : public  RtcpIntraFrameObserver {
 public:
  explicit EncoderStateFeedbackObserver(EncoderStateFeedback* owner)
      : owner_(owner) {}
  ~EncoderStateFeedbackObserver() {}

  // Implements RtcpIntraFrameObserver.
  virtual void OnReceivedIntraFrameRequest(uint32_t ssrc) {
    owner_->OnReceivedIntraFrameRequest(ssrc);
  }
  virtual void OnReceivedSLI(uint32_t ssrc, uint8_t picture_id) {
    owner_->OnReceivedSLI(ssrc, picture_id);
  }
  virtual void OnReceivedRPSI(uint32_t ssrc, uint64_t picture_id) {
    owner_->OnReceivedRPSI(ssrc, picture_id);
  }

 private:
  EncoderStateFeedback* owner_;
};

EncoderStateFeedback::EncoderStateFeedback()
    : crit_(CriticalSectionWrapper::CreateCriticalSection()),
      observer_(new EncoderStateFeedbackObserver(this)) {}

EncoderStateFeedback::~EncoderStateFeedback() {
  assert(encoders_.empty());
}

bool EncoderStateFeedback::AddEncoder(uint32_t ssrc, ViEEncoder* encoder)  {
  CriticalSectionScoped lock(crit_.get());
  if (encoders_.find(ssrc) != encoders_.end())
    return false;

  encoders_[ssrc] = encoder;
  return true;
}

void EncoderStateFeedback::RemoveEncoder(uint32_t ssrc)  {
  CriticalSectionScoped lock(crit_.get());
  SsrcEncoderMap::iterator it = encoders_.find(ssrc);
  if (it == encoders_.end())
    return;

  encoders_.erase(it);
}

RtcpIntraFrameObserver* EncoderStateFeedback::GetRtcpIntraFrameObserver() {
  return observer_.get();
}

void EncoderStateFeedback::OnReceivedIntraFrameRequest(uint32_t ssrc) {
  CriticalSectionScoped lock(crit_.get());
  SsrcEncoderMap::iterator it = encoders_.find(ssrc);
  if (it == encoders_.end())
    return;

  it->second->OnReceivedIntraFrameRequest(ssrc);
}

void EncoderStateFeedback::OnReceivedSLI(uint32_t ssrc, uint8_t picture_id) {
  CriticalSectionScoped lock(crit_.get());
  SsrcEncoderMap::iterator it = encoders_.find(ssrc);
  if (it == encoders_.end())
    return;

  it->second->OnReceivedSLI(ssrc, picture_id);
}

void EncoderStateFeedback::OnReceivedRPSI(uint32_t ssrc, uint64_t picture_id) {
  CriticalSectionScoped lock(crit_.get());
  SsrcEncoderMap::iterator it = encoders_.find(ssrc);
  if (it == encoders_.end())
    return;

  it->second->OnReceivedRPSI(ssrc, picture_id);
}

}  // namespace webrtc
