#include "webrtc/base/ssladapter.h"
#include "webrtc/base/thread.h"

int main(int argc, char* argv[]) {
  rtc::AutoThread auto_thread;
  rtc::Thread* thread = rtc::Thread::Current();
  rtc::InitializeSSL();
  rtc::CleanupSSL();
  return 0;
}
