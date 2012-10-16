// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/label_button_border.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "grit/ui_resources.h"
#include "ui/base/animation/animation.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/button/label_button.h"

namespace {

// Preferred padding between content and edge.
static const int kPreferredPaddingHorizontal = 6;
static const int kPreferredPaddingVertical = 5;

// Preferred padding between content and edge for NativeTheme border.
static const int kPreferredNativeThemePaddingHorizontal = 12;
static const int kPreferredNativeThemePaddingVertical = 5;

const int kHoverImageSet[] = {
  IDR_TEXTBUTTON_HOVER_TOP_LEFT,
  IDR_TEXTBUTTON_HOVER_TOP,
  IDR_TEXTBUTTON_HOVER_TOP_RIGHT,
  IDR_TEXTBUTTON_HOVER_LEFT,
  IDR_TEXTBUTTON_HOVER_CENTER,
  IDR_TEXTBUTTON_HOVER_RIGHT,
  IDR_TEXTBUTTON_HOVER_BOTTOM_LEFT,
  IDR_TEXTBUTTON_HOVER_BOTTOM,
  IDR_TEXTBUTTON_HOVER_BOTTOM_RIGHT,
};

const int kPressedImageSet[] = {
  IDR_TEXTBUTTON_PRESSED_TOP_LEFT,
  IDR_TEXTBUTTON_PRESSED_TOP,
  IDR_TEXTBUTTON_PRESSED_TOP_RIGHT,
  IDR_TEXTBUTTON_PRESSED_LEFT,
  IDR_TEXTBUTTON_PRESSED_CENTER,
  IDR_TEXTBUTTON_PRESSED_RIGHT,
  IDR_TEXTBUTTON_PRESSED_BOTTOM_LEFT,
  IDR_TEXTBUTTON_PRESSED_BOTTOM,
  IDR_TEXTBUTTON_PRESSED_BOTTOM_RIGHT,
};

}  // namespace

namespace views {

LabelButtonBorder::LabelButtonBorder(NativeThemeDelegate* delegate)
    : native_theme_delegate_(delegate),
      native_theme_(false) {
  SetImages(CustomButton::BS_HOT, BorderImages(kHoverImageSet));
  SetImages(CustomButton::BS_PUSHED, BorderImages(kPressedImageSet));
}

LabelButtonBorder::~LabelButtonBorder() {}

void LabelButtonBorder::Paint(const View& view, gfx::Canvas* canvas) const {
  const CustomButton* button = static_cast<const CustomButton*>(&view);
  if (native_theme()) {
    PaintNativeTheme(view, canvas);
  } else if (native_theme_delegate_->GetThemeAnimation() &&
             native_theme_delegate_->GetThemeAnimation()->is_animating()) {
    // TODO(pkasting|msw): Crossfade between button state image sets.
    canvas->SaveLayerAlpha(static_cast<uint8>(native_theme_delegate_->
        GetThemeAnimation()->CurrentValueBetween(0, 255)));
    canvas->DrawColor(SkColorSetARGB(0x00, 0xFF, 0xFF, 0xFF),
                      SkXfermode::kClear_Mode);
    PaintImages(view, canvas, button->state());
    canvas->Restore();
  } else {
    PaintImages(view, canvas, button->state());
  }
}

void LabelButtonBorder::GetInsets(gfx::Insets* insets) const {
  if (native_theme()) {
    insets->Set(kPreferredNativeThemePaddingVertical,
                kPreferredNativeThemePaddingHorizontal,
                kPreferredNativeThemePaddingVertical,
                kPreferredNativeThemePaddingHorizontal);
  } else {
    insets->Set(kPreferredPaddingVertical, kPreferredPaddingHorizontal,
                kPreferredPaddingVertical, kPreferredPaddingHorizontal);
  }
}

LabelButtonBorder::BorderImages::BorderImages() {}

LabelButtonBorder::BorderImages::~BorderImages() {}

LabelButtonBorder::BorderImages::BorderImages(const int image_ids[]) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  top_left = *rb.GetImageSkiaNamed(image_ids[0]);
  top = *rb.GetImageSkiaNamed(image_ids[1]);
  top_right = *rb.GetImageSkiaNamed(image_ids[2]);
  left = *rb.GetImageSkiaNamed(image_ids[3]);
  center = *rb.GetImageSkiaNamed(image_ids[4]);
  right = *rb.GetImageSkiaNamed(image_ids[5]);
  bottom_left = *rb.GetImageSkiaNamed(image_ids[6]);
  bottom = *rb.GetImageSkiaNamed(image_ids[7]);
  bottom_right = *rb.GetImageSkiaNamed(image_ids[8]);
}

