// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/shell_tooltip_manager.h"

#include <vector>

#include "base/location.h"
#include "base/string_split.h"
#include "base/time.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/event.h"
#include "ui/aura/window.h"
#include "ui/aura_shell/shell.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/text/text_elider.h"
#include "ui/gfx/font.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/screen.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"
#include "views/background.h"
#include "views/border.h"

namespace {

SkColor kTooltipBackground = 0xFFFFFFCC;
SkColor kTooltipBorder = 0xFF000000;
int kTooltipBorderWidth = 1;
int kTooltipTimeoutMs = 500;

// FIXME: get cursor offset from actual cursor size.
int kCursorOffsetX = 10;
int kCursorOffsetY = 15;

// Maximum number of characters we allow in a tooltip.
const size_t kMaxTooltipLength = 1024;

// Maximum number of lines we allow in the tooltip.
const size_t kMaxLines = 6;

// Trims the tooltip to fit, setting |text| to the clipped result,
// |max_width| to the width (in pixels) of the clipped text and |line_count|
// to the number of lines of text in the tooltip. |x| and |y| give the
// location of the tooltip in screen coordinates.
void TrimTooltipToFit(string16* text,
                      int* max_width,
                      int* line_count,
                      int x,
                      int y) {
  *max_width = 0;
  *line_count = 0;

  // Clamp the tooltip length to kMaxTooltipLength so that we don't
  // accidentally DOS the user with a mega tooltip.
  if (text->length() > kMaxTooltipLength)
    *text = text->substr(0, kMaxTooltipLength);

  // Determine the available width for the tooltip.
  int available_width = aura::TooltipClient::GetMaxWidth(x, y);

  // Split the string into at most kMaxLines lines.
  std::vector<string16> lines;
  base::SplitString(*text, '\n', &lines);
  if (lines.size() > kMaxLines)
    lines.resize(kMaxLines);
  *line_count = static_cast<int>(lines.size());

  // Format each line to fit.
  gfx::Font font = aura::TooltipClient::GetDefaultFont();
  string16 result;
  for (std::vector<string16>::iterator i = lines.begin(); i != lines.end();
       ++i) {
    string16 elided_text = ui::ElideText(*i, font, available_width, false);
    *max_width = std::max(*max_width, font.GetStringWidth(elided_text));
    if (!result.empty())
      result.push_back('\n');
    result.append(elided_text);
  }
  *text = result;
}

// Creates a widget of type TYPE_TOOLTIP
views::Widget* CreateTooltip() {
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params;
  // For aura, since we set the type to TOOLTIP_TYPE, the widget will get
  // auto-parented to the MenuAndTooltipsContainer.
  params.type = views::Widget::InitParams::TYPE_TOOLTIP;
  params.keep_on_top = true;
  params.accept_events = false;
  params.transparent = true;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget->Init(params);
  return widget;
}

}  // namespace

namespace aura_shell {

// Displays a widget with tooltip using a views::Label.
class ShellTooltipManager::Tooltip {
 public:
  Tooltip() {
    label_.set_background(
        views::Background::CreateSolidBackground(kTooltipBackground));
    label_.set_border(
        views::Border::CreateSolidBorder(kTooltipBorderWidth, kTooltipBorder));
    label_.set_parent_owned(false);
    widget_.reset(CreateTooltip());
    widget_->SetContentsView(&label_);
    widget_->Activate();
  }

  ~Tooltip() {
    widget_->Close();
  }

  // Updates the text on the tooltip and resizes to fit.
  void SetText(string16 tooltip_text, gfx::Point location) {
    int max_width, line_count;
    TrimTooltipToFit(&tooltip_text, &max_width, &line_count,
                     location.x(), location.y());
    label_.SetText(tooltip_text);

    SetTooltipBounds(location, max_width + 2 * kTooltipBorderWidth,
                     label_.GetPreferredSize().height());
  }

  // Shows the tooltip.
  void Show() {
    widget_->Show();
  }

  // Hides the tooltip.
  void Hide() {
    widget_->Hide();
  }

  bool IsVisible() {
    return widget_->IsVisible();
  }

 private:
  views::Label label_;
  scoped_ptr<views::Widget> widget_;

