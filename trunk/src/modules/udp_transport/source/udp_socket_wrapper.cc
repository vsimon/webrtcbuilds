/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "udp_socket_wrapper.h"

#include <stdlib.h>
#include <string.h>

#include "event_wrapper.h"
#include "trace.h"
#include "udp_socket_manager_wrapper.h"

#if defined(_WIN32)
    #include "udp_socket_windows.h"
    #include "udp_socket2_windows.h"
#else
    #include "udp_socket_posix.h"
#endif


namespace webrtc {
bool UdpSocketWrapper::_initiated = false;

// Temporary Android hack. The value 1024 is taken from
// <ndk>/build/platforms/android-1.5/arch-arm/usr/include/linux/posix_types.h
// TODO (tomasl): can we remove this now?
#ifndef FD_SETSIZE
#define FD_SETSIZE 1024
#endif

UdpSocketWrapper::UdpSocketWrapper() : _deleteEvent(NULL)
{
}

UdpSocketWrapper::~UdpSocketWrapper()
{
    if(_deleteEvent)
    {
      _deleteEvent->Set();
      _deleteEvent = NULL;
    }
}

void UdpSocketWrapper::SetEventToNull()
{
    if (_deleteEvent)
    {
        _deleteEvent = NULL;
    }
}

#ifdef USE_WINSOCK2
UdpSocketWrapper* UdpSocketWrapper::CreateSocket(const WebRtc_Word32 id,
                                                 UdpSocketManager* mgr,
                                                 CallbackObj obj,
                                                 IncomingSocketCallback cb,
                                                 bool ipV6Enable,
                                                 bool disableGQOS)
#else
UdpSocketWrapper* UdpSocketWrapper::CreateSocket(const WebRtc_Word32 id,
                                                 UdpSocketManager* mgr,
                                                 CallbackObj obj,
                                                 IncomingSocketCallback cb,
                                                 bool ipV6Enable,
                                                 bool /*disableGQOS*/)
#endif

{
    WEBRTC_TRACE(kTraceMemory, kTraceTransport, id,
                 "UdpSocketWrapper::CreateSocket");

    UdpSocketWrapper* s = 0;

#ifdef _WIN32
    if (!_initiated)
    {
        WSADATA wsaData;
        WORD wVersionRequested = MAKEWORD( 2, 2 );
        WebRtc_Word32 err = WSAStartup( wVersionRequested, &wsaData);
        if (err != 0)
        {
            WEBRTC_TRACE(
                kTraceError,
                kTraceTransport,
                id,
                "UdpSocketWrapper::CreateSocket failed to initialize sockets\
 WSAStartup error:%d",
                err);
            return NULL;
        }

        _initiated = true;
    }

#ifdef USE_WINSOCK2
    s = new UdpSocket2Windows(id, mgr, ipV6Enable, disableGQOS);
#else
    #pragma message("Error: No non-Winsock2 implementation for WinCE")
    s = new UdpSocketWindows(id, mgr, ipV6Enable);
#endif

#else
    if (!_initiated)
    {
        _initiated = true;
    }
    s = new UdpSocketPosix(id, mgr, ipV6Enable);
    if (s)
    {
        UdpSocketPosix* sl = static_cast<UdpSocketPosix*>(s);
        if (sl->GetFd() != INVALID_SOCKET && sl->GetFd() < FD_SETSIZE)
        {
            // ok
        } else
        {
            WEBRTC_TRACE(
                kTraceError,
                kTraceTransport,
                id,
                "UdpSocketWrapper::CreateSocket failed to initialize socket");
            delete s;
            s = NULL;
        }
    }
#endif
    if (s)
    {
        s->_deleteEvent = NULL;
        if (!s->SetCallback(obj, cb))
        {
            WEBRTC_TRACE(
                kTraceError,
                kTraceTransport,
                id,
                "UdpSocketWrapper::CreateSocket failed to ser callback");
            return(NULL);
        }
    }
    return s;
}

bool UdpSocketWrapper::StartReceiving()
{
    _wantsIncoming = true;
    return true;
}

bool UdpSocketWrapper::StopReceiving()
{
    _wantsIncoming = false;
    return true;
}
} // namespace webrtc
