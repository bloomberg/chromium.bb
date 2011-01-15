// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_AUDIO_H_
#define PPAPI_CPP_AUDIO_H_

#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/ppb_audio.h"
#include "ppapi/cpp/audio_config.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/resource.h"

namespace pp {

class Audio : public Resource {
 public:
  Audio() {}
  Audio(Instance* instance,
        const AudioConfig& config,
        PPB_Audio_Callback callback,
        void* user_data);

  AudioConfig& config() { return config_; }
  const AudioConfig& config() const { return config_; }

  bool StartPlayback();
  bool StopPlayback();

 private:
  AudioConfig config_;
};

}  // namespace pp

#endif  // PPAPI_CPP_AUDIO_H_

