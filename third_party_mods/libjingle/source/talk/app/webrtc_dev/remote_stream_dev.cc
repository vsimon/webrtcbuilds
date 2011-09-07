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
#include "talk/app/webrtc_dev/remote_stream_dev.h"

#include <string>

namespace webrtc {

scoped_refptr<RemoteMediaStream> RemoteMediaStreamImpl::Create(
    const std::string& label) {
  // To instantiate LocalStream use
  RefCountImpl<RemoteMediaStreamImpl>* stream =
      new RefCountImpl<RemoteMediaStreamImpl>(label);
  return stream;
}

RemoteMediaStreamImpl::RemoteMediaStreamImpl(const std::string& label)
    : media_stream_impl_(label) {
}

// Implement MediaStream
const std::string& RemoteMediaStreamImpl::label() {
  return media_stream_impl_.label();
}

scoped_refptr<MediaStreamTrackList> RemoteMediaStreamImpl::tracks() {
  return this;
}

MediaStream::ReadyState RemoteMediaStreamImpl::ready_state() {
  return media_stream_impl_.ready_state();
}

// Implement MediaStreamTrackList.
size_t RemoteMediaStreamImpl::count() {
  return tracks_.count();
}

scoped_refptr<MediaStreamTrack> RemoteMediaStreamImpl::at(size_t index) {
  return tracks_.at(index);
}

bool RemoteMediaStreamImpl::AddTrack(MediaStreamTrack* track) {
  tracks_.AddTrack(track);
  NotifierImpl<MediaStreamTrackList>::FireOnChanged();
}

}  // namespace webrtc
