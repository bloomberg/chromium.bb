// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_KEYBOARD_LAYOUT_ENGINE_MANAGER_H_
#define UI_OZONE_PUBLIC_KEYBOARD_LAYOUT_ENGINE_MANAGER_H_

#include <memory>

#include "base/component_export.h"
#include "base/logging.h"
#include "base/macros.h"

namespace ui {

class KeyboardLayoutEngine;

class COMPONENT_EXPORT(EVENTS_OZONE_LAYOUT) KeyboardLayoutEngineManager {
 public:
  virtual ~KeyboardLayoutEngineManager();

  static void SetKeyboardLayoutEngine(
      KeyboardLayoutEngine* keyboard_layout_engine);

  static void ResetKeyboardLayoutEngine();

  static KeyboardLayoutEngine* GetKeyboardLayoutEngine() {
    // Must run in a context with a KeyboardLayoutEngine.
    // Hint: Tests can use ui::ScopedKeyboardLayout to create one.
    // (production code should instead call InitializeForUI).
    DCHECK(keyboard_layout_engine_);
    return keyboard_layout_engine_;
  }

 private:
  static KeyboardLayoutEngine* keyboard_layout_engine_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardLayoutEngineManager);
};

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_KEYBOARD_LAYOUT_ENGINE_MANAGER_H_
