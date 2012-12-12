// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COREWM_SHADOW_CONTROLLER_H_
#define UI_VIEWS_COREWM_SHADOW_CONTROLLER_H_

#include <map>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/linked_ptr.h"
#include "base/scoped_observer.h"
#include "ui/aura/client/activation_change_observer.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window_observer.h"
#include "ui/views/views_export.h"

namespace aura {
class RootWindow;
class Window;
}
namespace gfx {
class Rect;
}

namespace views {
namespace corewm {

class Shadow;

// ShadowController observes changes to windows and creates and updates drop
// shadows as needed.
class VIEWS_EXPORT ShadowController :
    public aura::EnvObserver,
    public aura::WindowObserver,
    public aura::client::ActivationChangeObserver {
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

  explicit ShadowController(aura::RootWindow* root_window);
  virtual ~ShadowController();

  // aura::EnvObserver override:
  virtual void OnWindowInitialized(aura::Window* window) OVERRIDE;

  // aura::WindowObserver overrides:
  virtual void OnWindowPropertyChanged(
      aura::Window* window, const void* key, intptr_t old) OVERRIDE;
  virtual void OnWindowBoundsChanged(
      aura::Window* window,
      const gfx::Rect& old_bounds,
      const gfx::Rect& new_bounds) OVERRIDE;
  virtual void OnWindowDestroyed(aura::Window* window) OVERRIDE;

  // aura::client::ActivationChangeObserver overrides:
  virtual void OnWindowActivated(aura::Window* gained_active,
                                 aura::Window* lost_active) OVERRIDE;

 private:
  typedef std::map<aura::Window*, linked_ptr<Shadow> > WindowShadowMap;

  // Checks if |window| is visible and contains a property requesting a shadow.
  bool ShouldShowShadowForWindow(aura::Window* window) const;

  // Returns |window|'s shadow from |window_shadows_|, or NULL if no shadow
  // exists.
  Shadow* GetShadowForWindow(aura::Window* window);

  // Updates the shadow styles for windows when activation changes.
  void HandleWindowActivationChange(aura::Window* gaining_active,
                                    aura::Window* losing_active);

  // Shows or hides |window|'s shadow as needed (creating the shadow if
  // necessary).
  void HandlePossibleShadowVisibilityChange(aura::Window* window);

  // Creates a new shadow for |window| and stores it in |window_shadows_|.  The
  // shadow's bounds are initialized and it is added to the window's layer.
  void CreateShadowForWindow(aura::Window* window);

  WindowShadowMap window_shadows_;

  ScopedObserver<aura::Window, aura::WindowObserver> observer_manager_;

  aura::RootWindow* root_window_;

  DISALLOW_COPY_AND_ASSIGN(ShadowController);
};

}  // namespace corewm
}  // namespace views

#endif  // UI_VIEWS_COREWM_SHADOW_CONTROLLER_H_
