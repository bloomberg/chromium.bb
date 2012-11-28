// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_CLIENT_FOCUS_CLIENT_H_
#define UI_AURA_CLIENT_FOCUS_CLIENT_H_

#include "ui/aura/aura_export.h"

namespace ui {
class Event;
}

namespace aura {
class RootWindow;
class Window;

namespace client {
class FocusChangeObserver;

// An interface implemented by an object that manages window focus.
class AURA_EXPORT FocusClient {
 public:
  virtual ~FocusClient() {}

  // TODO(beng): these methods will be OBSOLETE by FocusChangeEvent.
  virtual void AddObserver(FocusChangeObserver* observer) = 0;
  virtual void RemoveObserver(FocusChangeObserver* observer) = 0;

  // Focuses |window|. Passing NULL clears focus.
  // TODO(beng): |event| may be obsolete.
  virtual void FocusWindow(Window* window, const ui::Event* event) = 0;

  // Retrieves the focused window, or NULL if there is none.
  virtual Window* GetFocusedWindow() = 0;
};

// Sets/Gets the focus client on the RootWindow.
AURA_EXPORT void SetFocusClient(RootWindow* root_window, FocusClient* client);
AURA_EXPORT FocusClient* GetFocusClient(Window* window);
AURA_EXPORT FocusClient* GetFocusClient(const Window* window);

}  // namespace clients
}  // namespace aura

#endif  // UI_AURA_CLIENT_FOCUS_CLIENT_H_
