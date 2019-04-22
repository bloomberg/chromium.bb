// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_MPRIS_MPRIS_SERVICE_OBSERVER_H_
#define UI_BASE_MPRIS_MPRIS_SERVICE_OBSERVER_H_

#include "base/component_export.h"
#include "base/observer_list_types.h"

namespace mpris {

// Interface to observe events on the MprisService.
class COMPONENT_EXPORT(MPRIS) MprisServiceObserver
    : public base::CheckedObserver {
 public:
  // Called when the service has completed setting up.
  virtual void OnServiceReady() {}

  // Called when the associated method from the org.mpris.MediaPlayer2.Player
  // interface is called.
  virtual void OnNext() {}
  virtual void OnPrevious() {}
  virtual void OnPause() {}
  virtual void OnPlayPause() {}
  virtual void OnStop() {}
  virtual void OnPlay() {}
};

}  // namespace mpris

#endif  // UI_BASE_MPRIS_MPRIS_SERVICE_OBSERVER_H_
