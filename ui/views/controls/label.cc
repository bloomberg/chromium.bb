// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/label.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/profiler/scoped_tracker.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/text_elider.h"
#include "ui/native_theme/native_theme.h"

namespace views {

// static
const char Label::kViewClassName[] = "Label";
const int Label::kFocusBorderPadding = 1;

Label::Label() {
  Init(base::string16(), gfx::FontList());
}

Label::Label(const base::string16& text) {
  Init(text, gfx::FontList());
}

Label::Label(const base::string16& text, const gfx::FontList& font_list) {
  Init(text, font_list);
}

Label::~Label() {
}

void Label::SetFontList(const gfx::FontList& font_list) {
  is_first_paint_text_ = true;
  render_text_->SetFontList(font_list);
  ResetLayout();
}

void Label::SetText(const base::string16& new_text) {
  if (new_text == text())
    return;
  is_first_paint_text_ = true;
  render_text_->SetText(new_text);
  ResetLayout();
}

void Label::SetAutoColorReadabilityEnabled(bool enabled) {
  if (auto_color_readability_ == enabled)
    return;
  is_first_paint_text_ = true;
  auto_color_readability_ = enabled;
  RecalculateColors();
}

void Label::SetEnabledColor(SkColor color) {
  if (enabled_color_set_ && requested_enabled_color_ == color)
    return;
  is_first_paint_text_ = true;
  requested_enabled_color_ = color;
  enabled_color_set_ = true;
  RecalculateColors();
}

void Label::SetDisabledColor(SkColor color) {
  if (disabled_color_set_ && requested_disabled_color_ == color)
    return;
  is_first_paint_text_ = true;
  requested_disabled_color_ = color;
  disabled_color_set_ = true;
  RecalculateColors();
}

void Label::SetBackgroundColor(SkColor color) {
  if (background_color_set_ && background_color_ == color)
    return;
  is_first_paint_text_ = true;
  background_color_ = color;
  background_color_set_ = true;
  RecalculateColors();
}

void Label::SetShadows(const gfx::ShadowValues& shadows) {
  // TODO(mukai): early exit if the specified shadows are same.
  is_first_paint_text_ = true;
  render_text_->set_shadows(shadows);
  ResetLayout();
}

void Label::SetSubpixelRenderingEnabled(bool subpixel_rendering_enabled) {
  if (subpixel_rendering_enabled_ == subpixel_rendering_enabled)
    return;
  is_first_paint_text_ = true;
  subpixel_rendering_enabled_ = subpixel_rendering_enabled;
  RecalculateColors();
}

void Label::SetHorizontalAlignment(gfx::HorizontalAlignment alignment) {
  // If the UI layout is right-to-left, flip the alignment direction.
  if (base::i18n::IsRTL() &&
      (alignment == gfx::ALIGN_LEFT || alignment == gfx::ALIGN_RIGHT)) {
    alignment = (alignment == gfx::ALIGN_LEFT) ?
        gfx::ALIGN_RIGHT : gfx::ALIGN_LEFT;
  }
  if (horizontal_alignment() == alignment)
    return;
  is_first_paint_text_ = true;
  render_text_->SetHorizontalAlignment(alignment);
  ResetLayout();
}

void Label::SetLineHeight(int height) {
  if (line_height() == height)
    return;
  is_first_paint_text_ = true;
  render_text_->SetMinLineHeight(height);
  ResetLayout();
}

void Label::SetMultiLine(bool multi_line) {
  DCHECK(!multi_line || (elide_behavior_ == gfx::ELIDE_TAIL ||
                         elide_behavior_ == gfx::NO_ELIDE));
  if (this->multi_line() == multi_line)
    return;
  is_first_paint_text_ = true;
  multi_line_ = multi_line;
  if (render_text_->MultilineSupported())
    render_text_->SetMultiline(multi_line);
  render_text_->SetReplaceNewlineCharsWithSymbols(!multi_line);
  ResetLayout();
}

void Label::SetObscured(bool obscured) {
  if (this->obscured() == obscured)
    return;
  is_first_paint_text_ = true;
  render_text_->SetObscured(obscured);
  ResetLayout();
}

void Label::SetAllowCharacterBreak(bool allow_character_break) {
  if (allow_character_break_ == allow_character_break)
    return;
  is_first_paint_text_ = true;
  allow_character_break_ = allow_character_break;
  ResetLayout();
}

void Label::SetElideBehavior(gfx::ElideBehavior elide_behavior) {
  DCHECK(!multi_line() || (elide_behavior_ == gfx::ELIDE_TAIL ||
                           elide_behavior_ == gfx::NO_ELIDE));
  if (elide_behavior_ == elide_behavior)
    return;
  is_first_paint_text_ = true;
  elide_behavior_ = elide_behavior;
  ResetLayout();
}

void Label::SetTooltipText(const base::string16& tooltip_text) {
  DCHECK(handles_tooltips_);
  tooltip_text_ = tooltip_text;
}

void Label::SetHandlesTooltips(bool enabled) {
  handles_tooltips_ = enabled;
}

void Label::SizeToFit(int max_width) {
  DCHECK(multi_line());
  max_width_ = max_width;
  SizeToPreferredSize();
}

base::string16 Label::GetDisplayTextForTesting() {
  lines_.clear();
  MaybeBuildRenderTextLines();
  base::string16 result;
  if (lines_.empty())
    return result;
  result.append(lines_[0]->GetDisplayText());
  for (size_t i = 1; i < lines_.size(); ++i) {
    result.append(1, '\n');
    result.append(lines_[i]->GetDisplayText());
  }
  return result;
}

gfx::Insets Label::GetInsets() const {
  gfx::Insets insets = View::GetInsets();
  if (focusable()) {
    insets += gfx::Insets(kFocusBorderPadding, kFocusBorderPadding,
                          kFocusBorderPadding, kFocusBorderPadding);
  }
  return insets;
}

int Label::GetBaseline() const {
  return GetInsets().top() + font_list().GetBaseline();
}

gfx::Size Label::GetPreferredSize() const {
  // TODO(vadimt): Remove ScopedTracker below once crbug.com/431326 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION("431326 Label::GetPreferredSize"));

  // Return a size of (0, 0) if the label is not visible and if the
  // |collapse_when_hidden_| flag is set.
  // TODO(munjal): This logic probably belongs to the View class. But for now,
  // put it here since putting it in View class means all inheriting classes
  // need to respect the |collapse_when_hidden_| flag.
  if (!visible() && collapse_when_hidden_)
    return gfx::Size();

  if (multi_line() && max_width_ != 0 && !text().empty())
    return gfx::Size(max_width_, GetHeightForWidth(max_width_));

  gfx::Size size(GetTextSize());
  const gfx::Insets insets = GetInsets();
  size.Enlarge(insets.width(), insets.height());
  return size;
}

gfx::Size Label::GetMinimumSize() const {
  if (!visible() && collapse_when_hidden_)
    return gfx::Size();

  gfx::Size size(0, font_list().GetHeight());
  if (elide_behavior_ == gfx::ELIDE_HEAD ||
      elide_behavior_ == gfx::ELIDE_MIDDLE ||
      elide_behavior_ == gfx::ELIDE_TAIL ||
      elide_behavior_ == gfx::ELIDE_EMAIL) {
    size.set_width(gfx::Canvas::GetStringWidth(
        base::string16(gfx::kEllipsisUTF16), font_list()));
  }
  if (!multi_line())
    size.SetToMin(GetTextSize());
  size.Enlarge(GetInsets().width(), GetInsets().height());
  return size;
}

int Label::GetHeightForWidth(int w) const {
  if (!visible() && collapse_when_hidden_)
    return 0;

  w -= GetInsets().width();
  if (!multi_line() || text().empty() || w <= 0)
    return std::max(line_height(), font_list().GetHeight());

  int height = 0;
  // RenderText doesn't support character breaks.
  // TODO(mukai): remove this restriction.
  if (render_text_->MultilineSupported() && !allow_character_break_) {
    // SetDisplayRect() has a side effect for later calls of GetStringSize().
    // Be careful to invoke |render_text_->SetDisplayRect(gfx::Rect())| to
    // cancel this effect before the next time GetStringSize() is called.
    // It would be beneficial not to cancel here, considering that some layout
    // managers invoke GetHeightForWidth() for the same width multiple times
    // and |render_text_| can cache the height.
    render_text_->SetDisplayRect(gfx::Rect(0, 0, w, 0));
    height = render_text_->GetStringSize().height();
  } else {
    std::vector<base::string16> lines = GetLinesForWidth(w);
    height = lines.size() * std::max(line_height(), font_list().GetHeight());
  }
  height -= gfx::ShadowValue::GetMargin(render_text_->shadows()).height();
  return height + GetInsets().height();
}

void Label::Layout() {
  lines_.clear();
}

const char* Label::GetClassName() const {
  return kViewClassName;
}

View* Label::GetTooltipHandlerForPoint(const gfx::Point& point) {
  if (!handles_tooltips_ ||
      (tooltip_text_.empty() && !ShouldShowDefaultTooltip()))
    return NULL;

  return HitTestPoint(point) ? this : NULL;
}

bool Label::CanProcessEventsWithinSubtree() const {
  // Send events to the parent view for handling.
  return false;
}

void Label::GetAccessibleState(ui::AXViewState* state) {
  state->role = ui::AX_ROLE_STATIC_TEXT;
  state->AddStateFlag(ui::AX_STATE_READ_ONLY);
  // Note that |render_text_| is never elided (see the comment in Init() too).
  state->name = render_text_->GetDisplayText();
}

bool Label::GetTooltipText(const gfx::Point& p, base::string16* tooltip) const {
  if (!handles_tooltips_)
    return false;

  if (!tooltip_text_.empty()) {
    tooltip->assign(tooltip_text_);
    return true;
  }

  if (ShouldShowDefaultTooltip()) {
    // Note that |render_text_| is never elided (see the comment in Init() too).
    tooltip->assign(render_text_->GetDisplayText());
    return true;
  }

  return false;
}

void Label::OnEnabledChanged() {
  RecalculateColors();
}

void Label::PaintText(gfx::Canvas* canvas) {
  MaybeBuildRenderTextLines();
  for (size_t i = 0; i < lines_.size(); ++i)
    lines_[i]->Draw(canvas);
}

void Label::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  if (previous_bounds.size() != size())
    InvalidateLayout();
}

