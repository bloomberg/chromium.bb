// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_video_decoder_impl.h"

#include <string>

#include "base/logging.h"
#include "media/video/picture.h"
#include "ppapi/c/dev/pp_video_dev.h"
#include "ppapi/c/dev/ppb_video_decoder_dev.h"
#include "ppapi/c/dev/ppp_video_decoder_dev.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/ppb_buffer_impl.h"
#include "webkit/plugins/ppapi/resource_tracker.h"
#include "webkit/plugins/ppapi/var.h"

namespace webkit {
namespace ppapi {

namespace {

PP_Bool GetConfigs(PP_Instance instance_id,
                   PP_VideoConfigElement* proto_config,
                   PP_VideoConfigElement* matching_configs,
                   uint32_t matching_configs_size,
                   uint32_t* num_of_matching_configs) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(instance_id);
  if (!instance)
    return PP_FALSE;

  scoped_refptr<PPB_VideoDecoder_Impl> decoder(
      new PPB_VideoDecoder_Impl(instance));

  return BoolToPPBool(decoder->GetConfigs(proto_config,
                                          matching_configs,
                                          matching_configs_size,
                                          num_of_matching_configs));
}

PP_Resource Create(PP_Instance instance_id,
                   PP_VideoConfigElement* decoder_config) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(instance_id);
  if (!instance)
    return 0;

  scoped_refptr<PPB_VideoDecoder_Impl> decoder(
      new PPB_VideoDecoder_Impl(instance));

  if (!decoder->Init(const_cast<PP_VideoConfigElement*>(decoder_config)))
    return 0;

