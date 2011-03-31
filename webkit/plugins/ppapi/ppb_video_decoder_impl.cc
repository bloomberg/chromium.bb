// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_video_decoder_impl.h"

#include <string>

#include "base/logging.h"
#include "ppapi/c/dev/pp_video_dev.h"
#include "ppapi/c/dev/ppb_video_decoder_dev.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/var.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/ppb_file_ref_impl.h"
#include "webkit/plugins/ppapi/resource_tracker.h"

namespace webkit {
namespace ppapi {

namespace {

PP_Bool GetConfigs(PP_Instance instance_id,
                   PP_VideoDecoderConfig_Dev* proto_config,
                   PP_VideoDecoderConfig_Dev* matching_configs,
                   int32_t matching_configs_size,
                   int32_t* num_of_matching_configs) {
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
                   PP_VideoDecoderConfig_Dev* decoder_config) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(instance_id);
  if (!instance)
    return 0;

  scoped_refptr<PPB_VideoDecoder_Impl> decoder(
      new PPB_VideoDecoder_Impl(instance));

  if (!decoder->Init(const_cast<PP_VideoDecoderConfig_Dev*>(decoder_config)))
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

void AssignPictureBuffer(PP_Resource video_decoder,
                         uint32_t no_of_buffers,
                         union PP_PictureData_Dev* picture_buffer) {
  scoped_refptr<PPB_VideoDecoder_Impl> decoder(
      Resource::GetAs<PPB_VideoDecoder_Impl>(video_decoder));
  if (!decoder)
    return;

  decoder->AssignPictureBuffer(no_of_buffers, picture_buffer);
}

void ReusePictureBuffer(PP_Resource video_decoder,
                        union PP_PictureData_Dev* picture_buffer) {
  scoped_refptr<PPB_VideoDecoder_Impl> decoder(
      Resource::GetAs<PPB_VideoDecoder_Impl>(video_decoder));
  if (!decoder)
    return;