void Label::OnPaint(gfx::Canvas* canvas) {
  View::OnPaint(canvas);
  if (is_first_paint_text_) {
    // TODO(vadimt): Remove ScopedTracker below once crbug.com/431326 is fixed.
    tracked_objects::ScopedTracker tracking_profile(
        FROM_HERE_WITH_EXPLICIT_FUNCTION("431326 Label::PaintText first"));

    is_first_paint_text_ = false;
    PaintText(canvas);
  } else {
    // TODO(vadimt): Remove ScopedTracker below once crbug.com/431326 is fixed.
    tracked_objects::ScopedTracker tracking_profile(
        FROM_HERE_WITH_EXPLICIT_FUNCTION("431326 Label::PaintText not first"));

    PaintText(canvas);
  }
  if (HasFocus()) {
    gfx::Rect focus_bounds = GetLocalBounds();
    focus_bounds.Inset(-kFocusBorderPadding, -kFocusBorderPadding);
    canvas->DrawFocusRect(focus_bounds);
  }
}

void Label::OnNativeThemeChanged(const ui::NativeTheme* theme) {
  UpdateColorsFromTheme(theme);
}

void Label::OnDeviceScaleFactorChanged(float device_scale_factor) {
  View::OnDeviceScaleFactorChanged(device_scale_factor);
  // When the device scale factor is changed, some font rendering parameters is
  // changed (especially, hinting). The bounding box of the text has to be
  // re-computed based on the new parameters. See crbug.com/441439
  ResetLayout();
}

