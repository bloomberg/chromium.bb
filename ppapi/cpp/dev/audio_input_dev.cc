// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/audio_input_dev.h"

#include "ppapi/c/dev/ppb_audio_input_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_AudioInput_Dev>() {
  return PPB_AUDIO_INPUT_DEV_INTERFACE;
}

}  // namespace

AudioInput_Dev::AudioInput_Dev(Instance* instance,
                               const AudioConfig& config,
                               PPB_AudioInput_Callback callback,
                               void* user_data)
    : config_(config) {
  if (has_interface<PPB_AudioInput_Dev>()) {
    PassRefFromConstructor(get_interface<PPB_AudioInput_Dev>()->Create(
        instance->pp_instance(), config.pp_resource(), callback, user_data));
  }
}

// static
bool AudioInput_Dev::IsAvailable() {
  return has_interface<PPB_AudioInput_Dev>();
}

bool AudioInput_Dev::StartCapture() {
  return has_interface<PPB_AudioInput_Dev>() &&
      get_interface<PPB_AudioInput_Dev>()->StartCapture(pp_resource());
}

bool AudioInput_Dev::StopCapture() {
  return has_interface<PPB_AudioInput_Dev>() &&
      get_interface<PPB_AudioInput_Dev>()->StopCapture(pp_resource());
}

}  // namespace pp
