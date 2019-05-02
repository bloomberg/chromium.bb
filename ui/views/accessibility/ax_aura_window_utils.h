// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_AX_AURA_WINDOW_UTILS_H_
#define UI_VIEWS_ACCESSIBILITY_AX_AURA_WINDOW_UTILS_H_

#include "ui/aura/window.h"
#include "ui/views/views_export.h"

namespace views {
class Widget;

// Singleton class that provides functions for walking a tree of aura::Windows
// for accessibility. in particular, for --single-process-mash we want the
// accessibility tree to jump from a proxy aura Window on the ash side direclty
// to its corresponding client window. This is just a temporary solution to
// that issue and should be removed once Mash is fully launched.
// crbug.com/911945
// TODO(jamescook): Delete this.
class VIEWS_EXPORT AXAuraWindowUtils {
 public:
  virtual ~AXAuraWindowUtils();

  static AXAuraWindowUtils* Get();

  // Replace this global instance with a subclass.
  static void Set(std::unique_ptr<AXAuraWindowUtils> new_instance);

  virtual aura::Window* GetParent(aura::Window* window);
  virtual aura::Window::Windows GetChildren(aura::Window* window);
  virtual bool IsRootWindow(aura::Window* window) const;
  virtual views::Widget* GetWidgetForNativeView(aura::Window* window);

 protected:
  AXAuraWindowUtils();

 private:
  DISALLOW_COPY_AND_ASSIGN(AXAuraWindowUtils);
};

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_AX_AURA_WINDOW_UTILS_H_
