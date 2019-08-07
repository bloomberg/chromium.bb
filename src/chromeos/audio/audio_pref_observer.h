// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_AUDIO_AUDIO_PREF_OBSERVER_H_
#define CHROMEOS_AUDIO_AUDIO_PREF_OBSERVER_H_

#include "base/component_export.h"
#include "base/memory/ref_counted.h"

namespace chromeos {

// Interface for observing audio preference changes.
class COMPONENT_EXPORT(CHROMEOS_AUDIO) AudioPrefObserver {
 public:
  // Called when audio policy prefs changed.
  virtual void OnAudioPolicyPrefChanged() = 0;

 protected:
  virtual ~AudioPrefObserver() {}
};

}  // namespace chromeos

#endif  // CHROMEOS_AUDIO_AUDIO_PREF_OBSERVER_H_