  return decoder->GetReference();
}

PP_Bool IsVideoDecoder(PP_Resource resource) {
  return BoolToPPBool(!!Resource::GetAs<PPB_VideoDecoder_Impl>(resource));
}

PP_Bool Decode(PP_Resource decoder_id,
               PP_VideoBitstreamBuffer_Dev* bitstream_buffer,
               PP_CompletionCallback callback) {
  scoped_refptr<PPB_VideoDecoder_Impl> decoder(
      Resource::GetAs<PPB_VideoDecoder_Impl>(decoder_id));
  if (!decoder)
    return PP_FALSE;

  return BoolToPPBool(decoder->Decode(bitstream_buffer, callback));
}

void AssignGLESBuffers(PP_Resource video_decoder,
                       uint32_t no_of_buffers,
                       PP_GLESBuffer_Dev* buffers) {
  scoped_refptr<PPB_VideoDecoder_Impl> decoder(
      Resource::GetAs<PPB_VideoDecoder_Impl>(video_decoder));
  if (!decoder)
    return;

  decoder->AssignGLESBuffers(no_of_buffers, buffers);
}

void AssignSysmemBuffers(PP_Resource video_decoder,
                         uint32_t no_of_buffers,
                         PP_SysmemBuffer_Dev* buffers) {
  scoped_refptr<PPB_VideoDecoder_Impl> decoder(
      Resource::GetAs<PPB_VideoDecoder_Impl>(video_decoder));
  if (!decoder)
    return;

  decoder->AssignSysmemBuffers(no_of_buffers, buffers);
}

void ReusePictureBuffer(PP_Resource video_decoder, int32_t picture_buffer_id) {
  scoped_refptr<PPB_VideoDecoder_Impl> decoder(
      Resource::GetAs<PPB_VideoDecoder_Impl>(video_decoder));
  if (!decoder)
    return;

  decoder->ReusePictureBuffer(picture_buffer_id);
}

PP_Bool Flush(PP_Resource video_decoder, PP_CompletionCallback callback) {
  scoped_refptr<PPB_VideoDecoder_Impl> decoder(
      Resource::GetAs<PPB_VideoDecoder_Impl>(video_decoder));
  if (!decoder)
    return PP_FALSE;

  return BoolToPPBool(decoder->Flush(callback));
}

PP_Bool Abort(PP_Resource video_decoder,
              PP_CompletionCallback callback) {
  scoped_refptr<PPB_VideoDecoder_Impl> decoder(
      Resource::GetAs<PPB_VideoDecoder_Impl>(video_decoder));
  if (!decoder)
    return PP_FALSE;

  return BoolToPPBool(decoder->Abort(callback));
}

const PPB_VideoDecoder_Dev ppb_videodecoder = {
  &GetConfigs,
  &Create,
  &IsVideoDecoder,
  &Decode,
  &AssignGLESBuffers,
  &AssignSysmemBuffers,
  &ReusePictureBuffer,
  &Flush,
  &Abort,
};

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
      abort_callback_(PP_BlockUntilComplete()),
      flush_callback_(PP_BlockUntilComplete()),
      bitstream_buffer_callback_(PP_BlockUntilComplete()) {
  ppp_videodecoder_ =
      static_cast<const PPP_VideoDecoder_Dev*>(instance->module()->
          GetPluginInterface(PPP_VIDEODECODER_DEV_INTERFACE));
}

PPB_VideoDecoder_Impl::~PPB_VideoDecoder_Impl() {
}

// static
const PPB_VideoDecoder_Dev* PPB_VideoDecoder_Impl::GetInterface() {
  return &ppb_videodecoder;
}

PPB_VideoDecoder_Impl* PPB_VideoDecoder_Impl::AsPPB_VideoDecoder_Impl() {
  return this;
}

bool PPB_VideoDecoder_Impl::GetConfigs(
    PP_VideoConfigElement* requested_configs,
    PP_VideoConfigElement* matching_configs,
    uint32_t matching_configs_size,
    uint32_t* num_of_matching_configs) {
  if (!instance())
    return false;
  if (!platform_video_decoder_.get())
    return false;
  if (!matching_configs)
    return false;

  std::vector<uint32> requested;
  CopyToConfigList(requested_configs, &requested);
  std::vector<uint32> matched;
  platform_video_decoder_->GetConfigs(requested, &matched);

  uint32 i;
  for (i = 0; i < matched.size() && i < matching_configs_size; i++)
    matching_configs[i] = matched[i];
  *num_of_matching_configs = i;

  return true;
}

bool PPB_VideoDecoder_Impl::Init(PP_VideoConfigElement* decoder_config) {
  if (!instance())
    return false;

  platform_video_decoder_.reset(
      instance()->delegate()->CreateVideoDecoder(this));

  std::vector<uint32> copied;
  // TODO(vrk): Validate configs before copy.
  CopyToConfigList(decoder_config, &copied);
  platform_video_decoder_->Initialize(copied);

  return platform_video_decoder_.get()? true : false;
}

bool PPB_VideoDecoder_Impl::Decode(
    PP_VideoBitstreamBuffer_Dev* bitstream_buffer,
    PP_CompletionCallback callback) {
  if (!platform_video_decoder_.get())
    return false;

  media::BitstreamBuffer decode_buffer(
      bitstream_buffer->id,
      Resource::GetAs<PPB_Buffer_Impl>(bitstream_buffer->data)->handle(),
      bitstream_buffer->size);

  // Store the callback to inform when bitstream buffer has been processed.
  // TODO(vmr): handle simultaneous decodes + callbacks.
  bitstream_buffer_callback_ = callback;

  return platform_video_decoder_->Decode(decode_buffer);
}

void PPB_VideoDecoder_Impl::AssignGLESBuffers(
    uint32_t no_of_buffers,
    PP_GLESBuffer_Dev* buffers) {
  if (!platform_video_decoder_.get())
    return;

  std::vector<media::GLESBuffer> wrapped_buffers;
  for (uint32 i = 0; i < no_of_buffers; i++) {
    PP_GLESBuffer_Dev in_buf = buffers[i];
    media::GLESBuffer buffer(in_buf);
    wrapped_buffers.push_back(buffer);
  }
  platform_video_decoder_->AssignGLESBuffers(wrapped_buffers);
}

void PPB_VideoDecoder_Impl::AssignSysmemBuffers(
    uint32_t no_of_buffers,
    PP_SysmemBuffer_Dev* buffers) {
  if (!platform_video_decoder_.get())
    return;

  std::vector<media::SysmemBuffer> wrapped_buffers;
  for (uint32 i = 0; i < no_of_buffers; i++) {
    PP_SysmemBuffer_Dev in_buf = buffers[i];
    media::SysmemBuffer buffer(in_buf);
    wrapped_buffers.push_back(buffer);
  }
  platform_video_decoder_->AssignSysmemBuffers(wrapped_buffers);
}

void PPB_VideoDecoder_Impl::ReusePictureBuffer(int32_t picture_buffer_id) {
  if (!platform_video_decoder_.get())
    return;
  platform_video_decoder_->ReusePictureBuffer(picture_buffer_id);
}

bool PPB_VideoDecoder_Impl::Flush(PP_CompletionCallback callback) {
  if (!platform_video_decoder_.get())
    return false;

  // Store the callback to be called when Flush() is done.
  // TODO(vmr): Check for current flush/abort operations.
  flush_callback_ = callback;

  return platform_video_decoder_->Flush();
}

bool PPB_VideoDecoder_Impl::Abort(PP_CompletionCallback callback) {
  if (!platform_video_decoder_.get())
    return false;

  // Store the callback to be called when Abort() is done.
  // TODO(vmr): Check for current flush/abort operations.
  abort_callback_ = callback;

  return platform_video_decoder_->Abort();
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
      resource.id, requested_num_of_buffers, out_dim, out_type);
}

