/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "udp_socket_posix.h"

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "trace.h"
#include "udp_socket_manager_wrapper.h"
#include "udp_socket_wrapper.h"

namespace webrtc {
UdpSocketPosix::UdpSocketPosix(const WebRtc_Word32 id, UdpSocketManager* mgr,
                               bool ipV6Enable)
{
    WEBRTC_TRACE(kTraceMemory, kTraceTransport, id,
                 "UdpSocketPosix::UdpSocketPosix()");

    _wantsIncoming = false;
    _error = 0;
    _mgr = mgr;

    _id = id;
    _obj = NULL;
    _incomingCb = NULL;
    _readyForDeletionCond = ConditionVariableWrapper::CreateConditionVariable();
    _closeBlockingCompletedCond =
        ConditionVariableWrapper::CreateConditionVariable();
    _cs = CriticalSectionWrapper::CreateCriticalSection();
    _readyForDeletion = false;
    _closeBlockingActive = false;
    _closeBlockingCompleted= false;
    if(ipV6Enable)
    {
        _socket = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    }
    else {
        _socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    }

    // Set socket to nonblocking mode.
    int enable_non_blocking = 1;
    if(ioctl(_socket, FIONBIO, &enable_non_blocking) == -1)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceTransport, id,
                     "Failed to make socket nonblocking");
    }
    // Enable close on fork for file descriptor so that it will not block until
    // forked process terminates.
    if(fcntl(_socket, F_SETFD, FD_CLOEXEC) == -1)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceTransport, id,
                     "Failed to set FD_CLOEXEC for socket");
    }
}

UdpSocketPosix::~UdpSocketPosix()
{
    if(_socket != INVALID_SOCKET)
    {
        close(_socket);
        _socket = INVALID_SOCKET;
    }
    if(_readyForDeletionCond)
    {
        delete _readyForDeletionCond;
    }

    if(_closeBlockingCompletedCond)
    {
        delete _closeBlockingCompletedCond;
    }

    if(_cs)
    {
        delete _cs;
    }
}

WebRtc_Word32 UdpSocketPosix::ChangeUniqueId(const WebRtc_Word32 id)
{
    _id = id;
    return 0;
}

bool UdpSocketPosix::SetCallback(CallbackObj obj, IncomingSocketCallback cb)
{
    _obj = obj;
    _incomingCb = cb;

    WEBRTC_TRACE(kTraceDebug, kTraceTransport, _id,
                 "UdpSocketPosix(%p)::SetCallback", this);

    if (_mgr->AddSocket(this))
      {
        WEBRTC_TRACE(kTraceDebug, kTraceTransport, _id,
                     "UdpSocketPosix(%p)::SetCallback socket added to manager",
                     this);
        return true;   // socket is now ready for action
      }

    WEBRTC_TRACE(kTraceDebug, kTraceTransport, _id,
                 "UdpSocketPosix(%p)::SetCallback error adding me to mgr",
                 this);
    return false;
}

bool UdpSocketPosix::SetSockopt(WebRtc_Word32 level, WebRtc_Word32 optname,
                            const WebRtc_Word8* optval, WebRtc_Word32 optlen)
{
   if(0 == setsockopt(_socket, level, optname, optval, optlen ))
   {
       return true;
   }

   _error = errno;
   WEBRTC_TRACE(kTraceError, kTraceTransport, _id,
                "UdpSocketPosix::SetSockopt(), error:%d", _error);
   return false;
}

WebRtc_Word32 UdpSocketPosix::SetTOS(WebRtc_Word32 serviceType)
{
    if (SetSockopt(IPPROTO_IP, IP_TOS ,(WebRtc_Word8*)&serviceType ,4) != 0)
    {
        return -1;
    }
    return 0;
}

bool UdpSocketPosix::Bind(const SocketAddress& name)
{
    int size = sizeof(sockaddr);
    if (0 == bind(_socket, reinterpret_cast<const sockaddr*>(&name),size))
    {
        return true;
    }
    _error = errno;
    WEBRTC_TRACE(kTraceError, kTraceTransport, _id,
                 "UdpSocketPosix::Bind() error: %d",_error);
    return false;
}

WebRtc_Word32 UdpSocketPosix::SendTo(const WebRtc_Word8* buf, WebRtc_Word32 len,
                                     const SocketAddress& to)
{
    int size = sizeof(sockaddr);
    int retVal = sendto(_socket,buf, len, 0,
                        reinterpret_cast<const sockaddr*>(&to), size);
    if(retVal == SOCKET_ERROR)
    {
        _error = errno;
        WEBRTC_TRACE(kTraceError, kTraceTransport, _id,
                     "UdpSocketPosix::SendTo() error: %d", _error);
    }

    return retVal;
}

bool UdpSocketPosix::ValidHandle()
{
    return _socket != INVALID_SOCKET;
}

void UdpSocketPosix::HasIncoming()
{
    // replace 2048 with a mcro define and figure out
    // where 2048 comes from
    WebRtc_Word8 buf[2048];
    int retval;
    SocketAddress from;
#if defined(WEBRTC_MAC)
    sockaddr sockaddrfrom;
    memset(&from, 0, sizeof(from));
    memset(&sockaddrfrom, 0, sizeof(sockaddrfrom));
    socklen_t fromlen = sizeof(sockaddrfrom);
#else
    memset(&from, 0, sizeof(from));
    socklen_t fromlen = sizeof(from);
#endif

#if defined(WEBRTC_MAC)
        retval = recvfrom(_socket,buf, sizeof(buf), 0,
                          reinterpret_cast<sockaddr*>(&sockaddrfrom), &fromlen);
        memcpy(&from, &sockaddrfrom, fromlen);
        from._sockaddr_storage.sin_family = sockaddrfrom.sa_family;
#else
        retval = recvfrom(_socket,buf, sizeof(buf), 0,
                          reinterpret_cast<sockaddr*>(&from), &fromlen);
#endif

    switch(retval)
    {
    case 0:
        // The peer has performed an orderly shutdown.
        break;
    case SOCKET_ERROR:
        break;
    default:
        if(_wantsIncoming && _incomingCb)
        {
          _incomingCb(_obj, buf, retval, &from);
        }
        break;
    }
}

void UdpSocketPosix::CloseBlocking()
{
    _cs->Enter();
    _closeBlockingActive = true;
    if(!CleanUp())
    {
        _closeBlockingActive = false;
        _cs->Leave();
        return;
    }

    while(!_readyForDeletion)
    {
        _readyForDeletionCond->SleepCS(*_cs);
    }
    _closeBlockingCompleted = true;
    _closeBlockingCompletedCond->Wake();
    _cs->Leave();
}

void UdpSocketPosix::ReadyForDeletion()
{
    _cs->Enter();
    if(!_closeBlockingActive)
    {
        _cs->Leave();
        return;
    }
    close(_socket);
    _socket = INVALID_SOCKET;
    _readyForDeletion = true;
    _readyForDeletionCond->Wake();
    while(!_closeBlockingCompleted)
    {
        _closeBlockingCompletedCond->SleepCS(*_cs);
    }
    _cs->Leave();
}

bool UdpSocketPosix::CleanUp()
{
    _wantsIncoming = false;

    if (_socket == INVALID_SOCKET)
    {
        return false;
    }

    WEBRTC_TRACE(kTraceDebug, kTraceTransport, _id,
                 "calling UdpSocketManager::RemoveSocket()...");
    _mgr->RemoveSocket(this);
    // After this, the socket should may be or will be as deleted. Return
    // immediately.
    return true;
}
} // namespace webrtc
