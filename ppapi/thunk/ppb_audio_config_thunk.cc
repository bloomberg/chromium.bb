// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/thunk/thunk.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_audio_config_api.h"
#include "ppapi/thunk/resource_creation_api.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Resource CreateStereo16bit(PP_Instance instance,
                              PP_AudioSampleRate sample_rate,
                              uint32_t sample_frame_count) {
  EnterFunction<ResourceCreationAPI> enter(instance, true);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateAudioConfig(instance, sample_rate,
                                              sample_frame_count);
}

uint32_t RecommendSampleFrameCount(PP_AudioSampleRate sample_rate,
                                   uint32_t requested_sample_frame_count) {
  // TODO(brettw) Currently we don't actually query to get a value from the
  // hardware, so we always return the input for in-range values.
  if (requested_sample_frame_count < PP_AUDIOMINSAMPLEFRAMECOUNT)
    return PP_AUDIOMINSAMPLEFRAMECOUNT;
  if (requested_sample_frame_count > PP_AUDIOMAXSAMPLEFRAMECOUNT)
    return PP_AUDIOMAXSAMPLEFRAMECOUNT;
  return requested_sample_frame_count;
}

PP_Bool IsAudioConfig(PP_Resource resource) {
  EnterResource<PPB_AudioConfig_API> enter(resource, false);
  return enter.succeeded() ? PP_TRUE : PP_FALSE;
}

PP_AudioSampleRate GetSampleRate(PP_Resource config_id) {
  EnterResource<PPB_AudioConfig_API> enter(config_id, true);
  if (enter.failed())
    return PP_AUDIOSAMPLERATE_NONE;
  return enter.object()->GetSampleRate();
}

uint32_t GetSampleFrameCount(PP_Resource config_id) {
  EnterResource<PPB_AudioConfig_API> enter(config_id, true);
  if (enter.failed())
    return 0;
  return enter.object()->GetSampleFrameCount();
}

const PPB_AudioConfig g_ppb_audio_config_thunk = {
  &CreateStereo16bit,
  &RecommendSampleFrameCount,
  &IsAudioConfig,
  &GetSampleRate,
  &GetSampleFrameCount
};

}  // namespace

const PPB_AudioConfig* GetPPB_AudioConfig_Thunk() {
  return &g_ppb_audio_config_thunk;
}

}  // namespace thunk
}  // namespace ppapi
