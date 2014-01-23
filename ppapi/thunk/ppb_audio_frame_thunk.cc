// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From ppb_audio_frame.idl modified Wed Jan 22 09:11:35 2014.

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_audio_frame.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppapi_thunk_export.h"
#include "ppapi/thunk/ppb_audio_frame_api.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Bool IsAudioFrame(PP_Resource resource) {
  VLOG(4) << "PPB_AudioFrame::IsAudioFrame()";
  EnterResource<PPB_AudioFrame_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

PP_TimeDelta GetTimestamp(PP_Resource frame) {
  VLOG(4) << "PPB_AudioFrame::GetTimestamp()";
  EnterResource<PPB_AudioFrame_API> enter(frame, true);
  if (enter.failed())
    return 0.0;
  return enter.object()->GetTimestamp();
}

void SetTimestamp(PP_Resource frame, PP_TimeDelta timestamp) {
  VLOG(4) << "PPB_AudioFrame::SetTimestamp()";
  EnterResource<PPB_AudioFrame_API> enter(frame, true);
  if (enter.failed())
    return;
  enter.object()->SetTimestamp(timestamp);
}

PP_AudioFrame_SampleSize GetSampleSize(PP_Resource frame) {
  VLOG(4) << "PPB_AudioFrame::GetSampleSize()";
  EnterResource<PPB_AudioFrame_API> enter(frame, true);
  if (enter.failed())
    return PP_AUDIOFRAME_SAMPLESIZE_UNKNOWN;
  return enter.object()->GetSampleSize();
}

uint32_t GetNumberOfChannels(PP_Resource frame) {
  VLOG(4) << "PPB_AudioFrame::GetNumberOfChannels()";
  EnterResource<PPB_AudioFrame_API> enter(frame, true);
  if (enter.failed())
    return 0;
  return enter.object()->GetNumberOfChannels();
}

uint32_t GetNumberOfSamples(PP_Resource frame) {
  VLOG(4) << "PPB_AudioFrame::GetNumberOfSamples()";
  EnterResource<PPB_AudioFrame_API> enter(frame, true);
  if (enter.failed())
    return 0;
  return enter.object()->GetNumberOfSamples();
}

void* GetDataBuffer(PP_Resource frame) {
  VLOG(4) << "PPB_AudioFrame::GetDataBuffer()";
  EnterResource<PPB_AudioFrame_API> enter(frame, true);
  if (enter.failed())
    return NULL;
  return enter.object()->GetDataBuffer();
}

uint32_t GetDataBufferSize(PP_Resource frame) {
  VLOG(4) << "PPB_AudioFrame::GetDataBufferSize()";
  EnterResource<PPB_AudioFrame_API> enter(frame, true);
  if (enter.failed())
    return 0;
  return enter.object()->GetDataBufferSize();
}

const PPB_AudioFrame_0_1 g_ppb_audioframe_thunk_0_1 = {
  &IsAudioFrame,
  &GetTimestamp,
  &SetTimestamp,
  &GetSampleSize,
  &GetNumberOfChannels,
  &GetNumberOfSamples,
  &GetDataBuffer,
  &GetDataBufferSize
};

}  // namespace

PPAPI_THUNK_EXPORT const PPB_AudioFrame_0_1* GetPPB_AudioFrame_0_1_Thunk() {
  return &g_ppb_audioframe_thunk_0_1;
}

}  // namespace thunk
}  // namespace ppapi
