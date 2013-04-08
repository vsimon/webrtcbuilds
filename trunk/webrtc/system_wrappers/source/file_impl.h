/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SYSTEM_WRAPPERS_SOURCE_FILE_IMPL_H_
#define WEBRTC_SYSTEM_WRAPPERS_SOURCE_FILE_IMPL_H_

#include <stdio.h>

#include "webrtc/system_wrappers/interface/file_wrapper.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"

namespace webrtc {

class RWLockWrapper;

class FileWrapperImpl : public FileWrapper {
 public:
  FileWrapperImpl();
  virtual ~FileWrapperImpl();

  virtual int FileName(char* file_name_utf8,
                       size_t size) const;

  virtual bool Open() const;

  virtual int OpenFile(const char* file_name_utf8,
                       bool read_only,
                       bool loop = false,
                       bool text = false);

  virtual int CloseFile();
  virtual int SetMaxFileSize(size_t bytes);
  virtual int Flush();

  virtual int Read(void* buf, int length);
  virtual bool Write(const void* buf, int length);
  virtual int WriteText(const char* format, ...);
  virtual int Rewind();

 private:
  int CloseFileImpl();
  int FlushImpl();

  scoped_ptr<RWLockWrapper> rw_lock_;

  FILE* id_;
  bool open_;
  bool looping_;
  bool read_only_;
  size_t max_size_in_bytes_;  // -1 indicates file size limitation is off
  size_t size_in_bytes_;
  char file_name_utf8_[kMaxFileNameSize];
};

} // namespace webrtc

#endif  // WEBRTC_SYSTEM_WRAPPERS_SOURCE_FILE_IMPL_H_
