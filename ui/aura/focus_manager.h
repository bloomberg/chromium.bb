// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_FOCUS_MANAGER_H_
#define UI_AURA_FOCUS_MANAGER_H_
#pragma once

#include "base/basictypes.h"
#include "base/observer_list.h"
#include "ui/aura/aura_export.h"

namespace aura {

class Event;
class FocusChangeObserver;
class Window;

// An interface implemented by the Desktop to expose the focused window and
// allow for it to be changed.
class AURA_EXPORT FocusManager {
 public:
  FocusManager();
  ~FocusManager();

  void AddObserver(FocusChangeObserver* observer);
  void RemoveObserver(FocusChangeObserver* observer);

  // Sets the currently focused window. Before the currently focused window is
  // changed, the previous focused window's delegate is sent a blur
  // notification, and after it is changed the new focused window is sent a
  // focused notification. Nothing happens if |window| and GetFocusedWindow()
  // match.
  void SetFocusedWindow(Window* window, const Event* event);

  // Returns the currently focused window or NULL if there is none.
  Window* GetFocusedWindow();

  // Returns true if |window| is the focused window.
  bool IsFocusedWindow(const Window* window) const;

 protected:
  aura::Window* focused_window_;

  ObserverList<FocusChangeObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(FocusManager);
};

}  // namespace aura

#endif  // UI_AURA_FOCUS_MANAGER_H_