void Label::VisibilityChanged(View* starting_from, bool is_visible) {
  if (!is_visible)
    lines_.clear();
}

void Label::Init(const base::string16& text, const gfx::FontList& font_list) {
  render_text_.reset(gfx::RenderText::CreateInstance());
  render_text_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  render_text_->SetDirectionalityMode(gfx::DIRECTIONALITY_FROM_TEXT);
  // NOTE: |render_text_| should not be elided at all. This is used to keep some
  // properties and to compute the size of the string.
  render_text_->SetElideBehavior(gfx::NO_ELIDE);
  render_text_->SetFontList(font_list);
  render_text_->SetCursorEnabled(false);

  elide_behavior_ = gfx::ELIDE_TAIL;
  enabled_color_set_ = disabled_color_set_ = background_color_set_ = false;
  subpixel_rendering_enabled_ = true;
  auto_color_readability_ = true;
  multi_line_ = false;
  UpdateColorsFromTheme(ui::NativeTheme::instance());
  handles_tooltips_ = true;
  collapse_when_hidden_ = false;
  allow_character_break_ = false;
  max_width_ = 0;
  is_first_paint_text_ = true;
  SetText(text);
}

void Label::ResetLayout() {
  InvalidateLayout();
  PreferredSizeChanged();
  SchedulePaint();
  lines_.clear();
}

