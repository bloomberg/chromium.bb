// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_COMPOSITOR_COMPOSITOR_OBSERVER_H_
#define UI_GFX_COMPOSITOR_COMPOSITOR_OBSERVER_H_
#pragma once

namespace ui {

class Compositor;

// A compositor observer is notified when compositing completes.
class CompositorObserver {
 public:
  // Called when compositing completes.
  virtual void OnCompositingEnded(Compositor* compositor) = 0;

 protected:
  virtual ~CompositorObserver() {}
};

} // namespace ui

#endif // UI_GFX_COMPOSITOR_COMPOSITOR_OBSERVER_H_
