// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/bounded_label.h"

#include <limits>

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "ui/base/text/text_elider.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/label.h"

namespace {

const size_t kPreferredLinesCacheSize = 10;
const size_t kPreferredSizeCacheSize = 10;

}  // namespace

namespace message_center {

// InnerBoundedLabel ///////////////////////////////////////////////////////////

// InnerBoundedLabel is a views::Label subclass that does all of the work for
// BoundedLabel. It is kept private to BoundedLabel to prevent outside code from
// calling a number of views::Label methods like SetFont() that would break
// BoundedLabel caching but can't be fixed because they're not virtual.
//
// TODO(dharcourt): Move the line limiting functionality to views::Label to make
// this unnecessary.

class InnerBoundedLabel : public views::Label {
 public:
  InnerBoundedLabel(const BoundedLabel& owner, size_t line_limit);
  virtual ~InnerBoundedLabel();

  void SetLineLimit(size_t limit);
  size_t GetLinesForWidth(int width);
  size_t GetPreferredLines();
  size_t GetActualLines();

  void ChangeNativeTheme(const ui::NativeTheme* theme);

  std::vector<string16> GetWrappedText(int width, size_t line_limit);

  // Overridden from views::Label.
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual int GetHeightForWidth(int width) OVERRIDE;

 protected:
  // Overridden from views::Label.
  virtual void OnBoundsChanged(const gfx::Rect& previous_bounds) OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

 private:
  gfx::Insets GetOwnerInsets();
  size_t GetPreferredLinesForWidth(int width);
  gfx::Size GetSizeForWidth(int width);
  int GetTextFlags();

  void ClearCaches();
  size_t GetCachedLinesForWidth(int width);
  void SetCachedLinesForWidth(int width, size_t lines);
  gfx::Size GetCachedSizeForWidth(int width);
  void SetCachedSizeForWidth(int width, gfx::Size size);

  const BoundedLabel* owner_;  // Weak reference.
  size_t line_limit_;
  std::map<int,size_t> lines_cache_;
  std::list<int> lines_widths_;  // Most recently used in front.
  std::map<int,gfx::Size> size_cache_;
  std::list<int> size_widths_;  // Most recently used in front.

  DISALLOW_COPY_AND_ASSIGN(InnerBoundedLabel);
};

InnerBoundedLabel::InnerBoundedLabel(const BoundedLabel& owner,
                                     size_t line_limit) {
  SetMultiLine(true);
  SetAllowCharacterBreak(true);
  SetElideBehavior(views::Label::ELIDE_AT_END);
  SetHorizontalAlignment(gfx::ALIGN_LEFT);
  set_collapse_when_hidden(true);
  owner_ = &owner;
  line_limit_ = line_limit;
}

InnerBoundedLabel::~InnerBoundedLabel() {
}

void InnerBoundedLabel::SetLineLimit(size_t limit) {
  if (limit != line_limit_) {
    std::swap(limit, line_limit_);
    if (GetPreferredLines() > line_limit_ || GetPreferredLines() > limit) {
      ClearCaches();
      PreferredSizeChanged();
    }
  }
}

size_t InnerBoundedLabel::GetLinesForWidth(int width) {
  return std::min(GetPreferredLinesForWidth(width), line_limit_);
}

size_t InnerBoundedLabel::GetPreferredLines() {
  return GetPreferredLinesForWidth(width());
}

size_t InnerBoundedLabel::GetActualLines() {
  return std::min(GetPreferredLinesForWidth(width()), line_limit_);
}

void InnerBoundedLabel::ChangeNativeTheme(const ui::NativeTheme* theme) {
  ClearCaches();
  OnNativeThemeChanged(theme);
}

std::vector<string16> InnerBoundedLabel::GetWrappedText(int width,
                                                        size_t line_limit) {
  // Short circuit simple case.
  if (line_limit == 0)
    return std::vector<string16>();

  // Restrict line_limit to ensure (line_limit + 1) * line_height <= INT_MAX.
  int line_height = std::max(font().GetHeight(), 2);  // At least 2 pixels.
  unsigned int max_limit = std::numeric_limits<int>::max() / line_height - 1;
  line_limit = (line_limit <= max_limit) ? line_limit : max_limit;

  // Split, using ui::IGNORE_LONG_WORDS instead of ui::WRAP_LONG_WORDS to
  // avoid an infinite loop in ui::ElideRectangleText() for small widths.
  std::vector<string16> lines;
  int height = static_cast<int>((line_limit + 1) * line_height);
  ui::ElideRectangleText(text(), font(), width, height,
                         ui::IGNORE_LONG_WORDS, &lines);

  // Elide if necessary.
  if (lines.size() > line_limit) {
    // Add an ellipsis to the last line. If this ellipsis makes the last line
    // too wide, that line will be further elided by the ui::ElideText below,
    // so for example "ABC" could become "ABC..." and then "AB...".
    string16 last = lines[line_limit - 1] + UTF8ToUTF16(ui::kEllipsis);
    if (font().GetStringWidth(last) > width)
      last = ui::ElideText(last, font(), width, ui::ELIDE_AT_END);
    lines.resize(line_limit - 1);
    lines.push_back(last);
  }

  return lines;
}

gfx::Size InnerBoundedLabel::GetPreferredSize() {
  return GetSizeForWidth(std::numeric_limits<int>::max());
}

int InnerBoundedLabel::GetHeightForWidth(int width) {
  return GetSizeForWidth(width).height();
}

void InnerBoundedLabel::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  ClearCaches();
  views::Label::OnBoundsChanged(previous_bounds);
}

