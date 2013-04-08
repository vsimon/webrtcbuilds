/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "webrtc/modules/audio_coding/main/source/acm_isac.h"

#include "webrtc/modules/audio_coding/main/source/acm_codec_database.h"
#include "webrtc/modules/audio_coding/main/source/acm_common_defs.h"
#include "webrtc/modules/audio_coding/main/source/acm_neteq.h"
#include "webrtc/modules/audio_coding/neteq/interface/webrtc_neteq.h"
#include "webrtc/modules/audio_coding/neteq/interface/webrtc_neteq_help_macros.h"
#include "webrtc/system_wrappers/interface/trace.h"

#ifdef WEBRTC_CODEC_ISAC
#include "webrtc/modules/audio_coding/codecs/isac/main/interface/isac.h"
#include "webrtc/modules/audio_coding/main/source/acm_isac_macros.h"
#endif

#ifdef WEBRTC_CODEC_ISACFX
#include "webrtc/modules/audio_coding/codecs/isac/fix/interface/isacfix.h"
#include "webrtc/modules/audio_coding/main/source/acm_isac_macros.h"
#endif

namespace webrtc {

// we need this otherwise we cannot use forward declaration
// in the header file
#if (defined(WEBRTC_CODEC_ISAC) || defined(WEBRTC_CODEC_ISACFX))
struct ACMISACInst {
  ACM_ISAC_STRUCT *inst;
};
#endif

#define ISAC_MIN_RATE 10000
#define ISAC_MAX_RATE 56000

// Tables for bandwidth estimates
#define NR_ISAC_BANDWIDTHS 24
static const WebRtc_Word32 kIsacRatesWb[NR_ISAC_BANDWIDTHS] = {
    10000, 11100, 12300, 13700, 15200, 16900,
    18800, 20900, 23300, 25900, 28700, 31900,
    10100, 11200, 12400, 13800, 15300, 17000,
    18900, 21000, 23400, 26000, 28800, 32000
};

static const WebRtc_Word32 kIsacRatesSwb[NR_ISAC_BANDWIDTHS] = {
    10000, 11000, 12400, 13800, 15300, 17000,
    18900, 21000, 23200, 25400, 27600, 29800,
    32000, 34100, 36300, 38500, 40700, 42900,
    45100, 47300, 49500, 51700, 53900, 56000,
};

#if (!defined(WEBRTC_CODEC_ISAC) && !defined(WEBRTC_CODEC_ISACFX))

ACMISAC::ACMISAC(WebRtc_Word16 /* codec_id */)
    : codec_inst_ptr_(NULL),
      is_enc_initialized_(false),
      isac_coding_mode_(CHANNEL_INDEPENDENT),
      enforce_frame_size_(false),
      isac_currentBN_(32000),
      samples_in10MsAudio_(160) {  // Initiates to 16 kHz mode.
  // Initiate decoder parameters for the 32 kHz mode.
  memset(&decoder_params32kHz_, 0, sizeof(WebRtcACMCodecParams));
  decoder_params32kHz_.codec_inst.pltype = -1;

  return;
}

ACMISAC::~ACMISAC() {
  return;
}

ACMGenericCodec* ACMISAC::CreateInstance(void) {
  return NULL;
}

WebRtc_Word16 ACMISAC::InternalEncode(
    WebRtc_UWord8* /* bitstream */,
    WebRtc_Word16* /* bitstream_len_byte */) {
  return -1;
}

WebRtc_Word16 ACMISAC::DecodeSafe(WebRtc_UWord8* /* bitstream */,
                                  WebRtc_Word16 /* bitstream_len_byte */,
                                  WebRtc_Word16* /* audio */,
                                  WebRtc_Word16* /* audio_samples */,
                                  WebRtc_Word8* /* speech_type */) {
  return 0;
}

WebRtc_Word16 ACMISAC::InternalInitEncoder(
    WebRtcACMCodecParams* /* codec_params */) {
  return -1;
}

WebRtc_Word16 ACMISAC::InternalInitDecoder(
    WebRtcACMCodecParams* /* codec_params */) {
  return -1;
}

WebRtc_Word16 ACMISAC::InternalCreateDecoder() {
  return -1;
}

void ACMISAC::DestructDecoderSafe() {
  return;
}

WebRtc_Word16 ACMISAC::InternalCreateEncoder() {
  return -1;
}

void ACMISAC::DestructEncoderSafe() {
  return;
}

WebRtc_Word32 ACMISAC::CodecDef(WebRtcNetEQ_CodecDef& /* codec_def */,
                                const CodecInst& /* codec_inst */) {
  return -1;
}

void ACMISAC::InternalDestructEncoderInst(void* /* ptr_inst */) {
  return;
}

WebRtc_Word16 ACMISAC::DeliverCachedIsacData(
    WebRtc_UWord8* /* bitstream */,
    WebRtc_Word16* /* bitstream_len_byte */,
    WebRtc_UWord32* /* timestamp */,
    WebRtcACMEncodingType* /* encoding_type */,
    const WebRtc_UWord16 /* isac_rate */,
    const WebRtc_UWord8 /* isac_bw_estimate */) {
  return -1;
}

WebRtc_Word16 ACMISAC::Transcode(WebRtc_UWord8* /* bitstream */,
                                 WebRtc_Word16* /* bitstream_len_byte */,
                                 WebRtc_Word16 /* q_bwe */,
                                 WebRtc_Word32 /* scale */,
                                 bool /* is_red */) {
  return -1;
}

WebRtc_Word16 ACMISAC::SetBitRateSafe(WebRtc_Word32 /* bit_rate */) {
  return -1;
}

WebRtc_Word32 ACMISAC::GetEstimatedBandwidthSafe() {
  return -1;
}

WebRtc_Word32 ACMISAC::SetEstimatedBandwidthSafe(
    WebRtc_Word32 /* estimated_bandwidth */) {
  return -1;
}

WebRtc_Word32 ACMISAC::GetRedPayloadSafe(WebRtc_UWord8* /* red_payload */,
                                         WebRtc_Word16* /* payload_bytes */) {
  return -1;
}

WebRtc_Word16 ACMISAC::UpdateDecoderSampFreq(WebRtc_Word16 /* codec_id */) {
  return -1;
}

WebRtc_Word16 ACMISAC::UpdateEncoderSampFreq(
    WebRtc_UWord16 /* encoder_samp_freq_hz */) {
  return -1;
}

WebRtc_Word16 ACMISAC::EncoderSampFreq(WebRtc_UWord16& /* samp_freq_hz */) {
  return -1;
}

WebRtc_Word32 ACMISAC::ConfigISACBandwidthEstimator(
    const WebRtc_UWord8 /* init_frame_size_msec */,
    const WebRtc_UWord16 /* init_rate_bit_per_sec */,
    const bool /* enforce_frame_size  */) {
  return -1;
}

WebRtc_Word32 ACMISAC::SetISACMaxPayloadSize(
    const WebRtc_UWord16 /* max_payload_len_bytes */) {
  return -1;
}

WebRtc_Word32 ACMISAC::SetISACMaxRate(
    const WebRtc_UWord32 /* max_rate_bit_per_sec */) {
  return -1;
}

void ACMISAC::UpdateFrameLen() {
  return;
}

void ACMISAC::CurrentRate(WebRtc_Word32& /*rate_bit_per_sec */) {
  return;
}

bool
ACMISAC::DecoderParamsSafe(
    WebRtcACMCodecParams* /* dec_params */,
    const WebRtc_UWord8   /* payload_type */) {
  return false;
}

void
ACMISAC::SaveDecoderParamSafe(
    const WebRtcACMCodecParams* /* codec_params */) {
  return;
}

WebRtc_Word16 ACMISAC::REDPayloadISAC(
    const WebRtc_Word32 /* isac_rate */,
    const WebRtc_Word16 /* isac_bw_estimate */,
    WebRtc_UWord8* /* payload */,
    WebRtc_Word16* /* payload_len_bytes */) {
  return -1;
}

#else     //===================== Actual Implementation =======================

#ifdef WEBRTC_CODEC_ISACFX

// How the scaling is computed. iSAC computes a gain based on the
// bottleneck. It follows the following expression for that
//
// G(BN_kbps) = pow(10, (a + b * BN_kbps + c * BN_kbps * BN_kbps) / 20.0)
//              / 3.4641;
//
// Where for 30 ms framelength we have,
//
// a = -23; b = 0.48; c = 0;
//
// As the default encoder is operating at 32kbps we have the scale as
//
// S(BN_kbps) = G(BN_kbps) / G(32);

#define ISAC_NUM_SUPPORTED_RATES 9

static const WebRtc_UWord16 kIsacSuportedRates[ISAC_NUM_SUPPORTED_RATES] = {
    32000,    30000,    26000,   23000,   21000,
    19000,    17000,   15000,    12000
};

static const float kIsacScale[ISAC_NUM_SUPPORTED_RATES] = {
    1.0f,    0.8954f,  0.7178f, 0.6081f, 0.5445f,
    0.4875f, 0.4365f,  0.3908f, 0.3311f
};

enum IsacSamplingRate {
  kIsacWideband = 16,
  kIsacSuperWideband = 32
};

static float ACMISACFixTranscodingScale(WebRtc_UWord16 rate) {
  // find the scale for transcoding, the scale is rounded
  // downward
  float scale = -1;
  for (WebRtc_Word16 n = 0; n < ISAC_NUM_SUPPORTED_RATES; n++) {
    if (rate >= kIsacSuportedRates[n]) {
      scale = kIsacScale[n];
      break;
    }
  }
  return scale;
}

static void ACMISACFixGetSendBitrate(ACM_ISAC_STRUCT* inst,
                                     WebRtc_Word32* bottleneck) {
  *bottleneck = WebRtcIsacfix_GetUplinkBw(inst);
}

static WebRtc_Word16 ACMISACFixGetNewBitstream(ACM_ISAC_STRUCT* inst,
                                               WebRtc_Word16 bwe_index,
                                               WebRtc_Word16 /* jitter_index */,
                                               WebRtc_Word32 rate,
                                               WebRtc_Word16* bitstream,
                                               bool is_red) {
  if (is_red) {
    // RED not supported with iSACFIX
    return -1;
  }
  float scale = ACMISACFixTranscodingScale((WebRtc_UWord16) rate);
  return WebRtcIsacfix_GetNewBitStream(inst, bwe_index, scale, bitstream);
}

static WebRtc_Word16 ACMISACFixGetSendBWE(ACM_ISAC_STRUCT* inst,
                                          WebRtc_Word16* rate_index,
                                          WebRtc_Word16* /* dummy */) {
  WebRtc_Word16 local_rate_index;
  WebRtc_Word16 status = WebRtcIsacfix_GetDownLinkBwIndex(inst,
                                                          &local_rate_index);
  if (status < 0) {
    return -1;
  } else {
    *rate_index = local_rate_index;
    return 0;
  }
}

static WebRtc_Word16 ACMISACFixControlBWE(ACM_ISAC_STRUCT* inst,
                                          WebRtc_Word32 rate_bps,
                                          WebRtc_Word16 frame_size_ms,
                                          WebRtc_Word16 enforce_frame_size) {
  return WebRtcIsacfix_ControlBwe(inst, (WebRtc_Word16) rate_bps, frame_size_ms,
                                  enforce_frame_size);
}

static WebRtc_Word16 ACMISACFixControl(ACM_ISAC_STRUCT* inst,
                                       WebRtc_Word32 rate_bps,
                                       WebRtc_Word16 frame_size_ms) {
  return WebRtcIsacfix_Control(inst, (WebRtc_Word16) rate_bps, frame_size_ms);
}

// The following two function should have the same signature as their counter
// part in iSAC floating-point, i.e. WebRtcIsac_EncSampRate &
// WebRtcIsac_DecSampRate.
static WebRtc_UWord16 ACMISACFixGetEncSampRate(ACM_ISAC_STRUCT* /* inst */) {
  return 16000;
}

static WebRtc_UWord16 ACMISACFixGetDecSampRate(ACM_ISAC_STRUCT* /* inst */) {
  return 16000;
}

#endif

ACMISAC::ACMISAC(WebRtc_Word16 codec_id)
    : is_enc_initialized_(false),
      isac_coding_mode_(CHANNEL_INDEPENDENT),
      enforce_frame_size_(false),
      isac_current_bn_(32000),
      samples_in_10ms_audio_(160) {  // Initiates to 16 kHz mode.
  codec_id_ = codec_id;

  // Create codec instance.
  codec_inst_ptr_ = new ACMISACInst;
  if (codec_inst_ptr_ == NULL) {
    return;
  }
  codec_inst_ptr_->inst = NULL;

  // Initiate decoder parameters for the 32 kHz mode.
  memset(&decoder_params_32khz_, 0, sizeof(WebRtcACMCodecParams));
  decoder_params_32khz_.codec_inst.pltype = -1;

  // TODO(tlegrand): Check if the following is really needed, now that
  // ACMGenericCodec has been updated to initialize this value.
  // Initialize values that can be used uninitialized otherwise
  decoder_params_.codec_inst.pltype = -1;
}

ACMISAC::~ACMISAC() {
  if (codec_inst_ptr_ != NULL) {
    if (codec_inst_ptr_->inst != NULL) {
      ACM_ISAC_FREE(codec_inst_ptr_->inst);
      codec_inst_ptr_->inst = NULL;
    }
    delete codec_inst_ptr_;
    codec_inst_ptr_ = NULL;
  }
  return;
}

ACMGenericCodec* ACMISAC::CreateInstance(void) {
  return NULL;
}

WebRtc_Word16 ACMISAC::InternalEncode(WebRtc_UWord8* bitstream,
                                      WebRtc_Word16* bitstream_len_byte) {
  // ISAC takes 10ms audio everytime we call encoder, therefor,
  // it should be treated like codecs with 'basic coding block'
  // non-zero, and the following 'while-loop' should not be necessary.
  // However, due to a mistake in the codec the frame-size might change
  // at the first 10ms pushed in to iSAC if the bit-rate is low, this is
  // sort of a bug in iSAC. to address this we treat iSAC as the
  // following.
  if (codec_inst_ptr_ == NULL) {
    return -1;
  }
  *bitstream_len_byte = 0;
  while ((*bitstream_len_byte == 0) && (in_audio_ix_read_ < frame_len_smpl_)) {
    if (in_audio_ix_read_ > in_audio_ix_write_) {
      // something is wrong.
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, unique_id_,
                   "The actual fram-size of iSAC appears to be larger that "
                   "expected. All audio pushed in but no bit-stream is "
                   "generated.");
      return -1;
    }
    *bitstream_len_byte = ACM_ISAC_ENCODE(codec_inst_ptr_->inst,
                                           &in_audio_[in_audio_ix_read_],
                                           (WebRtc_Word16*)bitstream);
    // increment the read index this tell the caller that how far
    // we have gone forward in reading the audio buffer
    in_audio_ix_read_ += samples_in_10ms_audio_;
  }
  if (*bitstream_len_byte == 0) {
    WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceAudioCoding, unique_id_,
                 "ISAC Has encoded the whole frame but no bit-stream is "
                 "generated.");
  }

  // a packet is generated iSAC, is set in adaptive mode may change
  // the frame length and we like to update the bottleneck value as
  // well, although updating bottleneck is not crucial
  if ((*bitstream_len_byte > 0) && (isac_coding_mode_ == ADAPTIVE)) {
    ACM_ISAC_GETSENDBITRATE(codec_inst_ptr_->inst, &isac_current_bn_);
  }
  UpdateFrameLen();
  return *bitstream_len_byte;
}

