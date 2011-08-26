// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_WIDGET_TOOLTIP_MANAGER_VIEWS_H_
#define VIEWS_WIDGET_TOOLTIP_MANAGER_VIEWS_H_
#pragma once

#include "base/message_loop.h"
#include "base/timer.h"
#include "views/controls/label.h"
#include "views/widget/native_widget.h"
#include "views/widget/tooltip_manager.h"
#include "views/widget/widget_delegate.h"
#include "views/view.h"

typedef union _GdkEvent GdkEvent;
typedef union _XEvent XEvent;
namespace ui {
union WaylandEvent;
}

namespace views {

namespace internal {
class RootView;
}

class Widget;

// TooltipManager implementation for Views.
class TooltipManagerViews : public TooltipManager,
                            public MessageLoopForUI::Observer {
 public:
  explicit TooltipManagerViews(internal::RootView* root_view);
  virtual ~TooltipManagerViews();

  // TooltipManager.
  virtual void UpdateTooltip() OVERRIDE;
  virtual void TooltipTextChanged(View* view) OVERRIDE;
  virtual void ShowKeyboardTooltip(View* view) OVERRIDE;
  virtual void HideKeyboardTooltip() OVERRIDE;

#if defined(USE_WAYLAND)
  virtual base::MessagePumpObserver::EventStatus WillProcessEvent(
      ui::WaylandEvent* event) OVERRIDE;
#else
  // MessageLoopForUI::Observer
  virtual base::MessagePumpObserver::EventStatus WillProcessXEvent(
      XEvent* xevent) OVERRIDE;
#endif

 private:
  void TooltipTimerFired();
  View* GetViewForTooltip(int x, int y, bool for_keyboard);

  // Updates the tooltip if required (if there is any change in the tooltip
  // text or the view.
  void UpdateIfRequired(int x, int y, bool for_keyboard);

  // Updates the tooltip. Gets the tooltip text from tooltip_view_ and displays
  // it at the current mouse position.
  void Update();

  // Adjusts the bounds given by the arguments to fit inside the parent view
  // and applies the adjusted bounds to the tooltip_label_.
  void SetTooltipBounds(gfx::Point mouse_pos, int tooltip_width,
                        int tooltip_height);

  // Creates a widget of type TYPE_TOOLTIP
  Widget* CreateTooltip();

  scoped_ptr<Widget> tooltip_widget_;
  internal::RootView* root_view_;
  View* tooltip_view_;
  std::wstring tooltip_text_;
  Label tooltip_label_;

  gfx::Point curr_mouse_pos_;
  base::RepeatingTimer<TooltipManagerViews> tooltip_timer_;

  DISALLOW_COPY_AND_ASSIGN(TooltipManagerViews);
};

}  // namespace views

#endif // VIEWS_WIDGET_TOOLTIP_MANAGER_VIEWS_H_
