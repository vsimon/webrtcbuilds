/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * vie_ref_count.h
 */

#ifndef WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_REF_COUNT_H_
#define WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_REF_COUNT_H_

namespace webrtc {
class CriticalSectionWrapper;
}

class ViERefCount
{
public:
    ViERefCount();
    ~ViERefCount();
    
    ViERefCount& operator++(int);
    ViERefCount& operator--(int);
    
    void Reset();
    int GetCount() const;

private:
    volatile int _count;
    webrtc::CriticalSectionWrapper& _crit;
};

#endif  // WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_REF_COUNT_H_
