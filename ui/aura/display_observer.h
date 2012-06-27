// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_DISPLAY_OBSERVER_H_
#define UI_AURA_DISPLAY_OBSERVER_H_
#pragma once

#include "ui/aura/aura_export.h"

namespace gfx {
class Display;
}

namespace aura {

// Observers for display configuration changes.
class AURA_EXPORT DisplayObserver {
 public:
  // Called when the |display|'s bound has changed.
  virtual void OnDisplayBoundsChanged(const gfx::Display& display) = 0;

  // Called when |new_display| has been added.
  virtual void OnDisplayAdded(const gfx::Display& new_display) = 0;

  // Called when |old_display| has been removed.
  virtual void OnDisplayRemoved(const gfx::Display& old_display) = 0;

 protected:
  virtual ~DisplayObserver();
};

}  // namespace aura

#endif  // UI_AURA_DISPLAY_OBSERVER_H_
