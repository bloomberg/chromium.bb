// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/label_button_border.h"

#include "base/logging.h"
#include "grit/ui_resources.h"
#include "ui/base/animation/animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/rect.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/native_theme_delegate.h"

namespace views {

namespace {

// Preferred padding between content and edge.
static const int kPreferredPaddingHorizontal = 6;
static const int kPreferredPaddingVertical = 5;

// Preferred padding between content and edge for NativeTheme border.
static const int kPreferredNativeThemePaddingHorizontal = 12;
static const int kPreferredNativeThemePaddingVertical = 5;

// The default hot and pushed button image IDs; normal has none by default.
const int kHotImages[] = IMAGE_GRID(IDR_TEXTBUTTON_HOVER);
const int kPushedImages[] = IMAGE_GRID(IDR_TEXTBUTTON_PRESSED);

Button::ButtonState GetButtonState(ui::NativeTheme::State state) {
  switch(state) {
    case ui::NativeTheme::kDisabled: return Button::STATE_DISABLED;
    case ui::NativeTheme::kHovered:  return Button::STATE_HOVERED;
    case ui::NativeTheme::kNormal:   return Button::STATE_NORMAL;
    case ui::NativeTheme::kPressed:  return Button::STATE_PRESSED;
    case ui::NativeTheme::kMaxState: NOTREACHED() << "Unknown state: " << state;
  }
  return Button::STATE_NORMAL;
}

// A helper function to paint the native theme or images as appropriate.
void PaintHelper(LabelButtonBorder* border,
                 gfx::Canvas* canvas,
                 const ui::NativeTheme* theme,
                 ui::NativeTheme::Part part,
                 ui::NativeTheme::State state,
                 const gfx::Rect& rect,
                 const ui::NativeTheme::ExtraParams& extra) {
  if (border->style() == Button::STYLE_NATIVE_TEXTBUTTON) {
    theme->Paint(canvas->sk_canvas(), part, state, rect, extra);
  } else {
    Painter* painter = border->GetPainter(GetButtonState(state));
    if (painter)
      painter->Paint(canvas, rect.size());
  }
}

}  // namespace

LabelButtonBorder::LabelButtonBorder(Button::ButtonStyle style)
    : style_(style) {
  if (style == Button::STYLE_TEXTBUTTON) {
    SetPainter(Button::STATE_HOVERED,
               Painter::CreateImageGridPainter(kHotImages));
    SetPainter(Button::STATE_PRESSED,
               Painter::CreateImageGridPainter(kPushedImages));
  }
}

LabelButtonBorder::~LabelButtonBorder() {}

void LabelButtonBorder::Paint(const View& view, gfx::Canvas* canvas) {
  DCHECK(view.GetClassName() == LabelButton::kViewClassName);
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
    PaintHelper(this, canvas, theme, part, state, rect, extra);

    // Composite the foreground state above the background state.
    const int alpha = animation->CurrentValueBetween(0, 255);
    canvas->SaveLayerAlpha(static_cast<uint8>(alpha));
    state = native_theme_delegate->GetForegroundThemeState(&extra);
    PaintHelper(this, canvas, theme, part, state, rect, extra);
    canvas->Restore();
  } else {
    state = native_theme_delegate->GetThemeState(&extra);
    PaintHelper(this, canvas, theme, part, state, rect, extra);
  }

  // Draw the Views focus border for the native theme style.
  if (style() == Button::STYLE_NATIVE_TEXTBUTTON &&
      view.focus_border() && extra.button.is_focused)
    view.focus_border()->Paint(view, canvas);
}

gfx::Insets LabelButtonBorder::GetInsets() const {
  if (style() == Button::STYLE_NATIVE_TEXTBUTTON) {
    return gfx::Insets(kPreferredNativeThemePaddingVertical,
                       kPreferredNativeThemePaddingHorizontal,
                       kPreferredNativeThemePaddingVertical,
                       kPreferredNativeThemePaddingHorizontal);
  }
  DCHECK_EQ(style(), Button::STYLE_TEXTBUTTON);
  return gfx::Insets(kPreferredPaddingVertical, kPreferredPaddingHorizontal,
                     kPreferredPaddingVertical, kPreferredPaddingHorizontal);
}

Painter* LabelButtonBorder::GetPainter(Button::ButtonState state) {
  return painters_[state].get();
}

void LabelButtonBorder::SetPainter(Button::ButtonState state,
                                   Painter* painter) {
  painters_[state].reset(painter);
}

}  // namespace views
