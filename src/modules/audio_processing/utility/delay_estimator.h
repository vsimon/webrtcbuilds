/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Performs delay estimation on binary converted spectra.
// The return value is  0 - OK and -1 - Error, unless otherwise stated.

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_UTILITY_DELAY_ESTIMATOR_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_UTILITY_DELAY_ESTIMATOR_H_

#include "signal_processing_library.h"
#include "typedefs.h"

typedef struct {
  // Pointer to bit counts
  int32_t* mean_bit_counts;

  // Array only used locally in ProcessBinarySpectrum() but whose size is
  // determined at run-time.
  int32_t* bit_counts;

  // Binary history variables.
  uint32_t* binary_far_history;
  uint32_t* binary_near_history;

  // Delay histogram variables.
  int* delay_histogram;
  int vad_counter;

  // Delay memory.
  int last_delay;

  // Buffer size.
  int history_size;

  // Near-end buffer size.
  int near_history_size;
} BinaryDelayEstimator_t;

// Releases the memory allocated by WebRtc_CreateBinaryDelayEstimator(...)
// Input:
//    - handle            : Pointer to the delay estimation instance
//
int WebRtc_FreeBinaryDelayEstimator(BinaryDelayEstimator_t* handle);

// Refer to WebRtc_CreateDelayEstimator() in delay_estimator_wrapper.h
int WebRtc_CreateBinaryDelayEstimator(BinaryDelayEstimator_t** handle,
                                      int max_delay,
                                      int lookahead);

// Initializes the delay estimation instance created with
// WebRtc_CreateBinaryDelayEstimator(...)
// Input:
//    - handle            : Pointer to the delay estimation instance
//
// Output:
//    - handle            : Initialized instance
//
int WebRtc_InitBinaryDelayEstimator(BinaryDelayEstimator_t* handle);

// Estimates and returns the delay between the binary far-end and binary near-
// end spectra. The value will be offset by the lookahead (i.e. the lookahead
// should be subtracted from the returned value).
// Inputs:
//    - handle                : Pointer to the delay estimation instance
//    - binary_far_spectrum   : Far-end binary spectrum
//    - binary_near_spectrum  : Near-end binary spectrum of the current block
//    - vad_value             : The VAD decision of the current block
//
// Output:
//    - handle                : Updated instance
//
// Return value:
//    - delay                 :  >= 0 - Calculated delay value
//                              -1    - Error
//                              -2    - Insufficient data for estimation.
//
int WebRtc_ProcessBinarySpectrum(BinaryDelayEstimator_t* handle,
                                 uint32_t binary_far_spectrum,
                                 uint32_t binary_near_spectrum,
                                 int vad_value);

// Returns the last calculated delay updated by the function
// WebRtc_ProcessBinarySpectrum(...)
//
// Input:
//    - handle                : Pointer to the delay estimation instance
//
// Return value:
//    - delay                 :  >= 0 - Last calculated delay value
//                              -1    - Error
//                              -2    - Insufficient data for estimation.
//
int WebRtc_binary_last_delay(BinaryDelayEstimator_t* handle);

// Returns the history size used in the far-end buffers to calculate the delay
// over.
//
// Input:
//    - handle                : Pointer to the delay estimation instance
//
// Return value:
//    - history_size          :  > 0  - Far-end history size
//                              -1    - Error
//
int WebRtc_history_size(BinaryDelayEstimator_t* handle);

// Updates the |mean_value| recursively with a step size of 2^-|factor|. This
// function is used internally in the Binary Delay Estimator as well as the
// Fixed point wrapper.
//
// Inputs:
//    - new_value             : The new value the mean should be updated with.
//    - factor                : The step size, in number of right shifts.
//
// Input/Output:
//    - mean_value            : Pointer to the mean value.
//
void WebRtc_MeanEstimatorFix(int32_t new_value,
                             int factor,
                             int32_t* mean_value);


#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_UTILITY_DELAY_ESTIMATOR_H_
