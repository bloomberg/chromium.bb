// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/widget/gtk_views_window.h"
#include "views/focus/focus_manager.h"

G_BEGIN_DECLS

G_DEFINE_TYPE(GtkViewsWindow, gtk_views_window, GTK_TYPE_WINDOW)

static void gtk_views_window_move_focus(GtkWindow *window,
                                        GtkDirectionType dir) {
  views::FocusManager* focus_manager =
      views::FocusManager::GetFocusManagerForNativeWindow(window);
  if (focus_manager) {
    // We only support tab traversing by tab keys, so just ignore all other
    // cases silently.
    if (dir == GTK_DIR_TAB_BACKWARD || dir == GTK_DIR_TAB_FORWARD)
      focus_manager->AdvanceFocus(dir == GTK_DIR_TAB_BACKWARD);
  } else if (GTK_WINDOW_CLASS(gtk_views_window_parent_class)->move_focus) {
    GTK_WINDOW_CLASS(gtk_views_window_parent_class)->move_focus(window, dir);
  }
}

static void gtk_views_window_class_init(
    GtkViewsWindowClass* views_window_class) {
  GtkWindowClass* window_class =
      reinterpret_cast<GtkWindowClass*>(views_window_class);
  window_class->move_focus = gtk_views_window_move_focus;
}

static void gtk_views_window_init(GtkViewsWindow* window) {
}

GtkWidget* gtk_views_window_new(GtkWindowType type) {
  return GTK_WIDGET(g_object_new(GTK_TYPE_VIEWS_WINDOW, "type", type, NULL));
}

G_END_DECLS
