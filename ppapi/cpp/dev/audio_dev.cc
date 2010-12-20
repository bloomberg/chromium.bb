// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/audio_dev.h"

#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_Audio_Dev>() {
  return PPB_AUDIO_DEV_INTERFACE;
}

}  // namespace

Audio_Dev::Audio_Dev(const Instance& instance,
                     const AudioConfig_Dev& config,
                     PPB_Audio_Callback callback,
                     void* user_data)
    : config_(config) {
  if (has_interface<PPB_Audio_Dev>()) {
    PassRefFromConstructor(get_interface<PPB_Audio_Dev>()->Create(
        instance.pp_instance(), config.pp_resource(), callback, user_data));
  }
}

bool Audio_Dev::StartPlayback() {
  return has_interface<PPB_Audio_Dev>() &&
      get_interface<PPB_Audio_Dev>()->StartPlayback(pp_resource());
}

bool Audio_Dev::StopPlayback() {
  return has_interface<PPB_Audio_Dev>() &&
      get_interface<PPB_Audio_Dev>()->StopPlayback(pp_resource());
}

}  // namespace pp

