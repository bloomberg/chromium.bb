// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_MATERIAL_DESIGN_MATERIAL_DESIGN_CONTROLLER_H_
#define UI_BASE_MATERIAL_DESIGN_MATERIAL_DESIGN_CONTROLLER_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "ui/base/ui_base_export.h"

namespace base {
template <typename T>
class NoDestructor;
}

namespace gfx {
class SingletonHwndObserver;
}

namespace ui {

class MaterialDesignControllerObserver;

namespace test {
class MaterialDesignControllerTestAPI;
}  // namespace test

// Central controller to handle material design modes.
class UI_BASE_EXPORT MaterialDesignController {
 public:
  // Initializes touch UI state based on command-line flags.
  static void Initialize();

  // Returns true if the touch-optimized UI material design mode is enabled.
  static bool IsTouchOptimizedUiEnabled();

  // Exposed for TabletModeClient on ChromeOS + ash.
  static void OnTabletModeToggled(bool enabled);

  static MaterialDesignController* GetInstance();

  void AddObserver(MaterialDesignControllerObserver* observer);
  void RemoveObserver(MaterialDesignControllerObserver* observer);

 private:
  friend class base::NoDestructor<MaterialDesignController>;
  friend class test::MaterialDesignControllerTestAPI;

  MaterialDesignController();
  ~MaterialDesignController() = delete;

  // Resets the initialization state to uninitialized. To be used by tests to
  // allow calling Initialize() more than once.
  static void Uninitialize();

  // Sets the touch UI state and notifies observers of the state change.
  static void SetTouchUi(bool touch_ui);

  // Tracks whether |touch_ui_| has been initialized.
  // Tests can use it to reset the state back to a clean state during tear down.
  static bool initialized_;

  // Whether the UI layout should be touch-optimized.
  static bool touch_ui_;

  // Whether |touch_ui_| should toggle on and off depending on the tablet state.
  static bool automatic_touch_ui_;

#if defined(OS_WIN)
  std::unique_ptr<gfx::SingletonHwndObserver> singleton_hwnd_observer_;
#endif

  base::ObserverList<MaterialDesignControllerObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(MaterialDesignController);
};

}  // namespace ui

#endif  // UI_BASE_MATERIAL_DESIGN_MATERIAL_DESIGN_CONTROLLER_H_
