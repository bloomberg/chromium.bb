// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_AUDIO_INPUT_DEV_H_
#define PPAPI_CPP_DEV_AUDIO_INPUT_DEV_H_

#include <vector>

#include "ppapi/c/dev/ppb_audio_input_dev.h"
#include "ppapi/cpp/audio_config.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/dev/device_ref_dev.h"
#include "ppapi/cpp/resource.h"

namespace pp {

class InstanceHandle;

class AudioInput_Dev : public Resource {
 public:
  /// An empty constructor for an AudioInput resource.
  AudioInput_Dev();

  /// This constructor tries to create an audio input resource using the v0.2
  /// interface, and falls back on the v0.1 interface if that is not available.
  /// Please use the 2-parameter Open() if you used this constructor.
  ///
  /// Note: This constructor is deprecated. Unless your code has to deal with
  /// browsers that only support the v0.1 interface, please use the 1-parameter
  /// constructor instead.
  AudioInput_Dev(const InstanceHandle& instance,
                 const AudioConfig& config,
                 PPB_AudioInput_Callback callback,
                 void* user_data);

  /// This constructor uses the v0.2 interface to create an audio input
  /// resource. Please use the 5-parameter Open() if you used this constructor.
  explicit AudioInput_Dev(const InstanceHandle& instance);

  virtual ~AudioInput_Dev();

  /// Static function for determining whether the browser supports the required
  /// AudioInput interface.
  ///
  /// @return true if the interface is available, false otherwise.
  static bool IsAvailable();

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

  int32_t EnumerateDevices(
      const CompletionCallbackWithOutput<std::vector<DeviceRef_Dev> >&
          callback);

  /// If |device_ref| is null (i.e., is_null() returns true), the default device
  /// will be used.
  /// In order to maintain backward compatibility, this method doesn't have
  /// input parameters config, audio_input_callback and user_data. Instead, it
  /// uses those values stored when the 4-parameter constructor was called.
  ///
  /// Note: This method is deprecated. Unless your code has to deal with
  /// browsers that only support the v0.1 interface, please use the other
  /// Open().
  int32_t Open(const DeviceRef_Dev& device_ref,
               const CompletionCallback& callback);

  int32_t Open(const DeviceRef_Dev& device_ref,
               const AudioConfig& config,
               PPB_AudioInput_Callback audio_input_callback,
               void* user_data,
               const CompletionCallback& callback);

  bool StartCapture();
  bool StopCapture();
  void Close();

 private:
  AudioConfig config_;

  // Used to store the arguments of Open() for the v0.2 interface.
  PPB_AudioInput_Callback audio_input_callback_;
  void* user_data_;
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_AUDIO_INPUT_DEV_H_
