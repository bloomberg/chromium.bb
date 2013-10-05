// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/corewm/tooltip_aura.h"

#include "base/command_line.h"
#include "base/strings/string_split.h"
#include "ui/aura/root_window.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/corewm/corewm_switches.h"
#include "ui/views/widget/widget.h"

namespace {

const SkColor kTooltipBackground = 0xFFFFFFCC;
const SkColor kTooltipBorder = 0xFF646450;
const int kTooltipBorderWidth = 1;
const int kTooltipHorizontalPadding = 3;

// Max visual tooltip width. If a tooltip is greater than this width, it will
// be wrapped.
const int kTooltipMaxWidthPixels = 400;

const size_t kMaxLines = 10;

// TODO(derat): This padding is needed on Chrome OS devices but seems excessive
// when running the same binary on a Linux workstation; presumably there's a
// difference in font metrics.  Rationalize this.
const int kTooltipVerticalPadding = 2;
const int kTooltipTimeoutMs = 500;

// FIXME: get cursor offset from actual cursor size.
const int kCursorOffsetX = 10;
const int kCursorOffsetY = 15;

// Creates a widget of type TYPE_TOOLTIP
views::Widget* CreateTooltipWidget(aura::Window* tooltip_window) {
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params;
  // For aura, since we set the type to TYPE_TOOLTIP, the widget will get
  // auto-parented to the right container.
  params.type = views::Widget::InitParams::TYPE_TOOLTIP;
  params.context = tooltip_window;
  DCHECK(params.context);
  params.keep_on_top = true;
  params.accept_events = false;
  widget->Init(params);
  return widget;
}

gfx::Font GetDefaultFont() {
  // TODO(varunjain): implementation duplicated in tooltip_manager_aura. Figure
  // out a way to merge.
  return ui::ResourceBundle::GetSharedInstance().GetFont(
      ui::ResourceBundle::BaseFont);
}

}  // namespace

namespace views {
namespace corewm {

TooltipAura::TooltipAura(gfx::ScreenType screen_type)
    : screen_type_(screen_type),
      widget_(NULL),
      tooltip_window_(NULL) {
  label_.set_background(
      views::Background::CreateSolidBackground(kTooltipBackground));
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kNoDropShadows)) {
    label_.set_border(
        views::Border::CreateSolidBorder(kTooltipBorderWidth,
                                         kTooltipBorder));
  }
  label_.set_owned_by_client();
  label_.SetMultiLine(true);
}

TooltipAura::~TooltipAura() {
  DestroyWidget();
}

// static
void TooltipAura::TrimTooltipToFit(int max_width,
                                   string16* text,
                                   int* width,
                                   int* line_count) {
  *width = 0;
  *line_count = 0;

  // Determine the available width for the tooltip.
  int available_width = std::min(kTooltipMaxWidthPixels, max_width);

  std::vector<string16> lines;
  base::SplitString(*text, '\n', &lines);
  std::vector<string16> result_lines;

  // Format each line to fit.
  gfx::Font font = GetDefaultFont();
  for (std::vector<string16>::iterator l = lines.begin(); l != lines.end();
      ++l) {
    // We break the line at word boundaries, then stuff as many words as we can
    // in the available width to the current line, and move the remaining words
    // to a new line.
    std::vector<string16> words;
    base::SplitStringDontTrim(*l, ' ', &words);
    int current_width = 0;
    string16 line;
    for (std::vector<string16>::iterator w = words.begin(); w != words.end();
        ++w) {
      string16 word = *w;
      if (w + 1 != words.end())
        word.push_back(' ');
      int word_width = font.GetStringWidth(word);
      if (current_width + word_width > available_width) {
        // Current width will exceed the available width. Must start a new line.
        if (!line.empty())
          result_lines.push_back(line);
        current_width = 0;
        line.clear();
      }
      current_width += word_width;
      line.append(word);
    }
    result_lines.push_back(line);
  }

  // Clamp number of lines to |kMaxLines|.
  if (result_lines.size() > kMaxLines) {
    result_lines.resize(kMaxLines);
    // Add ellipses character to last line.
    result_lines[kMaxLines - 1] = gfx::TruncateString(
        result_lines.back(), result_lines.back().length() - 1);
  }
  *line_count = result_lines.size();

  // Flatten the result.
  string16 result;
  for (std::vector<string16>::iterator l = result_lines.begin();
      l != result_lines.end(); ++l) {
    if (!result.empty())
      result.push_back('\n');
    int line_width = font.GetStringWidth(*l);
    // Since we only break at word boundaries, it could happen that due to some
    // very long word, line_width is greater than the available_width. In such
    // case, we simply truncate at available_width and add ellipses at the end.
    if (line_width > available_width) {
      *width = available_width;
      result.append(gfx::ElideText(*l, font, available_width,
                                   gfx::ELIDE_AT_END));
    } else {
      *width = std::max(*width, line_width);
      result.append(*l);
    }
  }
  *text = result;
}

