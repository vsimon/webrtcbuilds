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
 * WebRtcIsacfix_kTransform.c
 *
 * Transform functions
 *
 */

#include "webrtc/modules/audio_coding/codecs/isac/fix/source/codec.h"
#include "webrtc/modules/audio_coding/codecs/isac/fix/source/fft.h"
#include "webrtc/modules/audio_coding/codecs/isac/fix/source/settings.h"

#if (defined WEBRTC_DETECT_ARM_NEON || defined WEBRTC_ARCH_ARM_NEON)
/* Tables are defined in ARM assembly files. */
/* Cosine table 1 in Q14 */
extern const int16_t WebRtcIsacfix_kCosTab1[FRAMESAMPLES/2];
/* Sine table 1 in Q14 */
extern const int16_t WebRtcIsacfix_kSinTab1[FRAMESAMPLES/2];
/* Sine table 2 in Q14 */
extern const int16_t WebRtcIsacfix_kSinTab2[FRAMESAMPLES/4];
#else
/* Cosine table 1 in Q14 */
static const int16_t WebRtcIsacfix_kCosTab1[FRAMESAMPLES/2] = {
  16384,  16383,  16378,  16371,  16362,  16349,  16333,  16315,  16294,  16270,
  16244,  16214,  16182,  16147,  16110,  16069,  16026,  15980,  15931,  15880,
  15826,  15769,  15709,  15647,  15582,  15515,  15444,  15371,  15296,  15218,
  15137,  15053,  14968,  14879,  14788,  14694,  14598,  14500,  14399,  14295,
  14189,  14081,  13970,  13856,  13741,  13623,  13502,  13380,  13255,  13128,
  12998,  12867,  12733,  12597,  12458,  12318,  12176,  12031,  11885,  11736,
  11585,  11433,  11278,  11121,  10963,  10803,  10641,  10477,  10311,  10143,
  9974,   9803,   9630,   9456,   9280,   9102,   8923,   8743,   8561,   8377,
  8192,   8006,   7818,   7629,   7438,   7246,   7053,   6859,   6664,   6467,
  6270,   6071,   5872,   5671,   5469,   5266,   5063,   4859,   4653,   4447,
  4240,   4033,   3825,   3616,   3406,   3196,   2986,   2775,   2563,   2351,
  2139,   1926,   1713,   1499,   1285,   1072,    857,    643,    429,    214,
  0,   -214,   -429,   -643,   -857,  -1072,  -1285,  -1499,  -1713,  -1926,
  -2139,  -2351,  -2563,  -2775,  -2986,  -3196,  -3406,  -3616,  -3825,  -4033,
  -4240,  -4447,  -4653,  -4859,  -5063,  -5266,  -5469,  -5671,  -5872,  -6071,
  -6270,  -6467,  -6664,  -6859,  -7053,  -7246,  -7438,  -7629,  -7818,  -8006,
  -8192,  -8377,  -8561,  -8743,  -8923,  -9102,  -9280,  -9456,  -9630,  -9803,
  -9974, -10143, -10311, -10477, -10641, -10803, -10963, -11121, -11278, -11433,
  -11585, -11736, -11885, -12031, -12176, -12318, -12458, -12597, -12733,
  -12867, -12998, -13128, -13255, -13380, -13502, -13623, -13741, -13856,
  -13970, -14081, -14189, -14295, -14399, -14500, -14598, -14694, -14788,
  -14879, -14968, -15053, -15137, -15218, -15296, -15371, -15444, -15515,
  -15582, -15647, -15709, -15769, -15826, -15880, -15931, -15980, -16026,
  -16069, -16110, -16147, -16182, -16214, -16244, -16270, -16294, -16315,
  -16333, -16349, -16362, -16371, -16378, -16383
};

