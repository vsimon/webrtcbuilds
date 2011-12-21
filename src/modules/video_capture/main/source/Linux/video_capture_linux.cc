/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <errno.h>
#include <stdio.h>
#include <sys/mman.h>
#include <string.h>

#include <iostream>
#include <new>

#include "ref_count.h"
#include "trace.h"
#include "thread_wrapper.h"
#include "critical_section_wrapper.h"
#include "video_capture_linux.h"

namespace webrtc
{
namespace videocapturemodule
{
VideoCaptureModule* VideoCaptureImpl::Create(const WebRtc_Word32 id,
                                             const WebRtc_UWord8* deviceUniqueId)
{
    RefCountImpl<videocapturemodule::VideoCaptureModuleV4L2>* implementation =
        new RefCountImpl<videocapturemodule::VideoCaptureModuleV4L2>(id);

    if (!implementation || implementation->Init(deviceUniqueId) != 0)
    {
        delete implementation;
        implementation = NULL;
    }

    return implementation;
}

VideoCaptureModuleV4L2::VideoCaptureModuleV4L2(const WebRtc_Word32 id)
    : VideoCaptureImpl(id), _captureThread(NULL),
      _captureCritSect(CriticalSectionWrapper::CreateCriticalSection()),
      _deviceId(-1), _currentWidth(-1), _currentHeight(-1),
      _currentFrameRate(-1), _captureStarted(false), _captureVideoType(kVideoI420)
{
}

WebRtc_Word32 VideoCaptureModuleV4L2::Init(const WebRtc_UWord8* deviceUniqueIdUTF8)
{
    int len = strlen((const char*) deviceUniqueIdUTF8);
    _deviceUniqueId = new (std::nothrow) WebRtc_UWord8[len + 1];
    if (_deviceUniqueId)
    {
        memcpy(_deviceUniqueId, deviceUniqueIdUTF8, len + 1);
    }

    int fd;
    char device[32];
    bool found = false;

    /* detect /dev/video [0-63] entries */
    int n;
    for (n = 0; n < 64; n++)
    {
        struct stat s;
        sprintf(device, "/dev/video%d", n);
        if (stat(device, &s) == 0) //check validity of path
        {
            if ((fd = open(device, O_RDONLY)) > 0)
            {
                // query device capabilities
                struct v4l2_capability cap;
                if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == 0)
                {
                    if (cap.bus_info[0] != 0)
                    {
                        if (strncmp((const char*) cap.bus_info,
                                    (const char*) deviceUniqueIdUTF8,
                                    strlen((const char*) deviceUniqueIdUTF8)) == 0) //match with device id
                        {
                            close(fd);
                            found = true;
                            break; // fd matches with device unique id supplied
                        }
                    }
                }
                close(fd); // close since this is not the matching device
            }
        }
    }
    if (!found)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id, "no matching device found");
        return -1;
    }
    _deviceId = n; //store the device id
    return 0;
}

VideoCaptureModuleV4L2::~VideoCaptureModuleV4L2()
{
    StopCapture();
    if (_captureCritSect)
    {
        delete _captureCritSect;
    }
    if (_deviceFd != -1)
      close(_deviceFd);
}

