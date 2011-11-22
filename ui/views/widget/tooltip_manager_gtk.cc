// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/tooltip_manager_gtk.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "ui/gfx/font.h"
#include "ui/gfx/screen.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/widget/native_widget_gtk.h"
#include "views/view.h"

// WARNING: this implementation is good for a start, but it doesn't give us
// control of tooltip positioning both on mouse events and when showing from
// keyboard. We may need to write our own to give us the control we need.

namespace views {

static gfx::Font* LoadDefaultFont() {
  // Create a tooltip widget and extract the font from it (we have to realize
  // it to make sure the correct font gets set).
  GtkWidget* window = gtk_window_new(GTK_WINDOW_POPUP);
  gtk_widget_set_name(window, "gtk-tooltip");
  GtkWidget* label = gtk_label_new("");
  gtk_widget_show(label);

  gtk_container_add(GTK_CONTAINER(window), label);
  gtk_widget_realize(window);

  GtkStyle* style = gtk_widget_get_style(label);
  gfx::Font* font = new gfx::Font(style->font_desc);

  gtk_widget_destroy(window);

  return font;
}

// static
int TooltipManager::GetTooltipHeight() {
  // This is only used to position the tooltip, and we don't yet support
  // positioning the tooltip, it isn't worth trying to implement this.
  return 0;
}

// static
gfx::Font TooltipManager::GetDefaultFont() {
  static gfx::Font* font = NULL;
  if (!font)
    font = LoadDefaultFont();

  return *font;
}

// static
int TooltipManager::GetMaxWidth(int x, int y) {
  gfx::Rect monitor_bounds =
      gfx::Screen::GetMonitorAreaNearestPoint(gfx::Point(x, y));
  // GtkLabel (gtk_label_ensure_layout) forces wrapping at this size. We mirror
  // the size here otherwise tooltips wider than the size used by gtklabel end
  // up with extraneous empty lines.
  return monitor_bounds.width() == 0 ? 800 : (monitor_bounds.width() + 1) / 2;
}

TooltipManagerGtk::TooltipManagerGtk(NativeWidgetGtk* widget)
    : widget_(widget),
      keyboard_view_(NULL),
      tooltip_window_(widget->window_contents()) {
}

bool TooltipManagerGtk::ShowTooltip(int x, int y, bool for_keyboard,
                                    GtkTooltip* tooltip) {
  const View* view = NULL;
  gfx::Point view_loc;
  if (keyboard_view_) {
    view = keyboard_view_;
    view_loc.SetPoint(view->width() / 2, view->height() / 2);
  } else if (!for_keyboard) {
    View* root_view = widget_->GetWidget()->GetRootView();
    view = root_view->GetEventHandlerForPoint(gfx::Point(x, y));
    view_loc.SetPoint(x, y);
    View::ConvertPointFromWidget(view, &view_loc);
  } else {
    const FocusManager* focus_manager = widget_->GetWidget()->GetFocusManager();
    if (focus_manager) {
      view = focus_manager->GetFocusedView();
      if (view)
        view_loc.SetPoint(view->width() / 2, view->height() / 2);
    }
  }

  if (!view)
    return false;

  string16 text;
  if (!view->GetTooltipText(view_loc, &text))
    return false;

  // Sets the area of the tooltip. This way if different views in the same
  // widget have tooltips the tooltip doesn't get stuck at the same location.
  gfx::Rect vis_bounds = view->GetVisibleBounds();
  gfx::Point widget_loc(vis_bounds.origin());
  View::ConvertPointToWidget(view, &widget_loc);
  GdkRectangle tip_area = { widget_loc.x(), widget_loc.y(),
                            vis_bounds.width(), vis_bounds.height() };
  gtk_tooltip_set_tip_area(tooltip, &tip_area);

  int max_width, line_count;
  gfx::Point screen_loc(x, y);
  View::ConvertPointToScreen(widget_->GetWidget()->GetRootView(), &screen_loc);
  TrimTooltipToFit(&text, &max_width, &line_count, screen_loc.x(),
                   screen_loc.y());
  tooltip_window_.SetTooltipText(text);

  return true;
}

void TooltipManagerGtk::UpdateTooltip() {
  // UpdateTooltip may be invoked after the widget has been destroyed.
  GtkWidget* widget = widget_->GetNativeView();
  if (!widget)
    return;

  GdkDisplay* display = gtk_widget_get_display(widget);
  if (display)
    gtk_tooltip_trigger_tooltip_query(display);
}

void TooltipManagerGtk::TooltipTextChanged(View* view) {
  UpdateTooltip();
}

void TooltipManagerGtk::ShowKeyboardTooltip(View* view) {
  if (view == keyboard_view_)
    return;  // We're already showing the tip for the specified view.

  // We have to hide the current tooltip, then show again.
  HideKeyboardTooltip();

  string16 tooltip_text;
  if (!view->GetTooltipText(gfx::Point(), &tooltip_text))
    return;  // The view doesn't have a tooltip, nothing to do.

  keyboard_view_ = view;
  if (!SendShowHelpSignal()) {
    keyboard_view_ = NULL;
    return;
  }
}

void TooltipManagerGtk::HideKeyboardTooltip() {
  if (!keyboard_view_)
    return;

  SendShowHelpSignal();
  keyboard_view_ = NULL;
}

bool TooltipManagerGtk::SendShowHelpSignal() {
  GtkWidget* widget = widget_->window_contents();
  GType itype = G_TYPE_FROM_INSTANCE(G_OBJECT(widget));
  guint signal_id;
  GQuark detail;
  if (!g_signal_parse_name("show_help", itype, &signal_id, &detail, FALSE)) {
    NOTREACHED();
    return false;
  }
  gboolean result;
  g_signal_emit(widget, signal_id, 0, GTK_WIDGET_HELP_TOOLTIP, &result);
  return true;
}

}  // namespace views
