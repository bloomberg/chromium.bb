// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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

using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_Buffer_API;
using ppapi::thunk::PPB_Graphics3D_API;
using ppapi::thunk::PPB_VideoDecoder_API;

namespace webkit {
namespace ppapi {

PPB_VideoDecoder_Impl::PPB_VideoDecoder_Impl(PP_Instance instance)
    : Resource(instance),
      ppp_videodecoder_(NULL) {
  PluginModule* plugin_module = ResourceHelper::GetPluginModule(this);
  if (plugin_module) {
    ppp_videodecoder_ = static_cast<const PPP_VideoDecoder_Dev*>(
        plugin_module->GetPluginInterface(PPP_VIDEODECODER_DEV_INTERFACE));
  }
}

PPB_VideoDecoder_Impl::~PPB_VideoDecoder_Impl() {
}

PPB_VideoDecoder_API* PPB_VideoDecoder_Impl::AsPPB_VideoDecoder_API() {
  return this;
}

// Convert PP_VideoDecoder_Profile to media::VideoDecodeAccelerator::Profile.
static media::VideoDecodeAccelerator::Profile PPToMediaProfile(
    const PP_VideoDecoder_Profile pp_profile) {
  // TODO(fischman,vrk): this assumes the enum values in the two Profile types
  // match up exactly.  Add a COMPILE_ASSERT for this somewhere.
  return static_cast<media::VideoDecodeAccelerator::Profile>(pp_profile);
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

  platform_video_decoder_ = plugin_delegate->CreateVideoDecoder(
      this, command_buffer_route_id);
  if (!platform_video_decoder_)
    return false;

  FlushCommandBuffer();
  return platform_video_decoder_->Initialize(PPToMediaProfile(profile));
}

int32_t PPB_VideoDecoder_Impl::Decode(
    const PP_VideoBitstreamBuffer_Dev* bitstream_buffer,
    PP_CompletionCallback callback) {
  if (!callback.func)
    return PP_ERROR_BLOCKS_MAIN_THREAD;

  if (!platform_video_decoder_)
    return PP_ERROR_BADRESOURCE;

  EnterResourceNoLock<PPB_Buffer_API> enter(bitstream_buffer->data, true);
  if (enter.failed())
    return PP_ERROR_FAILED;

  PPB_Buffer_Impl* buffer = static_cast<PPB_Buffer_Impl*>(enter.object());
  media::BitstreamBuffer decode_buffer(
      bitstream_buffer->id,
      buffer->shared_memory()->handle(),
      static_cast<size_t>(bitstream_buffer->size));
  if (!SetBitstreamBufferCallback(bitstream_buffer->id, callback))
    return PP_ERROR_BADARGUMENT;

  FlushCommandBuffer();
  platform_video_decoder_->Decode(decode_buffer);
  return PP_OK_COMPLETIONPENDING;
}

void PPB_VideoDecoder_Impl::AssignPictureBuffers(
    uint32_t no_of_buffers,
    const PP_PictureBuffer_Dev* buffers) {
  if (!platform_video_decoder_)
    return;

  std::vector<media::PictureBuffer> wrapped_buffers;
  for (uint32 i = 0; i < no_of_buffers; i++) {
    PP_PictureBuffer_Dev in_buf = buffers[i];
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
  if (!platform_video_decoder_)
    return;

  FlushCommandBuffer();
  platform_video_decoder_->ReusePictureBuffer(picture_buffer_id);
}

int32_t PPB_VideoDecoder_Impl::Flush(PP_CompletionCallback callback) {
  if (!callback.func)
    return PP_ERROR_BLOCKS_MAIN_THREAD;

  if (!platform_video_decoder_)
    return PP_ERROR_BADRESOURCE;

  if (!SetFlushCallback(callback))
    return PP_ERROR_INPROGRESS;

  FlushCommandBuffer();
  platform_video_decoder_->Flush();
  return PP_OK_COMPLETIONPENDING;
}

int32_t PPB_VideoDecoder_Impl::Reset(PP_CompletionCallback callback) {
  if (!callback.func)
    return PP_ERROR_BLOCKS_MAIN_THREAD;

  if (!platform_video_decoder_)
    return PP_ERROR_BADRESOURCE;

  if (!SetResetCallback(callback))
    return PP_ERROR_INPROGRESS;

  FlushCommandBuffer();
  platform_video_decoder_->Reset();
  return PP_OK_COMPLETIONPENDING;
}

void PPB_VideoDecoder_Impl::Destroy() {
  if (!platform_video_decoder_)
    return;

  FlushCommandBuffer();
  platform_video_decoder_->Destroy();
  ::ppapi::VideoDecoderImpl::Destroy();
  platform_video_decoder_ = NULL;
  ppp_videodecoder_ = NULL;
}

void PPB_VideoDecoder_Impl::ProvidePictureBuffers(
    uint32 requested_num_of_buffers, const gfx::Size& dimensions) {
  if (!ppp_videodecoder_)
    return;

  PP_Size out_dim = PP_MakeSize(dimensions.width(), dimensions.height());
  ppp_videodecoder_->ProvidePictureBuffers(pp_instance(), pp_resource(),
                                           requested_num_of_buffers, &out_dim);
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

void PPB_VideoDecoder_Impl::NotifyEndOfStream() {
  if (!ppp_videodecoder_)
    return;
  ppp_videodecoder_->EndOfStream(pp_instance(), pp_resource());
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
