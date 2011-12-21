/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// A ring buffer to hold arbitrary data. Provides no thread safety. Unless
// otherwise specified, functions return 0 on success and -1 on error.

#include "ring_buffer.h"

#include <stddef.h> // size_t
#include <stdlib.h>
#include <string.h>

// TODO(bjornv): Remove tmp_buf_t once old buffer function has been replaced in
//               APM.
typedef struct {
    int readPos;
    int writePos;
    int size;
    int element_size;
    char rwWrap;
    bufdata_t *data;
} tmp_buf_t;

typedef struct {
  size_t read_pos;
  size_t write_pos;
  size_t element_count;
  size_t element_size;
  char rw_wrap;
  void* data;
} buf_t;

enum { SAME_WRAP, DIFF_WRAP };

// Get address of region(s) from which we can read data.
// If the region is contiguous, |data_ptr_bytes_2| will be zero.
// If non-contiguous, |data_ptr_bytes_2| will be the size in bytes of the second
// region. Returns room available to be read or |element_count|, whichever is
// smaller.
static size_t GetBufferReadRegions(buf_t* buf,
                                   size_t element_count,
                                   void** data_ptr_1,
                                   size_t* data_ptr_bytes_1,
                                   void** data_ptr_2,
                                   size_t* data_ptr_bytes_2) {

  const size_t readable_elements = WebRtc_available_read(buf);
  const size_t read_elements = (readable_elements < element_count ?
      readable_elements : element_count);
  const size_t margin = buf->element_count - buf->read_pos;

  // Check to see if read is not contiguous.
  if (read_elements > margin) {
    // Write data in two blocks that wrap the buffer.
    *data_ptr_1 = buf->data + (buf->read_pos * buf->element_size);
    *data_ptr_bytes_1 = margin * buf->element_size;
    *data_ptr_2 = buf->data;
    *data_ptr_bytes_2 = (read_elements - margin) * buf->element_size;
  } else {
    *data_ptr_1 = buf->data + (buf->read_pos * buf->element_size);
    *data_ptr_bytes_1 = read_elements * buf->element_size;
    *data_ptr_2 = NULL;
    *data_ptr_bytes_2 = 0;
  }

  return read_elements;
}

int WebRtcApm_CreateBuffer(void **bufInst, int size) {
    tmp_buf_t *buf = NULL;

  if (size < 0) {
    return -1;
  }

  buf = malloc(sizeof(tmp_buf_t));
  *bufInst = buf;
  if (buf == NULL) {
    return -1;
  }

  buf->data = malloc(size * sizeof(bufdata_t));
  if (buf->data == NULL) {
    free(buf);
    buf = NULL;
    return -1;
  }

  buf->size = size;
  buf->element_size = 1;

  return 0;
}

int WebRtcApm_InitBuffer(void *bufInst) {
  tmp_buf_t *buf = (tmp_buf_t*)bufInst;

  buf->readPos = 0;
  buf->writePos = 0;
  buf->rwWrap = SAME_WRAP;

  // Initialize buffer to zeros
  memset(buf->data, 0, sizeof(bufdata_t) * buf->size);

  return 0;
}

int WebRtcApm_FreeBuffer(void *bufInst) {
  tmp_buf_t *buf = (tmp_buf_t*)bufInst;

  if (buf == NULL) {
    return -1;
  }

  free(buf->data);
  free(buf);

  return 0;
}

int WebRtcApm_ReadBuffer(void *bufInst, bufdata_t *data, int size) {
  tmp_buf_t *buf = (tmp_buf_t*)bufInst;
  int n = 0, margin = 0;

  if (size <= 0 || size > buf->size) {
    return -1;
  }

  n = size;
  if (buf->rwWrap == DIFF_WRAP) {
    margin = buf->size - buf->readPos;
    if (n > margin) {
      buf->rwWrap = SAME_WRAP;
      memcpy(data, buf->data + buf->readPos, sizeof(bufdata_t) * margin);
      buf->readPos = 0;
      n = size - margin;
    } else {
      memcpy(data, buf->data + buf->readPos, sizeof(bufdata_t) * n);
      buf->readPos += n;
      return n;
    }
  }

  if (buf->rwWrap == SAME_WRAP) {
    margin = buf->writePos - buf->readPos;
    if (margin > n)
      margin = n;
    memcpy(data + size - n, buf->data + buf->readPos,
           sizeof(bufdata_t) * margin);
    buf->readPos += margin;
    n -= margin;
  }

  return size - n;
}

