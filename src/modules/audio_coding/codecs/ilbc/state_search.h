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

 WebRtcIlbcfix_StateSearch.h

******************************************************************/

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_STATE_SEARCH_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_STATE_SEARCH_H_

#include "defines.h"

/*----------------------------------------------------------------*
 *  encoding of start state
 *---------------------------------------------------------------*/

void WebRtcIlbcfix_StateSearch(
    iLBC_Enc_Inst_t *iLBCenc_inst,
    /* (i) Encoder instance */
    iLBC_bits *iLBC_encbits,/* (i/o) Encoded bits (output idxForMax
                               and idxVec, input state_first) */
    WebRtc_Word16 *residual,   /* (i) target residual vector */
    WebRtc_Word16 *syntDenum,  /* (i) lpc synthesis filter */
    WebRtc_Word16 *weightDenum  /* (i) weighting filter denuminator */
                               );

#endif
