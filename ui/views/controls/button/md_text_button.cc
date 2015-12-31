// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/md_text_button.h"

#include "base/i18n/case_conversion.h"
#include "ui/gfx/render_text.h"
#include "ui/native_theme/native_theme.h"

namespace views {

namespace {

// Inset between clickable region border and button contents (text).
const int kHorizontalPadding = 12;
const int kVerticalPadding = 6;

// Minimum size to reserve for the button contents.
const int kMinWidth = 48;

const gfx::FontList& GetFontList() {
  static base::LazyInstance<gfx::FontList>::Leaky font_list =
      LAZY_INSTANCE_INITIALIZER;
  return font_list.Get();
}

}  // namespace

MdTextButton::MdTextButton(ButtonListener* listener, const base::string16& text)
    : CustomButton(listener),
      render_text_(gfx::RenderText::CreateInstance()) {
  render_text_->SetFontList(GetFontList());
  render_text_->SetCursorEnabled(false);
  render_text_->SetText(base::i18n::ToUpper(text));

  SetFocusable(true);
}

MdTextButton::~MdTextButton() {}

void MdTextButton::OnPaint(gfx::Canvas* canvas) {
  UpdateColor();
  gfx::Rect rect = GetLocalBounds();
  rect.Inset(kHorizontalPadding, kVerticalPadding);
  render_text_->SetDisplayRect(rect);
  render_text_->Draw(canvas);
}

gfx::Size MdTextButton::GetPreferredSize() const {
  gfx::Size size = render_text_->GetStringSize();
  size.SetToMax(gfx::Size(kMinWidth, 0));
  size.Enlarge(kHorizontalPadding * 2, kVerticalPadding * 2);
  return size;
}

void MdTextButton::UpdateColor() {
  // TODO(estade): handle call to action theming and other things that can
  // affect the text color.
  render_text_->SetColor(GetNativeTheme()->GetSystemColor(
      enabled() ? ui::NativeTheme::kColorId_MdTextButtonEnabledColor
                : ui::NativeTheme::kColorId_MdTextButtonDisabledColor));
}

}  // namespace views
