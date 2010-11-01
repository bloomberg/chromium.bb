// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_AUDIO_DEV_H_
#define PPAPI_CPP_DEV_AUDIO_DEV_H_

#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/dev/ppb_audio_dev.h"
#include "ppapi/cpp/dev/audio_config_dev.h"
#include "ppapi/cpp/dev/buffer_dev.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/resource.h"

namespace pp {

class Audio_Dev : public Resource {
 public:
  Audio_Dev() {}
  Audio_Dev(const Instance& instance,
            const AudioConfig_Dev& config,
            PPB_Audio_Callback callback,
            void* user_data);

  AudioConfig_Dev& config() { return config_; }
  const AudioConfig_Dev& config() const { return config_; }

  bool StartPlayback();
  bool StopPlayback();

 private:
  AudioConfig_Dev config_;
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_AUDIO_DEV_H_

