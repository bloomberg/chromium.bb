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

AcceleratorHandler::AcceleratorHandler() : last_key_pressed_(0) {
}

bool AcceleratorHandler::Dispatch(GdkEvent* event) {
  if (event->type != GDK_KEY_PRESS && event->type != GDK_KEY_RELEASE) {
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

  if (event->type == GDK_KEY_PRESS) {
    KeyEvent view_key_event(key_event);

    // If it's the Alt key, don't send it to the focus manager until release
    // (to handle focusing the menu bar).
    if (view_key_event.GetKeyCode() == base::VKEY_MENU) {
      last_key_pressed_ = key_event->keyval;
      return true;
    }

    // FocusManager::OnKeyPressed and OnKeyReleased return false if this
    // message has been consumed and should not be propagated further.
    if (!focus_manager->OnKeyEvent(view_key_event)) {
      last_key_pressed_ = key_event->keyval;
      return true;
    }
  }

  // Key release, make sure to filter-out the key release for key press consumed
  // as accelerators to avoid unpaired key release.
  if (event->type == GDK_KEY_RELEASE &&
      key_event->keyval == last_key_pressed_) {
    // Special case: the Alt key can trigger an accelerator on release
    // rather than on press.
    if (base::WindowsKeyCodeForGdkKeyCode(key_event->keyval) ==
        base::VKEY_MENU) {
      Accelerator accelerator(base::VKEY_MENU, false, false, false);
      focus_manager->ProcessAccelerator(accelerator);
    }

    last_key_pressed_ = 0;
    return true;
  }

  gtk_main_do_event(event);
  return true;
}

}  // namespace views
