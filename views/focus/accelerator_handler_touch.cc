// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtk/gtk.h>

#include "views/accelerator.h"
#include "views/focus/accelerator_handler.h"
#include "views/focus/focus_manager.h"
#include "views/touchui/touch_event_dispatcher_gtk.h"
#include "views/widget/widget_gtk.h"

namespace views {

AcceleratorHandler::AcceleratorHandler() {}

bool AcceleratorHandler::Dispatch(GdkEvent* event) {
  // The logic for handling keyboard accelerators has been moved into
  // WidgetGtk::OnKeyEvent handler (views/widget/widget_gtk.cc).

  // TODO(wyck): Hijack TouchUI events at other calls to gtk_main_do_event.
  // There are more places where we call gtk_main_do_event.
  // In particular: the message pump itself, and the menu controller,
  // as well as native_menu_gtk.
  // This function contains the most important one important one, though.
  if (!DispatchEventForTouchUIGtk(event))
    gtk_main_do_event(event);

  return true;
}

}  // namespace views
