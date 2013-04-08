/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/main/source/acm_amr.h"

#include "webrtc/modules/audio_coding/main/interface/audio_coding_module_typedefs.h"
#include "webrtc/modules/audio_coding/main/source/acm_common_defs.h"
#include "webrtc/modules/audio_coding/main/source/acm_neteq.h"
#include "webrtc/modules/audio_coding/neteq/interface/webrtc_neteq.h"
#include "webrtc/modules/audio_coding/neteq/interface/webrtc_neteq_help_macros.h"
#include "webrtc/system_wrappers/interface/rw_lock_wrapper.h"
#include "webrtc/system_wrappers/interface/trace.h"

#ifdef WEBRTC_CODEC_AMR
// NOTE! GSM AMR is not included in the open-source package. The following
// interface file is needed:
//
// /modules/audio_coding/codecs/amr/main/interface/amr_interface.h
//
// The API in the header file should match the one below.
//
// int16_t WebRtcAmr_CreateEnc(AMR_encinst_t_** enc_inst);
// int16_t WebRtcAmr_CreateDec(AMR_decinst_t_** dec_inst);
// int16_t WebRtcAmr_FreeEnc(AMR_encinst_t_* enc_inst);
// int16_t WebRtcAmr_FreeDec(AMR_decinst_t_* dec_inst);
// int16_t WebRtcAmr_Encode(AMR_encinst_t_* enc_inst,
//                          int16_t* input,
//                          int16_t len,
//                          int16_t*output,
//                          int16_t mode);
//  int16_t WebRtcAmr_EncoderInit(AMR_encinst_t_* enc_inst,
//                               int16_t dtx_mode);
// int16_t WebRtcAmr_EncodeBitmode(AMR_encinst_t_* enc_inst,
//                                 int format);
// int16_t WebRtcAmr_Decode(AMR_decinst_t_* dec_inst);
// int16_t WebRtcAmr_DecodePlc(AMR_decinst_t_* dec_inst);
// int16_t WebRtcAmr_DecoderInit(AMR_decinst_t_* dec_inst);
// int16_t WebRtcAmr_DecodeBitmode(AMR_decinst_t_* dec_inst,
//                                 int format);
#include "amr_interface.h"
#endif

