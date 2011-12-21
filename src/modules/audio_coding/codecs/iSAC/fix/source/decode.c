/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * decode.c
 *
 * This C file contains the internal decoding function.
 *
 */

#include <string.h>

#include "bandwidth_estimator.h"
#include "codec.h"
#include "entropy_coding.h"
#include "pitch_estimator.h"
#include "settings.h"
#include "structs.h"




WebRtc_Word16 WebRtcIsacfix_DecodeImpl(WebRtc_Word16       *signal_out16,
                                       ISACFIX_DecInst_t *ISACdec_obj,
                                       WebRtc_Word16       *current_framesamples)
{
  int k;
  int err;
  WebRtc_Word16 BWno;
  WebRtc_Word16 len = 0;

  WebRtc_Word16 model;


  WebRtc_Word16 Vector_Word16_1[FRAMESAMPLES/2];
  WebRtc_Word16 Vector_Word16_2[FRAMESAMPLES/2];

  WebRtc_Word32 Vector_Word32_1[FRAMESAMPLES/2];
  WebRtc_Word32 Vector_Word32_2[FRAMESAMPLES/2];

  WebRtc_Word16 lofilt_coefQ15[ORDERLO*SUBFRAMES]; //refl. coeffs
  WebRtc_Word16 hifilt_coefQ15[ORDERHI*SUBFRAMES]; //refl. coeffs
  WebRtc_Word32 gain_lo_hiQ17[2*SUBFRAMES];

  WebRtc_Word16 PitchLags_Q7[PITCH_SUBFRAMES];
  WebRtc_Word16 PitchGains_Q12[PITCH_SUBFRAMES];
  WebRtc_Word16 AvgPitchGain_Q12;

  WebRtc_Word16 tmp_1, tmp_2;
  WebRtc_Word32 tmp32a, tmp32b;
  WebRtc_Word16 gainQ13;


  WebRtc_Word16 frame_nb; /* counter */
  WebRtc_Word16 frame_mode; /* 0 for 20ms and 30ms, 1 for 60ms */
  WebRtc_Word16 processed_samples;

  /* PLC */
  WebRtc_Word16 overlapWin[ 240 ];

  (ISACdec_obj->bitstr_obj).W_upper = 0xFFFFFFFF;
  (ISACdec_obj->bitstr_obj).streamval = 0;
  (ISACdec_obj->bitstr_obj).stream_index = 0;
  (ISACdec_obj->bitstr_obj).full = 1;


  /* decode framelength and BW estimation - not used, only for stream pointer*/
  err = WebRtcIsacfix_DecodeFrameLen(&ISACdec_obj->bitstr_obj, current_framesamples);
  if (err<0)  // error check
    return err;

  frame_mode = (WebRtc_Word16)WEBRTC_SPL_DIV(*current_framesamples, MAX_FRAMESAMPLES); /* 0, or 1 */
  processed_samples = (WebRtc_Word16)WEBRTC_SPL_DIV(*current_framesamples, frame_mode+1); /* either 320 (20ms) or 480 (30, 60 ms) */

  err = WebRtcIsacfix_DecodeSendBandwidth(&ISACdec_obj->bitstr_obj, &BWno);
  if (err<0)  // error check
    return err;

  /* one loop if it's one frame (20 or 30ms), 2 loops if 2 frames bundled together (60ms) */
  for (frame_nb = 0; frame_nb <= frame_mode; frame_nb++) {

    /* decode & dequantize pitch parameters */
    err = WebRtcIsacfix_DecodePitchGain(&(ISACdec_obj->bitstr_obj), PitchGains_Q12);
    if (err<0)  // error check
      return err;

    err = WebRtcIsacfix_DecodePitchLag(&ISACdec_obj->bitstr_obj, PitchGains_Q12, PitchLags_Q7);
    if (err<0)  // error check
      return err;

    AvgPitchGain_Q12 = (WebRtc_Word16)(((WebRtc_Word32)PitchGains_Q12[0] + PitchGains_Q12[1] + PitchGains_Q12[2] + PitchGains_Q12[3])>>2);

    /* decode & dequantize FiltCoef */
    err = WebRtcIsacfix_DecodeLpc(gain_lo_hiQ17, lofilt_coefQ15, hifilt_coefQ15,
                                  &ISACdec_obj->bitstr_obj, &model);

    if (err<0)  // error check
      return err;

    /* decode & dequantize spectrum */
    len = WebRtcIsacfix_DecodeSpec(&ISACdec_obj->bitstr_obj, Vector_Word16_1, Vector_Word16_2, AvgPitchGain_Q12);
    if (len < 0)  // error check
      return len;

    // Why does this need Q16 in and out? /JS
    WebRtcIsacfix_Spec2Time(Vector_Word16_1, Vector_Word16_2, Vector_Word32_1, Vector_Word32_2);

    for (k=0; k<FRAMESAMPLES/2; k++) {
      Vector_Word16_1[k] = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32(Vector_Word32_1[k]+64, 7); //Q16 -> Q9
    }

    /* ----  If this is recovery frame ---- */
    if( (ISACdec_obj->plcstr_obj).used == PLC_WAS_USED )
    {
      (ISACdec_obj->plcstr_obj).used = PLC_NOT_USED;
      if( (ISACdec_obj->plcstr_obj).B < 1000 )
      {
        (ISACdec_obj->plcstr_obj).decayCoeffPriodic = 4000;
      }

      ISACdec_obj->plcstr_obj.decayCoeffPriodic = WEBRTC_SPL_WORD16_MAX;    /* DECAY_RATE is in Q15 */
      ISACdec_obj->plcstr_obj.decayCoeffNoise = WEBRTC_SPL_WORD16_MAX;    /* DECAY_RATE is in Q15 */
      ISACdec_obj->plcstr_obj.pitchCycles = 0;

      PitchGains_Q12[0] = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(PitchGains_Q12[0], 700, 10 );

      /* ---- Add-overlap ---- */
      WebRtcSpl_GetHanningWindow( overlapWin, RECOVERY_OVERLAP );
      for( k = 0; k < RECOVERY_OVERLAP; k++ )
        Vector_Word16_1[k] = WEBRTC_SPL_ADD_SAT_W16(
            (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT( (ISACdec_obj->plcstr_obj).overlapLP[k], overlapWin[RECOVERY_OVERLAP - k - 1], 14),
            (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT( Vector_Word16_1[k], overlapWin[k], 14) );



    }

    /* --- Store side info --- */
    if( frame_nb == frame_mode )
    {
      /* --- LPC info */
      WEBRTC_SPL_MEMCPY_W16( (ISACdec_obj->plcstr_obj).lofilt_coefQ15, &lofilt_coefQ15[(SUBFRAMES-1)*ORDERLO], ORDERLO );
      WEBRTC_SPL_MEMCPY_W16( (ISACdec_obj->plcstr_obj).hifilt_coefQ15, &hifilt_coefQ15[(SUBFRAMES-1)*ORDERHI], ORDERHI );
      (ISACdec_obj->plcstr_obj).gain_lo_hiQ17[0] = gain_lo_hiQ17[(SUBFRAMES-1) * 2];
      (ISACdec_obj->plcstr_obj).gain_lo_hiQ17[1] = gain_lo_hiQ17[(SUBFRAMES-1) * 2 + 1];

      /* --- LTP info */
      (ISACdec_obj->plcstr_obj).AvgPitchGain_Q12 = PitchGains_Q12[3];
      (ISACdec_obj->plcstr_obj).lastPitchGain_Q12 = PitchGains_Q12[3];
      (ISACdec_obj->plcstr_obj).lastPitchLag_Q7 = PitchLags_Q7[3];

      if( PitchLags_Q7[3] < 3000 )
        (ISACdec_obj->plcstr_obj).lastPitchLag_Q7 += PitchLags_Q7[3];

      WEBRTC_SPL_MEMCPY_W16( (ISACdec_obj->plcstr_obj).prevPitchInvIn, Vector_Word16_1, FRAMESAMPLES/2 );

    }
    /* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

    /* inverse pitch filter */
    WebRtcIsacfix_PitchFilter(Vector_Word16_1, Vector_Word16_2, &ISACdec_obj->pitchfiltstr_obj, PitchLags_Q7, PitchGains_Q12, 4);

    if( frame_nb == frame_mode )
    {
      WEBRTC_SPL_MEMCPY_W16( (ISACdec_obj->plcstr_obj).prevPitchInvOut, &(Vector_Word16_2[FRAMESAMPLES/2 - (PITCH_MAX_LAG + 10)]), PITCH_MAX_LAG );
    }


    /* reduce gain to compensate for pitch enhancer */
    /* gain = 1.0f - 0.45f * AvgPitchGain; */
    tmp32a = WEBRTC_SPL_MUL_16_16_RSFT(AvgPitchGain_Q12, 29, 0); // Q18
    tmp32b = 262144 - tmp32a;  // Q18
    gainQ13 = (WebRtc_Word16) WEBRTC_SPL_RSHIFT_W32(tmp32b, 5); // Q13

    for (k = 0; k < FRAMESAMPLES/2; k++)
    {
      Vector_Word32_1[k] = (WebRtc_Word32) WEBRTC_SPL_LSHIFT_W32(WEBRTC_SPL_MUL_16_16(Vector_Word16_2[k], gainQ13), 3); // Q25
    }


    /* perceptual post-filtering (using normalized lattice filter) */
    WebRtcIsacfix_NormLatticeFilterAr(ORDERLO, (ISACdec_obj->maskfiltstr_obj).PostStateLoGQ0,
                                      Vector_Word32_1, lofilt_coefQ15, gain_lo_hiQ17, 0, Vector_Word16_1);

    /* --- Store Highpass Residual --- */
    for (k = 0; k < FRAMESAMPLES/2; k++)
      Vector_Word32_1[k]    = WEBRTC_SPL_LSHIFT_W32(Vector_Word32_2[k], 9); // Q16 -> Q25

    for( k = 0; k < PITCH_MAX_LAG + 10; k++ )
      (ISACdec_obj->plcstr_obj).prevHP[k] = Vector_Word32_1[FRAMESAMPLES/2 - (PITCH_MAX_LAG + 10) + k];


    WebRtcIsacfix_NormLatticeFilterAr(ORDERHI, (ISACdec_obj->maskfiltstr_obj).PostStateHiGQ0,
                                      Vector_Word32_1, hifilt_coefQ15, gain_lo_hiQ17, 1, Vector_Word16_2);

    /* recombine the 2 bands */

    /* Form the polyphase signals, and compensate for DC offset */
    for (k=0;k<FRAMESAMPLES/2;k++) {
      tmp_1 = (WebRtc_Word16)WebRtcSpl_SatW32ToW16(((WebRtc_Word32)Vector_Word16_1[k]+Vector_Word16_2[k] + 1)); /* Construct a new upper channel signal*/
      tmp_2 = (WebRtc_Word16)WebRtcSpl_SatW32ToW16(((WebRtc_Word32)Vector_Word16_1[k]-Vector_Word16_2[k])); /* Construct a new lower channel signal*/
      Vector_Word16_1[k] = tmp_1;
      Vector_Word16_2[k] = tmp_2;
    }

    WebRtcIsacfix_FilterAndCombine1(Vector_Word16_1, Vector_Word16_2, signal_out16 + frame_nb * processed_samples, &ISACdec_obj->postfiltbankstr_obj);

  }
  return len;
}
