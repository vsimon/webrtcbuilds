/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_engine/vie_renderer.h"

#include "common_video/libyuv/include/libyuv.h"
#include "modules/video_render/main/interface/video_render.h"
#include "modules/video_render/main/interface/video_render_defines.h"
#include "video_engine/vie_render_manager.h"

namespace webrtc {

ViERenderer* ViERenderer::CreateViERenderer(const WebRtc_Word32 render_id,
                                            const WebRtc_Word32 engine_id,
                                            VideoRender& render_module,
                                            ViERenderManager& render_manager,
                                            const WebRtc_UWord32 z_order,
                                            const float left,
                                            const float top,
                                            const float right,
                                            const float bottom) {
  ViERenderer* self = new ViERenderer(render_id, engine_id, render_module,
                                      render_manager);
  if (!self || self->Init(z_order, left, top, right, bottom) != 0) {
    delete self;
    self = NULL;
  }
  return self;
}

ViERenderer::~ViERenderer(void) {
  if (render_callback_)
    render_module_.DeleteIncomingRenderStream(render_id_);

  if (incoming_external_callback_)
    delete incoming_external_callback_;
}

WebRtc_Word32 ViERenderer::StartRender() {
  return render_module_.StartRender(render_id_);
}
WebRtc_Word32 ViERenderer::StopRender() {
  return render_module_.StopRender(render_id_);
}

WebRtc_Word32 ViERenderer::GetLastRenderedFrame(const WebRtc_Word32 renderID,
                                                VideoFrame& video_frame) {
  return render_module_.GetLastRenderedFrame(renderID, video_frame);
}

WebRtc_Word32 ViERenderer::ConfigureRenderer(const unsigned int z_order,
                                             const float left,
                                             const float top,
                                             const float right,
                                             const float bottom) {
  return render_module_.ConfigureRenderer(render_id_, z_order, left, top, right,
                                          bottom);
}

VideoRender& ViERenderer::RenderModule() {
  return render_module_;
}

WebRtc_Word32 ViERenderer::EnableMirroring(const WebRtc_Word32 render_id,
                                           const bool enable,
                                           const bool mirror_xaxis,
                                           const bool mirror_yaxis) {
  return render_module_.MirrorRenderStream(render_id, enable, mirror_xaxis,
                                           mirror_yaxis);
}

WebRtc_Word32 ViERenderer::SetTimeoutImage(const VideoFrame& timeout_image,
                                           const WebRtc_Word32 timeout_value) {
  return render_module_.SetTimeoutImage(render_id_, timeout_image,
                                        timeout_value);
}

WebRtc_Word32  ViERenderer::SetRenderStartImage(const VideoFrame& start_image) {
  return render_module_.SetStartImage(render_id_, start_image);
}

WebRtc_Word32 ViERenderer::SetExternalRenderer(
    const WebRtc_Word32 render_id,
    RawVideoType video_input_format,
    ExternalRenderer* external_renderer) {
  if (!incoming_external_callback_)
    return -1;

  incoming_external_callback_->SetViEExternalRenderer(external_renderer,
                                                      video_input_format);
  return render_module_.AddExternalRenderCallback(render_id,
                                                  incoming_external_callback_);
}

ViERenderer::ViERenderer(const WebRtc_Word32 render_id,
                         const WebRtc_Word32 engine_id,
                         VideoRender& render_module,
                         ViERenderManager& render_manager)
    : render_id_(render_id),
      engine_id_(engine_id),
      render_module_(render_module),
      render_manager_(render_manager),
      render_callback_(NULL),
      incoming_external_callback_(new ViEExternalRendererImpl()) {
}

WebRtc_Word32 ViERenderer::Init(const WebRtc_UWord32 z_order,
                                const float left,
                                const float top,
                                const float right,
                                const float bottom) {
  render_callback_ =
      static_cast<VideoRenderCallback*>(render_module_.AddIncomingRenderStream(
          render_id_, z_order, left, top, right, bottom));
  if (!render_callback_) {
    // Logging done.
    return -1;
  }
  return 0;
}

void ViERenderer::DeliverFrame(int id,
                               VideoFrame& video_frame,
                               int num_csrcs,
                               const WebRtc_UWord32 CSRC[kRtpCsrcSize]) {
  render_callback_->RenderFrame(render_id_, video_frame);
}

void ViERenderer::DelayChanged(int id, int frame_delay) {}

int ViERenderer::GetPreferedFrameSettings(int& width,
                                          int& height,
                                          int& frame_rate) {
    return -1;
}

void ViERenderer::ProviderDestroyed(int id) {
  // Remove the render stream since the provider is destroyed.
  render_manager_.RemoveRenderStream(render_id_);
}

ViEExternalRendererImpl::ViEExternalRendererImpl()
    : external_renderer_(NULL),
      external_renderer_format_(kVideoUnknown),
      external_renderer_width_(0),
      external_renderer_height_(0) {
}

int ViEExternalRendererImpl::SetViEExternalRenderer(
    ExternalRenderer* external_renderer,
    RawVideoType video_input_format) {
  external_renderer_ = external_renderer;
  external_renderer_format_ = video_input_format;
  return 0;
}

WebRtc_Word32 ViEExternalRendererImpl::RenderFrame(
    const WebRtc_UWord32 stream_id,
    VideoFrame&   video_frame) {
  VideoFrame converted_frame;
  VideoFrame* p_converted_frame = &converted_frame;

  // Convert to requested format.
  switch (external_renderer_format_) {
    case kVideoI420:
      p_converted_frame = &video_frame;
      break;
    case kVideoYV12:
      converted_frame.VerifyAndAllocate(CalcBufferSize(kYV12,
                                                       video_frame.Width(),
                                                       video_frame.Height()));
      ConvertI420ToYV12(video_frame.Buffer(), converted_frame.Buffer(),
                        video_frame.Width(), video_frame.Height(), 0);
      break;
    case kVideoYUY2:
      converted_frame.VerifyAndAllocate(CalcBufferSize(kYUY2,
                                                       video_frame.Width(),
                                                       video_frame.Height()));
      ConvertI420ToYUY2(video_frame.Buffer(), converted_frame.Buffer(),
                        video_frame.Width(), video_frame.Height(), 0);
      break;
    case kVideoUYVY:
      converted_frame.VerifyAndAllocate(CalcBufferSize(kUYVY,
                                                       video_frame.Width(),
                                                       video_frame.Height()));
      ConvertI420ToUYVY(video_frame.Buffer(), converted_frame.Buffer(),
                        video_frame.Width(), video_frame.Height(), 0);
      break;
    case kVideoIYUV:
      // no conversion available
      break;
    case kVideoARGB:
      converted_frame.VerifyAndAllocate(CalcBufferSize(kARGB,
                                                       video_frame.Width(),
                                                       video_frame.Height()));
      ConvertI420ToARGB(video_frame.Buffer(), converted_frame.Buffer(),
                        video_frame.Width(), video_frame.Height(), 0);
      break;
    case kVideoRGB24:
      converted_frame.VerifyAndAllocate(CalcBufferSize(kRGB24,
                                                       video_frame.Width(),
                                                       video_frame.Height()));
      ConvertI420ToRGB24(video_frame.Buffer(), converted_frame.Buffer(),
                         video_frame.Width(), video_frame.Height());
      break;
    case kVideoRGB565:
      converted_frame.VerifyAndAllocate(CalcBufferSize(kRGB565,
                                                       video_frame.Width(),
                                                       video_frame.Height()));
      ConvertI420ToRGB565(video_frame.Buffer(), converted_frame.Buffer(),
                          video_frame.Width(), video_frame.Height());
      break;
    case kVideoARGB4444:
      converted_frame.VerifyAndAllocate(CalcBufferSize(kARGB4444,
                                                       video_frame.Width(),
                                                       video_frame.Height()));
      ConvertI420ToARGB4444(video_frame.Buffer(), converted_frame.Buffer(),
                            video_frame.Width(), video_frame.Height(), 0);
      break;
    case kVideoARGB1555 :
      converted_frame.VerifyAndAllocate(CalcBufferSize(kARGB1555,
                                                       video_frame.Width(),
                                                       video_frame.Height()));
      ConvertI420ToARGB1555(video_frame.Buffer(), converted_frame.Buffer(),
                            video_frame.Width(), video_frame.Height(), 0);
      break;
    default:
      assert(false);
      p_converted_frame = NULL;
      break;
  }

  if (external_renderer_width_ != video_frame.Width() ||
      external_renderer_height_ != video_frame.Height()) {
    external_renderer_width_ = video_frame.Width();
    external_renderer_height_ = video_frame.Height();
    external_renderer_->FrameSizeChange(external_renderer_width_,
                                        external_renderer_height_, stream_id);
  }

  if (p_converted_frame) {
    external_renderer_->DeliverFrame(p_converted_frame->Buffer(),
                                     p_converted_frame->Length(),
                                     video_frame.TimeStamp());
  }
  return 0;
}

}  // namespace webrtc
