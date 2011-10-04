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

#include "base/logging.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/font.h"
#include "ui/gfx/screen.h"
#include "views/background.h"
#include "views/border.h"
#include "views/events/event.h"
#include "views/focus/focus_manager.h"
#include "views/view.h"
#include "views/widget/native_widget.h"
#include "views/widget/root_view.h"

#if defined(USE_WAYLAND)
#include "ui/wayland/events/wayland_event.h"
#endif

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

TooltipManagerViews::TooltipManagerViews(internal::RootView* root_view)
    : root_view_(root_view),
      tooltip_view_(NULL) {
  tooltip_label_.set_background(
      views::Background::CreateSolidBackground(kTooltipBackground));
  tooltip_widget_.reset(CreateTooltip());
  tooltip_widget_->SetContentsView(&tooltip_label_);
  tooltip_widget_->Activate();
  tooltip_widget_->SetAlwaysOnTop(true);
  tooltip_timer_.Start(FROM_HERE,
      base::TimeDelta::FromMilliseconds(kTooltipTimeoutMs),
      this, &TooltipManagerViews::TooltipTimerFired);
  MessageLoopForUI::current()->AddObserver(this);
}

TooltipManagerViews::~TooltipManagerViews() {
  MessageLoopForUI::current()->RemoveObserver(this);
  tooltip_widget_->CloseNow();
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

#if defined(USE_WAYLAND)
base::MessagePumpObserver::EventStatus TooltipManagerViews::WillProcessEvent(
      ui::WaylandEvent* event) {
  if (event->type == ui::WAYLAND_MOTION)
    OnMouseMoved(event->motion.x, event->motion.y);
  return base::MessagePumpObserver::EVENT_CONTINUE;
}
#elif defined(USE_X11)
base::EventStatus TooltipManagerViews::WillProcessEvent(
    const base::NativeEvent& event) {
  XGenericEventCookie* cookie = &event->xcookie;
  if (cookie->evtype == XI_Motion) {
    const XIDeviceEvent* xievent = static_cast<XIDeviceEvent*>(cookie->data);
    OnMouseMoved(static_cast<int>(xievent->event_x),
                 static_cast<int>(xievent->event_y));
  }
  return base::EVENT_CONTINUE;
}

void TooltipManagerViews::DidProcessEvent(const base::NativeEvent& event) {
}
#elif defined(OS_WIN)
base::EventStatus TooltipManagerViews::WillProcessEvent(
    const base::NativeEvent& event) {
  if (msg.message == WM_MOUSEMOVE)
    OnMouseMoved(GET_X_LPARAM(msg.lParam), GET_Y_LPARAM(msg.lParam));
  return base::EVENT_CONTINUE;
}

void TooltipManagerViews::DidProcessEvent(const base::NativeEvent& event) {
}
#endif

void TooltipManagerViews::TooltipTimerFired() {
  if (tooltip_widget_->IsVisible()) {
    UpdateIfRequired(curr_mouse_pos_.x(), curr_mouse_pos_.y(), false);
  } else {
    tooltip_view_ = GetViewForTooltip(curr_mouse_pos_.x(), curr_mouse_pos_.y(),
        false);
    Update();
  }
}

View* TooltipManagerViews::GetViewForTooltip(int x, int y, bool for_keyboard) {
  View* view = NULL;
  if (!for_keyboard) {
    view = root_view_->GetEventHandlerForPoint(gfx::Point(x, y));
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

  if (tooltip_view_ != view || tooltip_text_ != tooltip_text) {
    tooltip_view_ = view;
    Update();
  }
}

void TooltipManagerViews::Update() {
  if (!tooltip_view_ ||
      !tooltip_view_->GetTooltipText(gfx::Point(), &tooltip_text_))
    tooltip_text_.clear();

  if (tooltip_text_.empty()) {
    tooltip_widget_->Hide();
  } else {
    int max_width, line_count;
    string16 tooltip_text(tooltip_text_);
    TrimTooltipToFit(&tooltip_text, &max_width, &line_count,
                     curr_mouse_pos_.x(), curr_mouse_pos_.y());
    tooltip_label_.SetText(UTF16ToWideHack(tooltip_text));

    SetTooltipBounds(curr_mouse_pos_, max_width,
                     tooltip_label_.GetPreferredSize().height());
    tooltip_widget_->Show();
  }
}

void TooltipManagerViews::SetTooltipBounds(gfx::Point mouse_pos,
                                           int tooltip_width,
                                           int tooltip_height) {
  View::ConvertPointToScreen(root_view_, &mouse_pos);
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
  params.type = Widget::InitParams::TYPE_TOOLTIP;
  params.keep_on_top = true;
  params.accept_events = false;
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget->Init(params);
  widget->SetOpacity(0x00);
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
