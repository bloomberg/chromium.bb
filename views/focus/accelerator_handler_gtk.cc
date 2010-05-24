// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtk/gtk.h>

#include "base/keyboard_code_conversion_gtk.h"
#include "base/keyboard_codes.h"
#include "views/accelerator.h"
#include "views/focus/accelerator_handler.h"
#include "views/focus/focus_manager.h"
#include "views/widget/widget_gtk.h"
#include "views/window/window_gtk.h"

namespace views {

AcceleratorHandler::AcceleratorHandler() : only_menu_pressed_(false) {}

bool AcceleratorHandler::Dispatch(GdkEvent* event) {
  // When focus changes, we may not see a full key-press / key-release
  // pair, so clear the set of keys we think were pressed.
  if (event->type == GDK_FOCUS_CHANGE) {
    pressed_hardware_keys_.clear();
    consumed_vkeys_.clear();
    only_menu_pressed_ = false;
  }

  // Let Gtk process the event if there is a grabbed widget or it's not
  // a keyboard event.
  if (gtk_grab_get_current() ||
      (event->type != GDK_KEY_PRESS && event->type != GDK_KEY_RELEASE)) {
    gtk_main_do_event(event);
    return true;
  }

  GdkEventKey* key_event = reinterpret_cast<GdkEventKey*>(event);
  // Let's retrieve the focus manager for the GdkWindow.
  GdkWindow* window = gdk_window_get_toplevel(key_event->window);
  gpointer ptr;
  gdk_window_get_user_data(window, &ptr);
  if (!ptr && !gdk_window_is_visible(window)) {
    // The window is destroyed while we're handling key events.
    gtk_main_do_event(event);
    return true;
  }
  DCHECK(ptr);  // The top-level window is expected to always be associated
                // with the top-level gtk widget.
  WindowGtk* widget =
      WidgetGtk::GetWindowForNative(reinterpret_cast<GtkWidget*>(ptr));
  if (!widget) {
    // During dnd we get events for windows we don't control (such as the
    // window being dragged).
    gtk_main_do_event(event);
    return true;
  }
  FocusManager* focus_manager = widget->GetFocusManager();
  if (!focus_manager) {
    NOTREACHED();
    return true;
  }

  KeyEvent view_key_event(key_event);
  int key_code = view_key_event.GetKeyCode();
  int hardware_keycode = key_event->hardware_keycode;
  if (event->type == GDK_KEY_PRESS) {
    pressed_hardware_keys_.insert(hardware_keycode);

    // If it's the Alt key, don't send it to the focus manager until release
    // (to handle focusing the menu bar).
    if (key_code == base::VKEY_MENU && pressed_hardware_keys_.size() == 1) {
      only_menu_pressed_ = true;
      return true;
    }

    if (pressed_hardware_keys_.size() != 1)
      only_menu_pressed_ = false;

    // FocusManager::OnKeyEvent will return false if this message has been
    // consumed and should not be propagated further.
    if (!focus_manager->OnKeyEvent(view_key_event)) {
      consumed_vkeys_.insert(key_code);
      return true;
    }
  }

  if (event->type == GDK_KEY_RELEASE) {
    pressed_hardware_keys_.erase(hardware_keycode);

    // Make sure to filter out the key release for a key press consumed
    // as an accelerator, to avoid calling gtk_main_do_event with an
    // unpaired key release.
    if (consumed_vkeys_.find(key_code) != consumed_vkeys_.end()) {
      consumed_vkeys_.erase(key_code);
      return true;
    }

    // Special case: the Alt key can trigger an accelerator on release
    // rather than on press, but only if no other keys were pressed.
    if (key_code == base::VKEY_MENU && only_menu_pressed_) {
      Accelerator accelerator(base::VKEY_MENU, false, false, false);
      focus_manager->ProcessAccelerator(accelerator);
      return true;
    }
  }

  // Pass the event to gtk if we didn't consume it above.
  gtk_main_do_event(event);
  return true;
}

}  // namespace views
