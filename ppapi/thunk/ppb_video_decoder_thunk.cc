// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/pp_errors.h"
#include "ppapi/thunk/thunk.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_video_decoder_api.h"
#include "ppapi/thunk/resource_creation_api.h"

namespace ppapi {
namespace thunk {

namespace {

typedef EnterResource<PPB_VideoDecoder_API> EnterVideoDecoder;

PP_Resource Create(PP_Instance instance) {
  EnterFunction<ResourceCreationAPI> enter(instance, true);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateVideoDecoder(instance);
}

PP_Bool IsVideoDecoder(PP_Resource resource) {
  EnterVideoDecoder enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

PP_Bool GetConfigs(PP_Resource video_decoder,
                   const PP_VideoConfigElement* proto_config,
                   PP_VideoConfigElement* matching_configs,
                   uint32_t matching_configs_size,
                   uint32_t* num_of_matching_configs) {
  EnterVideoDecoder enter(video_decoder, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->GetConfigs(proto_config, matching_configs,
                                    matching_configs_size,
                                    num_of_matching_configs);
}

int32_t Initialize(PP_Resource video_decoder,
                   PP_Resource context_id,
                   const PP_VideoConfigElement* decoder_config,
                   PP_CompletionCallback callback) {
  EnterVideoDecoder enter(video_decoder, true);
  if (enter.failed())
    return PP_ERROR_BADRESOURCE;
  return enter.object()->Initialize(context_id, decoder_config, callback);
}

int32_t Decode(PP_Resource video_decoder,
               const PP_VideoBitstreamBuffer_Dev* bitstream_buffer,
               PP_CompletionCallback callback) {
  EnterVideoDecoder enter(video_decoder, true);
  if (enter.failed())
    return PP_ERROR_BADRESOURCE;
  return enter.object()->Decode(bitstream_buffer, callback);
}

void AssignGLESBuffers(PP_Resource video_decoder,
                       uint32_t no_of_buffers,
                       const PP_GLESBuffer_Dev* buffers) {
  EnterVideoDecoder enter(video_decoder, true);
  if (enter.succeeded())
    enter.object()->AssignGLESBuffers(no_of_buffers, buffers);
}

void AssignSysmemBuffers(PP_Resource video_decoder,
                         uint32_t no_of_buffers,
                         const PP_SysmemBuffer_Dev* buffers) {
  EnterVideoDecoder enter(video_decoder, true);
  if (enter.succeeded())
    enter.object()->AssignSysmemBuffers(no_of_buffers, buffers);
}

void ReusePictureBuffer(PP_Resource video_decoder, int32_t picture_buffer_id) {
  EnterVideoDecoder enter(video_decoder, true);
  if (enter.succeeded())
    enter.object()->ReusePictureBuffer(picture_buffer_id);
}

int32_t Flush(PP_Resource video_decoder, PP_CompletionCallback callback) {
  EnterVideoDecoder enter(video_decoder, true);
  if (enter.failed())
    return PP_ERROR_BADRESOURCE;
  return enter.object()->Flush(callback);
}

int32_t Abort(PP_Resource video_decoder,
              PP_CompletionCallback callback) {
  EnterVideoDecoder enter(video_decoder, true);
  if (enter.failed())
    return PP_ERROR_BADRESOURCE;
  return enter.object()->Abort(callback);
}

const PPB_VideoDecoder_Dev g_ppb_videodecoder_thunk = {
  &Create,
  &IsVideoDecoder,
  &GetConfigs,
  &Initialize,
  &Decode,
  &AssignGLESBuffers,
  &AssignSysmemBuffers,
  &ReusePictureBuffer,
  &Flush,
  &Abort
};

}  // namespace

const PPB_VideoDecoder_Dev* GetPPB_VideoDecoder_Thunk() {
  return &g_ppb_videodecoder_thunk;
}

}  // namespace thunk
}  // namespace ppapi