int TooltipAura::GetMaxWidth(const gfx::Point& location) const {
  // TODO(varunjain): implementation duplicated in tooltip_manager_aura. Figure
  // out a way to merge.
  gfx::Rect display_bounds = GetBoundsForTooltip(location);
  return (display_bounds.width() + 1) / 2;
}

gfx::Rect TooltipAura::GetBoundsForTooltip(
    const gfx::Point& origin) const {
  DCHECK(tooltip_window_);
  gfx::Rect widget_bounds;
  // For Desktop aura we constrain the tooltip to the bounds of the Widget
  // (which comes from the RootWindow).
  if (screen_type_ == gfx::SCREEN_TYPE_NATIVE &&
      gfx::SCREEN_TYPE_NATIVE != gfx::SCREEN_TYPE_ALTERNATE) {
    aura::RootWindow* root = tooltip_window_->GetRootWindow();
    widget_bounds = gfx::Rect(root->GetHostOrigin(), root->GetHostSize());
  }
  gfx::Screen* screen = gfx::Screen::GetScreenByType(screen_type_);
  gfx::Rect bounds(screen->GetDisplayNearestPoint(origin).bounds());
  if (!widget_bounds.IsEmpty())
    bounds.Intersect(widget_bounds);
  return bounds;
}

void TooltipAura::SetTooltipBounds(const gfx::Point& mouse_pos,
                                   int tooltip_width,
                                   int tooltip_height) {
  gfx::Rect tooltip_rect(mouse_pos.x(), mouse_pos.y(), tooltip_width,
                         tooltip_height);

  tooltip_rect.Offset(kCursorOffsetX, kCursorOffsetY);
  gfx::Rect display_bounds = GetBoundsForTooltip(mouse_pos);

  // If tooltip is out of bounds on the x axis, we simply shift it
  // horizontally by the offset.
  if (tooltip_rect.right() > display_bounds.right()) {
    int h_offset = tooltip_rect.right() - display_bounds.right();
    tooltip_rect.Offset(-h_offset, 0);
  }

  // If tooltip is out of bounds on the y axis, we flip it to appear above the
  // mouse cursor instead of below.
  if (tooltip_rect.bottom() > display_bounds.bottom())
    tooltip_rect.set_y(mouse_pos.y() - tooltip_height);

  tooltip_rect.AdjustToFit(display_bounds);
  widget_->SetBounds(tooltip_rect);
}

void TooltipAura::CreateWidget() {
  if (widget_) {
    // If the window for which the tooltip is being displayed changes and if the
    // tooltip window and the tooltip widget belong to different rootwindows
    // then we need to recreate the tooltip widget under the active root window
    // hierarchy to get it to display.
    if (widget_->GetNativeWindow()->GetRootWindow() ==
        tooltip_window_->GetRootWindow())
      return;
    DestroyWidget();
  }
  widget_ = CreateTooltipWidget(tooltip_window_);
  widget_->SetContentsView(&label_);
  widget_->AddObserver(this);
}

void TooltipAura::DestroyWidget() {
  if (widget_) {
    widget_->RemoveObserver(this);
    widget_->Close();
    widget_ = NULL;
  }
}

void TooltipAura::SetText(aura::Window* window,
                          const string16& tooltip_text,
                          const gfx::Point& location) {
  tooltip_window_ = window;
  int max_width, line_count;
  string16 trimmed_text(tooltip_text);
  TrimTooltipToFit(
      GetMaxWidth(location), &trimmed_text, &max_width, &line_count);
  label_.SetText(trimmed_text);

  int width = max_width + 2 * kTooltipHorizontalPadding;
  int height = label_.GetHeightForWidth(max_width) +
      2 * kTooltipVerticalPadding;
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kNoDropShadows)) {
    width += 2 * kTooltipBorderWidth;
    height += 2 * kTooltipBorderWidth;
  }
  CreateWidget();
  SetTooltipBounds(location, width, height);
}

void TooltipAura::Show() {
  if (widget_)
    widget_->Show();
}

void TooltipAura::Hide() {
  tooltip_window_ = NULL;
  if (widget_)
    widget_->Hide();
}

bool TooltipAura::IsVisible() {
  return widget_ && widget_->IsVisible();
}

void TooltipAura::OnWidgetDestroying(views::Widget* widget) {
  DCHECK_EQ(widget_, widget);
  widget_ = NULL;
  tooltip_window_ = NULL;
}

}  // namespace corewm
}  // namespace views
