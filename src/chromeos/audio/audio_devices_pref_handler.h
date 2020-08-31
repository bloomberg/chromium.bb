// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_AUDIO_AUDIO_DEVICES_PREF_HANDLER_H_
#define CHROMEOS_AUDIO_AUDIO_DEVICES_PREF_HANDLER_H_

#include "base/component_export.h"
#include "base/memory/ref_counted.h"
#include "chromeos/audio/audio_pref_observer.h"

namespace chromeos {

struct AudioDevice;

// Interface that handles audio preference related work, reads and writes
// audio preferences, and notifies AudioPrefObserver for audio preference
// changes.
class COMPONENT_EXPORT(CHROMEOS_AUDIO) AudioDevicesPrefHandler
    : public base::RefCountedThreadSafe<AudioDevicesPrefHandler> {
 public:
  // Integer because C++ does not allow static const double in header files.
  static const int kDefaultInputGainPercent = 50;
  static const int kDefaultOutputVolumePercent = 75;
  static const int kDefaultHdmiOutputVolumePercent = 100;

  // Gets the audio output volume value from prefs for a device. Since we can
  // only have either a gain or a volume for a device (depending on whether it
  // is input or output), we don't really care which value it is.
  virtual double GetOutputVolumeValue(const AudioDevice* device) = 0;
  virtual double GetInputGainValue(const AudioDevice* device) = 0;
  // Sets the audio volume or gain value to prefs for a device.
  virtual void SetVolumeGainValue(const AudioDevice& device, double value) = 0;

  // Reads the audio mute value from prefs for a device.
  virtual bool GetMuteValue(const AudioDevice& device) = 0;
  // Sets the audio mute value to prefs for a device.
  virtual void SetMuteValue(const AudioDevice& device, bool mute_on) = 0;

  // Sets the device active state in prefs.
  // Note: |activate_by_user| indicates whether |device| is set to active
  // by user or by priority, and it only matters when |active| is true.
  virtual void SetDeviceActive(const AudioDevice& device,
                               bool active,
                               bool activate_by_user) = 0;
  // Returns false if it fails to get device active state from prefs.
  // Otherwise, returns true, pass the active state data in |*active|
  // and |*activate_by_user|.
  // Note: |*activate_by_user| only matters when |*active| is true.
  virtual bool GetDeviceActive(const AudioDevice& device,
                               bool* active,
                               bool* activate_by_user) = 0;

  // Reads the audio output allowed value from prefs.
  virtual bool GetAudioOutputAllowedValue() = 0;

  // Adds an audio preference observer.
  virtual void AddAudioPrefObserver(AudioPrefObserver* observer) = 0;
  // Removes an audio preference observer.
  virtual void RemoveAudioPrefObserver(AudioPrefObserver* observer) = 0;

 protected:
  virtual ~AudioDevicesPrefHandler() {}

 private:
  friend class base::RefCountedThreadSafe<AudioDevicesPrefHandler>;
};

}  // namespace chromeos

#endif  // CHROMEOS_AUDIO_AUDIO_DEVICES_PREF_HANDLER_H_