WebRtc_Word16 ACMISAC::DecodeSafe(WebRtc_UWord8* /* bitstream */,
                                  WebRtc_Word16 /* bitstream_len_byte */,
                                  WebRtc_Word16* /* audio */,
                                  WebRtc_Word16* /* audio_sample */,
                                  WebRtc_Word8* /* speech_type */) {
  return 0;
}

WebRtc_Word16 ACMISAC::InternalInitEncoder(WebRtcACMCodecParams* codec_params) {
  // if rate is set to -1 then iSAC has to be in adaptive mode
  if (codec_params->codec_inst.rate == -1) {
    isac_coding_mode_ = ADAPTIVE;
  } else if ((codec_params->codec_inst.rate >= ISAC_MIN_RATE) &&
      (codec_params->codec_inst.rate <= ISAC_MAX_RATE)) {
    // sanity check that rate is in acceptable range
    isac_coding_mode_ = CHANNEL_INDEPENDENT;
    isac_current_bn_ = codec_params->codec_inst.rate;
  } else {
    return -1;
  }

  // we need to set the encoder sampling frequency.
  if (UpdateEncoderSampFreq((WebRtc_UWord16) codec_params->codec_inst.plfreq)
      < 0) {
    return -1;
  }
  if (ACM_ISAC_ENCODERINIT(codec_inst_ptr_->inst, isac_coding_mode_) < 0) {
    return -1;
  }

  // apply the frame-size and rate if operating in
  // channel-independent mode
  if (isac_coding_mode_ == CHANNEL_INDEPENDENT) {
    if (ACM_ISAC_CONTROL(codec_inst_ptr_->inst,
                         codec_params->codec_inst.rate,
                         codec_params->codec_inst.pacsize /
                         (codec_params->codec_inst.plfreq / 1000)) < 0) {
      return -1;
    }
  } else {
    // We need this for adaptive case and has to be called
    // after initialization
    ACM_ISAC_GETSENDBITRATE(codec_inst_ptr_->inst, &isac_current_bn_);
  }
  frame_len_smpl_ = ACM_ISAC_GETNEWFRAMELEN(codec_inst_ptr_->inst);
  return 0;
}

