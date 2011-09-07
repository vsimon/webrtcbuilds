/*
 * libjingle
 * Copyright 2011, Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <string>

#include "gtest/gtest.h"
#include "talk/app/webrtc_dev/local_stream_dev.h"
#include "talk/app/webrtc_dev/peerconnectionmanager_impl.h"
#include "talk/base/basicpacketsocketfactory.h"
#include "talk/base/scoped_ptr.h"
#include "talk/base/thread.h"
#include "talk/session/phone/webrtccommon.h"
#include "talk/session/phone/webrtcvoe.h"

static const char kAudioDeviceLabel[] = "dummy_audio_device";

namespace webrtc {

TEST(PeerConnectionManager, CreatePCUsingInternalModules) {
  scoped_refptr<PeerConnectionManager> manager(PeerConnectionManager::Create());
  ASSERT_TRUE(manager.get() != NULL);
  scoped_refptr<PeerConnection> pc1(manager->CreatePeerConnection(""));
  EXPECT_TRUE(pc1.get() != NULL);

  scoped_refptr<webrtc::PeerConnection> pc2(manager->CreatePeerConnection(""));
  EXPECT_TRUE(pc2.get() != NULL);
}

TEST(PeerConnectionManager, CreatePCUsingExternalModules) {
  // Create an audio device. Use the default sound card.
  AudioDeviceModule* module = AudioDeviceModule::Create(0);
  scoped_refptr<AudioDevice> audio_device(AudioDevice::Create(
      kAudioDeviceLabel, module));

  // Creata a libjingle thread used as internal worker thread.
  talk_base::scoped_ptr<talk_base::Thread> w_thread(new talk_base::Thread);
  EXPECT_TRUE(w_thread->Start());

  scoped_refptr<PcNetworkManager> network_manager(PcNetworkManager::Create(
      new talk_base::BasicNetworkManager));
  scoped_refptr<PcPacketSocketFactory> socket_factory(
      PcPacketSocketFactory::Create(new talk_base::BasicPacketSocketFactory));

  scoped_refptr<PeerConnectionManager> manager =
      PeerConnectionManager::Create(w_thread.get(),
                                    network_manager,
                                    socket_factory,
                                    audio_device);
  ASSERT_TRUE(manager.get() != NULL);

  scoped_refptr<webrtc::PeerConnection> pc1(manager->CreatePeerConnection(""));
  EXPECT_TRUE(pc1.get() != NULL);

  scoped_refptr<webrtc::PeerConnection> pc2(manager->CreatePeerConnection(""));
  EXPECT_TRUE(pc2.get() != NULL);
}

}  // namespace webrtc
