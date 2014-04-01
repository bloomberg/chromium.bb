// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_X_TEXT_EDIT_KEY_BINDINGS_DELEGATE_X11_H_
#define UI_EVENTS_X_TEXT_EDIT_KEY_BINDINGS_DELEGATE_X11_H_

#include <vector>

#include "ui/events/events_export.h"

namespace ui {
class Event;
class TextEditCommandX11;

// An interface which can interpret various text editing commands out of key
// events.
//
// On desktop Linux, we've traditionally supported the user's custom
// keybindings. We need to support this in both content/ and in views/.
class EVENTS_EXPORT TextEditKeyBindingsDelegateX11 {
 public:
  // Matches a key event against the users' platform specific key bindings,
  // false will be returned if the key event doesn't correspond to a predefined
  // key binding.  Edit commands matched with |event| will be stored in
  // |edit_commands|, if |edit_commands| is non-NULL.
  virtual bool MatchEvent(const ui::Event& event,
                          std::vector<TextEditCommandX11>* commands) = 0;

 protected:
  virtual ~TextEditKeyBindingsDelegateX11() {}
};

// Sets/Gets the global TextEditKeyBindingsDelegateX11. No ownership
// changes. Can be NULL.
EVENTS_EXPORT void SetTextEditKeyBindingsDelegate(
    TextEditKeyBindingsDelegateX11* delegate);
EVENTS_EXPORT TextEditKeyBindingsDelegateX11* GetTextEditKeyBindingsDelegate();

}  // namespace ui

#endif  // UI_EVENTS_X_TEXT_EDIT_KEY_BINDINGS_DELEGATE_X11_H_
