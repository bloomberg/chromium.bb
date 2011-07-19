// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/button/image_button.h"

#include "base/utf_string_conversions.h"
#include "ui/base/animation/throb_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/skbitmap_operations.h"

namespace views {

static const int kDefaultWidth = 16;   // Default button width if no theme.
static const int kDefaultHeight = 14;  // Default button height if no theme.

////////////////////////////////////////////////////////////////////////////////
// ImageButton, public:

ImageButton::ImageButton(ButtonListener* listener)
    : CustomButton(listener),
      h_alignment_(ALIGN_LEFT),
      v_alignment_(ALIGN_TOP),
      preferred_size_(kDefaultWidth, kDefaultHeight) {
  // By default, we request that the gfx::Canvas passed to our View::OnPaint()
  // implementation is flipped horizontally so that the button's bitmaps are
  // mirrored when the UI directionality is right-to-left.
  EnableCanvasFlippingForRTLUI(true);
}

ImageButton::~ImageButton() {
}

void ImageButton::SetImage(ButtonState aState, const SkBitmap* anImage) {
  images_[aState] = anImage ? *anImage : SkBitmap();
  PreferredSizeChanged();
}

void ImageButton::SetBackground(SkColor color,
                                const SkBitmap* image,
                                const SkBitmap* mask) {
  if (!image || !mask) {
    background_image_.reset();
    return;
  }

  background_image_ =
      SkBitmapOperations::CreateButtonBackground(color, *image, *mask);
}

void ImageButton::SetImageAlignment(HorizontalAlignment h_align,
                                    VerticalAlignment v_align) {
  h_alignment_ = h_align;
  v_alignment_ = v_align;
  SchedulePaint();
}

////////////////////////////////////////////////////////////////////////////////
// ImageButton, View overrides:

gfx::Size ImageButton::GetPreferredSize() {
  if (!images_[BS_NORMAL].isNull())
    return gfx::Size(images_[BS_NORMAL].width(), images_[BS_NORMAL].height());
  return preferred_size_;
}

void ImageButton::OnPaint(gfx::Canvas* canvas) {
  // Call the base class first to paint any background/borders.
  View::OnPaint(canvas);

  SkBitmap img = GetImageToPaint();

  if (!img.isNull()) {
    int x = 0, y = 0;

    if (h_alignment_ == ALIGN_CENTER)
      x = (width() - img.width()) / 2;
    else if (h_alignment_ == ALIGN_RIGHT)
      x = width() - img.width();

    if (v_alignment_ == ALIGN_MIDDLE)
      y = (height() - img.height()) / 2;
    else if (v_alignment_ == ALIGN_BOTTOM)
      y = height() - img.height();

    if (!background_image_.empty())
      canvas->DrawBitmapInt(background_image_, x, y);
    canvas->DrawBitmapInt(img, x, y);
  }
  OnPaintFocusBorder(canvas);
}

////////////////////////////////////////////////////////////////////////////////
// ImageButton, protected:

SkBitmap ImageButton::GetImageToPaint() {
  SkBitmap img;

  if (!images_[BS_HOT].isNull() && hover_animation_->is_animating()) {
    img = SkBitmapOperations::CreateBlendedBitmap(images_[BS_NORMAL],
        images_[BS_HOT], hover_animation_->GetCurrentValue());
  } else {
    img = images_[state_];
  }

  return !img.isNull() ? img : images_[BS_NORMAL];
}

////////////////////////////////////////////////////////////////////////////////
// ToggleImageButton, public:

ToggleImageButton::ToggleImageButton(ButtonListener* listener)
    : ImageButton(listener),
      toggled_(false) {
}

ToggleImageButton::~ToggleImageButton() {
}

void ToggleImageButton::SetToggled(bool toggled) {
  if (toggled == toggled_)
    return;

  for (int i = 0; i < BS_COUNT; ++i) {
    SkBitmap temp = images_[i];
    images_[i] = alternate_images_[i];
    alternate_images_[i] = temp;
  }
  toggled_ = toggled;
  SchedulePaint();
}

void ToggleImageButton::SetToggledImage(ButtonState state,
                                        const SkBitmap* image) {
  if (toggled_) {
    images_[state] = image ? *image : SkBitmap();
    if (state_ == state)
      SchedulePaint();
  } else {
    alternate_images_[state] = image ? *image : SkBitmap();
  }
}

void ToggleImageButton::SetToggledTooltipText(const std::wstring& tooltip) {
  toggled_tooltip_text_ = WideToUTF16Hack(tooltip);
}

////////////////////////////////////////////////////////////////////////////////
// ToggleImageButton, ImageButton overrides:

void ToggleImageButton::SetImage(ButtonState state, const SkBitmap* image) {
  if (toggled_) {
    alternate_images_[state] = image ? *image : SkBitmap();
  } else {
    images_[state] = image ? *image : SkBitmap();
    if (state_ == state)
      SchedulePaint();
  }
  PreferredSizeChanged();
}

////////////////////////////////////////////////////////////////////////////////
// ToggleImageButton, View overrides:

bool ToggleImageButton::GetTooltipText(const gfx::Point& p,
                                       std::wstring* tooltip) {
  if (!toggled_ || toggled_tooltip_text_.empty())
    return Button::GetTooltipText(p, tooltip);

  *tooltip = UTF16ToWideHack(toggled_tooltip_text_);
  return true;
}

}  // namespace views
