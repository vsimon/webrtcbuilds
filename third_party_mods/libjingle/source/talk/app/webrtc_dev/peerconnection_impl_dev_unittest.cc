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
#include "talk/app/webrtc_dev/peerconnection_dev.h"
#include "talk/app/webrtc_dev/peerconnection_impl_dev.h"
#include "talk/base/scoped_ptr.h"
#include "talk/base/thread.h"

static const char kStreamLabel1[] = "local_stream_1";

namespace webrtc {

class PeerConnectionImplTest : public testing::Test {
 public:

 protected:
  virtual void SetUp() {
    pc_factory_ = webrtc::PeerConnectionManager::Create();
    ASSERT_TRUE(pc_factory_.get() != NULL);
    pc_ = pc_factory_->CreatePeerConnection("");
    ASSERT_TRUE(pc_.get() != NULL);
  }

  scoped_refptr<webrtc::PeerConnectionManager> pc_factory_;
  scoped_refptr<PeerConnection> pc_;
};

TEST_F(PeerConnectionImplTest, AddRemoveStream) {
  // Create a local stream.
  std::string label(kStreamLabel1);
  scoped_refptr<LocalMediaStream> stream(LocalMediaStream::Create(label));

  pc_->AddStream(stream);
  pc_->CommitStreamChanges();
  EXPECT_EQ(pc_->local_streams()->count(), 1l);
  EXPECT_EQ(pc_->local_streams()->at(0)->label().compare(kStreamLabel1), 0);
}

}  // namespace webrtc
