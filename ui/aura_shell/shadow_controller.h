// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_SHADOW_CONTROLLER_H_
#define UI_AURA_SHELL_SHADOW_CONTROLLER_H_
#pragma once

#include <map>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/linked_ptr.h"
#include "ui/aura/desktop_observer.h"
#include "ui/aura/window_observer.h"

namespace aura {
class Window;
}
namespace gfx {
class Rect;
}

namespace aura_shell {
namespace internal {

class Shadow;

// ShadowController observes changes to windows and creates and updates drop
// shadows as needed.
class ShadowController : public aura::DesktopObserver,
                         public aura::WindowObserver {
public:
  class TestApi {
   public:
    explicit TestApi(ShadowController* controller) : controller_(controller) {}
    ~TestApi() {}

    Shadow* GetShadowForWindow(aura::Window* window) {
      return controller_->GetShadowForWindow(window);
    }

   private:
    ShadowController* controller_;  // not owned

    DISALLOW_COPY_AND_ASSIGN(TestApi);
  };

  explicit ShadowController();
  virtual ~ShadowController();

  // aura::DesktopObserver override:
  virtual void OnWindowInitialized(aura::Window* window) OVERRIDE;

  // aura::WindowObserver overrides:
  virtual void OnWindowParentChanged(
      aura::Window* window, aura::Window* parent) OVERRIDE;
  virtual void OnPropertyChanged(
      aura::Window* window, const char* name, void* old) OVERRIDE;
  virtual void OnWindowVisibilityChanged(
      aura::Window* window, bool visible) OVERRIDE;
  virtual void OnWindowBoundsChanged(
      aura::Window* window, const gfx::Rect& bounds) OVERRIDE;
  virtual void OnWindowStackingChanged(aura::Window* window) OVERRIDE;
  virtual void OnWindowDestroyed(aura::Window* window) OVERRIDE;

 private:
  typedef std::map<aura::Window*, linked_ptr<Shadow> > WindowShadowMap;

  // Checks if |window| is visible and contains a property requesting a shadow.
  bool ShouldShowShadowForWindow(aura::Window* window) const;

  // Returns |window|'s shadow from |window_shadows_|, or NULL if no shadow
  // exists.
  Shadow* GetShadowForWindow(aura::Window* window);

  // Shows or hides |window|'s shadow as needed (creating the shadow if
  // necessary).
  void HandlePossibleShadowVisibilityChange(aura::Window* window);

  // Creates a new shadow for |window| and stores it in |window_shadows_|.  The
  // shadow's visibility, bounds, and stacking are initialized appropriately.
  void CreateShadowForWindow(aura::Window* window);

  // Stacks |shadow|'s layer directly beneath |window|'s layer.
  void StackShadowBelowWindow(Shadow* shadow, aura::Window* window);

  WindowShadowMap window_shadows_;

  DISALLOW_COPY_AND_ASSIGN(ShadowController);
};

}  // namepsace aura_shell
}  // namepsace internal

#endif  // UI_AURA_SHELL_SHADOW_CONTROLLER_H_
