// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/widget/tooltip_manager_views.h"

#if defined(USE_X11)
#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>
#endif

#if defined(OS_WIN)
#include <windowsx.h>
#endif

#include "base/event_types.h"
#include "base/logging.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "third_party/skia/include/core/SkColor.h"
#if defined(USE_AURA)
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/event.h"
#include "ui/aura/window.h"
#endif
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/font.h"
#include "ui/gfx/screen.h"
#include "views/background.h"
#include "views/border.h"
#include "views/events/event.h"
#include "views/focus/focus_manager.h"
#include "views/view.h"
#include "views/widget/native_widget.h"

namespace {
SkColor kTooltipBackground = 0xFF7F7F00;
int kTooltipTimeoutMs = 500;

// FIXME: get cursor offset from actual cursor size.
int kCursorOffsetX = 10;
int kCursorOffsetY = 15;
}

namespace views {

// static
int TooltipManager::GetTooltipHeight() {
  // Not used for linux and chromeos.
  NOTREACHED();
  return 0;
}

// static
gfx::Font TooltipManager::GetDefaultFont() {
  return ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::BaseFont);
}

// static
int TooltipManager::GetMaxWidth(int x, int y) {
  // FIXME: change this. This is for now just copied from TooltipManagerGtk.

  // We always display the tooltip inside the root view. So the max width is
  // the width of the view.
  gfx::Rect monitor_bounds =
      gfx::Screen::GetMonitorAreaNearestPoint(gfx::Point(x, y));
  // GtkLabel (gtk_label_ensure_layout) forces wrapping at this size. We mirror
  // the size here otherwise tooltips wider than the size used by gtklabel end
  // up with extraneous empty lines.
  return monitor_bounds.width() == 0 ? 800 : (monitor_bounds.width() + 1) / 2;
}

TooltipManagerViews::TooltipManagerViews(views::View* root_view)
    : root_view_(root_view),
      tooltip_view_(NULL) {
  tooltip_label_.set_background(
      views::Background::CreateSolidBackground(kTooltipBackground));
  tooltip_widget_.reset(CreateTooltip());
  tooltip_widget_->SetContentsView(&tooltip_label_);
  tooltip_widget_->Activate();
  tooltip_widget_->SetAlwaysOnTop(true);
}

TooltipManagerViews::~TooltipManagerViews() {
  tooltip_widget_->CloseNow();
}

void TooltipManagerViews::UpdateForMouseEvent(const MouseEvent& event) {
  switch (event.type()) {
    case ui::ET_MOUSE_EXITED:
      // Mouse is exiting this widget. Stop showing the tooltip and the timer.
      if (tooltip_timer_.IsRunning())
        tooltip_timer_.Stop();
      if (tooltip_widget_->IsVisible())
        tooltip_widget_->Hide();
      break;
    case ui::ET_MOUSE_ENTERED:
      // Mouse just entered this widget. Start the timer to show the tooltip.
      if (tooltip_timer_.IsRunning())
        tooltip_timer_.Stop();
      tooltip_timer_.Start(FROM_HERE,
          base::TimeDelta::FromMilliseconds(kTooltipTimeoutMs),
          this, &TooltipManagerViews::TooltipTimerFired);
      break;
    case ui::ET_MOUSE_MOVED:
      OnMouseMoved(event.location().x(), event.location().y());
      break;
    case ui::ET_MOUSE_PRESSED:
    case ui::ET_MOUSE_RELEASED:
    case ui::ET_MOUSE_DRAGGED:
    case ui::ET_MOUSEWHEEL:
      // Hide the tooltip for click, release, drag, wheel events.
      if (tooltip_widget_->IsVisible())
        tooltip_widget_->Hide();
      break;
    default:
      NOTIMPLEMENTED();
  }
}

void TooltipManagerViews::UpdateTooltip() {
  UpdateIfRequired(curr_mouse_pos_.x(), curr_mouse_pos_.y(), false);
}

void TooltipManagerViews::TooltipTextChanged(View* view) {
  if (tooltip_widget_->IsVisible())
    UpdateIfRequired(curr_mouse_pos_.x(), curr_mouse_pos_.y(), false);
}

