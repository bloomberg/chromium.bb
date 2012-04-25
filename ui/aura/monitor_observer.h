// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_MONITOR_OBSERVER_H_
#define UI_AURA_MONITOR_OBSERVER_H_
#pragma once

#include "ui/aura/aura_export.h"

namespace gfx {
class Monitor;
}

namespace aura {

// Observers for monitor configuration changes.
class AURA_EXPORT MonitorObserver {
 public:
  // Called when the |monitor|'s bound has changed.
  virtual void OnMonitorBoundsChanged(const gfx::Monitor& monitor) = 0;

  // Called when |new_monitor| has been added.
  virtual void OnMonitorAdded(const gfx::Monitor& new_monitor) = 0;

  // Called when |old_monitor| has been removed.
  virtual void OnMonitorRemoved(const gfx::Monitor& old_monitor) = 0;

 protected:
  virtual ~MonitorObserver();
};

}  // namespace aura

#endif  // UI_AURA_MONITOR_OBSERVER_H_
