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
#include "ui/views/native_theme_delegate.h"

namespace {

// Preferred padding between content and edge.
static const int kPreferredPaddingHorizontal = 6;
static const int kPreferredPaddingVertical = 5;

// Preferred padding between content and edge for NativeTheme border.
static const int kPreferredNativeThemePaddingHorizontal = 12;
static const int kPreferredNativeThemePaddingVertical = 5;

}  // namespace

namespace views {

LabelButtonBorder::LabelButtonBorder() : native_theme_(false) {
  SetImages(CustomButton::BS_HOT, BorderImages(BorderImages::kHot));
  SetImages(CustomButton::BS_PUSHED, BorderImages(BorderImages::kPushed));
}

LabelButtonBorder::~LabelButtonBorder() {}

void LabelButtonBorder::Paint(const View& view, gfx::Canvas* canvas) const {
  const LabelButton* button = static_cast<const LabelButton*>(&view);
  if (native_theme())
    PaintNativeTheme(button, canvas);
  else
    PaintImages(button, canvas);
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

void LabelButtonBorder::SetImages(CustomButton::ButtonState state,
                                  const BorderImages& set) {
  images_[state] = set;
}

void LabelButtonBorder::PaintImages(const LabelButton* button,
                                    gfx::Canvas* canvas) const {
  const BorderImages& set = images_[button->state()];
  if (!set.top_left.isNull()) {
    const ui::Animation* animation =
        static_cast<const NativeThemeDelegate*>(button)->GetThemeAnimation();
    if (animation && animation->is_animating()) {
      // TODO(msw): Crossfade between image sets; no-op for equivalent images.
      const int alpha = animation->CurrentValueBetween(0, 255);
      canvas->SaveLayerAlpha(static_cast<uint8>(alpha));
      set.Paint(button->GetLocalBounds(), canvas);
      canvas->Restore();
    } else {
      set.Paint(button->GetLocalBounds(), canvas);
    }
  }
}

void LabelButtonBorder::PaintNativeTheme(const LabelButton* button,
                                         gfx::Canvas* canvas) const {
  const NativeThemeDelegate* native_theme_delegate =
      static_cast<const NativeThemeDelegate*>(button);
  ui::NativeTheme::Part part = native_theme_delegate->GetThemePart();
  gfx::Rect rect(native_theme_delegate->GetThemePaintRect());

  ui::NativeTheme::State state;
  ui::NativeTheme::ExtraParams extra;
  const ui::NativeTheme* theme = ui::NativeTheme::instance();
  const ui::Animation* animation = native_theme_delegate->GetThemeAnimation();
  if (animation && animation->is_animating()) {
    // Paint the background state.
    state = native_theme_delegate->GetBackgroundThemeState(&extra);
    theme->Paint(canvas->sk_canvas(), part, state, rect, extra);

    // Composite the foreground state above the background state.
    state = native_theme_delegate->GetForegroundThemeState(&extra);
    const int alpha = animation->CurrentValueBetween(0, 255);
    canvas->SaveLayerAlpha(static_cast<uint8>(alpha));
    theme->Paint(canvas->sk_canvas(), part, state, rect, extra);
    canvas->Restore();
  } else {
    state = native_theme_delegate->GetThemeState(&extra);
    theme->Paint(canvas->sk_canvas(), part, state, rect, extra);
  }

  // Draw the Views focus border for the native theme style.
  if (button->focus_border() && extra.button.is_focused)
    button->focus_border()->Paint(*button, canvas);
}

}  // namespace views