WebRtc_Word16 ACMISAC::InternalInitDecoder(WebRtcACMCodecParams* codec_params) {
  if (codec_inst_ptr_ == NULL) {
    return -1;
  }

  // set decoder sampling frequency.
  if (codec_params->codec_inst.plfreq == 32000 ||
      codec_params->codec_inst.plfreq == 48000) {
    UpdateDecoderSampFreq(ACMCodecDB::kISACSWB);
  } else {
    UpdateDecoderSampFreq(ACMCodecDB::kISAC);
  }

  // in a one-way communication we may never register send-codec.
  // However we like that the BWE to work properly so it has to
  // be initialized. The BWE is initialized when iSAC encoder is initialized.
  // Therefore, we need this.
  if (!encoder_initialized_) {
    // Since we don't require a valid rate or a valid packet size when
    // initializing the decoder, we set valid values before initializing encoder
    codec_params->codec_inst.rate = kIsacWbDefaultRate;
    codec_params->codec_inst.pacsize = kIsacPacSize960;
    if (InternalInitEncoder(codec_params) < 0) {
      return -1;
    }
    encoder_initialized_ = true;
  }

  return ACM_ISAC_DECODERINIT(codec_inst_ptr_->inst);
}

WebRtc_Word16 ACMISAC::InternalCreateDecoder() {
  if (codec_inst_ptr_ == NULL) {
    return -1;
  }
  WebRtc_Word16 status = ACM_ISAC_CREATE(&(codec_inst_ptr_->inst));

  // specific to codecs with one instance for encoding and decoding
  encoder_initialized_ = false;
  if (status < 0) {
    encoder_exist_ = false;
  } else {
    encoder_exist_ = true;
  }
  return status;
}

