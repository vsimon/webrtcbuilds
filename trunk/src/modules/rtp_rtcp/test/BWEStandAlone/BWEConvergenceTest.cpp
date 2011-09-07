/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "BWEConvergenceTest.h"

#include <fstream>
#include <string>

#include "TestSenderReceiver.h"
#include "TestLoadGenerator.h"
#include "event_wrapper.h"
#include "thread_wrapper.h"
#include "tick_util.h"


BWEConvergenceTestUp::BWEConvergenceTestUp(std::string testName, int startRateKbps, int availBWkbps)
:
_availBWkbps(availBWkbps),
BWEOneWayTest(testName, startRateKbps)
{
}


BWEConvergenceTestUp::~BWEConvergenceTestUp()
{
    if (_gen)
    {
        delete _gen;
        _gen = NULL;
    }
}


int BWEConvergenceTestUp::Init(std::string ip, WebRtc_UWord16 port)
{
    // create the load generator object
    const int rtpSampleRate = 90000;
    const int frameRate = 30;
    const double spreadFactor = 0.2;

    if (_master)
    {
        _gen = new CBRFixFRGenerator(_sendrec, _startRateKbps, rtpSampleRate, frameRate, spreadFactor);
        if (!_gen)
        {
            return (-1);
        }
    }

    return BWEOneWayTest::Init(ip, port);
}


bool BWEConvergenceTestUp::StoppingCriterionMaster()
{
    return ((_sendrec->BitrateSent() / 1000.0) > (0.9 * _availBWkbps));
}