void InnerBoundedLabel::OnPaint(gfx::Canvas* canvas) {
  views::Label::OnPaintBackground(canvas);
  views::Label::OnPaintFocusBorder(canvas);
  views::Label::OnPaintBorder(canvas);
  int height = GetSizeForWidth(width()).height() - GetOwnerInsets().height();
  if (height > 0) {
    gfx::Rect bounds = GetLocalBounds();
    bounds.Inset(GetOwnerInsets());
    bounds.set_y(bounds.y() + (bounds.height() - height) / 2);
    bounds.set_height(height);
    std::vector<string16> text = GetWrappedText(bounds.width(), line_limit_);
    PaintText(canvas, JoinString(text, '\n'), bounds, GetTextFlags());
  }
}

gfx::Insets InnerBoundedLabel::GetOwnerInsets() {
  return owner_->GetInsets();
}

size_t InnerBoundedLabel::GetPreferredLinesForWidth(int width) {
  size_t lines = GetCachedLinesForWidth(width);
  if (lines == std::numeric_limits<size_t>::max()) {
    int content_width = std::max(width - GetOwnerInsets().width(), 0);
    lines = GetWrappedText(content_width, lines).size();
    SetCachedLinesForWidth(width, lines);
  }
  return lines;
}

gfx::Size InnerBoundedLabel::GetSizeForWidth(int width) {
  gfx::Size size = GetCachedSizeForWidth(width);
  if (size.height() == std::numeric_limits<int>::max()) {
    int text_width = std::max(width - GetOwnerInsets().width(), 0);
    int text_height = 0;
    if (line_limit_ > 0) {
      std::vector<string16> text = GetWrappedText(text_width, line_limit_);
      gfx::Canvas::SizeStringInt(JoinString(text, '\n'), font(),
                                 &text_width, &text_height, GetTextFlags());
    }
    size.set_width(text_width + GetOwnerInsets().width());
    size.set_height(text_height + GetOwnerInsets().height());
    SetCachedSizeForWidth(width, size);
  }
  return size;
}

int InnerBoundedLabel::GetTextFlags() {
  int flags = gfx::Canvas::MULTI_LINE | gfx::Canvas::CHARACTER_BREAK;

  // We can't use subpixel rendering if the background is non-opaque.
  if (SkColorGetA(background_color()) != 0xFF)
    flags |= gfx::Canvas::NO_SUBPIXEL_RENDERING;

  if (directionality_mode() ==
      views::Label::AUTO_DETECT_DIRECTIONALITY) {
    base::i18n::TextDirection direction =
        base::i18n::GetFirstStrongCharacterDirection(text());
    if (direction == base::i18n::RIGHT_TO_LEFT)
      flags |= gfx::Canvas::FORCE_RTL_DIRECTIONALITY;
    else
      flags |= gfx::Canvas::FORCE_LTR_DIRECTIONALITY;
  }

  return flags | gfx::Canvas::TEXT_ALIGN_LEFT;
}

