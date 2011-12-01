// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/pp_errors.h"
#include "ppapi/thunk/common.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/thunk.h"
#include "ppapi/thunk/ppb_video_decoder_api.h"
#include "ppapi/thunk/resource_creation_api.h"

namespace ppapi {
namespace thunk {

namespace {

typedef EnterResource<PPB_VideoDecoder_API> EnterVideoDecoder;

PP_Resource Create(PP_Instance instance,
                   PP_Resource graphics_3d,
                   PP_VideoDecoder_Profile profile) {
  EnterFunction<ResourceCreationAPI> enter(instance, true);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateVideoDecoder(instance, graphics_3d, profile);
}

PP_Bool IsVideoDecoder(PP_Resource resource) {
  EnterVideoDecoder enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

int32_t Decode(PP_Resource video_decoder,
               const PP_VideoBitstreamBuffer_Dev* bitstream_buffer,
               PP_CompletionCallback callback) {
  EnterVideoDecoder enter(video_decoder, true);
  if (enter.failed())
    return MayForceCallback(callback, PP_ERROR_BADRESOURCE);
  int32_t result = enter.object()->Decode(bitstream_buffer, callback);
  return MayForceCallback(callback, result);
}

void AssignPictureBuffers(PP_Resource video_decoder,
                          uint32_t no_of_buffers,
                          const PP_PictureBuffer_Dev* buffers) {
  EnterVideoDecoder enter(video_decoder, true);
  if (enter.succeeded())
    enter.object()->AssignPictureBuffers(no_of_buffers, buffers);
}

void ReusePictureBuffer(PP_Resource video_decoder, int32_t picture_buffer_id) {
  EnterVideoDecoder enter(video_decoder, true);
  if (enter.succeeded())
    enter.object()->ReusePictureBuffer(picture_buffer_id);
}

int32_t Flush(PP_Resource video_decoder, PP_CompletionCallback callback) {
  EnterVideoDecoder enter(video_decoder, true);
  if (enter.failed())
    return MayForceCallback(callback, PP_ERROR_BADRESOURCE);
  int32_t result = enter.object()->Flush(callback);
  return MayForceCallback(callback, result);
}

int32_t Reset(PP_Resource video_decoder,
              PP_CompletionCallback callback) {
  EnterVideoDecoder enter(video_decoder, true);
  if (enter.failed())
    return MayForceCallback(callback, PP_ERROR_BADRESOURCE);
  int32_t result = enter.object()->Reset(callback);
  return MayForceCallback(callback, result);
}

void Destroy(PP_Resource video_decoder) {
  EnterVideoDecoder enter(video_decoder, true);
  if (enter.succeeded())
    enter.object()->Destroy();
}

const PPB_VideoDecoder_Dev g_ppb_videodecoder_thunk = {
  &Create,
  &IsVideoDecoder,
  &Decode,
  &AssignPictureBuffers,
  &ReusePictureBuffer,
  &Flush,
  &Reset,
  &Destroy
};

}  // namespace

const PPB_VideoDecoder_Dev* GetPPB_VideoDecoder_Dev_Thunk() {
  return &g_ppb_videodecoder_thunk;
}

}  // namespace thunk
}  // namespace ppapi
