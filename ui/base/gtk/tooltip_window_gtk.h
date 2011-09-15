// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_GTK_TOOLTIP_WINDOW_GTK_H_
#define UI_BASE_GTK_TOOLTIP_WINDOW_GTK_H_
#pragma once

#include <string>

#include "base/string16.h"
#include "ui/base/gtk/gtk_integers.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/base/ui_export.h"

typedef struct _GdkEventExpose GdkEventExpose;
typedef struct _GtkLabel GtkLabel;
typedef struct _GtkWidget GtkWidget;
typedef struct _GtkStyle GtkStyle;

namespace ui {

// TooltipWindowGtk provides a customized tooltip window and gives us a
// chance to apply RGBA colormap on it. This enables the GTK theme engine to
// draw tooltip with nice shadow and rounded corner on ChromeOS.
class UI_EXPORT TooltipWindowGtk {
 public:
  explicit TooltipWindowGtk(GtkWidget* widget);
  virtual ~TooltipWindowGtk();

  // Sets tooltip text to display.
  void SetTooltipText(const string16& text);

  GtkLabel* label();

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

} // namespace ui

#endif  // UI_BASE_GTK_TOOLTIP_WINDOW_GTK_H_
