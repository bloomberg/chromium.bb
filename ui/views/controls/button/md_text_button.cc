// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/md_text_button.h"

#include "base/i18n/case_conversion.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/painter.h"

namespace views {

namespace {

// Inset between clickable region border and button contents (text).
const int kHorizontalPadding = 12;
const int kVerticalPadding = 6;

// Minimum size to reserve for the button contents.
const int kMinWidth = 48;

}  // namespace

// static
LabelButton* MdTextButton::CreateStandardButton(ButtonListener* listener,
                                                const base::string16& text) {
  if (ui::MaterialDesignController::IsModeMaterial())
    return CreateMdButton(listener, text);

  LabelButton* button = new LabelButton(listener, text);
  button->SetStyle(STYLE_BUTTON);
  return button;
}

MdTextButton* MdTextButton::CreateMdButton(ButtonListener* listener,
                                           const base::string16& text) {
  MdTextButton* button = new MdTextButton(listener);
  button->SetText(text);
  // TODO(estade): can we get rid of the platform style border hoopla if
  // we apply the MD treatment to all buttons, even GTK buttons?
  button->SetBorder(
      Border::CreateEmptyBorder(kVerticalPadding, kHorizontalPadding,
                                kVerticalPadding, kHorizontalPadding));
  return button;
}

void MdTextButton::SetCallToAction(CallToAction cta) {
  if (cta_ == cta)
    return;

  cta_ = cta;
  UpdateColorsFromNativeTheme();
}

void MdTextButton::OnNativeThemeChanged(const ui::NativeTheme* theme) {
  LabelButton::OnNativeThemeChanged(theme);
  UpdateColorsFromNativeTheme();
}

SkColor MdTextButton::GetInkDropBaseColor() const {
  return color_utils::DeriveDefaultIconColor(label()->enabled_color());
}

void MdTextButton::SetText(const base::string16& text) {
  LabelButton::SetText(base::i18n::ToUpper(text));
}

MdTextButton::MdTextButton(ButtonListener* listener)
    : LabelButton(listener, base::string16()),
      ink_drop_delegate_(this, this),
      cta_(NO_CALL_TO_ACTION) {
  set_ink_drop_delegate(&ink_drop_delegate_);
  set_has_ink_drop_action_on_click(true);
  SetHorizontalAlignment(gfx::ALIGN_CENTER);
  Button::ConfigureDefaultFocus(this);
  SetMinSize(gfx::Size(kMinWidth, 0));
  SetFocusPainter(nullptr);
  UseMdFocusRing();
  label()->SetAutoColorReadabilityEnabled(false);
}

MdTextButton::~MdTextButton() {}

void MdTextButton::UpdateColorsFromNativeTheme() {
  ui::NativeTheme::ColorId fg_color_id = ui::NativeTheme::kColorId_NumColors;
  switch (cta_) {
    case NO_CALL_TO_ACTION:
      fg_color_id = ui::NativeTheme::kColorId_ButtonEnabledColor;
      break;
    case WEAK_CALL_TO_ACTION:
      fg_color_id = ui::NativeTheme::kColorId_CallToActionColor;
      break;
    case STRONG_CALL_TO_ACTION:
      fg_color_id = ui::NativeTheme::kColorId_TextOnCallToActionColor;
      break;
  }
  ui::NativeTheme* theme = GetNativeTheme();
  SetEnabledTextColors(theme->GetSystemColor(fg_color_id));

  set_background(
      cta_ == STRONG_CALL_TO_ACTION
          ? Background::CreateBackgroundPainter(
                true, Painter::CreateSolidRoundRectPainter(
                          theme->GetSystemColor(
                              ui::NativeTheme::kColorId_CallToActionColor),
                          kInkDropSmallCornerRadius))
          : nullptr);
}

}  // namespace views