WebRtc_Word32 VideoCaptureModuleV4L2::StartCapture(
                                        const VideoCaptureCapability& capability)
{
    if (_captureStarted)
    {
        if (capability.width == _currentWidth &&
            capability.height == _currentHeight &&
            _captureVideoType == capability.rawType)
        {
            return 0;
        }
        else
        {
            StopCapture();
        }
    }

    CriticalSectionScoped cs(*_captureCritSect);
    //first open /dev/video device
    char device[20];
    sprintf(device, "/dev/video%d", (int) _deviceId);

    if ((_deviceFd = open(device, O_RDWR | O_NONBLOCK, 0)) < 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                   "error in opening %s errono = %d", device, errno);
        return -1;
    }

    int nFormats = 2;
    unsigned int fmts[2] = { V4L2_PIX_FMT_YUV420, V4L2_PIX_FMT_YUYV };

    struct v4l2_format video_fmt;
    memset(&video_fmt, 0, sizeof(struct v4l2_format));
    video_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    video_fmt.fmt.pix.sizeimage = 0;
    video_fmt.fmt.pix.width = capability.width;
    video_fmt.fmt.pix.height = capability.height;

    bool formatMatch = false;
    for (int i = 0; i < nFormats; i++)
    {
        video_fmt.fmt.pix.pixelformat = fmts[i];
        if (ioctl(_deviceFd, VIDIOC_TRY_FMT, &video_fmt) < 0)
        {
            continue;
        }
        else
        {
            formatMatch = true;
            break;
        }
    }
    if (!formatMatch)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                   "no supporting video formats found");
        return -1;
    }
    if (video_fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_YUYV)
        _captureVideoType = kVideoYUY2;
    else
        _captureVideoType = kVideoI420;

    //set format and frame size now
    if (ioctl(_deviceFd, VIDIOC_S_FMT, &video_fmt) < 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                   "error in VIDIOC_S_FMT, errno = %d", errno);
        return -1;
    }

    // initialize current width and height
    _currentWidth = video_fmt.fmt.pix.width;
    _currentHeight = video_fmt.fmt.pix.height;
    _captureDelay = 120;
    if(_currentWidth >= 800)
      _currentFrameRate = 15;
    else
      _currentFrameRate = 30; // No way of knowing on Linux.

    if (!AllocateVideoBuffers())
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                   "failed to allocate video capture buffers");
        return -1;
    }

    //start capture thread;
    if (!_captureThread)
    {
        _captureThread = ThreadWrapper::CreateThread(
            VideoCaptureModuleV4L2::CaptureThread, this, kHighPriority);
        unsigned int id;
        _captureThread->Start(id);
    }

    // Needed to start UVC camera - from the uvcview application
    enum v4l2_buf_type type;
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(_deviceFd, VIDIOC_STREAMON, &type) == -1)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                     "Failed to turn on stream");
        return -1;
    }

    _captureStarted = true;
    return 0;
}

WebRtc_Word32 VideoCaptureModuleV4L2::StopCapture()
{
    if (_captureThread)
        _captureThread->SetNotAlive();// Make sure the capture thread stop stop using the critsect.


    CriticalSectionScoped cs(*_captureCritSect);

    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, -1, "StopCapture(), was running: %d",
               _captureStarted);

    if (!_captureStarted)
    {
        // we were not capturing!
        return 0;
    }

    _captureStarted = false;

    // stop the capture thread
    // Delete capture update thread and event
    if (_captureThread)
    {
        ThreadWrapper* temp = _captureThread;
        _captureThread = NULL;
        temp->SetNotAlive();
        if (temp->Stop())
        {
            delete temp;
        }
    }

    DeAllocateVideoBuffers();
    close(_deviceFd);
    _deviceFd = -1;

    return 0;
}

//critical section protected by the caller

