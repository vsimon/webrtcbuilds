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

 WebRtcIlbcfix_LpcEncode.h

******************************************************************/

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_LPC_ENCODE_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_LPC_ENCODE_H_

#include "defines.h"

/*----------------------------------------------------------------*
 *  lpc encoder
 *---------------------------------------------------------------*/

void WebRtcIlbcfix_LpcEncode(
    WebRtc_Word16 *syntdenum,  /* (i/o) synthesis filter coefficients
                                  before/after encoding */
    WebRtc_Word16 *weightdenum, /* (i/o) weighting denumerator coefficients
                                   before/after encoding */
    WebRtc_Word16 *lsf_index,  /* (o) lsf quantization index */
    WebRtc_Word16 *data,   /* (i) Speech to do LPC analysis on */
    iLBC_Enc_Inst_t *iLBCenc_inst
    /* (i/o) the encoder state structure */
                             );

#endif
