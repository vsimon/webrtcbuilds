/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * lpc_masking_model.h
 *
 * LPC functions
 *
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_FIX_SOURCE_LPC_MASKING_MODEL_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_FIX_SOURCE_LPC_MASKING_MODEL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "structs.h"

int32_t WebRtcIsacfix_CalculateResidualEnergy(int lpc_order,
                                              int32_t q_val_corr,
                                              int q_offset,
                                              int16_t* a_polynomial,
                                              int32_t* corr_coeffs,
                                              int* q_val_residual_energy);

void WebRtcIsacfix_GetVars(const WebRtc_Word16 *input,
                           const WebRtc_Word16 *pitchGains_Q12,
                           WebRtc_UWord32 *oldEnergy,
                           WebRtc_Word16 *varscale);

void WebRtcIsacfix_GetLpcCoef(WebRtc_Word16 *inLoQ0,
                              WebRtc_Word16 *inHiQ0,
                              MaskFiltstr_enc *maskdata,
                              WebRtc_Word16 snrQ10,
                              const WebRtc_Word16 *pitchGains_Q12,
                              WebRtc_Word32 *gain_lo_hiQ17,
                              WebRtc_Word16 *lo_coeffQ15,
                              WebRtc_Word16 *hi_coeffQ15);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_FIX_SOURCE_LPC_MASKING_MODEL_H_ */
