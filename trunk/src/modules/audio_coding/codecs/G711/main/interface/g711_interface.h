/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_CODECS_G711_MAIN_INTERFACE_G711_INTERFACE_H_
#define MODULES_AUDIO_CODING_CODECS_G711_MAIN_INTERFACE_G711_INTERFACE_H_

#include "typedefs.h"

// Comfort noise constants
#define G711_WEBRTC_SPEECH    1
#define G711_WEBRTC_CNG       2

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************************
 * WebRtcG711_EncodeA(...)
 *
 * This function encodes a G711 A-law frame and inserts it into a packet.
 * Input speech length has be of any length.
 *
 * Input:
 *      - state              : Dummy state to make this codec look more like
 *                             other codecs
 *      - speechIn           : Input speech vector
 *      - len                : Samples in speechIn
 *
 * Output:
 *      - encoded            : The encoded data vector
 *
 * Return value              : >0 - Length (in bytes) of coded data
 *                             -1 - Error
 */

WebRtc_Word16 WebRtcG711_EncodeA(void *state,
                                 WebRtc_Word16 *speechIn,
                                 WebRtc_Word16 len,
                                 WebRtc_Word16 *encoded);

/****************************************************************************
 * WebRtcG711_EncodeU(...)
 *
 * This function encodes a G711 U-law frame and inserts it into a packet.
 * Input speech length has be of any length.
 *
 * Input:
 *      - state              : Dummy state to make this codec look more like
 *                             other codecs
 *      - speechIn           : Input speech vector
 *      - len                : Samples in speechIn
 *
 * Output:
 *      - encoded            : The encoded data vector
 *
 * Return value              : >0 - Length (in bytes) of coded data
 *                             -1 - Error
 */

WebRtc_Word16 WebRtcG711_EncodeU(void *state,
                                 WebRtc_Word16 *speechIn,
                                 WebRtc_Word16 len,
                                 WebRtc_Word16 *encoded);

/****************************************************************************
 * WebRtcG711_DecodeA(...)
 *
 * This function decodes a packet G711 A-law frame.
 *
 * Input:
 *      - state              : Dummy state to make this codec look more like
 *                             other codecs
 *      - encoded            : Encoded data
 *      - len                : Bytes in encoded vector
 *
 * Output:
 *      - decoded            : The decoded vector
 *      - speechType         : 1 normal, 2 CNG (for G711 it should
 *                             always return 1 since G711 does not have a
 *                             built-in DTX/CNG scheme)
 *
 * Return value              : >0 - Samples in decoded vector
 *                             -1 - Error
 */

WebRtc_Word16 WebRtcG711_DecodeA(void *state,
                                 WebRtc_Word16 *encoded,
                                 WebRtc_Word16 len,
                                 WebRtc_Word16 *decoded,
                                 WebRtc_Word16 *speechType);

/****************************************************************************
 * WebRtcG711_DecodeU(...)
 *
 * This function decodes a packet G711 U-law frame.
 *
 * Input:
 *      - state              : Dummy state to make this codec look more like
 *                             other codecs
 *      - encoded            : Encoded data
 *      - len                : Bytes in encoded vector
 *
 * Output:
 *      - decoded            : The decoded vector
 *      - speechType         : 1 normal, 2 CNG (for G711 it should
 *                             always return 1 since G711 does not have a
 *                             built-in DTX/CNG scheme)
 *
 * Return value              : >0 - Samples in decoded vector
 *                             -1 - Error
 */

WebRtc_Word16 WebRtcG711_DecodeU(void *state,
                                 WebRtc_Word16 *encoded,
                                 WebRtc_Word16 len,
                                 WebRtc_Word16 *decoded,
                                 WebRtc_Word16 *speechType);

/**********************************************************************
* WebRtcG711_Version(...)
*
* This function gives the version string of the G.711 codec.
*
* Input:
*      - lenBytes:     the size of Allocated space (in Bytes) where
*                      the version number is written to (in string format).
*
* Output:
*      - version:      Pointer to a buffer where the version number is
*                      written to.
*
*/

WebRtc_Word16 WebRtcG711_Version(char* version, WebRtc_Word16 lenBytes);

#ifdef __cplusplus
}
#endif


#endif /* MODULES_AUDIO_CODING_CODECS_G711_MAIN_INTERFACE_G711_INTERFACE_H_ */
