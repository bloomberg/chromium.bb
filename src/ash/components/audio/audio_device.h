// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_AUDIO_AUDIO_DEVICE_H_
#define ASH_COMPONENTS_AUDIO_AUDIO_DEVICE_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "chromeos/dbus/audio/audio_node.h"

namespace ash {

// Ordered from the highest priority to the lowest.
enum class AudioDeviceType {
  kHeadphone,
  kMic,
  kUsb,
  kBluetooth,
  kBluetoothNbMic,
  kHdmi,
  kInternalSpeaker,
  kInternalMic,
  kFrontMic,
  kRearMic,
  kKeyboardMic,
  kHotword,
  kLineout,
  kPostMixLoopback,
  kPostDspLoopback,
  kAlsaLoopback,
  kOther,
};

struct COMPONENT_EXPORT(ASH_COMPONENTS_AUDIO) AudioDevice {
  AudioDevice();
  explicit AudioDevice(const chromeos::AudioNode& node);
  AudioDevice(const AudioDevice& other);
  std::string ToString() const;

  // Converts between the string type sent via D-Bus and AudioDeviceType.
  // Static so they can be used by tests.
  static std::string GetTypeString(AudioDeviceType type);
  static AudioDeviceType GetAudioType(const std::string& node_type);

  // Indicates that an input or output audio device is for simple usage like
  // playback or recording for user. In contrast, audio device such as
  // loopback, always on keyword recognition (HOTWORD), and keyboard mic are
  // not for simple usage.
  // One special case is ALSA loopback device, which will only exist under
  // testing, and we want it visible to users for e2e tests.
  bool is_for_simple_usage() const {
    return (type == AudioDeviceType::kHeadphone ||
            type == AudioDeviceType::kInternalMic ||
            type == AudioDeviceType::kFrontMic ||
            type == AudioDeviceType::kRearMic ||
            type == AudioDeviceType::kMic || type == AudioDeviceType::kUsb ||
            type == AudioDeviceType::kBluetooth ||
            type == AudioDeviceType::kBluetoothNbMic ||
            type == AudioDeviceType::kHdmi ||
            type == AudioDeviceType::kInternalSpeaker ||
            type == AudioDeviceType::kLineout ||
            type == AudioDeviceType::kAlsaLoopback);
  }

  bool IsExternalDevice() const;

  bool IsInternalMic() const;

  bool is_input = false;

  // Id of this audio device. The legacy |id| is assigned to be unique everytime
  // when each device got plugged, so that the same physical device will have
  // a different id after unplug then re-plug.
  // The |stable_device_id| is designed to be persistent across system reboot
  // and plug/unplug for the same physical device. It is guaranteed that
  // different type of hardware has different |stable_device_id|, but not
  // guaranteed to be different between the same kind of audio device, e.g
  // USB headset. |id| and |stable_device_id| can be used together to achieve
  // various goals.
  // Note that because algorithm used to determine |stable_device_id| changed in
  // system code, |stable_device_id_version| and |deprecated_stable_device_id|
  // have been introduced - to ensure backward compatibility until persisted
  // references to stable device ID have been updated where needed.
  // |stable_device_id_version| is the version of stable device ID set in
  // |stable_device_id|. If version is set to 2, |deprecated_stable_device_id|
  // will contain deprecated, v1 stable device id version.
  uint64_t id = 0;
  int stable_device_id_version = 0;
  uint64_t stable_device_id = 0;
  uint64_t deprecated_stable_device_id = 0;
  std::string display_name;
  std::string device_name;
  AudioDeviceType type = AudioDeviceType::kOther;
  uint8_t priority = 0;
  bool active = false;
  uint64_t plugged_time = 0;
  uint32_t max_supported_channels = 0;
};

typedef std::vector<AudioDevice> AudioDeviceList;
typedef std::map<uint64_t, AudioDevice> AudioDeviceMap;

struct AudioDeviceCompare {
  // Rules used to discern which device is higher,
  // 1.) Device Type:
  //       [Headphones/USB/Bluetooh > HDMI > Internal Speakers]
  //       [External Mic/USB Mic/Bluetooth > Internal Mic]
  // 2.) Device Plugged in Time:
  //       [Later > Earlier]
  bool operator()(const AudioDevice& a, const AudioDevice& b) const {
    if (a.priority < b.priority) {
      return true;
    } else if (b.priority < a.priority) {
      return false;
    } else if (a.plugged_time < b.plugged_time) {
      return true;
    } else {
      return false;
    }
  }
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the Chrome OS source code
// directory migration is finished.
namespace chromeos {
using ::ash::AudioDevice;
using ::ash::AudioDeviceList;
using ::ash::AudioDeviceType;
}  // namespace chromeos

#endif  // ASH_COMPONENTS_AUDIO_AUDIO_DEVICE_H_