/* Sine table 1 in Q14 */
static const int16_t WebRtcIsacfix_kSinTab1[FRAMESAMPLES/2] = {
  0,   214,   429,   643,   857,  1072,  1285,  1499,  1713,  1926,
  2139,  2351,  2563,  2775,  2986,  3196,  3406,  3616,  3825,  4033,
  4240,  4447,  4653,  4859,  5063,  5266,  5469,  5671,  5872,  6071,
  6270,  6467,  6664,  6859,  7053,  7246,  7438,  7629,  7818,  8006,
  8192,  8377,  8561,  8743,  8923,  9102,  9280,  9456,  9630,  9803,
  9974, 10143, 10311, 10477, 10641, 10803, 10963, 11121, 11278, 11433,
  11585, 11736, 11885, 12031, 12176, 12318, 12458, 12597, 12733, 12867,
  12998, 13128, 13255, 13380, 13502, 13623, 13741, 13856, 13970, 14081,
  14189, 14295, 14399, 14500, 14598, 14694, 14788, 14879, 14968, 15053,
  15137, 15218, 15296, 15371, 15444, 15515, 15582, 15647, 15709, 15769,
  15826, 15880, 15931, 15980, 16026, 16069, 16110, 16147, 16182, 16214,
  16244, 16270, 16294, 16315, 16333, 16349, 16362, 16371, 16378, 16383,
  16384, 16383, 16378, 16371, 16362, 16349, 16333, 16315, 16294, 16270,
  16244, 16214, 16182, 16147, 16110, 16069, 16026, 15980, 15931, 15880,
  15826, 15769, 15709, 15647, 15582, 15515, 15444, 15371, 15296, 15218,
  15137, 15053, 14968, 14879, 14788, 14694, 14598, 14500, 14399, 14295,
  14189, 14081, 13970, 13856, 13741, 13623, 13502, 13380, 13255, 13128,
  12998, 12867, 12733, 12597, 12458, 12318, 12176, 12031, 11885, 11736,
  11585, 11433, 11278, 11121, 10963, 10803, 10641, 10477, 10311, 10143,
  9974,  9803,  9630,  9456,  9280,  9102,  8923,  8743,  8561,  8377,
  8192,  8006,  7818,  7629,  7438,  7246,  7053,  6859,  6664,  6467,
  6270,  6071,  5872,  5671,  5469,  5266,  5063,  4859,  4653,  4447,
  4240,  4033,  3825,  3616,  3406,  3196,  2986,  2775,  2563,  2351,
  2139,  1926,  1713,  1499,  1285,  1072,   857,   643,   429,   214
};


/* Sine table 2 in Q14 */
static const int16_t WebRtcIsacfix_kSinTab2[FRAMESAMPLES/4] = {
  16384, -16381, 16375, -16367, 16356, -16342, 16325, -16305, 16283, -16257,
  16229, -16199, 16165, -16129, 16090, -16048, 16003, -15956, 15906, -15853,
  15798, -15739, 15679, -15615, 15549, -15480, 15408, -15334, 15257, -15178,
  15095, -15011, 14924, -14834, 14741, -14647, 14549, -14449, 14347, -14242,
  14135, -14025, 13913, -13799, 13682, -13563, 13441, -13318, 13192, -13063,
  12933, -12800, 12665, -12528, 12389, -12247, 12104, -11958, 11810, -11661,
  11509, -11356, 11200, -11042, 10883, -10722, 10559, -10394, 10227, -10059,
  9889,  -9717,  9543,  -9368,  9191,  -9013,  8833,  -8652,  8469,  -8285,
  8099,  -7912,  7723,  -7534,  7342,  -7150,  6957,  -6762,  6566,  -6369,
  6171,  -5971,  5771,  -5570,  5368,  -5165,  4961,  -4756,  4550,  -4344,
  4137,  -3929,  3720,  -3511,  3301,  -3091,  2880,  -2669,  2457,  -2245,
  2032,  -1819,  1606,  -1392,  1179,   -965,   750,   -536,   322,   -107
};
#endif  // WEBRTC_DETECT_ARM_NEON || WEBRTC_ARCH_ARM_NEON

