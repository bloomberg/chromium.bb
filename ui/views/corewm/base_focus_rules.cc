// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/corewm/base_focus_rules.h"

#include "ui/aura/root_window.h"
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
  return !window ||
      (window->IsVisible() && window->parent() == window->GetRootWindow());
}

bool BaseFocusRules::CanFocusWindow(aura::Window* window) {
  // See FocusRules: NULL is a valid focusable window (when clearing focus).
  return !window || window->CanFocus();
}

aura::Window* BaseFocusRules::GetActivatableWindow(aura::Window* window) {
  // BasicFocusRules considers only direct children of the RootWindow as
  // activatable.
  aura::Window* parent = window->parent();
  aura::Window* activatable = window;
  aura::RootWindow* root_window = window->GetRootWindow();
  while (parent != root_window) {
    activatable = parent;
    parent = parent->parent();
  }
  return activatable;
}

aura::Window* BaseFocusRules::GetFocusableWindow(aura::Window* window) {
  while (window && !CanFocusWindow(window))
    window = window->parent();
  return window;
}

aura::Window* BaseFocusRules::GetNextActivatableWindow(aura::Window* ignore) {
  DCHECK(ignore);

  // Can be called from the RootWindow's destruction, which has a NULL parent.
  if (!ignore->parent())
    return NULL;

  // In the basic scenarios handled by BasicFocusRules, the pool of activatable
  // windows is limited to the |ignore|'s siblings.
  const aura::Window::Windows& siblings = ignore->parent()->children();
  DCHECK(!siblings.empty());

  for (aura::Window::Windows::const_reverse_iterator rit = siblings.rbegin();
       rit != siblings.rend();
       ++rit) {
    aura::Window* cur = *rit;
    if (cur == ignore)
      continue;
    if (CanActivateWindow(cur))
      return cur;
  }
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
