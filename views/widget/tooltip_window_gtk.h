// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_WIDGET_TOOLTIP_WINDOW_GTK_H_
#define VIEWS_WIDGET_TOOLTIP_WINDOW_GTK_H_

#include <gtk/gtk.h>
#include <string>

#include "app/gtk_signal.h"

namespace views {

// TooltipWindowGtk provides a customized tooltip window and gives us a
// chance to apply RGBA colormap on it. This enables the GTK theme engine to
// draw tooltip with nice shadow and rounded corner on ChromeOS.
class TooltipWindowGtk {
 public:
  explicit TooltipWindowGtk(GtkWidget* widget);
  ~TooltipWindowGtk();

  // Sets tooltip text to display.
  void SetTooltipText(const std::wstring& text);

  GtkLabel* label() {
    return GTK_LABEL(label_);
  }

 protected:
  CHROMEGTK_CALLBACK_1(TooltipWindowGtk, gboolean, OnPaint, GdkEventExpose*);
  CHROMEGTK_CALLBACK_1(TooltipWindowGtk, void, OnStyleSet, GtkStyle*);

 private:
  void Init();

  // Underlying widget of this tooltip window.
  GtkWidget* host_;

  // GtkWindow of this tooltip window.
  GtkWidget* window_;

  // The alignment and label widgets contained of the tooltip window.
  GtkWidget* alignment_;
  GtkWidget* label_;

  DISALLOW_COPY_AND_ASSIGN(TooltipWindowGtk);
};

} // namespace views

#endif  // VIEWS_WIDGET_TOOLTIP_WINDOW_GTK_H_
