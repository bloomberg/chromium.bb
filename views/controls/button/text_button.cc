// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/button/text_button.h"

#include <algorithm>

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "ui/base/animation/throb_animation.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas_skia.h"
#include "views/controls/button/button.h"
#include "views/events/event.h"
#include "grit/app_resources.h"

namespace views {

// Default space between the icon and text.
static const int kDefaultIconTextSpacing = 5;

// Preferred padding between text and edge
static const int kPreferredPaddingHorizontal = 6;
static const int kPreferredPaddingVertical = 5;

// static
const SkColor TextButton::kEnabledColor = SkColorSetRGB(6, 45, 117);
// static
const SkColor TextButton::kHighlightColor = SkColorSetARGB(200, 255, 255, 255);
// static
const SkColor TextButton::kDisabledColor = SkColorSetRGB(161, 161, 146);
// static
const SkColor TextButton::kHoverColor = TextButton::kEnabledColor;

// How long the hover fade animation should last.
static const int kHoverAnimationDurationMs = 170;

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
  const TextButton* mb = static_cast<const TextButton*>(&view);
  int state = mb->state();

  // TextButton takes care of deciding when to call Paint.
  const MBBImageSet* set = &hot_set_;
  if (state == TextButton::BS_PUSHED)
    set = &pushed_set_;

