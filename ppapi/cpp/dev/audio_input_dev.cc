// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/audio_input_dev.h"

#include "ppapi/c/dev/ppb_audio_input_dev.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/dev/device_ref_dev.h"
#include "ppapi/cpp/dev/resource_array_dev.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module.h"
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

struct AudioInput_Dev::EnumerateDevicesState {
  EnumerateDevicesState(std::vector<DeviceRef_Dev>* in_devices,
                        const CompletionCallback& in_callback,
                        AudioInput_Dev* in_audio_input)
      : devices_resource(0),
        devices(in_devices),
        callback(in_callback),
        audio_input(in_audio_input) {
  }

  PP_Resource devices_resource;
  // This is owned by the user of AudioInput_Dev::EnumerateDevices().
  std::vector<DeviceRef_Dev>* devices;
  CompletionCallback callback;
  AudioInput_Dev* audio_input;
};

AudioInput_Dev::AudioInput_Dev() : enum_state_(NULL),
                                   audio_input_callback_(NULL),
                                   user_data_(NULL) {
}

AudioInput_Dev::AudioInput_Dev(const InstanceHandle& instance,
                               const AudioConfig& config,
                               PPB_AudioInput_Callback callback,
                               void* user_data)
    : config_(config),
      enum_state_(NULL),
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
    : enum_state_(NULL),
      audio_input_callback_(NULL),
      user_data_(NULL) {
  if (has_interface<PPB_AudioInput_Dev_0_2>()) {
    PassRefFromConstructor(get_interface<PPB_AudioInput_Dev_0_2>()->Create(
        instance.pp_instance()));
  }
}

AudioInput_Dev::AudioInput_Dev(const AudioInput_Dev& other)
    : Resource(other),
      config_(other.config_),
      enum_state_(NULL),
      audio_input_callback_(other.audio_input_callback_),
      user_data_(other.user_data_) {
}

AudioInput_Dev::~AudioInput_Dev() {
  AbortEnumerateDevices();
}

AudioInput_Dev& AudioInput_Dev::operator=(const AudioInput_Dev& other) {
  AbortEnumerateDevices();

  Resource::operator=(other);
  config_ = other.config_;
  audio_input_callback_ = other.audio_input_callback_;
  user_data_ = other.user_data_;
  return *this;
}

// static
bool AudioInput_Dev::IsAvailable() {
  return has_interface<PPB_AudioInput_Dev_0_2>() ||
         has_interface<PPB_AudioInput_Dev_0_1>();
}

int32_t AudioInput_Dev::EnumerateDevices(std::vector<DeviceRef_Dev>* devices,
                                         const CompletionCallback& callback) {
  if (!has_interface<PPB_AudioInput_Dev_0_2>())
    return callback.MayForce(PP_ERROR_NOINTERFACE);
  if (!devices)
    return callback.MayForce(PP_ERROR_BADARGUMENT);
  if (!callback.pp_completion_callback().func)
    return callback.MayForce(PP_ERROR_BLOCKS_MAIN_THREAD);
  if (enum_state_)
    return callback.MayForce(PP_ERROR_INPROGRESS);

  // It will be deleted in OnEnumerateDevicesComplete().
  enum_state_ = new EnumerateDevicesState(devices, callback, this);
  return get_interface<PPB_AudioInput_Dev_0_2>()->EnumerateDevices(
      pp_resource(), &enum_state_->devices_resource,
      PP_MakeCompletionCallback(&AudioInput_Dev::OnEnumerateDevicesComplete,
                                enum_state_));
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

void AudioInput_Dev::AbortEnumerateDevices() {
  if (enum_state_) {
    enum_state_->devices = NULL;
    Module::Get()->core()->CallOnMainThread(0, enum_state_->callback,
                                            PP_ERROR_ABORTED);
    enum_state_->audio_input = NULL;
    enum_state_ = NULL;
  }
}

// static
void AudioInput_Dev::OnEnumerateDevicesComplete(void* user_data,
                                                int32_t result) {
  EnumerateDevicesState* enum_state =
      static_cast<EnumerateDevicesState*>(user_data);

  bool need_to_callback = !!enum_state->audio_input;

  if (result == PP_OK) {
    // It will take care of releasing the reference.
    ResourceArray_Dev resources(pp::PASS_REF,
                                enum_state->devices_resource);

    if (need_to_callback) {
      enum_state->devices->clear();
      for (uint32_t index = 0; index < resources.size(); ++index) {
        DeviceRef_Dev device(resources[index]);
        enum_state->devices->push_back(device);
      }
    }
  }

  if (need_to_callback) {
    enum_state->audio_input->enum_state_ = NULL;
    enum_state->callback.Run(result);
  }

  delete enum_state;
}

}  // namespace pp
