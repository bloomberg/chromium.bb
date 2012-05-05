// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_COMPOSITOR_OBSERVER_H_
#define UI_COMPOSITOR_COMPOSITOR_OBSERVER_H_
#pragma once

#include "ui/compositor/compositor_export.h"

namespace ui {

class Compositor;

// A compositor observer is notified when compositing completes.
class COMPOSITOR_EXPORT CompositorObserver {
 public:
  // Called when compositing started: it has taken all the layer changes into
  // account and has issued the graphics commands.
  virtual void OnCompositingStarted(Compositor* compositor) = 0;

  // Called when compositing completes: the present to screen has completed.
  virtual void OnCompositingEnded(Compositor* compositor) = 0;

 protected:
  virtual ~CompositorObserver() {}
};

} // namespace ui

#endif  // UI_COMPOSITOR_COMPOSITOR_OBSERVER_H_
