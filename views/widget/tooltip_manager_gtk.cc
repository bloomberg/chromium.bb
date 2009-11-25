// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/widget/tooltip_manager_gtk.h"

#include "app/gfx/font.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "views/focus/focus_manager.h"
#include "views/widget/root_view.h"
#include "views/widget/widget_gtk.h"

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
  gfx::Font* font = new gfx::Font(gfx::Font::CreateFont(style->font_desc));

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
const std::wstring& TooltipManager::GetLineSeparator() {
  static std::wstring* line_separator = NULL;
  if (!line_separator)
    line_separator = new std::wstring(L"\n");
  return *line_separator;
}

// Callback from gtk_container_foreach. If |*label_p| is NULL and |widget| is
// a GtkLabel, |*label_p| is set to |widget|. Used to find the first GtkLabel
// in a container.
static void LabelLocatorCallback(GtkWidget* widget,
                                 gpointer label_p) {
  GtkWidget** label = static_cast<GtkWidget**>(label_p);
  if (!*label && GTK_IS_LABEL(widget))
    *label = widget;
}

// By default GtkTooltip wraps at a longish string. We want more control over
// that wrapping. The only way to do that is dig out the label and set
// gtk_label_set_max_width_chars, which is what this code does. I also tried
// setting a custom widget on the tooltip, but there is a bug in Gtk that
// triggers continually hiding/showing the widget in that case.
static void AdjustLabel(GtkTooltip* tooltip) {
  static const char kAdjustedLabelPropertyValue[] = "_adjusted_label_";
  gpointer adjusted_value = g_object_get_data(G_OBJECT(tooltip),
                                              kAdjustedLabelPropertyValue);
  if (adjusted_value)
    return;

  adjusted_value = reinterpret_cast<gpointer>(1);
  g_object_set_data(G_OBJECT(tooltip), kAdjustedLabelPropertyValue,
                    adjusted_value);

  GtkWidget* parent;
  {
    // Create a label so that we can get the parent. The Tooltip ends up taking
    // ownership of the label and deleting it.
    GtkWidget* label = gtk_label_new("");
    gtk_tooltip_set_custom(tooltip, label);
    parent = gtk_widget_get_parent(label);
    gtk_tooltip_set_custom(tooltip, NULL);
  }
  if (parent) {
    // We found the parent, find the first label, which is where the tooltip
    // text ends up going.
    GtkLabel* real_label = NULL;
    gtk_container_foreach(GTK_CONTAINER(parent), LabelLocatorCallback,
                          static_cast<gpointer>(&real_label));
    if (real_label)
      gtk_label_set_max_width_chars(GTK_LABEL(real_label), 3000);
  }
}

TooltipManagerGtk::TooltipManagerGtk(WidgetGtk* widget)
    : widget_(widget),
      keyboard_view_(NULL) {
}

bool TooltipManagerGtk::ShowTooltip(int x, int y, bool for_keyboard,
                                    GtkTooltip* tooltip) {
  View* view = NULL;
  gfx::Point view_loc;
  if (keyboard_view_) {
    view = keyboard_view_;
    view_loc.SetPoint(view->width() / 2, view->height() / 2);
  } else if (!for_keyboard) {
    RootView* root_view = widget_->GetRootView();
    view = root_view->GetViewForPoint(gfx::Point(x, y));
    view_loc.SetPoint(x, y);
    View::ConvertPointFromWidget(view, &view_loc);
  } else {
    FocusManager* focus_manager = widget_->GetFocusManager();
    if (focus_manager) {
      view = focus_manager->GetFocusedView();
      if (view)
        view_loc.SetPoint(view->width() / 2, view->height() / 2);
    }
  }

  if (!view)
    return false;

  std::wstring text;
  if (!view->GetTooltipText(view_loc.x(), view_loc.y(), &text))
    return false;

  AdjustLabel(tooltip);

  // Sets the area of the tooltip. This way if different views in the same
  // widget have tooltips the tooltip doesn't get stuck at the same location.
  gfx::Rect vis_bounds = view->GetVisibleBounds();
  gfx::Point widget_loc(vis_bounds.x(), vis_bounds.y());
  View::ConvertPointToWidget(view, &widget_loc);
  GdkRectangle tip_area = { widget_loc.x(), widget_loc.y(),
                            vis_bounds.width(), vis_bounds.height() };
  gtk_tooltip_set_tip_area(tooltip, &tip_area);

  int max_width, line_count;
  gfx::Point screen_loc(x, y);
  View::ConvertPointToScreen(widget_->GetRootView(), &screen_loc);
  TrimTooltipToFit(&text, &max_width, &line_count, screen_loc.x(),
                   screen_loc.y());
  gtk_tooltip_set_text(tooltip, WideToUTF8(text).c_str());

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

  std::wstring tooltip_text;
  if (!view->GetTooltipText(0, 0, &tooltip_text))
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
