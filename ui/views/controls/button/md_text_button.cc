// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/md_text_button.h"

#include "base/i18n/case_conversion.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/border.h"
#include "ui/views/painter.h"

namespace views {

namespace {

// Inset between clickable region border and button contents (text).
const int kHorizontalPadding = 12;
const int kVerticalPadding = 6;

// Minimum size to reserve for the button contents.
const int kMinWidth = 48;

// The amount to enlarge the focus border in all directions relative to the
// button.
const int kFocusBorderOutset = -4;

// The corner radius of the focus border roundrect.
const int kFocusBorderCornerRadius = 3;

class MdFocusRing : public views::View {
 public:
  MdFocusRing() {
    SetPaintToLayer(true);
    layer()->SetFillsBoundsOpaquely(false);
  }
  ~MdFocusRing() override {}

  void OnPaint(gfx::Canvas* canvas) override {
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor(GetNativeTheme()->GetSystemColor(
        ui::NativeTheme::kColorId_CallToActionColor));
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(1);
    gfx::RectF rect(GetLocalBounds());
    rect.Inset(gfx::InsetsF(0.5));
    canvas->DrawRoundRect(rect, kFocusBorderCornerRadius, paint);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MdFocusRing);
};

}  // namespace

// static
LabelButton* MdTextButton::CreateStandardButton(ButtonListener* listener,
                                                const base::string16& text) {
  if (ui::MaterialDesignController::IsModeMaterial()) {
    MdTextButton* button = new MdTextButton(listener);
    button->SetText(text);
    // TODO(estade): can we get rid of the platform style border hoopla if
    // we apply the MD treatment to all buttons, even GTK buttons?
    button->SetBorder(
        Border::CreateEmptyBorder(kVerticalPadding, kHorizontalPadding,
                                  kVerticalPadding, kHorizontalPadding));
    return button;
  }

  LabelButton* button = new LabelButton(listener, text);
  button->SetStyle(STYLE_BUTTON);
  return button;
}

void MdTextButton::Layout() {
  LabelButton::Layout();

  gfx::Rect focus_bounds = GetLocalBounds();
  focus_bounds.Inset(gfx::Insets(kFocusBorderOutset));
  focus_ring_->SetBoundsRect(focus_bounds);
}

void MdTextButton::OnFocus() {
  View::OnFocus();
  focus_ring_->SetVisible(true);
}

void MdTextButton::OnBlur() {
  View::OnBlur();
  focus_ring_->SetVisible(false);
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
      focus_ring_(new MdFocusRing()) {
  set_ink_drop_delegate(&ink_drop_delegate_);
  set_has_ink_drop_action_on_click(true);
  SetHorizontalAlignment(gfx::ALIGN_CENTER);
  SetFocusable(true);
  SetMinSize(gfx::Size(kMinWidth, 0));

  AddChildView(focus_ring_);
  focus_ring_->SetVisible(false);
  SetFocusPainter(nullptr);
  set_request_focus_on_press(false);
}

MdTextButton::~MdTextButton() {}

}  // namespace views
