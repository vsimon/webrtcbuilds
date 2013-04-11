/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ACM_TEST_SPATIAL_AUDIO_H
#define ACM_TEST_SPATIAL_AUDIO_H

#include "ACMTest.h"
#include "Channel.h"
#include "PCMFile.h"
#include "audio_coding_module.h"
#include "utility.h"

#define MAX_FILE_NAME_LENGTH_BYTE 500

namespace webrtc {

class SpatialAudio : public ACMTest
{
public:
    SpatialAudio(int testMode);
    ~SpatialAudio();

    void Perform();
private:
    int16_t Setup();
    void EncodeDecode(double leftPanning, double rightPanning);
    void EncodeDecode();

    AudioCodingModule* _acmLeft;
    AudioCodingModule* _acmRight;
    AudioCodingModule* _acmReceiver;
    Channel*               _channel;
    PCMFile                _inFile;
    PCMFile                _outFile;
    int                    _testMode;
};

} // namespace webrtc

#endif
