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

 WebRtcIlbcfix_NearestNeighbor.c

******************************************************************/

#include "defines.h"

/*----------------------------------------------------------------*
 * Find index in array such that the array element with said
 * index is the element of said array closest to "value"
 * according to the squared-error criterion
 *---------------------------------------------------------------*/

void WebRtcIlbcfix_NearestNeighbor(
    WebRtc_Word16 *index, /* (o) index of array element closest to value */
    WebRtc_Word16 *array, /* (i) data array (Q2) */
    WebRtc_Word16 value, /* (i) value (Q2) */
    WebRtc_Word16 arlength /* (i) dimension of data array (==8) */
                                   ){
  int i;
  WebRtc_Word16 diff;
  /* Stack based */
  WebRtc_Word32 crit[8];

  /* Calculate square distance */
  for(i=0;i<arlength;i++){
    diff=array[i]-value;
    crit[i]=WEBRTC_SPL_MUL_16_16(diff, diff);
  }

  /* Find the minimum square distance */
  *index=WebRtcSpl_MinIndexW32(crit, (WebRtc_Word16)arlength);
}