void ACMISAC::DestructDecoderSafe() {
  // codec with shared instance cannot delete.
  decoder_initialized_ = false;
  return;
}

WebRtc_Word16 ACMISAC::InternalCreateEncoder() {
  if (codec_inst_ptr_ == NULL) {
    return -1;
  }
  WebRtc_Word16 status = ACM_ISAC_CREATE(&(codec_inst_ptr_->inst));

  // specific to codecs with one instance for encoding and decoding
  decoder_initialized_ = false;
  if (status < 0) {
    decoder_exist_ = false;
  } else {
    decoder_exist_ = true;
  }
  return status;
}

void ACMISAC::DestructEncoderSafe() {
  // codec with shared instance cannot delete.
  encoder_initialized_ = false;
  return;
}

WebRtc_Word32 ACMISAC::CodecDef(WebRtcNetEQ_CodecDef& codec_def,
                                const CodecInst& codec_inst) {
  // Sanity checks
  if (codec_inst_ptr_ == NULL) {
    return -1;
  }
  if (!decoder_initialized_ || !decoder_exist_) {
    return -1;
  }
  // Fill up the structure by calling
  // "SET_CODEC_PAR" & "SET_ISAC_FUNCTION."
  // Then call NetEQ to add the codec to it's
  // database.
  if (codec_inst.plfreq == 16000) {
    SET_CODEC_PAR((codec_def), kDecoderISAC, codec_inst.pltype,
                  codec_inst_ptr_->inst, 16000);
#ifdef WEBRTC_CODEC_ISAC
    SET_ISAC_FUNCTIONS((codec_def));
#else
    SET_ISACfix_FUNCTIONS((codec_def));
#endif
  } else {
#ifdef WEBRTC_CODEC_ISAC
    // Decoder is either @ 16 kHz or 32 kHz. Even if encoder is set @ 48 kHz
    // decoding is @ 32 kHz.
    if (codec_inst.plfreq == 32000) {
      SET_CODEC_PAR((codec_def), kDecoderISACswb, codec_inst.pltype,
                    codec_inst_ptr_->inst, 32000);
      SET_ISACSWB_FUNCTIONS((codec_def));
    } else {
      SET_CODEC_PAR((codec_def), kDecoderISACfb, codec_inst.pltype,
                    codec_inst_ptr_->inst, 32000);
      SET_ISACFB_FUNCTIONS((codec_def));
    }
#else
  return -1;
#endif
  }
  return 0;
}

