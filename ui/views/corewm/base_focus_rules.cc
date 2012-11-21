// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/corewm/base_focus_rules.h"

#include "ui/aura/window.h"

namespace views {
namespace corewm {

////////////////////////////////////////////////////////////////////////////////
// BaseFocusRules, public:

BaseFocusRules::BaseFocusRules() {
}

BaseFocusRules::~BaseFocusRules() {
}

////////////////////////////////////////////////////////////////////////////////
// BaseFocusRules, FocusRules implementation:

bool BaseFocusRules::CanActivateWindow(aura::Window* window) {
  // TODO(beng):
  return true;
}

bool BaseFocusRules::CanFocusWindow(aura::Window* window) {
  // See FocusRules: NULL is a valid focusable window (when clearing focus).
  return !window || window->CanFocus();
}

aura::Window* BaseFocusRules::GetActivatableWindow(aura::Window* window) {
  // TODO(beng):
  return window;
}

aura::Window* BaseFocusRules::GetFocusableWindow(aura::Window* window) {
  while (window && !CanFocusWindow(window))
    window = window->parent();
  return window;
}

aura::Window* BaseFocusRules::GetNextActivatableWindow(aura::Window* ignore) {
  DCHECK(ignore);

  // TODO(beng):
  return NULL;
}

aura::Window* BaseFocusRules::GetNextFocusableWindow(aura::Window* ignore) {
  DCHECK(ignore);

  // Focus cycling is currently very primitive: we just climb the tree.
  // If need be, we could add the ability to look at siblings, descend etc.
  // For test coverage, see FocusControllerImplicitTestBase subclasses in
  // focus_controller_unittest.cc.
  return GetFocusableWindow(ignore->parent());
}


}  // namespace corewm
}  // namespace views
