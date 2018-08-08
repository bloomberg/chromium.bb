// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_SNOOPABLE_H_
#define SERVICES_AUDIO_SNOOPABLE_H_

#include <string>

#include "base/time/time.h"

namespace media {
class AudioBus;
class AudioParameters;
}  // namespace media

namespace audio {
class Snoopable {
 public:
  class Snooper {
   public:
    // Provides read-only access to the data flowing through a GroupMember.
    virtual void OnData(const media::AudioBus& audio_bus,
                        base::TimeTicks reference_time,
                        double volume) = 0;

   protected:
    virtual ~Snooper() = default;
  };

  enum class SnoopingMode {
    kDeferred,  // Deferred snooping is done on the audio thread.
    kRealtime   // Realtime snooping is done on the device thread. Must be fast!
  };

  // Returns the audio parameters of the snoopable audio data. The parameters
  // must not change for the lifetime of this group member, but can be different
  // than those of other members.
  virtual const media::AudioParameters& GetAudioParameters() const = 0;

  // Returns the id of the device the snoopable stream is connected to.
  virtual std::string GetDeviceId() const = 0;

  // Starts/Stops snooping on the audio data flowing through this group member.
  // The snooping modes are handled individually, so it's possible (though
  // inadvisable) to call StartSnooping twice with the same snooper, but with
  // different modes.
  virtual void StartSnooping(Snooper* snooper, SnoopingMode mode) = 0;
  virtual void StopSnooping(Snooper* snooper, SnoopingMode mode) = 0;

 protected:
  virtual ~Snoopable() = default;
};
}  // namespace audio

#endif  // SERVICES_AUDIO_SNOOPABLE_H_
