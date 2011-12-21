/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/******************************************************************

 iLBC Speech Coder ANSI-C Source Code

 WebRtcIlbcfix_LsfInterpolate2PloyEnc.h

******************************************************************/

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_LSF_INTERPOLATE_TO_POLY_ENC_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_LSF_INTERPOLATE_TO_POLY_ENC_H_

#include "defines.h"

/*----------------------------------------------------------------*
 *  lsf interpolator and conversion from lsf to a coefficients
 *  (subrutine to SimpleInterpolateLSF)
 *---------------------------------------------------------------*/

void WebRtcIlbcfix_LsfInterpolate2PloyEnc(
    WebRtc_Word16 *a,  /* (o) lpc coefficients Q12 */
    WebRtc_Word16 *lsf1, /* (i) first set of lsf coefficients Q13 */
    WebRtc_Word16 *lsf2, /* (i) second set of lsf coefficients Q13 */
    WebRtc_Word16 coef, /* (i) weighting coefficient to use between
                           lsf1 and lsf2 Q14 */
    WebRtc_Word16 length /* (i) length of coefficient vectors */
                                          );

#endif
