/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/system_wrappers/source/event_win.h"

#include "Mmsystem.h"

namespace webrtc {

EventWindows::EventWindows()
    : event_(::CreateEvent(NULL,    // security attributes
                           FALSE,   // manual reset
                           FALSE,   // initial state
                           NULL)),  // name of event
    timerID_(NULL) {
}

EventWindows::~EventWindows() {
  CloseHandle(event_);
}

bool EventWindows::Set() {
  // Note: setting an event that is already set has no effect.
  return SetEvent(event_) == 1 ? true : false;
}

bool EventWindows::Reset() {
  return ResetEvent(event_) == 1 ? true : false;
}

EventTypeWrapper EventWindows::Wait(unsigned long max_time) {
  unsigned long res = WaitForSingleObject(event_, max_time);
  switch (res) {
    case WAIT_OBJECT_0:
      return kEventSignaled;
    case WAIT_TIMEOUT:
      return kEventTimeout;
    default:
      return kEventError;
  }
}

bool EventWindows::StartTimer(bool periodic, unsigned long time) {
  if (timerID_ != NULL) {
    timeKillEvent(timerID_);
    timerID_ = NULL;
  }
  if (periodic) {
    timerID_ = timeSetEvent(time, 0, (LPTIMECALLBACK)HANDLE(event_), 0,
                            TIME_PERIODIC | TIME_CALLBACK_EVENT_PULSE);
  } else {
    timerID_ = timeSetEvent(time, 0, (LPTIMECALLBACK)HANDLE(event_), 0,
                            TIME_ONESHOT | TIME_CALLBACK_EVENT_SET);
  }

  if (timerID_ == NULL) {
    return false;
  }
  return true;
}

bool EventWindows::StopTimer() {
  timeKillEvent(timerID_);
  timerID_ = NULL;
  return true;
}

}  // namespace webrtc
