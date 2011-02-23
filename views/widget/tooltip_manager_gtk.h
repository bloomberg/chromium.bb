// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_WIDGET_TOOLTIP_MANAGER_GTK_H_
#define VIEWS_WIDGET_TOOLTIP_MANAGER_GTK_H_
#pragma once

#include <gtk/gtk.h>

#include "views/widget/tooltip_manager.h"
#include "views/widget/tooltip_window_gtk.h"

namespace views {

class WidgetGtk;

// TooltipManager implementation for Gtk.
class TooltipManagerGtk : public TooltipManager {
 public:
  explicit TooltipManagerGtk(WidgetGtk* widget);
  virtual ~TooltipManagerGtk() {}

  // Shows the tooltip at the specified location. Returns true if the tooltip
  // should be shown, false otherwise.
  bool ShowTooltip(int x, int y, bool for_keyboard, GtkTooltip* gtk_tooltip);

  // TooltipManager.
  virtual void UpdateTooltip();
  virtual void TooltipTextChanged(View* view);
  virtual void ShowKeyboardTooltip(View* view);
  virtual void HideKeyboardTooltip();

 private:
  // Sends the show_help signal to widget_. This signal triggers showing the
  // keyboard tooltip if it isn't showing, or hides it if it is showing.
  bool SendShowHelpSignal();

  // Our owner.
  WidgetGtk* widget_;

  // The view supplied to the last invocation of ShowKeyboardTooltip.
  View* keyboard_view_;

  // Customized tooltip window.
  TooltipWindowGtk tooltip_window_;

  DISALLOW_COPY_AND_ASSIGN(TooltipManagerGtk);
};

}  // namespace views

#endif // VIEWS_WIDGET_TOOLTIP_MANAGER_GTK_H_
