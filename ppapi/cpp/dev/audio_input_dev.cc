// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/audio_input_dev.h"

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/dev/resource_array_dev.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_AudioInput_Dev_0_2>() {
  return PPB_AUDIO_INPUT_DEV_INTERFACE_0_2;
}

template <> const char* interface_name<PPB_AudioInput_Dev_0_1>() {
  return PPB_AUDIO_INPUT_DEV_INTERFACE_0_1;
}

}  // namespace

AudioInput_Dev::AudioInput_Dev() : audio_input_callback_(NULL),
                                   user_data_(NULL) {
}

AudioInput_Dev::AudioInput_Dev(const InstanceHandle& instance,
                               const AudioConfig& config,
                               PPB_AudioInput_Callback callback,
                               void* user_data)
    : config_(config),
      audio_input_callback_(callback),
      user_data_(user_data) {
  if (has_interface<PPB_AudioInput_Dev_0_2>()) {
    PassRefFromConstructor(get_interface<PPB_AudioInput_Dev_0_2>()->Create(
        instance.pp_instance()));
  } else if (has_interface<PPB_AudioInput_Dev_0_1>()) {
    PassRefFromConstructor(get_interface<PPB_AudioInput_Dev_0_1>()->Create(
        instance.pp_instance(), config.pp_resource(), callback, user_data));
  }
}

AudioInput_Dev::AudioInput_Dev(const InstanceHandle& instance)
    : audio_input_callback_(NULL),
      user_data_(NULL) {
  if (has_interface<PPB_AudioInput_Dev_0_2>()) {
    PassRefFromConstructor(get_interface<PPB_AudioInput_Dev_0_2>()->Create(
        instance.pp_instance()));
  }
}

AudioInput_Dev::~AudioInput_Dev() {
}

// static
bool AudioInput_Dev::IsAvailable() {
  return has_interface<PPB_AudioInput_Dev_0_2>() ||
         has_interface<PPB_AudioInput_Dev_0_1>();
}

int32_t AudioInput_Dev::EnumerateDevices(
    const CompletionCallbackWithOutput<std::vector<DeviceRef_Dev> >& callback) {
  if (!has_interface<PPB_AudioInput_Dev_0_2>())
    return callback.MayForce(PP_ERROR_NOINTERFACE);
  if (!callback.pp_completion_callback().func)
    return callback.MayForce(PP_ERROR_BLOCKS_MAIN_THREAD);

  // ArrayOutputCallbackConverter is responsible to delete it.
  ResourceArray_Dev::ArrayOutputCallbackData* data =
      new ResourceArray_Dev::ArrayOutputCallbackData(
          callback.output(), callback.pp_completion_callback());
  return get_interface<PPB_AudioInput_Dev_0_2>()->EnumerateDevices(
      pp_resource(), &data->resource_array_output,
      PP_MakeCompletionCallback(
          &ResourceArray_Dev::ArrayOutputCallbackConverter, data));
}

int32_t AudioInput_Dev::Open(const DeviceRef_Dev& device_ref,
                             const CompletionCallback& callback) {
  if (has_interface<PPB_AudioInput_Dev_0_2>()) {
    return get_interface<PPB_AudioInput_Dev_0_2>()->Open(
        pp_resource(), device_ref.pp_resource(), config_.pp_resource(),
        audio_input_callback_, user_data_, callback.pp_completion_callback());
  }

  if (has_interface<PPB_AudioInput_Dev_0_1>()) {
    if (is_null())
      return callback.MayForce(PP_ERROR_FAILED);

    // If the v0.1 interface is being used and there is a valid resource handle,
    // then the default device has been successfully opened during resource
    // creation.
    if (device_ref.is_null())
      return callback.MayForce(PP_OK);

    // The v0.1 interface doesn't support devices other than the default one.
    return callback.MayForce(PP_ERROR_NOTSUPPORTED);
  }

  return callback.MayForce(PP_ERROR_NOINTERFACE);
}

int32_t AudioInput_Dev::Open(const DeviceRef_Dev& device_ref,
                             const AudioConfig& config,
                             PPB_AudioInput_Callback audio_input_callback,
                             void* user_data,
                             const CompletionCallback& callback) {
  if (has_interface<PPB_AudioInput_Dev_0_2>()) {
    return get_interface<PPB_AudioInput_Dev_0_2>()->Open(
        pp_resource(), device_ref.pp_resource(), config.pp_resource(),
        audio_input_callback, user_data, callback.pp_completion_callback());
  }

  return callback.MayForce(PP_ERROR_NOINTERFACE);
}

bool AudioInput_Dev::StartCapture() {
  if (has_interface<PPB_AudioInput_Dev_0_2>()) {
    return PP_ToBool(get_interface<PPB_AudioInput_Dev_0_2>()->StartCapture(
        pp_resource()));
  }

  if (has_interface<PPB_AudioInput_Dev_0_1>()) {
    return PP_ToBool(get_interface<PPB_AudioInput_Dev_0_1>()->StartCapture(
        pp_resource()));
  }

  return false;
}

bool AudioInput_Dev::StopCapture() {
  if (has_interface<PPB_AudioInput_Dev_0_2>()) {
    return PP_ToBool(get_interface<PPB_AudioInput_Dev_0_2>()->StopCapture(
        pp_resource()));
  }

  if (has_interface<PPB_AudioInput_Dev_0_1>()) {
    return PP_ToBool(get_interface<PPB_AudioInput_Dev_0_1>()->StopCapture(
        pp_resource()));
  }

  return false;
}

void AudioInput_Dev::Close() {
  if (has_interface<PPB_AudioInput_Dev_0_2>())
    get_interface<PPB_AudioInput_Dev_0_2>()->Close(pp_resource());
}

}  // namespace pp