  if (set) {
    Paint(view, canvas, *set);
  } else {
    // Do nothing
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
// TextButton, public:

TextButton::TextButton(ButtonListener* listener, const std::wstring& text)
    : CustomButton(listener),
      alignment_(ALIGN_LEFT),
      icon_placement_(ICON_ON_LEFT),
      font_(ResourceBundle::GetSharedInstance().GetFont(
          ResourceBundle::BaseFont)),
      color_(kEnabledColor),
      color_enabled_(kEnabledColor),
      color_disabled_(kDisabledColor),
      color_highlight_(kHighlightColor),
      color_hover_(kHoverColor),
      text_halo_color_(0),
      has_text_halo_(false),
      has_hover_icon_(false),
      has_pushed_icon_(false),
      max_width_(0),
      normal_has_border_(false),
      show_multiple_icon_states_(true),
      prefix_type_(PREFIX_NONE),
      icon_text_spacing_(kDefaultIconTextSpacing) {
  SetText(text);
  set_border(new TextButtonBorder);
  SetAnimationDuration(kHoverAnimationDurationMs);
}

TextButton::~TextButton() {
}

void TextButton::SetText(const std::wstring& text) {
  text_ = WideToUTF16Hack(text);
  SetAccessibleName(WideToUTF16Hack(text));
  UpdateTextSize();
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

void TextButton::SetFont(const gfx::Font& font) {
  font_ = font;
  UpdateTextSize();
}

void TextButton::SetEnabledColor(SkColor color) {
  color_enabled_ = color;
  UpdateColor();
}

void TextButton::SetDisabledColor(SkColor color) {
  color_disabled_ = color;
  UpdateColor();
}

void TextButton::SetHighlightColor(SkColor color) {
  color_highlight_ = color;
}

void TextButton::SetHoverColor(SkColor color) {
  color_hover_ = color;
}

void TextButton::SetTextHaloColor(SkColor color) {
  text_halo_color_ = color;
  has_text_halo_ = true;
}

void TextButton::ClearMaxTextSize() {
  max_text_size_ = text_size_;
}

void TextButton::SetNormalHasBorder(bool normal_has_border) {
  normal_has_border_ = normal_has_border;
}

void TextButton::SetShowMultipleIconStates(bool show_multiple_icon_states) {
  show_multiple_icon_states_ = show_multiple_icon_states;
}

void TextButton::Paint(gfx::Canvas* canvas, bool for_drag) {
  if (!for_drag) {
    PaintBackground(canvas);

    if (show_multiple_icon_states_ && hover_animation_->is_animating()) {
      // Draw the hover bitmap into an offscreen buffer, then blend it
      // back into the current canvas.
      canvas->SaveLayerAlpha(
          static_cast<int>(hover_animation_->GetCurrentValue() * 255));
      canvas->AsCanvasSkia()->drawARGB(0, 255, 255, 255,
                                       SkXfermode::kClear_Mode);
      PaintBorder(canvas);
      canvas->Restore();
    } else if ((show_multiple_icon_states_ &&
                (state_ == BS_HOT || state_ == BS_PUSHED)) ||
               (state_ == BS_NORMAL && normal_has_border_)) {
      PaintBorder(canvas);
    }

    PaintFocusBorder(canvas);
  }

  SkBitmap icon = icon_;
  if (show_multiple_icon_states_) {
    if (has_hover_icon_ && (state() == BS_HOT))
      icon = icon_hover_;
    else if (has_pushed_icon_ && (state() == BS_PUSHED))
      icon = icon_pushed_;
  }

  gfx::Insets insets = GetInsets();
  int available_width = width() - insets.width();
  int available_height = height() - insets.height();
  // Use the actual text (not max) size to properly center the text.
  int content_width = text_size_.width();
  if (icon.width() > 0) {
    content_width += icon.width();
    if (!text_.empty())
      content_width += icon_text_spacing_;
  }
  // Place the icon along the left edge.
  int icon_x;
  if (alignment_ == ALIGN_LEFT) {
    icon_x = insets.left();
  } else if (alignment_ == ALIGN_RIGHT) {
    icon_x = available_width - content_width;
  } else {
    icon_x =
        std::max(0, (available_width - content_width) / 2) + insets.left();
  }
  int text_x = icon_x;
  if (icon.width() > 0)
    text_x += icon.width() + icon_text_spacing_;
  const int text_width = std::min(text_size_.width(),
                                  width() - insets.right() - text_x);
  int text_y = (available_height - text_size_.height()) / 2 + insets.top();

  // If the icon should go on the other side, swap the elements.
  if (icon_placement_ == ICON_ON_RIGHT) {
    int new_text_x = icon_x;
    icon_x = new_text_x + text_width + icon_text_spacing_;
    text_x = new_text_x;
  }

  if (text_width > 0) {
    // Because the text button can (at times) draw multiple elements on the
    // canvas, we can not mirror the button by simply flipping the canvas as
    // doing this will mirror the text itself. Flipping the canvas will also
    // make the icons look wrong because icons are almost always represented as
    // direction insentisive bitmaps and such bitmaps should never be flipped
    // horizontally.
    //
    // Due to the above, we must perform the flipping manually for RTL UIs.
    gfx::Rect text_bounds(text_x, text_y, text_width, text_size_.height());
    text_bounds.set_x(GetMirroredXForRect(text_bounds));

    SkColor text_color = (show_multiple_icon_states_ &&
        (state() == BS_HOT || state() == BS_PUSHED)) ? color_hover_ : color_;

    int draw_string_flags = gfx::CanvasSkia::DefaultCanvasTextAlignment() |
        PrefixTypeToCanvasType(prefix_type_);

    if (for_drag) {
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

  if (icon.width() > 0) {
    int icon_y = (available_height - icon.height()) / 2 + insets.top();

    // Mirroring the icon position if necessary.
    gfx::Rect icon_bounds(icon_x, icon_y, icon.width(), icon.height());
    icon_bounds.set_x(GetMirroredXForRect(icon_bounds));
    canvas->DrawBitmapInt(icon, icon_bounds.x(), icon_bounds.y());
  }
}

void TextButton::UpdateColor() {
  color_ = IsEnabled() ? color_enabled_ : color_disabled_;
}

void TextButton::UpdateTextSize() {
  int width = 0, height = 0;
  gfx::CanvasSkia::SizeStringInt(
      text_, font_, &width, &height,
      gfx::Canvas::NO_ELLIPSIS | PrefixTypeToCanvasType(prefix_type_));

  // Add 2 extra pixels to width and height when text halo is used.
  if (has_text_halo_) {
    width += 2;
    height += 2;
  }

  text_size_.SetSize(width, font_.GetHeight());
  max_text_size_.SetSize(std::max(max_text_size_.width(), text_size_.width()),
                         std::max(max_text_size_.height(),
                                  text_size_.height()));
  PreferredSizeChanged();
}

////////////////////////////////////////////////////////////////////////////////
// TextButton, View overrides:

gfx::Size TextButton::GetPreferredSize() {
  gfx::Insets insets = GetInsets();

  // Use the max size to set the button boundaries.
  gfx::Size prefsize(max_text_size_.width() + icon_.width() + insets.width(),
                     std::max(max_text_size_.height(), icon_.height()) +
                         insets.height());

  if (icon_.width() > 0 && !text_.empty())
    prefsize.Enlarge(icon_text_spacing_, 0);

  if (max_width_ > 0)
    prefsize.set_width(std::min(max_width_, prefsize.width()));

  return prefsize;
}

gfx::Size TextButton::GetMinimumSize() {
  return max_text_size_;
}

void TextButton::SetEnabled(bool enabled) {
  if (enabled != IsEnabled()) {
    CustomButton::SetEnabled(enabled);
  }
  // We should always call UpdateColor() since the state of the button might be
  // changed by other functions like CustomButton::SetState().
  UpdateColor();
  SchedulePaint();
}

std::string TextButton::GetClassName() const {
  return kViewClassName;
}

void TextButton::Paint(gfx::Canvas* canvas) {
  Paint(canvas, false);
}

}  // namespace views
