/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This sub-API supports the following functionalities:
//  - Secure RTP (SRTP).
//  - External encryption and decryption.


#ifndef WEBRTC_VIDEO_ENGINE_MAIN_INTERFACE_VIE_ENCRYPTION_H_
#define WEBRTC_VIDEO_ENGINE_MAIN_INTERFACE_VIE_ENCRYPTION_H_

#include "common_types.h"

namespace webrtc
{
class VideoEngine;

// ----------------------------------------------------------------------------
//	ViEEncryption
// ----------------------------------------------------------------------------

class WEBRTC_DLLEXPORT ViEEncryption
{
public:
    enum
    {
        kMaxSrtpKeyLength = 30
    };

    // Factory for the ViEEncryption sub‐API and increases an internal reference
    // counter if successful. Returns NULL if the API is not supported or if
    // construction fails.
    static ViEEncryption* GetInterface(VideoEngine* videoEngine);

    // Releases the ViEEncryption sub-API and decreases an internal reference
    // counter.
    // Returns the new reference count. This value should be zero
    // for all sub-API:s before the VideoEngine object can be safely deleted.
    virtual int Release() = 0;

    // This function enables SRTP on send packets for a specific channel.
    virtual int EnableSRTPSend(const int videoChannel,
                               const CipherTypes cipherType,
                               const unsigned int cipherKeyLength,
                               const AuthenticationTypes authType,
                               const unsigned int authKeyLength,
                               const unsigned int authTagLength,
                               const SecurityLevels level,
                               const unsigned char key[kMaxSrtpKeyLength],
                               const bool useForRTCP = false) = 0;

    // This function disables SRTP for the specified channel.
    virtual int DisableSRTPSend(const int videoChannel) = 0;

    // This function enables SRTP on the received packets for a specific
    // channel.
    virtual int EnableSRTPReceive(const int videoChannel,
                                  const CipherTypes cipherType,
                                  const unsigned int cipherKeyLength,
                                  const AuthenticationTypes authType,
                                  const unsigned int authKeyLength,
                                  const unsigned int authTagLength,
                                  const SecurityLevels level,
                                  const unsigned char key[kMaxSrtpKeyLength],
                                  const bool useForRTCP = false) = 0;

    // This function disables SRTP on received packets for a specific channel.
    virtual int DisableSRTPReceive(const int videoChannel) = 0;

    // This function registers a encryption derived instance and enables
    // external encryption for the specified channel.
    virtual int RegisterExternalEncryption(const int videoChannel,
                                           Encryption& encryption) = 0;

    // This function deregisters a registered encryption derived instance
    // and disables external encryption.
    virtual int DeregisterExternalEncryption(const int videoChannel) = 0;

protected:
    ViEEncryption() {};
    virtual ~ViEEncryption() {};
};
} // namespace webrtc
#endif  // WEBRTC_VIDEO_ENGINE_MAIN_INTERFACE_VIE_ENCRYPTION_H_