void PPB_VideoDecoder_Impl::PictureReady(const media::Picture& picture) {
  if (!ppp_videodecoder_)
    return;

  ScopedResourceId resource(this);
  PP_Picture_Dev out_pic;
  CopyToPictureDev(picture, &out_pic);
  ppp_videodecoder_->PictureReady(resource.id, out_pic);
}

void PPB_VideoDecoder_Impl::DismissPictureBuffer(int32 picture_buffer_id) {
  if (!ppp_videodecoder_)
    return;

  ScopedResourceId resource(this);
  ppp_videodecoder_->DismissPictureBuffer(resource.id, picture_buffer_id);
}

void PPB_VideoDecoder_Impl::NotifyEndOfStream() {
  if (!ppp_videodecoder_)
    return;

  ScopedResourceId resource(this);
  ppp_videodecoder_->EndOfStream(resource.id);
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
  ppp_videodecoder_->NotifyError(resource.id,
      static_cast<PP_VideoDecodeError_Dev>(error));
}

void PPB_VideoDecoder_Impl::NotifyAbortDone() {
  if (abort_callback_.func == NULL)
    return;

  // Call the callback that was stored to be called when Abort is done.
  PP_CompletionCallback callback = PP_BlockUntilComplete();
  std::swap(callback, abort_callback_);
  PP_RunCompletionCallback(&callback, PP_OK);
}

void PPB_VideoDecoder_Impl::NotifyEndOfBitstreamBuffer(
    int32 bitstream_buffer_id) {
  if (bitstream_buffer_callback_.func == NULL)
    return;

  // Call the callback that was stored to be called when bitstream was sent for
  // decoding.
  PP_CompletionCallback callback = PP_BlockUntilComplete();
  std::swap(callback, bitstream_buffer_callback_);
  PP_RunCompletionCallback(&callback, PP_OK);
}

void PPB_VideoDecoder_Impl::NotifyFlushDone() {
  if (flush_callback_.func == NULL)
    return;

  // Call the callback that was stored to be called when Flush is done.
  PP_CompletionCallback callback = PP_BlockUntilComplete();
  std::swap(callback, flush_callback_);
  PP_RunCompletionCallback(&callback, PP_OK);
}

}  // namespace ppapi
}  // namespace webkit

// These functions are declared in picture.h but are defined here because of
// dependencies (we can't depend on ppapi types from media).
namespace media {
BufferInfo::BufferInfo(const PP_BufferInfo_Dev& info)
    : id_(info.id),
      size_(info.size.width, info.size.height) {
}

// TODO(vrk): This assigns the PP_Resource context to be
// the context_id. Not sure what it's actually supposed to be.
GLESBuffer::GLESBuffer(const PP_GLESBuffer_Dev& buffer)
    : texture_id_(buffer.texture_id),
      context_id_(buffer.context),
      info_(buffer.info) {
}

SysmemBuffer::SysmemBuffer(const PP_SysmemBuffer_Dev& buffer)
    : info_(buffer.info) {
  scoped_refptr<webkit::ppapi::PPB_Buffer_Impl> pepper_buffer =
      webkit::ppapi::Resource::GetAs<webkit::ppapi::PPB_Buffer_Impl>(
          buffer.data);
  assert(pepper_buffer->is_mapped());
  data_ = pepper_buffer->mapped_buffer();
}

Picture::Picture(const PP_Picture_Dev& picture)
    : picture_buffer_id_(picture.picture_buffer_id),
      bitstream_buffer_id_(picture.bitstream_buffer_id),
      visible_size_(picture.visible_size.width, picture.visible_size.height),
      decoded_size_(picture.decoded_size.width, picture.decoded_size.height) {
}

}  // namespace media
