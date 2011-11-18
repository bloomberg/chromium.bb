// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_AUDIO_INPUT_DEV_H_
#define PPAPI_CPP_DEV_AUDIO_INPUT_DEV_H_

#include "ppapi/c/dev/ppb_audio_input_dev.h"
#include "ppapi/cpp/audio_config.h"
#include "ppapi/cpp/resource.h"

namespace pp {

class Instance;

class AudioInput_Dev : public Resource {
 public:
  /// An empty constructor for an AudioInput resource.
  AudioInput_Dev() {}

  AudioInput_Dev(Instance* instance,
                 const AudioConfig& config,
                 PPB_AudioInput_Callback callback,
                 void* user_data);

  /// Getter function for returning the internal <code>PPB_AudioConfig</code>
  /// struct.
  ///
  /// @return A mutable reference to the PPB_AudioConfig struct.
  AudioConfig& config() { return config_; }

  /// Getter function for returning the internal <code>PPB_AudioConfig</code>
  /// struct.
  ///
  /// @return A const reference to the internal <code>PPB_AudioConfig</code>
  /// struct.
  const AudioConfig& config() const { return config_; }

  bool StartCapture();
  bool StopCapture();

 private:
  AudioConfig config_;
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_AUDIO_INPUT_DEV_H_