  decoder->ReusePictureBuffer(picture_buffer);
}

PP_Bool Flush(PP_Resource decoder_id,
              PP_CompletionCallback callback) {
  scoped_refptr<PPB_VideoDecoder_Impl> decoder(
      Resource::GetAs<PPB_VideoDecoder_Impl>(decoder_id));
  if (!decoder)
    return PP_FALSE;

  return BoolToPPBool(decoder->Flush(callback));
}

PP_Bool Abort(PP_Resource decoder_id,
              PP_CompletionCallback callback) {
  scoped_refptr<PPB_VideoDecoder_Impl> decoder(
      Resource::GetAs<PPB_VideoDecoder_Impl>(decoder_id));
  if (!decoder)
    return PP_FALSE;

  return BoolToPPBool(decoder->Abort(callback));
}


const PPB_VideoDecoder_Dev ppb_videodecoder = {
  &GetConfigs,
  &Create,
  &IsVideoDecoder,
  &Decode,
  &AssignPictureBuffer,
  &ReusePictureBuffer,
  &Flush,
  &Abort,
};

}  // namespace

PPB_VideoDecoder_Impl::PPB_VideoDecoder_Impl(PluginInstance* instance)
    : Resource(instance),
      callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      abort_callback_(PP_BlockUntilComplete()),
      flush_callback_(PP_BlockUntilComplete()),
      bitstream_buffer_callback_(PP_BlockUntilComplete()) {
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
    PP_VideoDecoderConfig_Dev* proto_config,
    PP_VideoDecoderConfig_Dev* matching_configs,
    int32_t matching_configs_size,
    int32_t* num_of_matching_configs) {
  if (!instance())
    return false;
  if (!platform_video_decoder_.get())
    return false;

  // TODO(vmr): Implement.
  NOTIMPLEMENTED();

  return false;
}

bool PPB_VideoDecoder_Impl::Init(PP_VideoDecoderConfig_Dev* decoder_config) {
  if (!instance())
    return false;

  platform_video_decoder_.reset(
      instance()->delegate()->CreateVideoDecoder(decoder_config));

  return platform_video_decoder_.get()? true : false;
}

bool PPB_VideoDecoder_Impl::Decode(
    PP_VideoBitstreamBuffer_Dev* bitstream_buffer,
    PP_CompletionCallback callback) {
  if (!platform_video_decoder_.get())
    return false;

  media::BitstreamBuffer* decode_buffer = NULL;
  // TODO(vmr): Convert bitstream_buffer to BitstreamBuffer object.
  NOTIMPLEMENTED();

  // Store the callback to inform when bitstream buffer has been processed.
  // TODO(vmr): handle simultaneous decodes + callbacks.
  bitstream_buffer_callback_ = callback;

  return platform_video_decoder_->Decode(
      decode_buffer,
      callback_factory_.NewCallback(
          &PPB_VideoDecoder_Impl::OnBitstreamBufferProcessed));
}

void PPB_VideoDecoder_Impl::AssignPictureBuffer(
    uint32_t no_of_picture_buffers,
    PP_PictureData_Dev* picture_buffers) {
  if (!platform_video_decoder_.get())
    return;

  // TODO(vmr): Map PP_PictureData_Dev into PictureBuffer object.
  NOTIMPLEMENTED();

  media::VideoDecodeAccelerator::PictureBuffer* buffer = NULL;
  platform_video_decoder_->ReusePictureBuffer(buffer);
}

void PPB_VideoDecoder_Impl::ReusePictureBuffer(
    PP_PictureData_Dev* picture_buffer) {
  if (!platform_video_decoder_.get())
    return;

  // TODO(vmr): Map PP_PictureData_Dev into PictureBuffer object.
  NOTIMPLEMENTED();

  media::VideoDecodeAccelerator::PictureBuffer* buffer = NULL;
  platform_video_decoder_->ReusePictureBuffer(buffer);
}

bool PPB_VideoDecoder_Impl::Flush(PP_CompletionCallback callback) {
  if (!platform_video_decoder_.get())
    return false;

  // Store the callback to be called when Flush() is done.
  // TODO(vmr): Check for current flush/abort operations.
  flush_callback_ = callback;

  return platform_video_decoder_->Flush(
      callback_factory_.NewCallback(
          &PPB_VideoDecoder_Impl::OnFlushComplete));
}

bool PPB_VideoDecoder_Impl::Abort(PP_CompletionCallback callback) {
  if (!platform_video_decoder_.get())
    return false;

  // Store the callback to be called when Abort() is done.
  // TODO(vmr): Check for current flush/abort operations.
  abort_callback_ = callback;

  return platform_video_decoder_->Abort(
      callback_factory_.NewCallback(
          &PPB_VideoDecoder_Impl::OnAbortComplete));
}

void PPB_VideoDecoder_Impl::ProvidePictureBuffers(
    uint32_t requested_num_of_buffers,
    const std::vector<uint32_t>& buffer_properties) {
  // TODO(vmr): Implement.
  NOTIMPLEMENTED();
}

void PPB_VideoDecoder_Impl::PictureReady(
    media::VideoDecodeAccelerator::Picture* picture) {
  // TODO(vmr): Implement.
  NOTIMPLEMENTED();

  // Convert the picture.
  // Get plugin's PPP interface function pointers.
  // PPP_VideoDecoder* ppp_videodecoder;
  // Call ProvidePictureBuffers function pointer and return.
  // ppp_videodecoder->PictureReady(resource_decoder, picture,
  //                                pic_buffer_used_again);
}

void PPB_VideoDecoder_Impl::DismissPictureBuffer(
      media::VideoDecodeAccelerator::PictureBuffer* picture_buffer) {
  // TODO(vmr): Implement.
  NOTIMPLEMENTED();
}

void PPB_VideoDecoder_Impl::NotifyPictureReady() {
  // No need to react here as we are already reacting to PictureReady.
}

void PPB_VideoDecoder_Impl::NotifyEndOfStream() {
  // TODO(vmr): Implement.
  NOTIMPLEMENTED();

  // Get decoder's resource handle.
  // PP_Resource resource_decoder;
  // Get plugin's PPP interface function pointers.
  // PPP_VideoDecoder* ppp_videodecoder;
  // Call EndOfStream function pointer and return.
  // ppp_videodecoder->EndOfStream(resource_decoder);
}

void PPB_VideoDecoder_Impl::NotifyError(
    media::VideoDecodeAccelerator::Error error) {
  // TODO(vmr): Implement.
  NOTIMPLEMENTED();

  // Get decoder's resource handle.
  // PP_Resource resource_decoder;
  // Get plugin's PPP interface function pointers.
  // PPP_VideoDecoder* ppp_videodecoder;
  // Call NotifyError function pointer.
  // ppp_videodecoder->NotifyError(error, error_data, data_size);
}

void PPB_VideoDecoder_Impl::OnAbortComplete() {
  if (abort_callback_.func == NULL)
    return;

  // Call the callback that was stored to be called when Abort is done.
  PP_CompletionCallback callback = PP_BlockUntilComplete();
  std::swap(callback, abort_callback_);
  PP_RunCompletionCallback(&callback, PP_OK);
}

void PPB_VideoDecoder_Impl::OnBitstreamBufferProcessed() {
  if (bitstream_buffer_callback_.func == NULL)
    return;

  // Call the callback that was stored to be called when bitstream was sent for
  // decoding.
  PP_CompletionCallback callback = PP_BlockUntilComplete();
  std::swap(callback, bitstream_buffer_callback_);
  PP_RunCompletionCallback(&callback, PP_OK);
}

void PPB_VideoDecoder_Impl::OnFlushComplete() {
  if (flush_callback_.func == NULL)
    return;

  // Call the callback that was stored to be called when Flush is done.
  PP_CompletionCallback callback = PP_BlockUntilComplete();
  std::swap(callback, flush_callback_);
  PP_RunCompletionCallback(&callback, PP_OK);
}

}  // namespace ppapi
}  // namespace webkit