void WebRtcIsacfix_Time2SpecC(int16_t *inre1Q9,
                              int16_t *inre2Q9,
                              int16_t *outreQ7,
                              int16_t *outimQ7)
{

  int k;
  int32_t tmpreQ16[FRAMESAMPLES/2], tmpimQ16[FRAMESAMPLES/2];
  int16_t tmp1rQ14, tmp1iQ14;
  int32_t xrQ16, xiQ16, yrQ16, yiQ16;
  int32_t v1Q16, v2Q16;
  int16_t factQ19, sh;

  /* Multiply with complex exponentials and combine into one complex vector */
  factQ19 = 16921; // 0.5/sqrt(240) in Q19 is round(.5/sqrt(240)*(2^19)) = 16921
  for (k = 0; k < FRAMESAMPLES/2; k++) {
    tmp1rQ14 = WebRtcIsacfix_kCosTab1[k];
    tmp1iQ14 = WebRtcIsacfix_kSinTab1[k];
    xrQ16 = WEBRTC_SPL_RSHIFT_W32(WEBRTC_SPL_MUL_16_16(tmp1rQ14, inre1Q9[k]) + WEBRTC_SPL_MUL_16_16(tmp1iQ14, inre2Q9[k]), 7);
    xiQ16 = WEBRTC_SPL_RSHIFT_W32(WEBRTC_SPL_MUL_16_16(tmp1rQ14, inre2Q9[k]) - WEBRTC_SPL_MUL_16_16(tmp1iQ14, inre1Q9[k]), 7);
    tmpreQ16[k] = WEBRTC_SPL_RSHIFT_W32(WEBRTC_SPL_MUL_16_32_RSFT16(factQ19, xrQ16)+4, 3); // (Q16*Q19>>16)>>3 = Q16
    tmpimQ16[k] = WEBRTC_SPL_RSHIFT_W32(WEBRTC_SPL_MUL_16_32_RSFT16(factQ19, xiQ16)+4, 3); // (Q16*Q19>>16)>>3 = Q16
  }


  xrQ16  = WebRtcSpl_MaxAbsValueW32(tmpreQ16, FRAMESAMPLES/2);
  yrQ16 = WebRtcSpl_MaxAbsValueW32(tmpimQ16, FRAMESAMPLES/2);
  if (yrQ16>xrQ16) {
    xrQ16 = yrQ16;
  }

  sh = WebRtcSpl_NormW32(xrQ16);
  sh = sh-24; //if sh becomes >=0, then we should shift sh steps to the left, and the domain will become Q(16+sh)
  //if sh becomes <0, then we should shift -sh steps to the right, and the domain will become Q(16+sh)

  //"Fastest" vectors
  if (sh>=0) {
    for (k=0; k<FRAMESAMPLES/2; k++) {
      inre1Q9[k] = (int16_t) WEBRTC_SPL_LSHIFT_W32(tmpreQ16[k], sh); //Q(16+sh)
      inre2Q9[k] = (int16_t) WEBRTC_SPL_LSHIFT_W32(tmpimQ16[k], sh); //Q(16+sh)
    }
  } else {
    int32_t round = WEBRTC_SPL_LSHIFT_W32((int32_t)1, -sh-1);
    for (k=0; k<FRAMESAMPLES/2; k++) {
      inre1Q9[k] = (int16_t) WEBRTC_SPL_RSHIFT_W32(tmpreQ16[k]+round, -sh); //Q(16+sh)
      inre2Q9[k] = (int16_t) WEBRTC_SPL_RSHIFT_W32(tmpimQ16[k]+round, -sh); //Q(16+sh)
    }
  }

  /* Get DFT */
  WebRtcIsacfix_FftRadix16Fastest(inre1Q9, inre2Q9, -1); // real call

  //"Fastest" vectors
  if (sh>=0) {
    for (k=0; k<FRAMESAMPLES/2; k++) {
      tmpreQ16[k] = WEBRTC_SPL_RSHIFT_W32((int32_t)inre1Q9[k], sh); //Q(16+sh) -> Q16
      tmpimQ16[k] = WEBRTC_SPL_RSHIFT_W32((int32_t)inre2Q9[k], sh); //Q(16+sh) -> Q16
    }
  } else {
    for (k=0; k<FRAMESAMPLES/2; k++) {
      tmpreQ16[k] = WEBRTC_SPL_LSHIFT_W32((int32_t)inre1Q9[k], -sh); //Q(16+sh) -> Q16
      tmpimQ16[k] = WEBRTC_SPL_LSHIFT_W32((int32_t)inre2Q9[k], -sh); //Q(16+sh) -> Q16
    }
  }


  /* Use symmetry to separate into two complex vectors and center frames in time around zero */
  for (k = 0; k < FRAMESAMPLES/4; k++) {
    xrQ16 = tmpreQ16[k] + tmpreQ16[FRAMESAMPLES/2 - 1 - k];
    yiQ16 = -tmpreQ16[k] + tmpreQ16[FRAMESAMPLES/2 - 1 - k];
    xiQ16 = tmpimQ16[k] - tmpimQ16[FRAMESAMPLES/2 - 1 - k];
    yrQ16 = tmpimQ16[k] + tmpimQ16[FRAMESAMPLES/2 - 1 - k];
    tmp1rQ14 = -WebRtcIsacfix_kSinTab2[FRAMESAMPLES/4 - 1 - k];
    tmp1iQ14 = WebRtcIsacfix_kSinTab2[k];
    v1Q16 = WEBRTC_SPL_MUL_16_32_RSFT14(tmp1rQ14, xrQ16) - WEBRTC_SPL_MUL_16_32_RSFT14(tmp1iQ14, xiQ16);
    v2Q16 = WEBRTC_SPL_MUL_16_32_RSFT14(tmp1iQ14, xrQ16) + WEBRTC_SPL_MUL_16_32_RSFT14(tmp1rQ14, xiQ16);
    outreQ7[k] = (int16_t) WEBRTC_SPL_RSHIFT_W32(v1Q16, 9);
    outimQ7[k] = (int16_t) WEBRTC_SPL_RSHIFT_W32(v2Q16, 9);
    v1Q16 = -WEBRTC_SPL_MUL_16_32_RSFT14(tmp1iQ14, yrQ16) - WEBRTC_SPL_MUL_16_32_RSFT14(tmp1rQ14, yiQ16);
    v2Q16 = -WEBRTC_SPL_MUL_16_32_RSFT14(tmp1rQ14, yrQ16) + WEBRTC_SPL_MUL_16_32_RSFT14(tmp1iQ14, yiQ16);
    outreQ7[FRAMESAMPLES/2 - 1 - k] = (int16_t)WEBRTC_SPL_RSHIFT_W32(v1Q16, 9); //CalcLrIntQ(v1Q16, 9);
    outimQ7[FRAMESAMPLES/2 - 1 - k] = (int16_t)WEBRTC_SPL_RSHIFT_W32(v2Q16, 9); //CalcLrIntQ(v2Q16, 9);

  }
}


