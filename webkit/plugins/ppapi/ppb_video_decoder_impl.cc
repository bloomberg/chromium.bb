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
#include "ppapi/thunk/enter.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/ppb_buffer_impl.h"
#include "webkit/plugins/ppapi/ppb_context_3d_impl.h"
#include "webkit/plugins/ppapi/resource_tracker.h"
#include "webkit/plugins/ppapi/var.h"

using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_Buffer_API;
using ppapi::thunk::PPB_Context3D_API;
using ppapi::thunk::PPB_VideoDecoder_API;

namespace webkit {
namespace ppapi {

namespace {

// Utility methods to convert data to and from the ppapi C-types and their
// C++ media-namespace equivalents.
void CopyToPictureDev(const media::Picture& input, PP_Picture_Dev* output) {
  DCHECK(output);
  output->picture_buffer_id = input.picture_buffer_id();
  output->bitstream_buffer_id = input.bitstream_buffer_id();
  output->visible_size =
      PP_MakeSize(input.visible_size().width(), input.visible_size().height());
  output->decoded_size =
      PP_MakeSize(input.decoded_size().width(), input.decoded_size().height());
}

void CopyToConfigList(
    const PP_VideoConfigElement* configs, std::vector<uint32>* output) {
  DCHECK(configs);
  DCHECK(output);
  // TODO(vrk): This is assuming PP_VideoAttributeDictionary and
  // VideoAttributeKey have identical enum values. There is no compiler
  // assert to guarantee this. We either need to add such asserts or
  // merge PP_VideoAttributeDictionary and VideoAttributeKey.
  const PP_VideoConfigElement* current = configs;
  while (*current != PP_VIDEOATTR_DICTIONARY_TERMINATOR) {
    output->push_back(static_cast<uint32>(*configs));
    current++;
  }
}

}  // namespace

PPB_VideoDecoder_Impl::PPB_VideoDecoder_Impl(PluginInstance* instance)
    : Resource(instance),
      callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      context3d_id_(0),
      abort_callback_(PP_BlockUntilComplete()),
      flush_callback_(PP_BlockUntilComplete()) {
  ppp_videodecoder_ =
      static_cast<const PPP_VideoDecoder_Dev*>(instance->module()->
          GetPluginInterface(PPP_VIDEODECODER_DEV_INTERFACE));
}

PPB_VideoDecoder_Impl::~PPB_VideoDecoder_Impl() {
  if (context3d_id_)
    ResourceTracker::Get()->UnrefResource(context3d_id_);
}

PPB_VideoDecoder_API* PPB_VideoDecoder_Impl::AsPPB_VideoDecoder_API() {
  return this;
}

PP_Bool PPB_VideoDecoder_Impl::GetConfigs(
    const PP_VideoConfigElement* requested_configs,
    PP_VideoConfigElement* matching_configs,
    uint32_t matching_configs_size,
    uint32_t* num_of_matching_configs) {
  if (!instance())
    return PP_FALSE;
  if (!platform_video_decoder_.get())
    return PP_FALSE;
  if (!matching_configs)
    return PP_FALSE;

  std::vector<uint32> requested;
  CopyToConfigList(requested_configs, &requested);
  std::vector<uint32> matched;
  platform_video_decoder_->GetConfigs(requested, &matched);

  uint32 i;
  for (i = 0; i < matched.size() && i < matching_configs_size; i++)
    matching_configs[i] = matched[i];
  *num_of_matching_configs = i;

  return PP_TRUE;
}

int32_t PPB_VideoDecoder_Impl::Initialize(
    PP_Resource context_id,
    const PP_VideoConfigElement* decoder_config,
    PP_CompletionCallback callback) {
  if (!callback.func)
    return PP_ERROR_BADARGUMENT;

  if (!instance())
    return PP_ERROR_FAILED;

  EnterResourceNoLock<PPB_Context3D_API> enter(context_id, true);
  if (enter.failed())
    return PP_ERROR_BADRESOURCE;
  PPB_Context3D_Impl* context3d =
      static_cast<PPB_Context3D_Impl*>(enter.object());

  context3d_id_ = context_id;
  ResourceTracker::Get()->AddRefResource(context3d_id_);
  int command_buffer_route_id =
      context3d->platform_context()->GetCommandBufferRouteId();
  if (command_buffer_route_id == 0)
    return PP_ERROR_FAILED;

  platform_video_decoder_.reset(
      instance()->delegate()->CreateVideoDecoder(
          this, command_buffer_route_id, context3d->gles2_impl()->helper()));

  if (!platform_video_decoder_.get())
    return PP_ERROR_FAILED;

  std::vector<uint32> copied;
  // TODO(vrk): Validate configs before copy.
  CopyToConfigList(decoder_config, &copied);
  if (platform_video_decoder_->Initialize(copied)) {
    initialization_callback_ = callback;
    return PP_OK_COMPLETIONPENDING;
  } else {
    return PP_ERROR_FAILED;
  }
}

int32_t PPB_VideoDecoder_Impl::Decode(
    const PP_VideoBitstreamBuffer_Dev* bitstream_buffer,
    PP_CompletionCallback callback) {
  if (!platform_video_decoder_.get())
    return PP_ERROR_BADRESOURCE;

  EnterResourceNoLock<PPB_Buffer_API> enter(bitstream_buffer->data, true);
  if (enter.failed())
    return PP_ERROR_FAILED;

  PPB_Buffer_Impl* buffer = static_cast<PPB_Buffer_Impl*>(enter.object());
  media::BitstreamBuffer decode_buffer(bitstream_buffer->id,
                                       buffer->shared_memory()->handle(),
                                       static_cast<size_t>(buffer->size()));
  CHECK(bitstream_buffer_callbacks_.insert(std::make_pair(
      bitstream_buffer->id, callback)).second);

  platform_video_decoder_->Decode(decode_buffer);
  return PP_OK_COMPLETIONPENDING;
}

void PPB_VideoDecoder_Impl::AssignGLESBuffers(
    uint32_t no_of_buffers,
    const PP_GLESBuffer_Dev* buffers) {
  if (!platform_video_decoder_.get())
    return;

  std::vector<media::GLESBuffer> wrapped_buffers;
  for (uint32 i = 0; i < no_of_buffers; i++) {
    PP_GLESBuffer_Dev in_buf = buffers[i];
    media::GLESBuffer buffer(
        in_buf.info.id,
        gfx::Size(in_buf.info.size.width, in_buf.info.size.height),
        in_buf.texture_id);
    wrapped_buffers.push_back(buffer);
  }
  platform_video_decoder_->AssignGLESBuffers(wrapped_buffers);
}

void PPB_VideoDecoder_Impl::AssignSysmemBuffers(
    uint32_t no_of_buffers,
    const PP_SysmemBuffer_Dev* buffers) {
  if (!platform_video_decoder_.get())
    return;

  std::vector<media::SysmemBuffer> wrapped_buffers;
  for (uint32 i = 0; i < no_of_buffers; i++) {
    PP_SysmemBuffer_Dev in_buf = buffers[i];
    // TODO(brettw) we should properly handle the errors here if the buffer
    // isn't a valid image rather than CHECKing.
    EnterResourceNoLock<PPB_Buffer_API> enter(in_buf.data, true);
    CHECK(enter.succeeded());
    webkit::ppapi::PPB_Buffer_Impl* pepper_buffer =
        static_cast<webkit::ppapi::PPB_Buffer_Impl*>(enter.object());
    CHECK(pepper_buffer->IsMapped());
    media::SysmemBuffer buffer(
        in_buf.info.id,
        gfx::Size(in_buf.info.size.width, in_buf.info.size.height),
        pepper_buffer->Map());
    wrapped_buffers.push_back(buffer);
  }
  platform_video_decoder_->AssignSysmemBuffers(wrapped_buffers);
}

void PPB_VideoDecoder_Impl::ReusePictureBuffer(int32_t picture_buffer_id) {
  if (!platform_video_decoder_.get())
    return;
  platform_video_decoder_->ReusePictureBuffer(picture_buffer_id);
}

int32_t PPB_VideoDecoder_Impl::Flush(PP_CompletionCallback callback) {
  if (!platform_video_decoder_.get())
    return PP_ERROR_BADRESOURCE;

  // Store the callback to be called when Flush() is done.
  // TODO(vmr): Check for current flush/abort operations.
  flush_callback_ = callback;

  platform_video_decoder_->Flush();
  return PP_OK_COMPLETIONPENDING;
}

int32_t PPB_VideoDecoder_Impl::Abort(PP_CompletionCallback callback) {
  if (!platform_video_decoder_.get())
    return PP_ERROR_BADRESOURCE;

  // Store the callback to be called when Abort() is done.
  // TODO(vmr): Check for current flush/abort operations.
  abort_callback_ = callback;

  platform_video_decoder_->Abort();
  return PP_OK_COMPLETIONPENDING;
}

void PPB_VideoDecoder_Impl::ProvidePictureBuffers(
    uint32 requested_num_of_buffers,
    const gfx::Size& dimensions,
    media::VideoDecodeAccelerator::MemoryType type) {
  if (!ppp_videodecoder_)
    return;

  // TODO(vrk): Compiler assert or use switch statement instead of making
  // a blind cast.
  PP_PictureBufferType_Dev out_type =
      static_cast<PP_PictureBufferType_Dev>(type);
  PP_Size out_dim = PP_MakeSize(dimensions.width(), dimensions.height());
  ScopedResourceId resource(this);
  ppp_videodecoder_->ProvidePictureBuffers(
      instance()->pp_instance(), resource.id, requested_num_of_buffers,
      out_dim, out_type);
}

void PPB_VideoDecoder_Impl::PictureReady(const media::Picture& picture) {
  if (!ppp_videodecoder_)
    return;

  ScopedResourceId resource(this);
  PP_Picture_Dev out_pic;
  CopyToPictureDev(picture, &out_pic);
  ppp_videodecoder_->PictureReady(
      instance()->pp_instance(), resource.id, out_pic);
}

void PPB_VideoDecoder_Impl::DismissPictureBuffer(int32 picture_buffer_id) {
  if (!ppp_videodecoder_)
    return;

  ScopedResourceId resource(this);
  ppp_videodecoder_->DismissPictureBuffer(
      instance()->pp_instance(), resource.id, picture_buffer_id);
}

void PPB_VideoDecoder_Impl::NotifyEndOfStream() {
  if (!ppp_videodecoder_)
    return;

  ScopedResourceId resource(this);
  ppp_videodecoder_->EndOfStream(instance()->pp_instance(), resource.id);
}

void PPB_VideoDecoder_Impl::NotifyError(
    media::VideoDecodeAccelerator::Error error) {
  if (!ppp_videodecoder_)
    return;

  ScopedResourceId resource(this);
  // TODO(vrk): This is assuming VideoDecodeAccelerator::Error and
  // PP_VideoDecodeError_Dev have identical enum values. There is no compiler
  // assert to guarantee this. We either need to add such asserts or
  // merge these two enums.
  ppp_videodecoder_->NotifyError(instance()->pp_instance(), resource.id,
      static_cast<PP_VideoDecodeError_Dev>(error));
}

void PPB_VideoDecoder_Impl::NotifyAbortDone() {
  if (abort_callback_.func == NULL)
    return;

  // Call the callback that was stored to be called when Abort is done.
  PP_RunAndClearCompletionCallback(&abort_callback_, PP_OK);
}

void PPB_VideoDecoder_Impl::NotifyEndOfBitstreamBuffer(
    int32 bitstream_buffer_id) {
  CallbackById::iterator it =
      bitstream_buffer_callbacks_.find(bitstream_buffer_id);
  DCHECK(it != bitstream_buffer_callbacks_.end());
  PP_CompletionCallback cc = it->second;
  bitstream_buffer_callbacks_.erase(it);
  PP_RunCompletionCallback(&cc, PP_OK);
}

void PPB_VideoDecoder_Impl::NotifyFlushDone() {
  if (flush_callback_.func == NULL)
    return;

  // Call the callback that was stored to be called when Flush is done.
  PP_RunAndClearCompletionCallback(&flush_callback_, PP_OK);
}

void PPB_VideoDecoder_Impl::NotifyInitializeDone() {
  if (initialization_callback_.func == NULL)
    return;

  PP_RunAndClearCompletionCallback(&initialization_callback_, PP_OK);
}

}  // namespace ppapi
}  // namespace webkit
