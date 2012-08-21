// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SINGLE_DISPLAY_MANAGER_H_
#define UI_AURA_SINGLE_DISPLAY_MANAGER_H_

#include "base/compiler_specific.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/display_manager.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/display.h"

namespace gfx {
class Rect;
}

namespace aura {

// A display manager assuming there is one display.
class AURA_EXPORT SingleDisplayManager : public DisplayManager,
                                         public WindowObserver {
 public:
  SingleDisplayManager();
  virtual ~SingleDisplayManager();

  // DisplayManager overrides:
  virtual void OnNativeDisplaysChanged(
      const std::vector<gfx::Display>& display) OVERRIDE;
  virtual RootWindow* CreateRootWindowForDisplay(
      const gfx::Display& display) OVERRIDE;
  virtual gfx::Display* GetDisplayAt(size_t index) OVERRIDE;

  virtual size_t GetNumDisplays() const OVERRIDE;

  virtual const gfx::Display& GetDisplayNearestWindow(
      const Window* window) const OVERRIDE;
  virtual const gfx::Display& GetDisplayNearestPoint(
      const gfx::Point& point) const OVERRIDE;
  virtual const gfx::Display& GetDisplayMatching(
      const gfx::Rect& match_rect) const OVERRIDE;
  virtual std::string GetDisplayNameAt(size_t index) OVERRIDE;

  // WindowObserver overrides:
  virtual void OnWindowBoundsChanged(Window* window,
                                     const gfx::Rect& old_bounds,
                                     const gfx::Rect& new_bounds) OVERRIDE;
  virtual void OnWindowDestroying(Window* window) OVERRIDE;

 private:
  void Init();
  void Update(const gfx::Size size);

  RootWindow* root_window_;
  gfx::Display display_;

  DISALLOW_COPY_AND_ASSIGN(SingleDisplayManager);
};

}  // namespace aura

#endif  // UI_AURA_SINGLE_DISPLAY_MANAGER_H_