void ACMISAC::InternalDestructEncoderInst(void* ptr_inst) {
  if (ptr_inst != NULL) {
    ACM_ISAC_FREE((ACM_ISAC_STRUCT *) ptr_inst);
  }
  return;
}

WebRtc_Word16 ACMISAC::Transcode(WebRtc_UWord8* bitstream,
                                 WebRtc_Word16* bitstream_len_byte,
                                 WebRtc_Word16 q_bwe,
                                 WebRtc_Word32 rate,
                                 bool is_red) {
  WebRtc_Word16 jitter_info = 0;
  // transcode from a higher rate to lower rate sanity check
  if (codec_inst_ptr_ == NULL) {
    return -1;
  }

  *bitstream_len_byte = ACM_ISAC_GETNEWBITSTREAM(codec_inst_ptr_->inst, q_bwe,
                                                 jitter_info, rate,
                                                 (WebRtc_Word16*)bitstream,
                                                 (is_red) ? 1 : 0);

  if (*bitstream_len_byte < 0) {
    // error happened
    *bitstream_len_byte = 0;
    return -1;
  } else {
    return *bitstream_len_byte;
  }
}

WebRtc_Word16 ACMISAC::SetBitRateSafe(WebRtc_Word32 bit_rate) {
  if (codec_inst_ptr_ == NULL) {
    return -1;
  }
  WebRtc_UWord16 encoder_samp_freq;
  EncoderSampFreq(encoder_samp_freq);
  bool reinit = false;
  // change the BN of iSAC
  if (bit_rate == -1) {
    // ADAPTIVE MODE
    // Check if it was already in adaptive mode
    if (isac_coding_mode_ != ADAPTIVE) {
      // was not in adaptive, then set the mode to adaptive
      // and flag for re-initialization
      isac_coding_mode_ = ADAPTIVE;
      reinit = true;
    }
  } else if ((bit_rate >= ISAC_MIN_RATE) && (bit_rate <= ISAC_MAX_RATE)) {
    // Sanity check if the rate valid
    // check if it was in channel-independent mode before
    if (isac_coding_mode_ != CHANNEL_INDEPENDENT) {
      // was not in channel independent, set the mode to
      // channel-independent and flag for re-initialization
      isac_coding_mode_ = CHANNEL_INDEPENDENT;
      reinit = true;
    }
    // store the bottleneck
    isac_current_bn_ = (WebRtc_UWord16) bit_rate;
  } else {
    // invlaid rate
    return -1;
  }

  WebRtc_Word16 status = 0;
  if (reinit) {
    // initialize and check if it is successful
    if (ACM_ISAC_ENCODERINIT(codec_inst_ptr_->inst, isac_coding_mode_) < 0) {
      // failed initialization
      return -1;
    }
  }
  if (isac_coding_mode_ == CHANNEL_INDEPENDENT) {
    status = ACM_ISAC_CONTROL(
        codec_inst_ptr_->inst, isac_current_bn_,
        (encoder_samp_freq == 32000 || encoder_samp_freq == 48000) ? 30 :
            (frame_len_smpl_ / 16));
    if (status < 0) {
      status = -1;
    }
  }

  // Update encoder parameters
  encoder_params_.codec_inst.rate = bit_rate;

  UpdateFrameLen();
  return status;
}

