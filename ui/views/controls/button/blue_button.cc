// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/blue_button.h"

#include "grit/ui_resources.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/sys_color_change_listener.h"
#include "ui/views/controls/button/label_button_border.h"

namespace {

// Default text and shadow colors for the blue button.
const SkColor kBlueButtonTextColor = SK_ColorWHITE;
const SkColor kBlueButtonShadowColor = SkColorSetRGB(0x53, 0x8C, 0xEA);

}  // namespace

namespace views {

// static
const char BlueButton::kViewClassName[] = "views/BlueButton";

BlueButton::BlueButton(ButtonListener* listener, const base::string16& text)
    : LabelButton(listener, text) {
  // Inherit STYLE_BUTTON insets, minimum size, alignment, etc.
  SetStyle(STYLE_BUTTON);

  // Insets for splitting the images.
  const gfx::Insets insets(5, 5, 5, 5);
  scoped_ptr<LabelButtonBorder> button_border(new LabelButtonBorder(style()));
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  button_border->SetPainter(false, STATE_NORMAL, Painter::CreateImagePainter(
          *rb.GetImageSkiaNamed(IDR_BLUE_BUTTON_NORMAL), insets));
  button_border->SetPainter(false, STATE_HOVERED, Painter::CreateImagePainter(
          *rb.GetImageSkiaNamed(IDR_BLUE_BUTTON_HOVER), insets));
  button_border->SetPainter(false, STATE_PRESSED, Painter::CreateImagePainter(
          *rb.GetImageSkiaNamed(IDR_BLUE_BUTTON_PRESSED), insets));
  button_border->SetPainter(false, STATE_DISABLED, Painter::CreateImagePainter(
          *rb.GetImageSkiaNamed(IDR_BLUE_BUTTON_DISABLED), insets));
  button_border->SetPainter(true, STATE_NORMAL, Painter::CreateImagePainter(
          *rb.GetImageSkiaNamed(IDR_BLUE_BUTTON_FOCUSED_NORMAL), insets));
  button_border->SetPainter(true, STATE_HOVERED, Painter::CreateImagePainter(
          *rb.GetImageSkiaNamed(IDR_BLUE_BUTTON_FOCUSED_HOVER), insets));
  button_border->SetPainter(true, STATE_PRESSED, Painter::CreateImagePainter(
          *rb.GetImageSkiaNamed(IDR_BLUE_BUTTON_FOCUSED_PRESSED), insets));
  button_border->SetPainter(true, STATE_DISABLED, Painter::CreateImagePainter(
          *rb.GetImageSkiaNamed(IDR_BLUE_BUTTON_DISABLED), insets));
  SetBorder(button_border.PassAs<Border>());
}

BlueButton::~BlueButton() {}

void BlueButton::ResetColorsFromNativeTheme() {
  LabelButton::ResetColorsFromNativeTheme();
  if (!gfx::IsInvertedColorScheme()) {
    for (size_t state = STATE_NORMAL; state < STATE_COUNT; ++state)
      SetTextColor(static_cast<ButtonState>(state), kBlueButtonTextColor);
    label()->SetShadowColors(kBlueButtonShadowColor, kBlueButtonShadowColor);
    label()->SetShadowOffset(0, 1);
  }
}

const char* BlueButton::GetClassName() const {
  return BlueButton::kViewClassName;
}

}  // namespace views