scoped_ptr<gfx::RenderText> Label::CreateRenderText(
    const base::string16& text,
    gfx::HorizontalAlignment alignment,
    gfx::DirectionalityMode directionality,
    gfx::ElideBehavior elide_behavior) {
  scoped_ptr<gfx::RenderText> render_text(
      render_text_->CreateInstanceOfSameType());
  render_text->SetHorizontalAlignment(alignment);
  render_text->SetDirectionalityMode(directionality);
  render_text->SetElideBehavior(elide_behavior);
  render_text->SetObscured(obscured());
  render_text->SetMinLineHeight(line_height());
  render_text->SetFontList(font_list());
  render_text->set_shadows(shadows());
  render_text->SetCursorEnabled(false);
  render_text->SetText(text);
  return render_text.Pass();
}

void Label::MaybeBuildRenderTextLines() {
  if (!lines_.empty())
    return;

  gfx::Rect rect = GetContentsBounds();
  if (rect.IsEmpty())
    return;

  gfx::HorizontalAlignment alignment = horizontal_alignment();
  gfx::DirectionalityMode directionality = render_text_->directionality_mode();
  if (multi_line()) {
    // Force the directionality and alignment of the first line on other lines.
    bool rtl =
        render_text_->GetDisplayTextDirection() == base::i18n::RIGHT_TO_LEFT;
    if (alignment == gfx::ALIGN_TO_HEAD)
      alignment = rtl ? gfx::ALIGN_RIGHT : gfx::ALIGN_LEFT;
    directionality =
        rtl ? gfx::DIRECTIONALITY_FORCE_RTL : gfx::DIRECTIONALITY_FORCE_LTR;
  }

  // Text eliding is not supported for multi-lined Labels.
  // TODO(mukai): Add multi-lined elided text support.
  gfx::ElideBehavior elide_behavior =
      multi_line() ? gfx::NO_ELIDE : elide_behavior_;
  if (!multi_line() ||
      (render_text_->MultilineSupported() && !allow_character_break_)) {
    scoped_ptr<gfx::RenderText> render_text =
        CreateRenderText(text(), alignment, directionality, elide_behavior);
    render_text->SetDisplayRect(rect);
    render_text->SetMultiline(multi_line());
    lines_.push_back(render_text.release());
  } else {
    std::vector<base::string16> lines = GetLinesForWidth(rect.width());
    if (lines.size() > 1)
      rect.set_height(std::max(line_height(), font_list().GetHeight()));

    const int bottom = GetContentsBounds().bottom();
    for (size_t i = 0; i < lines.size() && rect.y() <= bottom; ++i) {
      scoped_ptr<gfx::RenderText> line =
          CreateRenderText(lines[i], alignment, directionality, elide_behavior);
      line->SetDisplayRect(rect);
      lines_.push_back(line.release());
      rect.set_y(rect.y() + rect.height());
    }
    // Append the remaining text to the last visible line.
    for (size_t i = lines_.size(); i < lines.size(); ++i)
      lines_.back()->SetText(lines_.back()->text() + lines[i]);
  }
  RecalculateColors();
}