WebRtc_Word32 ACMISAC::GetEstimatedBandwidthSafe() {
  WebRtc_Word16 bandwidth_index = 0;
  WebRtc_Word16 delay_index = 0;
  int samp_rate;

  // Get bandwidth information
  ACM_ISAC_GETSENDBWE(codec_inst_ptr_->inst, &bandwidth_index, &delay_index);

  // Validy check of index
  if ((bandwidth_index < 0) || (bandwidth_index >= NR_ISAC_BANDWIDTHS)) {
    return -1;
  }

  // Check sample frequency
  samp_rate = ACM_ISAC_GETDECSAMPRATE(codec_inst_ptr_->inst);
  if (samp_rate == 16000) {
    return kIsacRatesWb[bandwidth_index];
  } else {
    return kIsacRatesSwb[bandwidth_index];
  }
}

WebRtc_Word32 ACMISAC::SetEstimatedBandwidthSafe(
    WebRtc_Word32 estimated_bandwidth) {
  int samp_rate;
  WebRtc_Word16 bandwidth_index;

  // Check sample frequency and choose appropriate table
  samp_rate = ACM_ISAC_GETENCSAMPRATE(codec_inst_ptr_->inst);

  if (samp_rate == 16000) {
    // Search through the WB rate table to find the index
    bandwidth_index = NR_ISAC_BANDWIDTHS / 2 - 1;
    for (int i = 0; i < (NR_ISAC_BANDWIDTHS / 2); i++) {
      if (estimated_bandwidth == kIsacRatesWb[i]) {
        bandwidth_index = i;
        break;
      } else if (estimated_bandwidth
          == kIsacRatesWb[i + NR_ISAC_BANDWIDTHS / 2]) {
        bandwidth_index = i + NR_ISAC_BANDWIDTHS / 2;
        break;
      } else if (estimated_bandwidth < kIsacRatesWb[i]) {
        bandwidth_index = i;
        break;
      }
    }
  } else {
    // Search through the SWB rate table to find the index
    bandwidth_index = NR_ISAC_BANDWIDTHS - 1;
    for (int i = 0; i < NR_ISAC_BANDWIDTHS; i++) {
      if (estimated_bandwidth <= kIsacRatesSwb[i]) {
        bandwidth_index = i;
        break;
      }
    }
  }

  // Set iSAC Bandwidth Estimate
  ACM_ISAC_SETBWE(codec_inst_ptr_->inst, bandwidth_index);

  return 0;
}

