// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_MATERIAL_DESIGN_MATERIAL_DESIGN_CONTROLLER_H_
#define UI_BASE_MATERIAL_DESIGN_MATERIAL_DESIGN_CONTROLLER_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "ui/base/ui_base_export.h"

namespace base {
template <typename T>
class NoDestructor;
}

namespace ui {

class MaterialDesignControllerObserver;

namespace test {
class MaterialDesignControllerTestAPI;
}  // namespace test

// Central controller to handle material design modes.
class UI_BASE_EXPORT MaterialDesignController {
 public:
  // The different material design modes.
  enum Mode {
    // Material Refresh design targeted at mouse devices.
    MATERIAL_REFRESH,
    // Material Refresh design optimized for touch devices.
    MATERIAL_TOUCH_REFRESH,
  };

  // Initializes |mode_|. Must be called before checking |mode_|.
  static void Initialize();

  // Returns true if the touch-optimized UI material design mode is enabled.
  static bool IsTouchOptimizedUiEnabled();

  // Exposed for TabletModeClient on ChromeOS + ash.
  static void OnTabletModeToggled(bool enabled);

  static bool is_mode_initialized() { return is_mode_initialized_; }

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

  // Set |mode_| to |mode| and updates |is_mode_initialized_| to true. Can be
  // used by tests to directly set the mode.
  static void SetMode(Mode mode);

  // Tracks whether |mode_| has been initialized. This is necessary to avoid
  // checking the |mode_| early in initialization before a call to Initialize().
  // Tests can use it to reset the state back to a clean state during tear down.
  static bool is_mode_initialized_;

  // The current Mode to be used by the system.
  static Mode mode_;

  // Whether |mode_| should toggle between MATERIAL_REFRESH and
  // MATERIAL_TOUCH_REFRESH depending on the tablet state.
  static bool is_refresh_dynamic_ui_;

  base::ObserverList<MaterialDesignControllerObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(MaterialDesignController);
};

}  // namespace ui

#endif  // UI_BASE_MATERIAL_DESIGN_MATERIAL_DESIGN_CONTROLLER_H_