void WebRtcIsacfix_Spec2TimeC(int16_t *inreQ7, int16_t *inimQ7, int32_t *outre1Q16, int32_t *outre2Q16)
{

  int k;
  int16_t tmp1rQ14, tmp1iQ14;
  int32_t xrQ16, xiQ16, yrQ16, yiQ16;
  int32_t tmpInRe, tmpInIm, tmpInRe2, tmpInIm2;
  int16_t factQ11;
  int16_t sh;

  for (k = 0; k < FRAMESAMPLES/4; k++) {
    /* Move zero in time to beginning of frames */
    tmp1rQ14 = -WebRtcIsacfix_kSinTab2[FRAMESAMPLES/4 - 1 - k];
    tmp1iQ14 = WebRtcIsacfix_kSinTab2[k];

    tmpInRe = WEBRTC_SPL_LSHIFT_W32((int32_t) inreQ7[k], 9);  // Q7 -> Q16
    tmpInIm = WEBRTC_SPL_LSHIFT_W32((int32_t) inimQ7[k], 9);  // Q7 -> Q16
    tmpInRe2 = WEBRTC_SPL_LSHIFT_W32((int32_t) inreQ7[FRAMESAMPLES/2 - 1 - k], 9);  // Q7 -> Q16
    tmpInIm2 = WEBRTC_SPL_LSHIFT_W32((int32_t) inimQ7[FRAMESAMPLES/2 - 1 - k], 9);  // Q7 -> Q16

    xrQ16 = WEBRTC_SPL_MUL_16_32_RSFT14(tmp1rQ14, tmpInRe) + WEBRTC_SPL_MUL_16_32_RSFT14(tmp1iQ14, tmpInIm);
    xiQ16 = WEBRTC_SPL_MUL_16_32_RSFT14(tmp1rQ14, tmpInIm) - WEBRTC_SPL_MUL_16_32_RSFT14(tmp1iQ14, tmpInRe);
    yrQ16 = -WEBRTC_SPL_MUL_16_32_RSFT14(tmp1rQ14, tmpInIm2) - WEBRTC_SPL_MUL_16_32_RSFT14(tmp1iQ14, tmpInRe2);
    yiQ16 = -WEBRTC_SPL_MUL_16_32_RSFT14(tmp1rQ14, tmpInRe2) + WEBRTC_SPL_MUL_16_32_RSFT14(tmp1iQ14, tmpInIm2);

    /* Combine into one vector,  z = x + j * y */
    outre1Q16[k] = xrQ16 - yiQ16;
    outre1Q16[FRAMESAMPLES/2 - 1 - k] = xrQ16 + yiQ16;
    outre2Q16[k] = xiQ16 + yrQ16;
    outre2Q16[FRAMESAMPLES/2 - 1 - k] = -xiQ16 + yrQ16;
  }

  /* Get IDFT */
  tmpInRe  = WebRtcSpl_MaxAbsValueW32(outre1Q16, 240);
  tmpInIm = WebRtcSpl_MaxAbsValueW32(outre2Q16, 240);
  if (tmpInIm>tmpInRe) {
    tmpInRe = tmpInIm;
  }

  sh = WebRtcSpl_NormW32(tmpInRe);
  sh = sh-24; //if sh becomes >=0, then we should shift sh steps to the left, and the domain will become Q(16+sh)
  //if sh becomes <0, then we should shift -sh steps to the right, and the domain will become Q(16+sh)

  //"Fastest" vectors
  if (sh>=0) {
    for (k=0; k<240; k++) {
      inreQ7[k] = (int16_t) WEBRTC_SPL_LSHIFT_W32(outre1Q16[k], sh); //Q(16+sh)
      inimQ7[k] = (int16_t) WEBRTC_SPL_LSHIFT_W32(outre2Q16[k], sh); //Q(16+sh)
    }
  } else {
    int32_t round = WEBRTC_SPL_LSHIFT_W32((int32_t)1, -sh-1);
    for (k=0; k<240; k++) {
      inreQ7[k] = (int16_t) WEBRTC_SPL_RSHIFT_W32(outre1Q16[k]+round, -sh); //Q(16+sh)
      inimQ7[k] = (int16_t) WEBRTC_SPL_RSHIFT_W32(outre2Q16[k]+round, -sh); //Q(16+sh)
    }
  }

  WebRtcIsacfix_FftRadix16Fastest(inreQ7, inimQ7, 1); // real call

  //"Fastest" vectors
  if (sh>=0) {
    for (k=0; k<240; k++) {
      outre1Q16[k] = WEBRTC_SPL_RSHIFT_W32((int32_t)inreQ7[k], sh); //Q(16+sh) -> Q16
      outre2Q16[k] = WEBRTC_SPL_RSHIFT_W32((int32_t)inimQ7[k], sh); //Q(16+sh) -> Q16
    }
  } else {
    for (k=0; k<240; k++) {
      outre1Q16[k] = WEBRTC_SPL_LSHIFT_W32((int32_t)inreQ7[k], -sh); //Q(16+sh) -> Q16
      outre2Q16[k] = WEBRTC_SPL_LSHIFT_W32((int32_t)inimQ7[k], -sh); //Q(16+sh) -> Q16
    }
  }

  /* Divide through by the normalizing constant: */
  /* scale all values with 1/240, i.e. with 273 in Q16 */
  /* 273/65536 ~= 0.0041656                            */
  /*     1/240 ~= 0.0041666                            */
  for (k=0; k<240; k++) {
    outre1Q16[k] = WEBRTC_SPL_MUL_16_32_RSFT16(273, outre1Q16[k]);
    outre2Q16[k] = WEBRTC_SPL_MUL_16_32_RSFT16(273, outre2Q16[k]);
  }

  /* Demodulate and separate */
  factQ11 = 31727; // sqrt(240) in Q11 is round(15.49193338482967*2048) = 31727
  for (k = 0; k < FRAMESAMPLES/2; k++) {
    tmp1rQ14 = WebRtcIsacfix_kCosTab1[k];
    tmp1iQ14 = WebRtcIsacfix_kSinTab1[k];
    xrQ16 = WEBRTC_SPL_MUL_16_32_RSFT14(tmp1rQ14, outre1Q16[k]) - WEBRTC_SPL_MUL_16_32_RSFT14(tmp1iQ14, outre2Q16[k]);
    xiQ16 = WEBRTC_SPL_MUL_16_32_RSFT14(tmp1rQ14, outre2Q16[k]) + WEBRTC_SPL_MUL_16_32_RSFT14(tmp1iQ14, outre1Q16[k]);
    xrQ16 = WEBRTC_SPL_MUL_16_32_RSFT11(factQ11, xrQ16);
    xiQ16 = WEBRTC_SPL_MUL_16_32_RSFT11(factQ11, xiQ16);
    outre2Q16[k] = xiQ16;
    outre1Q16[k] = xrQ16;
  }
}
