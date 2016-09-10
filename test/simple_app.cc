#include "webrtc/base/thread.h"
#include "webrtc/p2p/base/basicpacketsocketfactory.h"
#include "webrtc/api/peerconnectioninterface.h"
#include "webrtc/api/test/fakeconstraints.h"
#include "webrtc/media/engine/webrtcvideocapturerfactory.h"

int main(int argc, char* argv[]) {
  // something from base
  rtc::Thread* thread = rtc::Thread::Current();

  // something from p2p
  std::unique_ptr<rtc::BasicPacketSocketFactory> socket_factory(
    new rtc::BasicPacketSocketFactory());

  // something from api
  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface>
    peer_connection_factory = webrtc::CreatePeerConnectionFactory();

  // something from api/test
  webrtc::FakeConstraints constraints;

  // something from media/engine
  cricket::WebRtcVideoDeviceCapturerFactory factory;
  cricket::VideoCapturer* capturer = factory.Create(cricket::Device("", 0));

  return 0;
}