WebRtc_Word32 ACMISAC::GetRedPayloadSafe(
#if (!defined(WEBRTC_CODEC_ISAC))
    WebRtc_UWord8* /* red_payload */, WebRtc_Word16* /* payload_bytes */) {
  return -1;
#else
    WebRtc_UWord8* red_payload, WebRtc_Word16* payload_bytes) {
  WebRtc_Word16 bytes = WebRtcIsac_GetRedPayload(codec_inst_ptr_->inst,
                                                 (WebRtc_Word16*)red_payload);
  if (bytes < 0) {
    return -1;
  }
  *payload_bytes = bytes;
  return 0;
#endif
}

WebRtc_Word16 ACMISAC::UpdateDecoderSampFreq(
#ifdef WEBRTC_CODEC_ISAC
    WebRtc_Word16 codec_id) {
    // The decoder supports only wideband and super-wideband.
  if (ACMCodecDB::kISAC == codec_id) {
    return WebRtcIsac_SetDecSampRate(codec_inst_ptr_->inst, 16000);
  } else if (ACMCodecDB::kISACSWB == codec_id ||
      ACMCodecDB::kISACFB == codec_id) {
    return WebRtcIsac_SetDecSampRate(codec_inst_ptr_->inst, 32000);
  } else {
    return -1;
  }
#else
    WebRtc_Word16 /* codec_id */) {
  return 0;
#endif
}

WebRtc_Word16 ACMISAC::UpdateEncoderSampFreq(
#ifdef WEBRTC_CODEC_ISAC
    WebRtc_UWord16 encoder_samp_freq_hz) {
  WebRtc_UWord16 current_samp_rate_hz;
  EncoderSampFreq(current_samp_rate_hz);

  if (current_samp_rate_hz != encoder_samp_freq_hz) {
    if ((encoder_samp_freq_hz != 16000) &&
        (encoder_samp_freq_hz != 32000) &&
        (encoder_samp_freq_hz != 48000)) {
      return -1;
    } else {
      in_audio_ix_read_ = 0;
      in_audio_ix_write_ = 0;
      in_timestamp_ix_write_ = 0;
      if (WebRtcIsac_SetEncSampRate(codec_inst_ptr_->inst,
                                    encoder_samp_freq_hz) < 0) {
        return -1;
      }
      samples_in_10ms_audio_ = encoder_samp_freq_hz / 100;
      frame_len_smpl_ = ACM_ISAC_GETNEWFRAMELEN(codec_inst_ptr_->inst);
      encoder_params_.codec_inst.pacsize = frame_len_smpl_;
      encoder_params_.codec_inst.plfreq = encoder_samp_freq_hz;
      return 0;
    }
  }
#else
    WebRtc_UWord16 /* codec_id */) {
#endif
  return 0;
}

