// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_CORE_SHADOW_CONTROLLER_H_
#define UI_WM_CORE_SHADOW_CONTROLLER_H_

#include <map>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "ui/wm/core/shadow_types.h"
#include "ui/wm/core/wm_core_export.h"
#include "ui/wm/public/activation_change_observer.h"

namespace aura {
class Window;
}

namespace wm {

class ActivationClient;
class Shadow;

// ShadowController observes changes to windows and creates and updates drop
// shadows as needed. ShadowController itself is light weight and per
// ActivationClient. ShadowController delegates to its implementation class,
// which observes all window creation.
class WM_CORE_EXPORT ShadowController : public ActivationChangeObserver {
 public:
  static constexpr ShadowElevation kActiveNormalShadowElevation =
      ShadowElevation::LARGE;

  // Returns the shadow for the |window|, or NULL if no shadow exists.
  static Shadow* GetShadowForWindow(aura::Window* window);

  explicit ShadowController(ActivationClient* activation_client);
  ~ShadowController() override;

  // ActivationChangeObserver overrides:
  void OnWindowActivated(ActivationChangeObserver::ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

 private:
  class Impl;

  ActivationClient* activation_client_;

  scoped_refptr<Impl> impl_;

  DISALLOW_COPY_AND_ASSIGN(ShadowController);
};

}  // namespace wm

#endif  // UI_WM_CORE_SHADOW_CONTROLLER_H_
