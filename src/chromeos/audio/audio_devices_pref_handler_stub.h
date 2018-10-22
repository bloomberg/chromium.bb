// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_AUDIO_AUDIO_DEVICES_PREF_HANDLER_STUB_H_
#define CHROMEOS_AUDIO_AUDIO_DEVICES_PREF_HANDLER_STUB_H_

#include <stdint.h>

#include <map>

#include "base/macros.h"
#include "chromeos/audio/audio_devices_pref_handler.h"

namespace chromeos {

// Stub class for AudioDevicesPrefHandler, used for testing.
class CHROMEOS_EXPORT AudioDevicesPrefHandlerStub
    : public AudioDevicesPrefHandler {
 public:
  struct DeviceState {
    bool active;
    bool activate_by_user;
  };

  using AudioDeviceMute = std::map<uint64_t, bool>;
  using AudioDeviceVolumeGain = std::map<uint64_t, int>;
  using AudioDeviceStateMap = std::map<uint64_t, DeviceState>;

  AudioDevicesPrefHandlerStub();

  // AudioDevicesPrefHandler:
  double GetOutputVolumeValue(const AudioDevice* device) override;
  double GetInputGainValue(const AudioDevice* device) override;
  void SetVolumeGainValue(const AudioDevice& device, double value) override;
  bool GetMuteValue(const AudioDevice& device) override;
  void SetMuteValue(const AudioDevice& device, bool mute_on) override;
  void SetDeviceActive(const AudioDevice& device,
                       bool active,
                       bool activate_by_user) override;
  bool GetDeviceActive(const AudioDevice& device,
                       bool* active,
                       bool* activate_by_user) override;
  bool GetAudioOutputAllowedValue() override;
  void AddAudioPrefObserver(AudioPrefObserver* observer) override;
  void RemoveAudioPrefObserver(AudioPrefObserver* observer) override;

 protected:
  ~AudioDevicesPrefHandlerStub() override;

 private:
  AudioDeviceMute audio_device_mute_map_;
  AudioDeviceVolumeGain audio_device_volume_gain_map_;
  AudioDeviceStateMap audio_device_state_map_;

  DISALLOW_COPY_AND_ASSIGN(AudioDevicesPrefHandlerStub);
};

}  // namespace chromeos

#endif  // CHROMEOS_AUDIO_AUDIO_DEVICES_PREF_HANDLER_STUB_H_