int WebRtcApm_WriteBuffer(void *bufInst, const bufdata_t *data, int size) {
  tmp_buf_t *buf = (tmp_buf_t*)bufInst;
  int n = 0, margin = 0;

  if (size < 0 || size > buf->size) {
    return -1;
  }

  n = size;
  if (buf->rwWrap == SAME_WRAP) {
    margin = buf->size - buf->writePos;
    if (n > margin) {
      buf->rwWrap = DIFF_WRAP;
      memcpy(buf->data + buf->writePos, data, sizeof(bufdata_t) * margin);
      buf->writePos = 0;
      n = size - margin;
    } else {
      memcpy(buf->data + buf->writePos, data, sizeof(bufdata_t) * n);
      buf->writePos += n;
      return n;
    }
  }

  if (buf->rwWrap == DIFF_WRAP) {
    margin = buf->readPos - buf->writePos;
    if (margin > n)
      margin = n;
    memcpy(buf->data + buf->writePos, data + size - n,
           sizeof(bufdata_t) * margin);
    buf->writePos += margin;
    n -= margin;
  }

  return size - n;
}

int WebRtcApm_FlushBuffer(void *bufInst, int size) {
  tmp_buf_t *buf = (tmp_buf_t*)bufInst;
  int n = 0, margin = 0;

  if (size <= 0 || size > buf->size) {
    return -1;
  }

  n = size;
  if (buf->rwWrap == DIFF_WRAP) {
    margin = buf->size - buf->readPos;
    if (n > margin) {
      buf->rwWrap = SAME_WRAP;
      buf->readPos = 0;
      n = size - margin;
    } else {
      buf->readPos += n;
      return n;
    }
  }

  if (buf->rwWrap == SAME_WRAP) {
    margin = buf->writePos - buf->readPos;
    if (margin > n)
      margin = n;
    buf->readPos += margin;
    n -= margin;
  }

  return size - n;
}

int WebRtcApm_StuffBuffer(void *bufInst, int size) {
  tmp_buf_t *buf = (tmp_buf_t*)bufInst;
  int n = 0, margin = 0;

  if (size <= 0 || size > buf->size) {
    return -1;
  }

  n = size;
  if (buf->rwWrap == SAME_WRAP) {
    margin = buf->readPos;
    if (n > margin) {
      buf->rwWrap = DIFF_WRAP;
      buf->readPos = buf->size - 1;
      n -= margin + 1;
    } else {
      buf->readPos -= n;
      return n;
    }
  }

  if (buf->rwWrap == DIFF_WRAP) {
    margin = buf->readPos - buf->writePos;
    if (margin > n)
      margin = n;
    buf->readPos -= margin;
    n -= margin;
  }

  return size - n;
}

int WebRtcApm_get_buffer_size(const void *bufInst) {
  const tmp_buf_t *buf = (tmp_buf_t*)bufInst;

  if (buf->rwWrap == SAME_WRAP)
    return buf->writePos - buf->readPos;
  else
    return buf->size - buf->readPos + buf->writePos;
}

int WebRtc_CreateBuffer(void** handle,
                        size_t element_count,
                        size_t element_size) {
  buf_t* self = NULL;

  if (handle == NULL) {
    return -1;
  }

  self = malloc(sizeof(buf_t));
  if (self == NULL) {
    return -1;
  }
  *handle = self;

  self->data = malloc(element_count * element_size);
  if (self->data == NULL) {
    free(self);
    self = NULL;
    return -1;
  }

  self->element_count = element_count;
  self->element_size = element_size;

  return 0;
}

int WebRtc_InitBuffer(void* handle) {
  buf_t* self = (buf_t*) handle;

  if (self == NULL) {
    return -1;
  }

  self->read_pos = 0;
  self->write_pos = 0;
  self->rw_wrap = SAME_WRAP;

  // Initialize buffer to zeros
  memset(self->data, 0, self->element_count * self->element_size);

  return 0;
}