std::vector<base::string16> Label::GetLinesForWidth(int width) const {
  std::vector<base::string16> lines;
  const gfx::WordWrapBehavior wrap =
      allow_character_break_ ? gfx::WRAP_LONG_WORDS : gfx::TRUNCATE_LONG_WORDS;
  gfx::ElideRectangleText(render_text_->GetDisplayText(), font_list(), width,
                          std::numeric_limits<int>::max(), wrap, &lines);
  return lines;
}

gfx::Size Label::GetTextSize() const {
  gfx::Size size;
  if (text().empty()) {
    size = gfx::Size(0, std::max(line_height(), font_list().GetHeight()));
  } else if (!multi_line() ||
             (render_text_->MultilineSupported() && !allow_character_break_)) {
    // Cancel the display rect of |render_text_|. The display rect may be
    // specified in GetHeightForWidth(), and specifying empty Rect cancels
    // its effect. See also the comment in GetHeightForWidth().
    render_text_->SetDisplayRect(gfx::Rect());
    size = render_text_->GetStringSize();
  } else {
    // Get the natural text size, unelided and only wrapped on newlines.
    std::vector<base::string16> lines;
    base::SplitString(render_text_->GetDisplayText(), '\n', &lines);
    scoped_ptr<gfx::RenderText> render_text(gfx::RenderText::CreateInstance());
    render_text->SetFontList(font_list());
    for (size_t i = 0; i < lines.size(); ++i) {
      render_text->SetText(lines[i]);
      const gfx::Size line = render_text->GetStringSize();
      size.set_width(std::max(size.width(), line.width()));
      size.set_height(std::max(line_height(), size.height() + line.height()));
    }
  }
  const gfx::Insets shadow_margin = -gfx::ShadowValue::GetMargin(shadows());
  size.Enlarge(shadow_margin.width(), shadow_margin.height());
  return size;
}

void Label::RecalculateColors() {
  actual_enabled_color_ = auto_color_readability_ ?
      color_utils::GetReadableColor(requested_enabled_color_,
                                    background_color_) :
      requested_enabled_color_;
  actual_disabled_color_ = auto_color_readability_ ?
      color_utils::GetReadableColor(requested_disabled_color_,
                                    background_color_) :
      requested_disabled_color_;

  SkColor color = enabled() ? actual_enabled_color_ : actual_disabled_color_;
  bool subpixel_rendering_suppressed =
      SkColorGetA(background_color_) != 0xFF || !subpixel_rendering_enabled_;
  for (size_t i = 0; i < lines_.size(); ++i) {
    lines_[i]->SetColor(color);
    lines_[i]->set_subpixel_rendering_suppressed(subpixel_rendering_suppressed);
  }
  SchedulePaint();
}

void Label::UpdateColorsFromTheme(const ui::NativeTheme* theme) {
  if (!enabled_color_set_) {
    requested_enabled_color_ = theme->GetSystemColor(
        ui::NativeTheme::kColorId_LabelEnabledColor);
  }
  if (!disabled_color_set_) {
    requested_disabled_color_ = theme->GetSystemColor(
        ui::NativeTheme::kColorId_LabelDisabledColor);
  }
  if (!background_color_set_) {
    background_color_ = theme->GetSystemColor(
        ui::NativeTheme::kColorId_LabelBackgroundColor);
  }
  RecalculateColors();
}

bool Label::ShouldShowDefaultTooltip() const {
  const gfx::Size text_size = GetTextSize();
  const gfx::Size size = GetContentsBounds().size();
  return !obscured() && (text_size.width() > size.width() ||
                         (multi_line() && text_size.height() > size.height()));
}

}  // namespace views
