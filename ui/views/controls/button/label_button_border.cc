// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/label_button_border.h"

#include "base/logging.h"
#include "grit/ui_resources.h"
#include "ui/base/animation/animation.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/sys_color_change_listener.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/native_theme_delegate.h"

namespace views {

namespace {

// Insets for the unified button images. This assumes that the images
// are of a 9 grid, of 5x5 size each.
const int kButtonInsets = 5;

// The text-button hot and pushed image IDs; normal is unadorned by default.
const int kTextHoveredImages[] = IMAGE_GRID(IDR_TEXTBUTTON_HOVER);
const int kTextPressedImages[] = IMAGE_GRID(IDR_TEXTBUTTON_PRESSED);

Button::ButtonState GetButtonState(ui::NativeTheme::State state) {
  switch (state) {
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
    Painter* painter =
        border->GetPainter(extra.button.is_focused, GetButtonState(state));
    // Paint any corresponding unfocused painter if there is no focused painter.
    if (!painter && extra.button.is_focused)
      painter = border->GetPainter(false, GetButtonState(state));
    if (painter)
      painter->Paint(canvas, rect.size());
  }
}

}  // namespace

LabelButtonBorder::LabelButtonBorder(Button::ButtonStyle style)
    : style_(style) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  const gfx::Insets insets(kButtonInsets,
                           kButtonInsets,
                           kButtonInsets,
                           kButtonInsets);

  if (style == Button::STYLE_BUTTON) {
    set_insets(gfx::Insets(8, 13, 8, 13));
    SetPainter(false, Button::STATE_NORMAL,
               Painter::CreateImagePainter(
                   *rb.GetImageSkiaNamed(IDR_BUTTON_NORMAL), insets));
    SetPainter(false, Button::STATE_HOVERED,
               Painter::CreateImagePainter(
                   *rb.GetImageSkiaNamed(IDR_BUTTON_HOVER), insets));
    SetPainter(false, Button::STATE_PRESSED,
               Painter::CreateImagePainter(
                   *rb.GetImageSkiaNamed(IDR_BUTTON_PRESSED), insets));
    SetPainter(false, Button::STATE_DISABLED,
               Painter::CreateImagePainter(
                   *rb.GetImageSkiaNamed(IDR_BUTTON_DISABLED), insets));
    SetPainter(true, Button::STATE_NORMAL,
               Painter::CreateImagePainter(
                   *rb.GetImageSkiaNamed(IDR_BUTTON_FOCUSED_NORMAL), insets));
    SetPainter(true, Button::STATE_HOVERED,
               Painter::CreateImagePainter(
                   *rb.GetImageSkiaNamed(IDR_BUTTON_FOCUSED_HOVER), insets));
    SetPainter(true, Button::STATE_PRESSED,
               Painter::CreateImagePainter(
                   *rb.GetImageSkiaNamed(IDR_BUTTON_FOCUSED_PRESSED), insets));
    SetPainter(true, Button::STATE_DISABLED,
               Painter::CreateImagePainter(
                   *rb.GetImageSkiaNamed(IDR_BUTTON_DISABLED), insets));
  } else if (style == Button::STYLE_TEXTBUTTON) {
    set_insets(gfx::Insets(5, 6, 5, 6));
    SetPainter(false, Button::STATE_HOVERED,
               Painter::CreateImageGridPainter(kTextHoveredImages));
    SetPainter(false, Button::STATE_PRESSED,
               Painter::CreateImageGridPainter(kTextPressedImages));
  } else if (style == Button::STYLE_NATIVE_TEXTBUTTON) {
    set_insets(gfx::Insets(5, 12, 5, 12));
  }
}

LabelButtonBorder::~LabelButtonBorder() {}

void LabelButtonBorder::Paint(const View& view, gfx::Canvas* canvas) {
  const NativeThemeDelegate* native_theme_delegate =
      static_cast<const LabelButton*>(&view);
  ui::NativeTheme::Part part = native_theme_delegate->GetThemePart();
  gfx::Rect rect(native_theme_delegate->GetThemePaintRect());
  ui::NativeTheme::ExtraParams extra;
  const ui::NativeTheme* theme = view.GetNativeTheme();
  const ui::Animation* animation = native_theme_delegate->GetThemeAnimation();
  ui::NativeTheme::State state = native_theme_delegate->GetThemeState(&extra);

  if (animation && animation->is_animating()) {
    // Composite the background and foreground painters during state animations.
    int alpha = animation->CurrentValueBetween(0, 0xff);
    state = native_theme_delegate->GetBackgroundThemeState(&extra);
    canvas->SaveLayerAlpha(static_cast<uint8>(0xff - alpha));
    PaintHelper(this, canvas, theme, part, state, rect, extra);
    canvas->Restore();

    state = native_theme_delegate->GetForegroundThemeState(&extra);
    canvas->SaveLayerAlpha(static_cast<uint8>(alpha));
    PaintHelper(this, canvas, theme, part, state, rect, extra);
    canvas->Restore();
  } else {
    PaintHelper(this, canvas, theme, part, state, rect, extra);
  }

  // For inverted color schemes, draw a solid fill with the button color.
  if (gfx::IsInvertedColorScheme()) {
    rect.Inset(insets_);
    canvas->FillRect(rect, extra.button.background_color);
  }

  // Draw the Views focus border for the native theme style.
  if (style() == Button::STYLE_NATIVE_TEXTBUTTON &&
      view.focus_border() && extra.button.is_focused)
    view.focus_border()->Paint(view, canvas);
}

gfx::Insets LabelButtonBorder::GetInsets() const {
  return insets_;
}

Painter* LabelButtonBorder::GetPainter(bool focused,
                                       Button::ButtonState state) {
  return painters_[focused ? 1 : 0][state].get();
}

void LabelButtonBorder::SetPainter(bool focused,
                                   Button::ButtonState state,
                                   Painter* painter) {
  painters_[focused ? 1 : 0][state].reset(painter);
}

}  // namespace views