int WebRtc_FreeBuffer(void* handle) {
  buf_t* self = (buf_t*) handle;

  if (self == NULL) {
    return -1;
  }

  free(self->data);
  free(self);

  return 0;
}

size_t WebRtc_ReadBuffer(void* handle,
                         void** data_ptr,
                         void* data,
                         size_t element_count) {

  buf_t* self = (buf_t*) handle;

  if (self == NULL) {
    return 0;
  }
  if (data == NULL) {
    return 0;
  }
  if (data_ptr == NULL) {
    return 0;
  }

  {
    void* buf_ptr_1 = NULL;
    void* buf_ptr_2 = NULL;
    size_t buf_ptr_bytes_1 = 0;
    size_t buf_ptr_bytes_2 = 0;
    const size_t read_count = GetBufferReadRegions(self,
                                                   element_count,
                                                   &buf_ptr_1,
                                                   &buf_ptr_bytes_1,
                                                   &buf_ptr_2,
                                                   &buf_ptr_bytes_2);

    if (buf_ptr_bytes_2 > 0) {
      // We have a wrap around when reading the buffer. Copy the buffer data to
      // |data| and point to it.
      memcpy(data, buf_ptr_1, buf_ptr_bytes_1);
      memcpy(data + buf_ptr_bytes_1, buf_ptr_2, buf_ptr_bytes_2);
      *data_ptr = data;
    } else {
      *data_ptr = buf_ptr_1;
    }

    // Update read position
    WebRtc_MoveReadPtr(handle, (int) read_count);

    return read_count;
  }
}

size_t WebRtc_WriteBuffer(void* handle,
                          const void* data,
                          size_t element_count) {

  buf_t* self = (buf_t*) handle;

  if (self == NULL) {
    return 0;
  }
  if (data == NULL) {
    return 0;
  }

  {
    const size_t free_elements = WebRtc_available_write(handle);
    const size_t write_elements = (free_elements < element_count ? free_elements
        : element_count);
    size_t n = write_elements;
    const size_t margin = self->element_count - self->write_pos;

    if (write_elements > margin) {
      // Buffer wrap around when writing.
      memcpy(self->data + (self->write_pos * self->element_size),
             data, margin * self->element_size);
      self->write_pos = 0;
      n -= margin;
      self->rw_wrap = DIFF_WRAP;
    }
    memcpy(self->data + (self->write_pos * self->element_size),
           data + ((write_elements - n) * self->element_size),
           n * self->element_size);
    self->write_pos += n;

    return write_elements;
  }
}

int WebRtc_MoveReadPtr(void* handle, int element_count) {

  buf_t* self = (buf_t*) handle;

  if (self == NULL) {
    return 0;
  }

  {
    // We need to be able to take care of negative changes, hence use "int"
    // instead of "size_t".
    const int free_elements = (int) WebRtc_available_write(handle);
    const int readable_elements = (int) WebRtc_available_read(handle);
    int read_pos = (int) self->read_pos;

    if (element_count > readable_elements) {
      element_count = readable_elements;
    }
    if (element_count < -free_elements) {
      element_count = -free_elements;
    }

    read_pos += element_count;
    if (read_pos > (int) self->element_count) {
      // Buffer wrap around. Restart read position and wrap indicator.
      read_pos -= (int) self->element_count;
      self->rw_wrap = SAME_WRAP;
    }
    if (read_pos < 0) {
      // Buffer wrap around. Restart read position and wrap indicator.
      read_pos += (int) self->element_count;
      self->rw_wrap = DIFF_WRAP;
    }

    self->read_pos = (size_t) read_pos;

    return element_count;
  }
}

size_t WebRtc_available_read(const void* handle) {
  const buf_t* self = (buf_t*) handle;

  if (self == NULL) {
    return 0;
  }

  if (self->rw_wrap == SAME_WRAP) {
    return self->write_pos - self->read_pos;
  } else {
    return self->element_count - self->read_pos + self->write_pos;
  }
}

size_t WebRtc_available_write(const void* handle) {
  const buf_t* self = (buf_t*) handle;

  if (self == NULL) {
    return 0;
  }

  return self->element_count - WebRtc_available_read(handle);
}
