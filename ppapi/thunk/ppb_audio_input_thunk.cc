// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/pp_errors.h"
#include "ppapi/shared_impl/ppb_device_ref_shared.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_device_ref_api.h"
#include "ppapi/thunk/ppb_audio_input_api.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

typedef EnterResource<PPB_AudioInput_API> EnterAudioInput;

PP_Resource Create0_1(PP_Instance instance,
                      PP_Resource config,
                      PPB_AudioInput_Callback audio_input_callback,
                      void* user_data) {
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;

  return enter.functions()->CreateAudioInput0_1(
      instance, config, audio_input_callback, user_data);
}

PP_Resource Create0_2(PP_Instance instance) {
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;

  return enter.functions()->CreateAudioInput(instance);
}

PP_Bool IsAudioInput(PP_Resource resource) {
  EnterAudioInput enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

int32_t EnumerateDevices(PP_Resource audio_input,
                         PP_Resource* devices,
                         PP_CompletionCallback callback) {
  EnterAudioInput enter(audio_input, callback, true);
  if (enter.failed())
    return enter.retval();

  return enter.SetResult(enter.object()->EnumerateDevices(devices,
                                                          enter.callback()));
}

int32_t Open(PP_Resource audio_input,
             PP_Resource device_ref,
             PP_Resource config,
             PPB_AudioInput_Callback audio_input_callback,
             void* user_data,
             PP_CompletionCallback callback) {
  EnterAudioInput enter(audio_input, callback, true);
  if (enter.failed())
    return enter.retval();

  std::string device_id;
  // |device_id| remains empty if |device_ref| is 0, which means the default
  // device.
  if (device_ref != 0) {
    EnterResourceNoLock<PPB_DeviceRef_API> enter_device_ref(device_ref, true);
    if (enter_device_ref.failed())
      return enter.SetResult(PP_ERROR_BADRESOURCE);
    device_id = enter_device_ref.object()->GetDeviceRefData().id;
  }

  return enter.SetResult(enter.object()->Open(
      device_id, config, audio_input_callback, user_data, enter.callback()));
}

PP_Resource GetCurrentConfig(PP_Resource audio_input) {
  EnterAudioInput enter(audio_input, true);
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

void Close(PP_Resource audio_input) {
  EnterAudioInput enter(audio_input, true);
  if (enter.succeeded())
    enter.object()->Close();
}

const PPB_AudioInput_Dev_0_1 g_ppb_audioinput_0_1_thunk = {
  &Create0_1,
  &IsAudioInput,
  &GetCurrentConfig,
  &StartCapture,
  &StopCapture
};

const PPB_AudioInput_Dev_0_2 g_ppb_audioinput_0_2_thunk = {
  &Create0_2,
  &IsAudioInput,
  &EnumerateDevices,
  &Open,
  &GetCurrentConfig,
  &StartCapture,
  &StopCapture,
  &Close
};

}  // namespace

const PPB_AudioInput_Dev_0_1* GetPPB_AudioInput_Dev_0_1_Thunk() {
  return &g_ppb_audioinput_0_1_thunk;
}

const PPB_AudioInput_Dev_0_2* GetPPB_AudioInput_Dev_0_2_Thunk() {
  return &g_ppb_audioinput_0_2_thunk;
}

}  // namespace thunk
}  // namespace ppapi