void TooltipManagerViews::ShowKeyboardTooltip(View* view) {
  NOTREACHED();
}

void TooltipManagerViews::HideKeyboardTooltip() {
  NOTREACHED();
}

void TooltipManagerViews::TooltipTimerFired() {
  UpdateIfRequired(curr_mouse_pos_.x(), curr_mouse_pos_.y(), false);
}

View* TooltipManagerViews::GetViewForTooltip(int x, int y, bool for_keyboard) {
  View* view = NULL;
  if (!for_keyboard) {
    // Convert x,y from screen coordinates to |root_view_| coordinates.
    gfx::Point point(x, y);
    View::ConvertPointFromWidget(root_view_, &point);
    view = root_view_->GetEventHandlerForPoint(point);
  } else {
    FocusManager* focus_manager = root_view_->GetFocusManager();
    if (focus_manager)
      view = focus_manager->GetFocusedView();
  }
  return view;
}

void TooltipManagerViews::UpdateIfRequired(int x, int y, bool for_keyboard) {
  View* view = GetViewForTooltip(x, y, for_keyboard);
  string16 tooltip_text;
  if (view)
    view->GetTooltipText(gfx::Point(x, y), &tooltip_text);

#if defined(USE_AURA)
  // In aura, and aura::Window can also have a tooltip. If the view doesnot have
  // a tooltip, we must also check for the aura::Window underneath the cursor.
  if (tooltip_text.empty()) {
    aura::Window* root = reinterpret_cast<aura::Window*>(
        root_view_->GetWidget()->GetNativeView());
    if (root) {
      aura::Window* window = root->GetEventHandlerForPoint(gfx::Point(x, y));
      if (window) {
        void* property = window->GetProperty(aura::kTooltipTextKey);
        if (property)
          tooltip_text = *reinterpret_cast<string16*>(property);
      }
    }
  }
#endif

  if (tooltip_view_ != view || tooltip_text_ != tooltip_text) {
    tooltip_view_ = view;
    tooltip_text_ = tooltip_text;
    Update();
  }
}

void TooltipManagerViews::Update() {
  if (tooltip_text_.empty()) {
    tooltip_widget_->Hide();
  } else {
    int max_width, line_count;
    string16 tooltip_text(tooltip_text_);
    TrimTooltipToFit(&tooltip_text, &max_width, &line_count,
                     curr_mouse_pos_.x(), curr_mouse_pos_.y());
    tooltip_label_.SetText(tooltip_text);

    SetTooltipBounds(curr_mouse_pos_, max_width,
                     tooltip_label_.GetPreferredSize().height());
    tooltip_widget_->Show();
  }
}

void TooltipManagerViews::SetTooltipBounds(gfx::Point mouse_pos,
                                           int tooltip_width,
                                           int tooltip_height) {
  gfx::Rect tooltip_rect(mouse_pos.x(), mouse_pos.y(), tooltip_width,
                         tooltip_height);

  tooltip_rect.Offset(kCursorOffsetX, kCursorOffsetY);
  gfx::Rect monitor_bounds =
      gfx::Screen::GetMonitorAreaNearestPoint(tooltip_rect.origin());
  tooltip_widget_->SetBounds(tooltip_rect.AdjustToFit(monitor_bounds));
}

Widget* TooltipManagerViews::CreateTooltip() {
  Widget* widget = new Widget;
  Widget::InitParams params;
  // For aura, since we set the type to TOOLTIP_TYPE, the widget will get
  // auto-parented to the MenuAndTooltipsContainer.
  params.type = Widget::InitParams::TYPE_TOOLTIP;
  params.keep_on_top = true;
  params.accept_events = false;
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget->Init(params);
  widget->SetOpacity(0xFF);
  return widget;
}

void TooltipManagerViews::OnMouseMoved(int x, int y) {
  if (tooltip_timer_.IsRunning())
    tooltip_timer_.Reset();
  curr_mouse_pos_.SetPoint(x, y);

  // If tooltip is visible, we may want to hide it. If it is not, we are ok.
  if (tooltip_widget_->IsVisible())
    UpdateIfRequired(curr_mouse_pos_.x(), curr_mouse_pos_.y(), false);
}

}  // namespace views