void InnerBoundedLabel::ClearCaches() {
  lines_cache_.clear();
  lines_widths_.clear();
  size_cache_.clear();
  size_widths_.clear();
}

size_t InnerBoundedLabel::GetCachedLinesForWidth(int width) {
  size_t lines = std::numeric_limits<size_t>::max();
  if (lines_cache_.find(width) != lines_cache_.end()) {
    lines = lines_cache_[width];
    lines_widths_.remove(width);
    lines_widths_.push_front(width);
  }
  return lines;
}

void InnerBoundedLabel::SetCachedLinesForWidth(int width, size_t lines) {
  if (lines_cache_.size() >= kPreferredLinesCacheSize) {
    lines_cache_.erase(lines_widths_.back());
    lines_widths_.pop_back();
  }
  lines_cache_[width] = lines;
  lines_widths_.push_front(width);
}

gfx::Size InnerBoundedLabel::GetCachedSizeForWidth(int width) {
  gfx::Size size(width, std::numeric_limits<int>::max());
  if (size_cache_.find(width) != size_cache_.end()) {
    size = size_cache_[width];
    size_widths_.remove(width);
    size_widths_.push_front(width);
  }
  return size;
}

void InnerBoundedLabel::SetCachedSizeForWidth(int width, gfx::Size size) {
  if (size_cache_.size() >= kPreferredLinesCacheSize) {
    size_cache_.erase(size_widths_.back());
    size_widths_.pop_back();
  }
  size_cache_[width] = size;
  size_widths_.push_front(width);
}

// BoundedLabel ///////////////////////////////////////////////////////////

BoundedLabel::BoundedLabel(const string16& text,
                           gfx::Font font,
                           size_t line_limit) {
  label_.reset(new InnerBoundedLabel(*this, line_limit));
  label_->SetFont(font);
  label_->SetText(text);
}

BoundedLabel::BoundedLabel(const string16& text, size_t line_limit) {
  label_.reset(new InnerBoundedLabel(*this, line_limit));
  label_->SetText(text);
}

BoundedLabel::~BoundedLabel() {
}

void BoundedLabel::SetLineLimit(size_t lines) {
  label_->SetLineLimit(lines);
}

size_t BoundedLabel::GetLinesForWidth(int width) {
  return visible() ? label_->GetLinesForWidth(width) : 0;
}

size_t BoundedLabel::GetPreferredLines() {
  return visible() ? label_->GetPreferredLines() : 0;
}

size_t BoundedLabel::GetActualLines() {
  return visible() ? label_->GetActualLines() : 0;
}

void BoundedLabel::SetColors(SkColor textColor, SkColor backgroundColor) {
  label_->SetEnabledColor(textColor);
  label_->SetBackgroundColor(backgroundColor);
}

int BoundedLabel::GetBaseline() const {
  return label_->GetBaseline();
}

gfx::Size BoundedLabel::GetPreferredSize() {
  return visible() ? label_->GetPreferredSize() : gfx::Size();
}

int BoundedLabel::GetHeightForWidth(int weight) {
  return visible() ? label_->GetHeightForWidth(weight) : 0;
}

void BoundedLabel::Paint(gfx::Canvas* canvas) {
  if (visible())
    label_->Paint(canvas);
}

bool BoundedLabel::HitTestRect(const gfx::Rect& rect) const {
  return label_->HitTestRect(rect);
}

void BoundedLabel::GetAccessibleState(ui::AccessibleViewState* state) {
  label_->GetAccessibleState(state);
}

void BoundedLabel::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  label_->SetBoundsRect(bounds());
  views::View::OnBoundsChanged(previous_bounds);
}

void BoundedLabel::OnNativeThemeChanged(const ui::NativeTheme* theme) {
  label_->ChangeNativeTheme(theme);
}

string16 BoundedLabel::GetWrappedTextForTest(int width, size_t line_limit) {
  return JoinString(label_->GetWrappedText(width, line_limit), '\n');
}

}  // namespace message_center
