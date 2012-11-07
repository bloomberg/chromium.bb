// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/label_button_border.h"

#include "base/logging.h"
#include "ui/base/animation/animation.h"
#include "ui/base/native_theme/native_theme.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/rect.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/native_theme_delegate.h"

namespace {

// Preferred padding between content and edge.
static const int kPreferredPaddingHorizontal = 6;
static const int kPreferredPaddingVertical = 5;

// Preferred padding between content and edge for NativeTheme border.
static const int kPreferredNativeThemePaddingHorizontal = 12;
static const int kPreferredNativeThemePaddingVertical = 5;

views::CustomButton::ButtonState GetButtonStateForThemeState(
    ui::NativeTheme::State state) {
  switch(state) {
    case ui::NativeTheme::kDisabled: return views::CustomButton::BS_DISABLED;
    case ui::NativeTheme::kHovered:  return views::CustomButton::BS_HOT;
    case ui::NativeTheme::kNormal:   return views::CustomButton::BS_NORMAL;
    case ui::NativeTheme::kPressed:  return views::CustomButton::BS_PUSHED;
    case ui::NativeTheme::kMaxState: NOTREACHED() << "Unknown state: " << state;
  }
  return views::CustomButton::BS_NORMAL;
}

}  // namespace

namespace views {

LabelButtonBorder::LabelButtonBorder() : native_theme_(false) {
  SetImages(CustomButton::BS_HOT, BorderImages(BorderImages::kHot));
  SetImages(CustomButton::BS_PUSHED, BorderImages(BorderImages::kPushed));
}

LabelButtonBorder::~LabelButtonBorder() {}

void LabelButtonBorder::Paint(const View& view, gfx::Canvas* canvas) const {
  const NativeThemeDelegate* native_theme_delegate =
      static_cast<const LabelButton*>(&view);
  ui::NativeTheme::Part part = native_theme_delegate->GetThemePart();
  gfx::Rect rect(native_theme_delegate->GetThemePaintRect());
  ui::NativeTheme::State state;
  ui::NativeTheme::ExtraParams extra;
  const ui::NativeTheme* theme = view.GetNativeTheme();
  const ui::Animation* animation = native_theme_delegate->GetThemeAnimation();

  if (animation && animation->is_animating()) {
    // Paint the background state.
    state = native_theme_delegate->GetBackgroundThemeState(&extra);
    if (native_theme()) {
      theme->Paint(canvas->sk_canvas(), part, state, rect, extra);
    } else {
      BorderImages* set = const_cast<BorderImages*>(
          &images_[GetButtonStateForThemeState(state)]);
      if (!set->IsEmpty())
        set->Paint(canvas, view.size());
    }

    // Composite the foreground state above the background state.
    const int alpha = animation->CurrentValueBetween(0, 255);
    canvas->SaveLayerAlpha(static_cast<uint8>(alpha));
    state = native_theme_delegate->GetForegroundThemeState(&extra);
    if (native_theme()) {
      theme->Paint(canvas->sk_canvas(), part, state, rect, extra);
    } else {
      BorderImages* set = const_cast<BorderImages*>(
          &images_[GetButtonStateForThemeState(state)]);
      if (!set->IsEmpty())
        set->Paint(canvas, view.size());
    }
    canvas->Restore();
  } else {
    state = native_theme_delegate->GetThemeState(&extra);
    if (native_theme()) {
      theme->Paint(canvas->sk_canvas(), part, state, rect, extra);
    } else {
      BorderImages* set = const_cast<BorderImages*>(
          &images_[GetButtonStateForThemeState(state)]);
      if (!set->IsEmpty())
        set->Paint(canvas, view.size());
    }
  }

  if (native_theme()) {
    // Draw the Views focus border for the native theme style.
    if (view.focus_border() && extra.button.is_focused)
      view.focus_border()->Paint(view, canvas);
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

void LabelButtonBorder::SetImages(CustomButton::ButtonState state,
                                  const BorderImages& set) {
  images_[state] = set;
}

}  // namespace views
