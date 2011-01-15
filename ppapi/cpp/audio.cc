// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/audio.h"

#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_Audio>() {
  return PPB_AUDIO_INTERFACE;
}

}  // namespace

Audio::Audio(Instance* instance,
             const AudioConfig& config,
             PPB_Audio_Callback callback,
             void* user_data)
    : config_(config) {
  if (has_interface<PPB_Audio>()) {
    PassRefFromConstructor(get_interface<PPB_Audio>()->Create(
        instance->pp_instance(), config.pp_resource(), callback, user_data));
  }
}

bool Audio::StartPlayback() {
  return has_interface<PPB_Audio>() &&
      get_interface<PPB_Audio>()->StartPlayback(pp_resource());
}

bool Audio::StopPlayback() {
  return has_interface<PPB_Audio>() &&
      get_interface<PPB_Audio>()->StopPlayback(pp_resource());
}

}  // namespace pp