void LabelButtonBorder::SetImages(CustomButton::ButtonState state,
                                  const BorderImages& set) {
  images_[state] = set;
}

void LabelButtonBorder::PaintImages(const View& view,
                                    gfx::Canvas* canvas,
                                    CustomButton::ButtonState state) const {
  const BorderImages& set = images_[state];
  if (set.top_left.isNull())
    return;

  const int width = view.bounds().width();
  const int height = view.bounds().height();
  const int tl_width = set.top_left.width();
  const int tl_height = set.top_left.height();
  const int t_height = set.top.height();
  const int tr_height = set.top_right.height();
  const int l_width = set.left.width();
  const int r_width = set.right.width();
  const int bl_width = set.bottom_left.width();
  const int bl_height = set.bottom_left.height();
  const int b_height = set.bottom.height();
  const int br_width = set.bottom_right.width();
  const int br_height = set.bottom_right.height();

  canvas->DrawImageInt(set.top_left, 0, 0);
  canvas->DrawImageInt(set.top, 0, 0, set.top.width(), t_height, tl_width, 0,
      width - tl_width - set.top_right.width(), t_height, false);
  canvas->DrawImageInt(set.top_right, width - set.top_right.width(), 0);
  canvas->DrawImageInt(set.left, 0, 0, l_width, set.left.height(), 0,
      tl_height, tl_width, height - tl_height - bl_height, false);
  canvas->DrawImageInt(set.center, 0, 0, set.center.width(),
      set.center.height(), l_width, t_height, width - l_width - r_width,
      height - t_height - b_height, false);
  canvas->DrawImageInt(set.right, 0, 0, r_width, set.right.height(),
                       width - r_width, tr_height, r_width,
                       height - tr_height - br_height, false);
  canvas->DrawImageInt(set.bottom_left, 0, height - bl_height);
  canvas->DrawImageInt(set.bottom, 0, 0, set.bottom.width(), b_height,
                       bl_width, height - b_height,
                       width - bl_width - br_width, b_height, false);
  canvas->DrawImageInt(set.bottom_right, width - br_width,
                       height - br_height);
}

void LabelButtonBorder::PaintNativeTheme(const View& view,
                                         gfx::Canvas* canvas) const {
  const ui::NativeTheme* theme = ui::NativeTheme::instance();
  ui::NativeTheme::Part part = native_theme_delegate_->GetThemePart();
  gfx::Rect rect(native_theme_delegate_->GetThemePaintRect());

  ui::NativeTheme::State state;
  ui::NativeTheme::ExtraParams extra;
  const ui::Animation* animation = native_theme_delegate_->GetThemeAnimation();
  if (animation && animation->is_animating()) {
    // Paint the background state.
    state = native_theme_delegate_->GetBackgroundThemeState(&extra);
    theme->Paint(canvas->sk_canvas(), part, state, rect, extra);

    // Composite the foreground state above the background state.
    state = native_theme_delegate_->GetForegroundThemeState(&extra);
    const int alpha = animation->CurrentValueBetween(0, 255);
    canvas->SaveLayerAlpha(static_cast<uint8>(alpha));
    theme->Paint(canvas->sk_canvas(), part, state, rect, extra);
    canvas->Restore();
  } else {
    state = native_theme_delegate_->GetThemeState(&extra);
    theme->Paint(canvas->sk_canvas(), part, state, rect, extra);
  }

  // Draw the Views focus border for the native theme style.
  if (view.focus_border() && extra.button.is_focused)
    view.focus_border()->Paint(view, canvas);
}

}  // namespace views
