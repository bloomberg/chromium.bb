// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_RESOURCE_MATERIAL_DESIGN_MATERIAL_DESIGN_CONTROLLER_H_
#define UI_BASE_RESOURCE_MATERIAL_DESIGN_MATERIAL_DESIGN_CONTROLLER_H_

#include "base/macros.h"
#include "ui/base/ui_base_export.h"

namespace ui {

namespace test {
class MaterialDesignControllerTestAPI;
}  // namespace test

// Central controller to handle material design modes.
class UI_BASE_EXPORT MaterialDesignController {
 public:
  // The different material design modes.
  enum class Mode {
    // Basic material design.
    MATERIAL,
    // Material design targeted at mouse/touch hybrid devices.
    MATERIAL_HYBRID,
    // Classic, non-material design.
    NON_MATERIAL
  };

  // Get the current Mode that should be used by the system.
  static Mode GetMode();

 private:
  friend class test::MaterialDesignControllerTestAPI;

  // Tracks whether |mode_| has been initialized. This is necessary so tests can
  // reset the state back to a clean state during tear down.
  static bool is_mode_initialized_;

  // The current Mode to be used by the system.
  static Mode mode_;

  // Declarations only. Do not allow construction of an object.
  MaterialDesignController();
  ~MaterialDesignController();

  // Initializes |mode_|.
  static void InitializeMode();

  // Resets the Mode state to uninitialized. To be used by tests to cleanup
  // global state.
  static void UninitializeMode();

  // Set |mode_| to |mode| and updates |is_mode_initialized_| to true. Can be
  // used by tests to directly set the mode.
  static void SetMode(Mode mode);

  DISALLOW_COPY_AND_ASSIGN(MaterialDesignController);
};

}  // namespace ui

#endif  // UI_BASE_RESOURCE_MATERIAL_DESIGN_MATERIAL_DESIGN_CONTROLLER_H_
