// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_video_decoder_impl.h"

#include <string>

#include "base/logging.h"
#include "base/message_loop.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "media/video/picture.h"
#include "ppapi/c/dev/pp_video_dev.h"
#include "ppapi/c/dev/ppb_video_decoder_dev.h"
#include "ppapi/c/dev/ppp_video_decoder_dev.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/shared_impl/resource_tracker.h"
#include "ppapi/thunk/enter.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/ppb_buffer_impl.h"
#include "webkit/plugins/ppapi/ppb_graphics_3d_impl.h"
#include "webkit/plugins/ppapi/resource_helper.h"

using ppapi::TrackedCallback;
using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_Buffer_API;
using ppapi::thunk::PPB_Graphics3D_API;
using ppapi::thunk::PPB_VideoDecoder_API;

namespace webkit {
namespace ppapi {

PPB_VideoDecoder_Impl::PPB_VideoDecoder_Impl(PP_Instance instance)
    : PPB_VideoDecoder_Shared(instance),
      ppp_videodecoder_(NULL) {
  PluginModule* plugin_module = ResourceHelper::GetPluginModule(this);
  if (plugin_module) {
    ppp_videodecoder_ = static_cast<const PPP_VideoDecoder_Dev*>(
        plugin_module->GetPluginInterface(PPP_VIDEODECODER_DEV_INTERFACE));
  }
}

PPB_VideoDecoder_Impl::~PPB_VideoDecoder_Impl() {
  Destroy();
}

// Convert PP_VideoDecoder_Profile to media::VideoCodecProfile.
static media::VideoCodecProfile PPToMediaProfile(
    const PP_VideoDecoder_Profile pp_profile) {
  switch (pp_profile) {
    case PP_VIDEODECODER_H264PROFILE_NONE:
      // HACK: PPAPI contains a bogus "none" h264 profile that doesn't
      // correspond to anything in h.264; but a number of released chromium
      // versions silently promoted this to Baseline profile, so we retain that
      // behavior here.  Fall through.
    case PP_VIDEODECODER_H264PROFILE_BASELINE:
      return media::H264PROFILE_BASELINE;
    case PP_VIDEODECODER_H264PROFILE_MAIN:
      return media::H264PROFILE_MAIN;
    case PP_VIDEODECODER_H264PROFILE_EXTENDED:
      return media::H264PROFILE_EXTENDED;
    case PP_VIDEODECODER_H264PROFILE_HIGH:
      return media::H264PROFILE_HIGH;
    case PP_VIDEODECODER_H264PROFILE_HIGH10PROFILE:
      return media::H264PROFILE_HIGH10PROFILE;
    case PP_VIDEODECODER_H264PROFILE_HIGH422PROFILE:
      return media::H264PROFILE_HIGH422PROFILE;
    case PP_VIDEODECODER_H264PROFILE_HIGH444PREDICTIVEPROFILE:
      return media::H264PROFILE_HIGH444PREDICTIVEPROFILE;
    case PP_VIDEODECODER_H264PROFILE_SCALABLEBASELINE:
      return media::H264PROFILE_SCALABLEBASELINE;
    case PP_VIDEODECODER_H264PROFILE_SCALABLEHIGH:
      return media::H264PROFILE_SCALABLEHIGH;
    case PP_VIDEODECODER_H264PROFILE_STEREOHIGH:
      return media::H264PROFILE_STEREOHIGH;
    case PP_VIDEODECODER_H264PROFILE_MULTIVIEWHIGH:
      return media::H264PROFILE_MULTIVIEWHIGH;
    case PP_VIDEODECODER_VP8PROFILE_MAIN:
      return media::VP8PROFILE_MAIN;
    default:
      return media::VIDEO_CODEC_PROFILE_UNKNOWN;
  }
}

// static
PP_Resource PPB_VideoDecoder_Impl::Create(
    PP_Instance instance,
    PP_Resource graphics_context,
    PP_VideoDecoder_Profile profile) {
  EnterResourceNoLock<PPB_Graphics3D_API> enter_context(graphics_context, true);
  if (enter_context.failed())
    return 0;
  PPB_Graphics3D_Impl* graphics3d_impl =
      static_cast<PPB_Graphics3D_Impl*>(enter_context.object());

  scoped_refptr<PPB_VideoDecoder_Impl> decoder(
      new PPB_VideoDecoder_Impl(instance));
  if (decoder->Init(graphics_context, graphics3d_impl->platform_context(),
                    graphics3d_impl->gles2_impl(), profile))
    return decoder->GetReference();
  return 0;
}

bool PPB_VideoDecoder_Impl::Init(
    PP_Resource graphics_context,
    PluginDelegate::PlatformContext3D* context,
    gpu::gles2::GLES2Implementation* gles2_impl,
    PP_VideoDecoder_Profile profile) {
  InitCommon(graphics_context, gles2_impl);

  int command_buffer_route_id = context->GetCommandBufferRouteId();
  if (command_buffer_route_id == 0)
    return false;

  PluginDelegate* plugin_delegate = ResourceHelper::GetPluginDelegate(this);
  if (!plugin_delegate)
    return false;

  platform_video_decoder_.reset(plugin_delegate->CreateVideoDecoder(
      this, command_buffer_route_id));
  if (!platform_video_decoder_.get())
    return false;

  FlushCommandBuffer();
  return platform_video_decoder_->Initialize(PPToMediaProfile(profile));
}

int32_t PPB_VideoDecoder_Impl::Decode(
    const PP_VideoBitstreamBuffer_Dev* bitstream_buffer,
    scoped_refptr<TrackedCallback> callback) {
  if (!platform_video_decoder_.get())
    return PP_ERROR_BADRESOURCE;

  EnterResourceNoLock<PPB_Buffer_API> enter(bitstream_buffer->data, true);
  if (enter.failed())
    return PP_ERROR_FAILED;

  PPB_Buffer_Impl* buffer = static_cast<PPB_Buffer_Impl*>(enter.object());
  DCHECK_GE(bitstream_buffer->id, 0);
  media::BitstreamBuffer decode_buffer(
      bitstream_buffer->id,
      buffer->shared_memory()->handle(),
      bitstream_buffer->size);
  if (!SetBitstreamBufferCallback(bitstream_buffer->id, callback))
    return PP_ERROR_BADARGUMENT;

  FlushCommandBuffer();
  platform_video_decoder_->Decode(decode_buffer);
  return PP_OK_COMPLETIONPENDING;
}

void PPB_VideoDecoder_Impl::AssignPictureBuffers(
    uint32_t no_of_buffers,
    const PP_PictureBuffer_Dev* buffers) {
  if (!platform_video_decoder_.get())
    return;

  std::vector<media::PictureBuffer> wrapped_buffers;
  for (uint32 i = 0; i < no_of_buffers; i++) {
    PP_PictureBuffer_Dev in_buf = buffers[i];
    DCHECK_GE(in_buf.id, 0);
    media::PictureBuffer buffer(
        in_buf.id,
        gfx::Size(in_buf.size.width, in_buf.size.height),
        in_buf.texture_id);
    wrapped_buffers.push_back(buffer);
  }

  FlushCommandBuffer();
  platform_video_decoder_->AssignPictureBuffers(wrapped_buffers);
}

void PPB_VideoDecoder_Impl::ReusePictureBuffer(int32_t picture_buffer_id) {
  if (!platform_video_decoder_.get())
    return;

  FlushCommandBuffer();
  platform_video_decoder_->ReusePictureBuffer(picture_buffer_id);
}

int32_t PPB_VideoDecoder_Impl::Flush(scoped_refptr<TrackedCallback> callback) {
  if (!platform_video_decoder_.get())
    return PP_ERROR_BADRESOURCE;

  if (!SetFlushCallback(callback))
    return PP_ERROR_INPROGRESS;

  FlushCommandBuffer();
  platform_video_decoder_->Flush();
  return PP_OK_COMPLETIONPENDING;
}

int32_t PPB_VideoDecoder_Impl::Reset(scoped_refptr<TrackedCallback> callback) {
  if (!platform_video_decoder_.get())
    return PP_ERROR_BADRESOURCE;

  if (!SetResetCallback(callback))
    return PP_ERROR_INPROGRESS;

  FlushCommandBuffer();
  platform_video_decoder_->Reset();
  return PP_OK_COMPLETIONPENDING;
}

void PPB_VideoDecoder_Impl::Destroy() {
  FlushCommandBuffer();

  if (platform_video_decoder_)
    platform_video_decoder_.release()->Destroy();
  ppp_videodecoder_ = NULL;

  ::ppapi::PPB_VideoDecoder_Shared::Destroy();
}

void PPB_VideoDecoder_Impl::ProvidePictureBuffers(
    uint32 requested_num_of_buffers,
    const gfx::Size& dimensions,
    uint32 texture_target) {
  if (!ppp_videodecoder_)
    return;

  PP_Size out_dim = PP_MakeSize(dimensions.width(), dimensions.height());
  ppp_videodecoder_->ProvidePictureBuffers(pp_instance(), pp_resource(),
      requested_num_of_buffers, &out_dim, texture_target);
}

void PPB_VideoDecoder_Impl::PictureReady(const media::Picture& picture) {
  if (!ppp_videodecoder_)
    return;

  PP_Picture_Dev output;
  output.picture_buffer_id = picture.picture_buffer_id();
  output.bitstream_buffer_id = picture.bitstream_buffer_id();
  ppp_videodecoder_->PictureReady(pp_instance(), pp_resource(), &output);
}

void PPB_VideoDecoder_Impl::DismissPictureBuffer(int32 picture_buffer_id) {
  if (!ppp_videodecoder_)
    return;
  ppp_videodecoder_->DismissPictureBuffer(pp_instance(), pp_resource(),
                                          picture_buffer_id);
}

void PPB_VideoDecoder_Impl::NotifyError(
    media::VideoDecodeAccelerator::Error error) {
  if (!ppp_videodecoder_)
    return;

  // TODO(vrk): This is assuming VideoDecodeAccelerator::Error and
  // PP_VideoDecodeError_Dev have identical enum values. There is no compiler
  // assert to guarantee this. We either need to add such asserts or
  // merge these two enums.
  ppp_videodecoder_->NotifyError(pp_instance(), pp_resource(),
                                 static_cast<PP_VideoDecodeError_Dev>(error));
}

void PPB_VideoDecoder_Impl::NotifyResetDone() {
  RunResetCallback(PP_OK);
}

void PPB_VideoDecoder_Impl::NotifyEndOfBitstreamBuffer(
    int32 bitstream_buffer_id) {
  RunBitstreamBufferCallback(bitstream_buffer_id, PP_OK);
}

void PPB_VideoDecoder_Impl::NotifyFlushDone() {
  RunFlushCallback(PP_OK);
}

void PPB_VideoDecoder_Impl::NotifyInitializeDone() {
  NOTREACHED() << "PlatformVideoDecoder::Initialize() is synchronous!";
}

}  // namespace ppapi
}  // namespace webkit
