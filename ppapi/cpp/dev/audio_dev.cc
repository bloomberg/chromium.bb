// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/audio_dev.h"

#include "ppapi/cpp/module_impl.h"

namespace {

DeviceFuncs<PPB_Audio_Dev> audio_f(PPB_AUDIO_DEV_INTERFACE);

}  // namespace

namespace pp {

Audio_Dev::Audio_Dev(const Instance& instance,
                     const AudioConfig_Dev& config,
                     PPB_Audio_Callback callback,
                     void* user_data)
    : config_(config) {
  if (audio_f) {
    PassRefFromConstructor(audio_f->Create(instance.pp_instance(),
                                           config.pp_resource(),
                                           callback, user_data));
  }
}

bool Audio_Dev::StartPlayback() {
  return audio_f && audio_f->StartPlayback(pp_resource());
}

bool Audio_Dev::StopPlayback() {
  return audio_f && audio_f->StopPlayback(pp_resource());
}

}  // namespace pp