namespace webrtc {

#ifndef WEBRTC_CODEC_AMR
ACMAMR::ACMAMR(WebRtc_Word16 /* codec_id */)
    : encoder_inst_ptr_(NULL),
      decoder_inst_ptr_(NULL),
      encoding_mode_(-1),  // Invalid value.
      encoding_rate_(0),  // Invalid value.
      encoder_packing_format_(AMRBandwidthEfficient),
      decoder_packing_format_(AMRBandwidthEfficient) {
  return;
}

ACMAMR::~ACMAMR() {
  return;
}

WebRtc_Word16 ACMAMR::InternalEncode(WebRtc_UWord8* /* bitstream */,
                                     WebRtc_Word16* /* bitstream_len_byte */) {
  return -1;
}

WebRtc_Word16 ACMAMR::DecodeSafe(WebRtc_UWord8* /* bitstream */,
                                 WebRtc_Word16 /* bitstream_len_byte */,
                                 WebRtc_Word16* /* audio */,
                                 WebRtc_Word16* /* audio_samples */,
                                 WebRtc_Word8* /* speech_type */) {
  return -1;
}

WebRtc_Word16 ACMAMR::EnableDTX() {
  return -1;
}

WebRtc_Word16 ACMAMR::DisableDTX() {
  return -1;
}

WebRtc_Word16 ACMAMR::InternalInitEncoder(
    WebRtcACMCodecParams* /* codec_params */) {
  return -1;
}

WebRtc_Word16 ACMAMR::InternalInitDecoder(
    WebRtcACMCodecParams* /* codec_params */) {
  return -1;
}

WebRtc_Word32 ACMAMR::CodecDef(WebRtcNetEQ_CodecDef& /* codec_def */,
                               const CodecInst& /* codec_inst */) {
  return -1;
}

ACMGenericCodec* ACMAMR::CreateInstance(void) {
  return NULL;
}

WebRtc_Word16 ACMAMR::InternalCreateEncoder() {
  return -1;
}

void ACMAMR::DestructEncoderSafe() {
  return;
}

WebRtc_Word16 ACMAMR::InternalCreateDecoder() {
  return -1;
}

void ACMAMR::DestructDecoderSafe() {
  return;
}

WebRtc_Word16 ACMAMR::SetBitRateSafe(const WebRtc_Word32 /* rate */) {
  return -1;
}

void ACMAMR::InternalDestructEncoderInst(void* /* ptr_inst */) {
  return;
}

WebRtc_Word16 ACMAMR::SetAMREncoderPackingFormat(
    ACMAMRPackingFormat /* packing_format */) {
  return -1;
}

ACMAMRPackingFormat ACMAMR::AMREncoderPackingFormat() const {
  return AMRUndefined;
}

WebRtc_Word16 ACMAMR::SetAMRDecoderPackingFormat(
    ACMAMRPackingFormat /* packing_format */) {
  return -1;
}

ACMAMRPackingFormat ACMAMR::AMRDecoderPackingFormat() const {
  return AMRUndefined;
}

#else     //===================== Actual Implementation =======================

#define WEBRTC_AMR_MR475  0
#define WEBRTC_AMR_MR515  1
#define WEBRTC_AMR_MR59   2
#define WEBRTC_AMR_MR67   3
#define WEBRTC_AMR_MR74   4
#define WEBRTC_AMR_MR795  5
#define WEBRTC_AMR_MR102  6
#define WEBRTC_AMR_MR122  7

ACMAMR::ACMAMR(WebRtc_Word16 codec_id)
    : encoder_inst_ptr_(NULL),
      decoder_inst_ptr_(NULL),
      encoding_mode_(-1),  // invalid value
      encoding_rate_(0) {  // invalid value
  codec_id_ = codec_id;
  has_internal_dtx_ = true;
  encoder_packing_format_ = AMRBandwidthEfficient;
  decoder_packing_format_ = AMRBandwidthEfficient;
  return;
}

ACMAMR::~ACMAMR() {
  if (encoder_inst_ptr_ != NULL) {
    WebRtcAmr_FreeEnc(encoder_inst_ptr_);
    encoder_inst_ptr_ = NULL;
  }
  if (decoder_inst_ptr_ != NULL) {
    WebRtcAmr_FreeDec(decoder_inst_ptr_);
    decoder_inst_ptr_ = NULL;
  }
  return;
}

WebRtc_Word16 ACMAMR::InternalEncode(WebRtc_UWord8* bitstream,
                                     WebRtc_Word16* bitstream_len_byte) {
  WebRtc_Word16 vad_decision = 1;
  // sanity check, if the rate is set correctly. we might skip this
  // sanity check. if rate is not set correctly, initialization flag
  // should be false and should not be here.
  if ((encoding_mode_ < WEBRTC_AMR_MR475) ||
      (encoding_mode_ > WEBRTC_AMR_MR122)) {
    *bitstream_len_byte = 0;
    return -1;
  }
  *bitstream_len_byte = WebRtcAmr_Encode(encoder_inst_ptr_,
                                         &in_audio_[in_audio_ix_read_],
                                         frame_len_smpl_,
                                         (WebRtc_Word16*)bitstream,
                                         encoding_mode_);

  // Update VAD, if internal DTX is used
  if (has_internal_dtx_ && dtx_enabled_) {
    if (*bitstream_len_byte <= (7 * frame_len_smpl_ / 160)) {
      vad_decision = 0;
    }
    for (WebRtc_Word16 n = 0; n < MAX_FRAME_SIZE_10MSEC; n++) {
      vad_label_[n] = vad_decision;
    }
  }
  // increment the read index
  in_audio_ix_read_ += frame_len_smpl_;
  return *bitstream_len_byte;
}

WebRtc_Word16 ACMAMR::DecodeSafe(WebRtc_UWord8* /* bitstream */,
                                 WebRtc_Word16 /* bitstream_len_byte */,
                                 WebRtc_Word16* /* audio */,
                                 WebRtc_Word16* /* audio_samples */,
                                 WebRtc_Word8* /* speech_type */) {
  return 0;
}

WebRtc_Word16 ACMAMR::EnableDTX() {
  if (dtx_enabled_) {
    return 0;
  } else if (encoder_exist_) {  // check if encoder exist
    // enable DTX
    if (WebRtcAmr_EncoderInit(encoder_inst_ptr_, 1) < 0) {
      return -1;
    }
    dtx_enabled_ = true;
    return 0;
  } else {
    return -1;
  }
}

WebRtc_Word16 ACMAMR::DisableDTX() {
  if (!dtx_enabled_) {
    return 0;
  } else if (encoder_exist_) {  // check if encoder exist
    // disable DTX
    if (WebRtcAmr_EncoderInit(encoder_inst_ptr_, 0) < 0) {
      return -1;
    }
    dtx_enabled_ = false;
    return 0;
  } else {
    // encoder doesn't exists, therefore disabling is harmless
    return 0;
  }
}

WebRtc_Word16 ACMAMR::InternalInitEncoder(WebRtcACMCodecParams* codec_params) {
  WebRtc_Word16 status = SetBitRateSafe((codec_params->codec_inst).rate);
  status += (WebRtcAmr_EncoderInit(
      encoder_inst_ptr_, ((codec_params->enable_dtx) ? 1 : 0)) < 0) ? -1 : 0;
  status += (WebRtcAmr_EncodeBitmode(
      encoder_inst_ptr_, encoder_packing_format_) < 0) ? -1 : 0;
  return (status < 0) ? -1 : 0;
}

WebRtc_Word16 ACMAMR::InternalInitDecoder(
    WebRtcACMCodecParams* /* codec_params */) {
  WebRtc_Word16 status =
      ((WebRtcAmr_DecoderInit(decoder_inst_ptr_) < 0) ? -1 : 0);
  status += WebRtcAmr_DecodeBitmode(decoder_inst_ptr_, decoder_packing_format_);
  return (status < 0) ? -1 : 0;
}

WebRtc_Word32 ACMAMR::CodecDef(WebRtcNetEQ_CodecDef& codec_def,
                               const CodecInst& codec_inst) {
  if (!decoder_initialized_) {
    // Todo:
    // log error
    return -1;
  }
  // Fill up the structure by calling
  // "SET_CODEC_PAR" & "SET_AMR_FUNCTION."
  // Then call NetEQ to add the codec to it's
  // database.
  SET_CODEC_PAR((codec_def), kDecoderAMR, codec_inst.pltype, decoder_inst_ptr_,
                8000);
  SET_AMR_FUNCTIONS((codec_def));
  return 0;
}

ACMGenericCodec* ACMAMR::CreateInstance(void) {
  return NULL;
}

WebRtc_Word16 ACMAMR::InternalCreateEncoder() {
  return WebRtcAmr_CreateEnc(&encoder_inst_ptr_);
}

void ACMAMR::DestructEncoderSafe() {
  if (encoder_inst_ptr_ != NULL) {
    WebRtcAmr_FreeEnc(encoder_inst_ptr_);
    encoder_inst_ptr_ = NULL;
  }
  // there is no encoder set the following
  encoder_exist_ = false;
  encoder_initialized_ = false;
  encoding_mode_ = -1;  // invalid value
  encoding_rate_ = 0;  // invalid value
}

WebRtc_Word16 ACMAMR::InternalCreateDecoder() {
  return WebRtcAmr_CreateDec(&decoder_inst_ptr_);
}

void ACMAMR::DestructDecoderSafe() {
  if (decoder_inst_ptr_ != NULL) {
    WebRtcAmr_FreeDec(decoder_inst_ptr_);
    decoder_inst_ptr_ = NULL;
  }
  // there is no encoder instance set the followings
  decoder_exist_ = false;
  decoder_initialized_ = false;
}

WebRtc_Word16 ACMAMR::SetBitRateSafe(const WebRtc_Word32 rate) {
  switch (rate) {
    case 4750: {
      encoding_mode_ = WEBRTC_AMR_MR475;
      encoding_rate_ = 4750;
      break;
    }
    case 5150: {
      encoding_mode_ = WEBRTC_AMR_MR515;
      encoding_rate_ = 5150;
      break;
    }
    case 5900: {
      encoding_mode_ = WEBRTC_AMR_MR59;
      encoding_rate_ = 5900;
      break;
    }
    case 6700: {
      encoding_mode_ = WEBRTC_AMR_MR67;
      encoding_rate_ = 6700;
      break;
    }
    case 7400: {
      encoding_mode_ = WEBRTC_AMR_MR74;
      encoding_rate_ = 7400;
      break;
    }
    case 7950: {
      encoding_mode_ = WEBRTC_AMR_MR795;
      encoding_rate_ = 7950;
      break;
    }
    case 10200: {
      encoding_mode_ = WEBRTC_AMR_MR102;
      encoding_rate_ = 10200;
      break;
    }
    case 12200: {
      encoding_mode_ = WEBRTC_AMR_MR122;
      encoding_rate_ = 12200;
      break;
    }
    default: {
      return -1;
    }
  }
  return 0;
}

void ACMAMR::InternalDestructEncoderInst(void* ptr_inst) {
  // Free the memory where ptr_inst is pointing to
  if (ptr_inst != NULL) {
    WebRtcAmr_FreeEnc(reinterpret_cast<AMR_encinst_t_*>(ptr_inst));
  }
  return;
}

WebRtc_Word16 ACMAMR::SetAMREncoderPackingFormat(
    ACMAMRPackingFormat packing_format) {
  if ((packing_format != AMRBandwidthEfficient) &&
      (packing_format != AMROctetAlligned) &&
      (packing_format != AMRFileStorage)) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, unique_id_,
                 "Invalid AMR Encoder packing-format.");
    return -1;
  } else {
    if (WebRtcAmr_EncodeBitmode(encoder_inst_ptr_, packing_format) < 0) {
      return -1;
    } else {
      encoder_packing_format_ = packing_format;
      return 0;
    }
  }
}

ACMAMRPackingFormat ACMAMR::AMREncoderPackingFormat() const {
  return encoder_packing_format_;
}

WebRtc_Word16 ACMAMR::SetAMRDecoderPackingFormat(
    ACMAMRPackingFormat packing_format) {
  if ((packing_format != AMRBandwidthEfficient) &&
      (packing_format != AMROctetAlligned) &&
      (packing_format != AMRFileStorage)) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, unique_id_,
                 "Invalid AMR decoder packing-format.");
    return -1;
  } else {
    if (WebRtcAmr_DecodeBitmode(decoder_inst_ptr_, packing_format) < 0) {
      return -1;
    } else {
      decoder_packing_format_ = packing_format;
      return 0;
    }
  }
}

ACMAMRPackingFormat ACMAMR::AMRDecoderPackingFormat() const {
  return decoder_packing_format_;
}

#endif
}