bool VideoCaptureModuleV4L2::AllocateVideoBuffers()
{
    struct v4l2_requestbuffers rbuffer;
    memset(&rbuffer, 0, sizeof(v4l2_requestbuffers));

    rbuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    rbuffer.memory = V4L2_MEMORY_MMAP;
    rbuffer.count = kNoOfV4L2Bufffers;

    if (ioctl(_deviceFd, VIDIOC_REQBUFS, &rbuffer) < 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                   "Could not get buffers from device. errno = %d", errno);
        return false;
    }

    if (rbuffer.count > kNoOfV4L2Bufffers)
        rbuffer.count = kNoOfV4L2Bufffers;

    _buffersAllocatedByDevice = rbuffer.count;

    //Map the buffers
    pool = new Buffer[rbuffer.count];

    for (unsigned int i = 0; i < rbuffer.count; i++)
    {
        struct v4l2_buffer buffer;
        memset(&buffer, 0, sizeof(v4l2_buffer));
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = i;

        if (ioctl(_deviceFd, VIDIOC_QUERYBUF, &buffer) < 0)
        {
            return false;
        }

        pool[i].start = mmap(NULL, buffer.length, PROT_READ | PROT_WRITE, MAP_SHARED,
                             _deviceFd, buffer.m.offset);

        if (MAP_FAILED == pool[i].start)
        {
            for (unsigned int j = 0; j < i; j++)
                munmap(pool[j].start, pool[j].length);
            return false;
        }

        pool[i].length = buffer.length;

        if (ioctl(_deviceFd, VIDIOC_QBUF, &buffer) < 0)
        {
            return false;
        }
    }
    return true;
}

bool VideoCaptureModuleV4L2::DeAllocateVideoBuffers()
{
    // unmap buffers
    for (int i = 0; i < _buffersAllocatedByDevice; i++)
        munmap(pool[i].start, pool[i].length);

    delete[] pool;

    // turn off stream
    enum v4l2_buf_type type;
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(_deviceFd, VIDIOC_STREAMOFF, &type) < 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                   "VIDIOC_STREAMOFF error. errno: %d", errno);
    }

    return true;
}

bool VideoCaptureModuleV4L2::CaptureStarted()
{
    return _captureStarted;
}

bool VideoCaptureModuleV4L2::CaptureThread(void* obj)
{
    return static_cast<VideoCaptureModuleV4L2*> (obj)->CaptureProcess();
}
bool VideoCaptureModuleV4L2::CaptureProcess()
{
    int retVal = 0;
    fd_set rSet;
    struct timeval timeout;

    _captureCritSect->Enter();
    if (!_captureThread)
    {
        // terminating
        _captureCritSect->Leave();
        return false;
    }

    FD_ZERO(&rSet);
    FD_SET(_deviceFd, &rSet);
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    retVal = select(_deviceFd + 1, &rSet, NULL, NULL, &timeout);
    if (retVal < 0 && errno != EINTR) // continue if interrupted
    {
        // select failed
        _captureCritSect->Leave();
        return false;
    }
    else if (retVal == 0)
    {
        // select timed out
        _captureCritSect->Leave();
        return true;
    }
    else if (!FD_ISSET(_deviceFd, &rSet))
    {
        // not event on camera handle
        _captureCritSect->Leave();
        return true;
    }

    if (_captureStarted)
    {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(struct v4l2_buffer));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        // dequeue a buffer - repeat until dequeued properly!
        while (ioctl(_deviceFd, VIDIOC_DQBUF, &buf) < 0)
        {
            if (errno != EINTR)
            {
                WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                           "could not sync on a buffer on device %s", strerror(errno));
                _captureCritSect->Leave();
                return true;
            }
        }
        VideoCaptureCapability frameInfo;
        frameInfo.width = _currentWidth;
        frameInfo.height = _currentHeight;
        frameInfo.rawType = _captureVideoType;

        // convert to to I420 if needed
        IncomingFrame((unsigned char*) pool[buf.index].start,
                      buf.bytesused, frameInfo);
        // enqueue the buffer again
        if (ioctl(_deviceFd, VIDIOC_QBUF, &buf) == -1)
        {
            WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideoCapture, _id,
                       "Failed to enqueue capture buffer");
        }
    }
    _captureCritSect->Leave();
    usleep(0);
    return true;
}

WebRtc_Word32 VideoCaptureModuleV4L2::CaptureSettings(VideoCaptureCapability& settings)
{
    settings.width = _currentWidth;
    settings.height = _currentHeight;
    settings.maxFPS = _currentFrameRate;
    settings.rawType=_captureVideoType;

    return 0;
}
} // namespace videocapturemodule
} // namespace webrtc
