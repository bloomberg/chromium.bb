// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/pp_errors.h"
#include "ppapi/thunk/common.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_audio_input_api.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

typedef EnterResource<PPB_AudioInput_API> EnterAudioInput;

PP_Resource Create(PP_Instance instance,
                   PP_Resource config_id,
                   PPB_AudioInput_Callback callback,
                   void* user_data) {
  EnterFunction<ResourceCreationAPI> enter(instance, true);
  if (enter.failed())
    return 0;

  return enter.functions()->CreateAudioInput(instance, config_id,
      callback, user_data);
}

PP_Bool IsAudioInput(PP_Resource resource) {
  EnterAudioInput enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

PP_Resource GetCurrentConfiguration(PP_Resource audio_id) {
  EnterAudioInput enter(audio_id, true);
  if (enter.failed())
    return 0;
  return enter.object()->GetCurrentConfig();
}

PP_Bool StartCapture(PP_Resource audio_input) {
  EnterAudioInput enter(audio_input, true);
  if (enter.failed())
    return PP_FALSE;

  return enter.object()->StartCapture();
}

PP_Bool StopCapture(PP_Resource audio_input) {
  EnterAudioInput enter(audio_input, true);
  if (enter.failed())
    return PP_FALSE;

  return enter.object()->StopCapture();
}

const PPB_AudioInput_Dev g_ppb_audioinput_thunk = {
  &Create,
  &IsAudioInput,
  &GetCurrentConfiguration,
  &StartCapture,
  &StopCapture
};

}  // namespace

const PPB_AudioInput_Dev* GetPPB_AudioInput_Dev_Thunk() {
  return &g_ppb_audioinput_thunk;
}

}  // namespace thunk
}  // namespace ppapi
