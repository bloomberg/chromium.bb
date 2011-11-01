// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_WINDOW_OBSERVER_H_
#define UI_AURA_WINDOW_OBSERVER_H_
#pragma once

#include "ui/aura/aura_export.h"

namespace aura {

class Window;

class AURA_EXPORT WindowObserver {
 public:
  // Invoked when the Window |new_window| has been added.
  virtual void OnWindowAdded(Window* new_window) {}

  // Invoked prior to removing |window|.
  virtual void OnWillRemoveWindow(Window* window) {}

  // Invoked when |SetProperty| or |SetIntProperty| is called on |window|.
  // |old| is the old property value.
  virtual void OnPropertyChanged(Window* window, const char* key, void* old) {}

  // Invoked when the SetVisible() is invoked on a window. |visible| is the
  // value supplied to SetVisible(). If |visible| is true, window->IsVisible()
  // may still return false. See description in Window::IsVisible() for details.
  virtual void OnWindowVisibilityChanged(Window* window, bool visible) {}

  // Invoked when the Window has been destroyed (i.e. from its destructor).
  virtual void OnWindowDestroyed(Window* window) {}

 protected:
  virtual ~WindowObserver() {}
};

}  // namespace aura

#endif  // UI_AURA_WINDOW_OBSERVER_H_