WebRtc_Word16 ACMISAC::EncoderSampFreq(WebRtc_UWord16& samp_freq_hz) {
  samp_freq_hz = ACM_ISAC_GETENCSAMPRATE(codec_inst_ptr_->inst);
  return 0;
}

WebRtc_Word32 ACMISAC::ConfigISACBandwidthEstimator(
    const WebRtc_UWord8 init_frame_size_msec,
    const WebRtc_UWord16 init_rate_bit_per_sec,
    const bool enforce_frame_size) {
  WebRtc_Word16 status;
  {
    WebRtc_UWord16 samp_freq_hz;
    EncoderSampFreq(samp_freq_hz);
    // TODO(turajs): at 32kHz we hardcode calling with 30ms and enforce
    // the frame-size otherwise we might get error. Revise if
    // control-bwe is changed.
    if (samp_freq_hz == 32000 || samp_freq_hz == 48000) {
      status = ACM_ISAC_CONTROL_BWE(codec_inst_ptr_->inst,
                                    init_rate_bit_per_sec, 30, 1);
    } else {
      status = ACM_ISAC_CONTROL_BWE(codec_inst_ptr_->inst,
                                    init_rate_bit_per_sec,
                                    init_frame_size_msec,
                                    enforce_frame_size ? 1 : 0);
    }
  }
  if (status < 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, unique_id_,
                 "Couldn't config iSAC BWE.");
    return -1;
  }
  UpdateFrameLen();
  ACM_ISAC_GETSENDBITRATE(codec_inst_ptr_->inst, &isac_current_bn_);
  return 0;
}

WebRtc_Word32 ACMISAC::SetISACMaxPayloadSize(
    const WebRtc_UWord16 max_payload_len_bytes) {
  return ACM_ISAC_SETMAXPAYLOADSIZE(codec_inst_ptr_->inst,
                                    max_payload_len_bytes);
}

WebRtc_Word32 ACMISAC::SetISACMaxRate(
    const WebRtc_UWord32 max_rate_bit_per_sec) {
  return ACM_ISAC_SETMAXRATE(codec_inst_ptr_->inst, max_rate_bit_per_sec);
}

void ACMISAC::UpdateFrameLen() {
  frame_len_smpl_ = ACM_ISAC_GETNEWFRAMELEN(codec_inst_ptr_->inst);
  encoder_params_.codec_inst.pacsize = frame_len_smpl_;
}

void ACMISAC::CurrentRate(WebRtc_Word32& rate_bit_per_sec) {
  if (isac_coding_mode_ == ADAPTIVE) {
    ACM_ISAC_GETSENDBITRATE(codec_inst_ptr_->inst, &rate_bit_per_sec);
  }
}

bool ACMISAC::DecoderParamsSafe(WebRtcACMCodecParams* dec_params,
                                const WebRtc_UWord8 payload_type) {
  if (decoder_initialized_) {
    if (payload_type == decoder_params_.codec_inst.pltype) {
      memcpy(dec_params, &decoder_params_, sizeof(WebRtcACMCodecParams));
      return true;
    }
    if (payload_type == decoder_params_32khz_.codec_inst.pltype) {
      memcpy(dec_params, &decoder_params_32khz_, sizeof(WebRtcACMCodecParams));
      return true;
    }
  }
  return false;
}

void ACMISAC::SaveDecoderParamSafe(const WebRtcACMCodecParams* codec_params) {
  // set decoder sampling frequency.
  if (codec_params->codec_inst.plfreq == 32000 ||
      codec_params->codec_inst.plfreq == 48000) {
    memcpy(&decoder_params_32khz_, codec_params, sizeof(WebRtcACMCodecParams));
  } else {
    memcpy(&decoder_params_, codec_params, sizeof(WebRtcACMCodecParams));
  }
}

WebRtc_Word16 ACMISAC::REDPayloadISAC(const WebRtc_Word32 isac_rate,
                                      const WebRtc_Word16 isac_bw_estimate,
                                      WebRtc_UWord8* payload,
                                      WebRtc_Word16* payload_len_bytes) {
  WebRtc_Word16 status;
  ReadLockScoped rl(codec_wrapper_lock_);
  status = Transcode(payload, payload_len_bytes, isac_bw_estimate, isac_rate,
                     true);
  return status;
}

#endif

}  // namespace webrtc
