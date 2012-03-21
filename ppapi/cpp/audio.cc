// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/audio.h"

#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_Audio_1_0>() {
  return PPB_AUDIO_INTERFACE_1_0;
}

}  // namespace

Audio::Audio(const InstanceHandle& instance,
             const AudioConfig& config,
             PPB_Audio_Callback callback,
             void* user_data)
    : config_(config) {
  if (has_interface<PPB_Audio_1_0>()) {
    PassRefFromConstructor(get_interface<PPB_Audio_1_0>()->Create(
        instance.pp_instance(), config.pp_resource(), callback, user_data));
  }
}

bool Audio::StartPlayback() {
  return has_interface<PPB_Audio_1_0>() &&
      get_interface<PPB_Audio_1_0>()->StartPlayback(pp_resource());
}

bool Audio::StopPlayback() {
  return has_interface<PPB_Audio_1_0>() &&
      get_interface<PPB_Audio_1_0>()->StopPlayback(pp_resource());
}

}  // namespace pp