  // Adjusts the bounds given by the arguments to fit inside the desktop
  // and applies the adjusted bounds to the label_.
  void SetTooltipBounds(gfx::Point mouse_pos,
                        int tooltip_width,
                        int tooltip_height) {
    gfx::Rect tooltip_rect(mouse_pos.x(), mouse_pos.y(), tooltip_width,
                           tooltip_height);

    tooltip_rect.Offset(kCursorOffsetX, kCursorOffsetY);
    gfx::Rect monitor_bounds =
        gfx::Screen::GetMonitorAreaNearestPoint(tooltip_rect.origin());
    widget_->SetBounds(tooltip_rect.AdjustToFit(monitor_bounds));
  }

};

////////////////////////////////////////////////////////////////////////////////
// ShellTooltipManager public:

ShellTooltipManager::ShellTooltipManager() : aura::EventFilter(NULL),
                                             tooltip_window_(NULL),
                                             tooltip_(new Tooltip) {
  tooltip_timer_.Start(FROM_HERE,
      base::TimeDelta::FromMilliseconds(kTooltipTimeoutMs),
      this, &ShellTooltipManager::TooltipTimerFired);
}

ShellTooltipManager::~ShellTooltipManager() {
  if (tooltip_window_)
    tooltip_window_->RemoveObserver(this);
}

void ShellTooltipManager::UpdateTooltip(aura::Window* target) {
  // If tooltip is visible, we may want to hide it. If it is not, we are ok.
  if (tooltip_window_ == target && tooltip_->IsVisible())
    UpdateIfRequired();
}

bool ShellTooltipManager::PreHandleKeyEvent(aura::Window* target,
                                            aura::KeyEvent* event) {
  return false;
}

bool ShellTooltipManager::PreHandleMouseEvent(aura::Window* target,
                                              aura::MouseEvent* event) {
  switch (event->type()) {
    case ui::ET_MOUSE_MOVED:
      if (tooltip_window_ != target) {
        if (tooltip_window_)
          tooltip_window_->RemoveObserver(this);
        tooltip_window_ = target;
        tooltip_window_->AddObserver(this);
      }
      curr_mouse_loc_ = event->location();
      if (tooltip_timer_.IsRunning())
        tooltip_timer_.Reset();

      if (tooltip_->IsVisible())
        UpdateIfRequired();
      break;
    case ui::ET_MOUSE_PRESSED:
    case ui::ET_MOUSE_RELEASED:
    case ui::ET_MOUSE_DRAGGED:
    case ui::ET_MOUSEWHEEL:
      // Hide the tooltip for click, release, drag, wheel events.
      if (tooltip_->IsVisible())
        tooltip_->Hide();
      break;
    default:
      break;
  }
  return false;
}

ui::TouchStatus ShellTooltipManager::PreHandleTouchEvent(
    aura::Window* target,
    aura::TouchEvent* event) {
  return ui::TOUCH_STATUS_UNKNOWN;
}

void ShellTooltipManager::OnWindowDestroyed(aura::Window* window) {
  if (tooltip_window_ == window) {
    tooltip_window_->RemoveObserver(this);
    tooltip_window_ = NULL;
  }
}

void ShellTooltipManager::TooltipTimerFired() {
  UpdateIfRequired();
}

void ShellTooltipManager::UpdateIfRequired() {
  string16 tooltip_text;
  if (tooltip_window_) {
    void* property = tooltip_window_->GetProperty(aura::kTooltipTextKey);
    if (property)
      tooltip_text = *static_cast<string16*>(property);
  }

  if (tooltip_text_ != tooltip_text) {
    tooltip_text_ = tooltip_text;
    if (tooltip_text_.empty()) {
      tooltip_->Hide();
    } else {
      string16 tooltip_text(tooltip_text_);
      gfx::Point widget_loc = curr_mouse_loc_;
      widget_loc = widget_loc.Add(tooltip_window_->GetScreenBounds().origin());
      tooltip_->SetText(tooltip_text, widget_loc);
      tooltip_->Show();
    }
  }
}

}  // namespace aura_shell

namespace aura {

// static
gfx::Font TooltipClient::GetDefaultFont() {
  return ui::ResourceBundle::GetSharedInstance().GetFont(
      ui::ResourceBundle::BaseFont);
}

// static
int TooltipClient::GetMaxWidth(int x, int y) {
  gfx::Rect monitor_bounds =
      gfx::Screen::GetMonitorAreaNearestPoint(gfx::Point(x, y));
  return (monitor_bounds.width() + 1) / 2;
}

}  // namespace aura
