// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/button/text_button.h"

#include <algorithm>

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "grit/app_resources.h"
#include "ui/base/animation/throb_animation.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas_skia.h"
#include "views/controls/button/button.h"
#include "views/events/event.h"
#include "views/widget/widget.h"

namespace views {

// Default space between the icon and text.
static const int kDefaultIconTextSpacing = 5;

// Preferred padding between text and edge
static const int kPreferredPaddingHorizontal = 6;
static const int kPreferredPaddingVertical = 5;

// static
const SkColor TextButtonBase::kEnabledColor = SkColorSetRGB(6, 45, 117);
// static
const SkColor TextButtonBase::kHighlightColor =
    SkColorSetARGB(200, 255, 255, 255);
// static
const SkColor TextButtonBase::kDisabledColor = SkColorSetRGB(161, 161, 146);
// static
const SkColor TextButtonBase::kHoverColor = TextButton::kEnabledColor;

// How long the hover fade animation should last.
static const int kHoverAnimationDurationMs = 170;

// static
const char TextButtonBase::kViewClassName[] = "views/TextButtonBase";
// static
const char TextButton::kViewClassName[] = "views/TextButton";

static int PrefixTypeToCanvasType(TextButton::PrefixType type) {
  switch (type) {
    case TextButton::PREFIX_HIDE:
      return gfx::Canvas::HIDE_PREFIX;
    case TextButton::PREFIX_SHOW:
      return gfx::Canvas::SHOW_PREFIX;
    case TextButton::PREFIX_NONE:
      return 0;
    default:
      NOTREACHED();
      return 0;
  }
}

////////////////////////////////////////////////////////////////////////////////
//
// TextButtonBorder - constructors, destructors, initialization
//
////////////////////////////////////////////////////////////////////////////////

TextButtonBorder::TextButtonBorder() {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  hot_set_.top_left = rb.GetBitmapNamed(IDR_TEXTBUTTON_TOP_LEFT_H);
  hot_set_.top = rb.GetBitmapNamed(IDR_TEXTBUTTON_TOP_H);
  hot_set_.top_right = rb.GetBitmapNamed(IDR_TEXTBUTTON_TOP_RIGHT_H);
  hot_set_.left = rb.GetBitmapNamed(IDR_TEXTBUTTON_LEFT_H);
  hot_set_.center = rb.GetBitmapNamed(IDR_TEXTBUTTON_CENTER_H);
  hot_set_.right = rb.GetBitmapNamed(IDR_TEXTBUTTON_RIGHT_H);
  hot_set_.bottom_left = rb.GetBitmapNamed(IDR_TEXTBUTTON_BOTTOM_LEFT_H);
  hot_set_.bottom = rb.GetBitmapNamed(IDR_TEXTBUTTON_BOTTOM_H);
  hot_set_.bottom_right = rb.GetBitmapNamed(IDR_TEXTBUTTON_BOTTOM_RIGHT_H);

  pushed_set_.top_left = rb.GetBitmapNamed(IDR_TEXTBUTTON_TOP_LEFT_P);
  pushed_set_.top = rb.GetBitmapNamed(IDR_TEXTBUTTON_TOP_P);
  pushed_set_.top_right = rb.GetBitmapNamed(IDR_TEXTBUTTON_TOP_RIGHT_P);
  pushed_set_.left = rb.GetBitmapNamed(IDR_TEXTBUTTON_LEFT_P);
  pushed_set_.center = rb.GetBitmapNamed(IDR_TEXTBUTTON_CENTER_P);
  pushed_set_.right = rb.GetBitmapNamed(IDR_TEXTBUTTON_RIGHT_P);
  pushed_set_.bottom_left = rb.GetBitmapNamed(IDR_TEXTBUTTON_BOTTOM_LEFT_P);
  pushed_set_.bottom = rb.GetBitmapNamed(IDR_TEXTBUTTON_BOTTOM_P);
  pushed_set_.bottom_right = rb.GetBitmapNamed(IDR_TEXTBUTTON_BOTTOM_RIGHT_P);
}

TextButtonBorder::~TextButtonBorder() {
}


////////////////////////////////////////////////////////////////////////////////
//
// TextButtonBorder - painting
//
////////////////////////////////////////////////////////////////////////////////
void TextButtonBorder::Paint(const View& view, gfx::Canvas* canvas) const {
  const TextButtonBase* mb = static_cast<const TextButton*>(&view);
  int state = mb->state();

  // TextButton takes care of deciding when to call Paint.
  const MBBImageSet* set = &hot_set_;
  if (state == TextButton::BS_PUSHED)
    set = &pushed_set_;

  bool is_animating = mb->GetAnimation()->is_animating();
  bool show_mult_icons = mb->show_multiple_icon_states();
  bool do_paint = (show_mult_icons && is_animating) ||
      (show_mult_icons && (state == TextButton::BS_HOT ||
                           state == TextButton::BS_PUSHED)) ||
      (state == TextButton::BS_NORMAL && mb->normal_has_border());
  if (set && do_paint) {
    if (is_animating) {
      canvas->SaveLayerAlpha(
          static_cast<uint8>(mb->GetAnimation()->CurrentValueBetween(0, 255)));
      canvas->AsCanvasSkia()->drawARGB(0, 255, 255, 255,
                                       SkXfermode::kClear_Mode);
    }

    Paint(view, canvas, *set);

    if (is_animating)
      canvas->Restore();
  }
}

void TextButtonBorder::Paint(const View& view, gfx::Canvas* canvas,
    const MBBImageSet& set) const {
  gfx::Rect bounds = view.bounds();

  // Draw the top left image
  canvas->DrawBitmapInt(*set.top_left, 0, 0);

  // Tile the top image
  canvas->TileImageInt(
      *set.top,
      set.top_left->width(),
      0,
      bounds.width() - set.top_right->width() - set.top_left->width(),
      set.top->height());

  // Draw the top right image
  canvas->DrawBitmapInt(*set.top_right,
                        bounds.width() - set.top_right->width(), 0);

  // Tile the left image
  canvas->TileImageInt(
      *set.left,
      0,
      set.top_left->height(),
      set.top_left->width(),
      bounds.height() - set.top->height() - set.bottom_left->height());

  // Tile the center image
  canvas->TileImageInt(
      *set.center,
      set.left->width(),
      set.top->height(),
      bounds.width() - set.right->width() - set.left->width(),
      bounds.height() - set.bottom->height() - set.top->height());

  // Tile the right image
  canvas->TileImageInt(
      *set.right,
      bounds.width() - set.right->width(),
      set.top_right->height(),
      bounds.width(),
      bounds.height() - set.bottom_right->height() -
          set.top_right->height());

  // Draw the bottom left image
  canvas->DrawBitmapInt(*set.bottom_left,
                        0,
                        bounds.height() - set.bottom_left->height());

  // Tile the bottom image
  canvas->TileImageInt(
      *set.bottom,
      set.bottom_left->width(),
      bounds.height() - set.bottom->height(),
      bounds.width() - set.bottom_right->width() -
          set.bottom_left->width(),
      set.bottom->height());

  // Draw the bottom right image
  canvas->DrawBitmapInt(*set.bottom_right,
                        bounds.width() - set.bottom_right->width(),
                        bounds.height() -  set.bottom_right->height());
}

void TextButtonBorder::GetInsets(gfx::Insets* insets) const {
  insets->Set(kPreferredPaddingVertical, kPreferredPaddingHorizontal,
              kPreferredPaddingVertical, kPreferredPaddingHorizontal);
}

////////////////////////////////////////////////////////////////////////////////
//
// TextButtonNativeThemeBorder
//
////////////////////////////////////////////////////////////////////////////////

TextButtonNativeThemeBorder::TextButtonNativeThemeBorder(
    NativeThemeDelegate* delegate)
    : delegate_(delegate) {
}

TextButtonNativeThemeBorder::~TextButtonNativeThemeBorder() {
}

void TextButtonNativeThemeBorder::Paint(const View& view,
                                        gfx::Canvas* canvas) const {
  const TextButtonBase* tb = static_cast<const TextButton*>(&view);
  const gfx::NativeTheme* native_theme = gfx::NativeTheme::instance();
  gfx::NativeTheme::Part part = delegate_->GetThemePart();
  gfx::CanvasSkia* skia_canvas = canvas->AsCanvasSkia();
  gfx::Rect rect(delegate_->GetThemePaintRect());

  if (tb->show_multiple_icon_states() &&
      delegate_->GetThemeAnimation() != NULL &&
      delegate_->GetThemeAnimation()->is_animating()) {
    // Paint background state.
    gfx::NativeTheme::ExtraParams prev_extra;
    gfx::NativeTheme::State prev_state =
        delegate_->GetBackgroundThemeState(&prev_extra);
    native_theme->Paint(skia_canvas, part, prev_state, rect, prev_extra);

    // Composite foreground state above it.
    gfx::NativeTheme::ExtraParams extra;
    gfx::NativeTheme::State state = delegate_->GetForegroundThemeState(&extra);
    int alpha = delegate_->GetThemeAnimation()->CurrentValueBetween(0, 255);
    skia_canvas->SaveLayerAlpha(static_cast<uint8>(alpha));
    native_theme->Paint(skia_canvas, part, state, rect, extra);
    skia_canvas->Restore();
  } else {
    gfx::NativeTheme::ExtraParams extra;
    gfx::NativeTheme::State state = delegate_->GetThemeState(&extra);
    native_theme->Paint(skia_canvas, part, state, rect, extra);
  }
}

void TextButtonNativeThemeBorder::GetInsets(gfx::Insets* insets) const {
  insets->Set(kPreferredPaddingVertical, kPreferredPaddingHorizontal,
              kPreferredPaddingVertical, kPreferredPaddingHorizontal);
}


////////////////////////////////////////////////////////////////////////////////
// TextButtonBase, public:

TextButtonBase::TextButtonBase(ButtonListener* listener,
                               const std::wstring& text)
    : CustomButton(listener),
      alignment_(ALIGN_LEFT),
      font_(ResourceBundle::GetSharedInstance().GetFont(
          ResourceBundle::BaseFont)),
      color_(kEnabledColor),
      color_enabled_(kEnabledColor),
      color_disabled_(kDisabledColor),
      color_highlight_(kHighlightColor),
      color_hover_(kHoverColor),
      text_halo_color_(0),
      has_text_halo_(false),
      active_text_shadow_color_(0),
      inactive_text_shadow_color_(0),
      has_shadow_(false),
      shadow_offset_(gfx::Point(1, 1)),
      max_width_(0),
      normal_has_border_(false),
      show_multiple_icon_states_(true),
      is_default_(false),
      multi_line_(false),
      prefix_type_(PREFIX_NONE) {
  SetText(text);
  SetAnimationDuration(kHoverAnimationDurationMs);
}

TextButtonBase::~TextButtonBase() {
}

void TextButtonBase::SetIsDefault(bool is_default) {
  if (is_default == is_default_)
    return;
  is_default_ = is_default;
  if (is_default_)
    AddAccelerator(Accelerator(ui::VKEY_RETURN, false, false, false));
  else
    RemoveAccelerator(Accelerator(ui::VKEY_RETURN, false, false, false));
  SchedulePaint();
}

void TextButtonBase::SetText(const std::wstring& text) {
  text_ = WideToUTF16Hack(text);
  SetAccessibleName(WideToUTF16Hack(text));
  UpdateTextSize();
}

void TextButtonBase::SetFont(const gfx::Font& font) {
  font_ = font;
  UpdateTextSize();
}

void TextButtonBase::SetEnabledColor(SkColor color) {
  color_enabled_ = color;
  UpdateColor();
}

void TextButtonBase::SetDisabledColor(SkColor color) {
  color_disabled_ = color;
  UpdateColor();
}

void TextButtonBase::SetHighlightColor(SkColor color) {
  color_highlight_ = color;
}

void TextButtonBase::SetHoverColor(SkColor color) {
  color_hover_ = color;
}

void TextButtonBase::SetTextHaloColor(SkColor color) {
  text_halo_color_ = color;
  has_text_halo_ = true;
}

void TextButtonBase::SetTextShadowColors(SkColor active_color,
                                         SkColor inactive_color) {
  active_text_shadow_color_ = active_color;
  inactive_text_shadow_color_ = inactive_color;
  has_shadow_ = true;
}

void TextButtonBase::SetTextShadowOffset(int x, int y) {
  shadow_offset_.SetPoint(x, y);
}

void TextButtonBase::ClearMaxTextSize() {
  max_text_size_ = text_size_;
}

void TextButtonBase::SetNormalHasBorder(bool normal_has_border) {
  normal_has_border_ = normal_has_border;
}

void TextButtonBase::SetShowMultipleIconStates(bool show_multiple_icon_states) {
  show_multiple_icon_states_ = show_multiple_icon_states;
}

void TextButtonBase::ClearEmbellishing() {
  has_shadow_ = false;
  has_text_halo_ = false;
}

void TextButtonBase::SetMultiLine(bool multi_line) {
  if (multi_line != multi_line_) {
    multi_line_ = multi_line;
    UpdateTextSize();
    PreferredSizeChanged();
    SchedulePaint();
  }
}

gfx::Size TextButtonBase::GetPreferredSize() {
  gfx::Insets insets = GetInsets();

  // Use the max size to set the button boundaries.
  gfx::Size prefsize(max_text_size_.width() + insets.width(),
                     max_text_size_.height() + insets.height());

  if (max_width_ > 0)
    prefsize.set_width(std::min(max_width_, prefsize.width()));

  return prefsize;
}

void TextButtonBase::OnPaint(gfx::Canvas* canvas) {
  PaintButton(canvas, PB_NORMAL);
}

const ui::Animation* TextButtonBase::GetAnimation() const {
  return hover_animation_.get();
}

void TextButtonBase::UpdateColor() {
  color_ = IsEnabled() ? color_enabled_ : color_disabled_;
}

void TextButtonBase::UpdateTextSize() {
  int h = font_.GetHeight();
  int w = multi_line_ ? width() : 0;
  int flags = ComputeCanvasStringFlags();
  if (!multi_line_)
    flags |= gfx::Canvas::NO_ELLIPSIS;

  gfx::CanvasSkia::SizeStringInt(text_, font_, &w, &h, flags);

  // Add 2 extra pixels to width and height when text halo is used.
  if (has_text_halo_) {
    w += 2;
    h += 2;
  }

  text_size_.SetSize(w, h);
  max_text_size_.SetSize(std::max(max_text_size_.width(), text_size_.width()),
                         std::max(max_text_size_.height(),
                                  text_size_.height()));
  PreferredSizeChanged();
}

int TextButtonBase::ComputeCanvasStringFlags() const {
  int flags = PrefixTypeToCanvasType(prefix_type_);

  if (multi_line_) {
    flags |= gfx::Canvas::MULTI_LINE;

    switch (alignment_) {
      case ALIGN_LEFT:
        flags |= gfx::Canvas::TEXT_ALIGN_LEFT;
        break;
      case ALIGN_RIGHT:
        flags |= gfx::Canvas::TEXT_ALIGN_RIGHT;
        break;
      case ALIGN_CENTER:
        flags |= gfx::Canvas::TEXT_ALIGN_CENTER;
        break;
    }
  }

  return flags;
}

void TextButtonBase::GetExtraParams(
    gfx::NativeTheme::ExtraParams* params) const {
  params->button.checked = false;
  params->button.indeterminate = false;
  params->button.is_default = false;
  params->button.has_border = false;
  params->button.classic_state = 0;
  params->button.background_color = kEnabledColor;
}

gfx::Rect TextButtonBase::GetContentBounds(int extra_width) const {
  gfx::Insets insets = GetInsets();
  int available_width = width() - insets.width();
  int content_width = text_size_.width() + extra_width;
  int content_x = 0;
  switch(alignment_) {
    case ALIGN_LEFT:
      content_x = insets.left();
      break;
    case ALIGN_RIGHT:
      content_x = width() - insets.right() - content_width;
      if (content_x < insets.left())
        content_x = insets.left();
      break;
    case ALIGN_CENTER:
      content_x = insets.left() + std::max(0,
          (available_width - content_width) / 2);
      break;
  }
  content_width = std::min(content_width,
                           width() - insets.right() - content_x);
  int available_height = height() - insets.height();
  int content_y = (available_height - text_size_.height()) / 2 + insets.top();

  gfx::Rect bounds(content_x, content_y, content_width, text_size_.height());
  return bounds;
}

gfx::Rect TextButtonBase::GetTextBounds() const {
  return GetContentBounds(0);
}

void TextButtonBase::PaintButton(gfx::Canvas* canvas, PaintButtonMode mode) {
  if (mode == PB_NORMAL) {
    OnPaintBackground(canvas);
    OnPaintBorder(canvas);
    OnPaintFocusBorder(canvas);
  }

  gfx::Rect text_bounds(GetTextBounds());
  if (text_bounds.width() > 0) {
    // Because the text button can (at times) draw multiple elements on the
    // canvas, we can not mirror the button by simply flipping the canvas as
    // doing this will mirror the text itself. Flipping the canvas will also
    // make the icons look wrong because icons are almost always represented as
    // direction-insensitive bitmaps and such bitmaps should never be flipped
    // horizontally.
    //
    // Due to the above, we must perform the flipping manually for RTL UIs.
    text_bounds.set_x(GetMirroredXForRect(text_bounds));

    SkColor text_color = (show_multiple_icon_states_ &&
        (state() == BS_HOT || state() == BS_PUSHED)) ? color_hover_ : color_;

    int draw_string_flags = gfx::CanvasSkia::DefaultCanvasTextAlignment() |
        ComputeCanvasStringFlags();

    if (mode == PB_FOR_DRAG) {
#if defined(OS_WIN)
      // TODO(erg): Either port DrawStringWithHalo to linux or find an
      // alternative here.
      canvas->AsCanvasSkia()->DrawStringWithHalo(
          text_, font_, text_color, color_highlight_, text_bounds.x(),
          text_bounds.y(), text_bounds.width(), text_bounds.height(),
          draw_string_flags);
#else
      canvas->DrawStringInt(text_,
                            font_,
                            text_color,
                            text_bounds.x(),
                            text_bounds.y(),
                            text_bounds.width(),
                            text_bounds.height(),
                            draw_string_flags);
#endif
    } else if (has_text_halo_) {
      canvas->AsCanvasSkia()->DrawStringWithHalo(
          text_, font_, text_color, text_halo_color_,
          text_bounds.x(), text_bounds.y(), text_bounds.width(),
          text_bounds.height(), draw_string_flags);
    } else if (has_shadow_) {
      SkColor shadow_color =
          GetWidget()->IsActive() ? active_text_shadow_color_ :
                                    inactive_text_shadow_color_;
      canvas->DrawStringInt(text_,
                            font_,
                            shadow_color,
                            text_bounds.x() + shadow_offset_.x(),
                            text_bounds.y() + shadow_offset_.y(),
                            text_bounds.width(),
                            text_bounds.height(),
                            draw_string_flags);
      canvas->DrawStringInt(text_,
                            font_,
                            text_color,
                            text_bounds.x(),
                            text_bounds.y(),
                            text_bounds.width(),
                            text_bounds.height(),
                            draw_string_flags);
    } else {
      canvas->DrawStringInt(text_,
                            font_,
                            text_color,
                            text_bounds.x(),
                            text_bounds.y(),
                            text_bounds.width(),
                            text_bounds.height(),
                            draw_string_flags);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// TextButtonBase, View overrides:

gfx::Size TextButtonBase::GetMinimumSize() {
  return max_text_size_;
}

void TextButtonBase::OnEnabledChanged() {
  // We should always call UpdateColor() since the state of the button might be
  // changed by other functions like CustomButton::SetState().
  UpdateColor();
  CustomButton::OnEnabledChanged();
}

std::string TextButtonBase::GetClassName() const {
  return kViewClassName;
}

////////////////////////////////////////////////////////////////////////////////
// TextButtonBase, NativeThemeDelegate overrides:

gfx::Rect TextButtonBase::GetThemePaintRect() const {
  return bounds();
}

gfx::NativeTheme::State TextButtonBase::GetThemeState(
    gfx::NativeTheme::ExtraParams* params) const {
  GetExtraParams(params);
  switch(state()) {
    case BS_DISABLED:
      return gfx::NativeTheme::kDisabled;
    case BS_NORMAL:
      return gfx::NativeTheme::kNormal;
    case BS_HOT:
      return gfx::NativeTheme::kHovered;
    case BS_PUSHED:
      return gfx::NativeTheme::kPressed;
    default:
      NOTREACHED() << "Unknown state: " << state();
      return gfx::NativeTheme::kNormal;
  }
}

const ui::Animation* TextButtonBase::GetThemeAnimation() const {
  return hover_animation_.get();
}

gfx::NativeTheme::State TextButtonBase::GetBackgroundThemeState(
  gfx::NativeTheme::ExtraParams* params) const {
  GetExtraParams(params);
  return gfx::NativeTheme::kNormal;
}

gfx::NativeTheme::State TextButtonBase::GetForegroundThemeState(
  gfx::NativeTheme::ExtraParams* params) const {
  GetExtraParams(params);
  return gfx::NativeTheme::kHovered;
}

////////////////////////////////////////////////////////////////////////////////
//
// TextButton
//
////////////////////////////////////////////////////////////////////////////////

TextButton::TextButton(ButtonListener* listener,
                       const std::wstring& text)
    : TextButtonBase(listener, text),
      icon_placement_(ICON_ON_LEFT),
      has_hover_icon_(false),
      has_pushed_icon_(false),
      icon_text_spacing_(kDefaultIconTextSpacing) {
  set_border(new TextButtonBorder);
}

TextButton::~TextButton() {
}

void TextButton::SetIcon(const SkBitmap& icon) {
  icon_ = icon;
}

void TextButton::SetHoverIcon(const SkBitmap& icon) {
  icon_hover_ = icon;
  has_hover_icon_ = true;
}

void TextButton::SetPushedIcon(const SkBitmap& icon) {
  icon_pushed_ = icon;
  has_pushed_icon_ = true;
}

gfx::Size TextButton::GetPreferredSize() {
  gfx::Size prefsize(TextButtonBase::GetPreferredSize());
  prefsize.Enlarge(icon_.width(), 0);
  prefsize.set_height(std::max(prefsize.height(), icon_.height()));

  // Use the max size to set the button boundaries.
  if (icon_.width() > 0 && !text_.empty())
    prefsize.Enlarge(icon_text_spacing_, 0);

  if (max_width_ > 0)
    prefsize.set_width(std::min(max_width_, prefsize.width()));

  return prefsize;
}

void TextButton::PaintButton(gfx::Canvas* canvas, PaintButtonMode mode) {
  TextButtonBase::PaintButton(canvas, mode);

  SkBitmap icon = icon_;
  if (show_multiple_icon_states_) {
    if (has_hover_icon_ && (state() == BS_HOT))
      icon = icon_hover_;
    else if (has_pushed_icon_ && (state() == BS_PUSHED))
      icon = icon_pushed_;
  }

  if (icon.width() > 0) {
    gfx::Rect text_bounds = GetTextBounds();
    int icon_x;
    int spacing = text_.empty() ? 0 : icon_text_spacing_;
    if (icon_placement_ == ICON_ON_LEFT) {
      icon_x = text_bounds.x() - icon.width() - spacing;
    } else {
      icon_x = text_bounds.right() + spacing;
    }

    gfx::Insets insets = GetInsets();
    int available_height = height() - insets.height();
    int icon_y = (available_height - icon.height()) / 2 + insets.top();

    // Mirroring the icon position if necessary.
    gfx::Rect icon_bounds(icon_x, icon_y, icon.width(), icon.height());
    icon_bounds.set_x(GetMirroredXForRect(icon_bounds));
    canvas->DrawBitmapInt(icon, icon_bounds.x(), icon_bounds.y());
  }
}

std::string TextButton::GetClassName() const {
  return kViewClassName;
}

gfx::NativeTheme::Part TextButton::GetThemePart() const {
  return gfx::NativeTheme::kPushButton;
}

void TextButton::GetExtraParams(gfx::NativeTheme::ExtraParams* params) const {
  params->button.checked = false;
  params->button.indeterminate = false;
  params->button.is_default = is_default_;
  params->button.has_border = false;
  params->button.classic_state = 0;
  params->button.background_color = kEnabledColor;
}

gfx::Rect TextButton::GetTextBounds() const {
  int extra_width = 0;

  SkBitmap icon = icon_;
  if (show_multiple_icon_states_) {
    if (has_hover_icon_ && (state() == BS_HOT))
      icon = icon_hover_;
    else if (has_pushed_icon_ && (state() == BS_PUSHED))
      icon = icon_pushed_;
  }

  if (icon.width() > 0)
    extra_width = icon.width() + (text_.empty() ? 0 : icon_text_spacing_);

  gfx::Rect bounds(GetContentBounds(extra_width));

  if (extra_width > 0) {
    // Make sure the icon is always fully visible.
    if (icon_placement_ == ICON_ON_LEFT) {
      bounds.Inset(extra_width, 0, 0, 0);
    } else {
      bounds.Inset(0, 0, extra_width, 0);
    }
  }

  return bounds;
}

}  // namespace views


