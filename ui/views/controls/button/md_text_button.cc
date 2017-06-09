// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/md_text_button.h"

#include "base/i18n/case_conversion.h"
#include "base/memory/ptr_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_painted_layer_delegates.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/blue_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/painter.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/style/typography.h"

namespace views {

namespace {

bool UseMaterialSecondaryButtons() {
#if defined(OS_MACOSX)
  return true;
#else
  return ui::MaterialDesignController::IsSecondaryUiMaterial();
#endif  // defined(OS_MACOSX)
}

LabelButton* CreateButton(ButtonListener* listener,
                          const base::string16& text,
                          bool md) {
  if (md)
    return MdTextButton::Create(listener, text, style::CONTEXT_BUTTON_MD);

  LabelButton* button = new LabelButton(listener, text, style::CONTEXT_BUTTON);
  button->SetStyleDeprecated(CustomButton::STYLE_BUTTON);
  return button;
}

}  // namespace

// static
LabelButton* MdTextButton::CreateSecondaryUiButton(ButtonListener* listener,
                                                   const base::string16& text) {
  return CreateButton(listener, text, UseMaterialSecondaryButtons());
}

// static
LabelButton* MdTextButton::CreateSecondaryUiBlueButton(
    ButtonListener* listener,
    const base::string16& text) {
  if (UseMaterialSecondaryButtons()) {
    MdTextButton* md_button =
        MdTextButton::Create(listener, text, style::CONTEXT_BUTTON_MD);
    md_button->SetProminent(true);
    return md_button;
  }

  return new BlueButton(listener, text);
}

// static
MdTextButton* MdTextButton::Create(ButtonListener* listener,
                                   const base::string16& text,
                                   int button_context) {
  MdTextButton* button = new MdTextButton(listener, button_context);
  button->SetText(text);
  button->SetFocusForPlatform();
  return button;
}

MdTextButton::~MdTextButton() {}

void MdTextButton::SetProminent(bool is_prominent) {
  if (is_prominent_ == is_prominent)
    return;

  is_prominent_ = is_prominent;
  UpdateColors();
}

void MdTextButton::SetBgColorOverride(const base::Optional<SkColor>& color) {
  bg_color_override_ = color;
  UpdateColors();
}

void MdTextButton::OnPaintBackground(gfx::Canvas* canvas) {
  LabelButton::OnPaintBackground(canvas);
  if (hover_animation().is_animating() || state() == STATE_HOVERED) {
    const int kHoverAlpha = is_prominent_ ? 0x0c : 0x05;
    SkScalar alpha = hover_animation().CurrentValueBetween(0, kHoverAlpha);
    canvas->FillRect(GetLocalBounds(), SkColorSetA(SK_ColorBLACK, alpha));
  }
}

void MdTextButton::OnFocus() {
  LabelButton::OnFocus();
  FocusRing::Install(this);
}

void MdTextButton::OnBlur() {
  LabelButton::OnBlur();
  FocusRing::Uninstall(this);
}

void MdTextButton::OnNativeThemeChanged(const ui::NativeTheme* theme) {
  LabelButton::OnNativeThemeChanged(theme);
  UpdateColors();
}

SkColor MdTextButton::GetInkDropBaseColor() const {
  return color_utils::DeriveDefaultIconColor(label()->enabled_color());
}

std::unique_ptr<InkDrop> MdTextButton::CreateInkDrop() {
  return CreateDefaultFloodFillInkDropImpl();
}

std::unique_ptr<views::InkDropRipple> MdTextButton::CreateInkDropRipple()
    const {
  return std::unique_ptr<views::InkDropRipple>(
      new views::FloodFillInkDropRipple(
          size(), GetInkDropCenterBasedOnLastEvent(), GetInkDropBaseColor(),
          ink_drop_visible_opacity()));
}

void MdTextButton::StateChanged(ButtonState old_state) {
  LabelButton::StateChanged(old_state);
  UpdateColors();
}

std::unique_ptr<views::InkDropHighlight> MdTextButton::CreateInkDropHighlight()
    const {
  // The prominent button hover effect is a shadow.
  const int kYOffset = 1;
  const int kSkiaBlurRadius = 2;
  const int shadow_alpha = is_prominent_ ? 0x3D : 0x1A;
  std::vector<gfx::ShadowValue> shadows;
  // The notion of blur that gfx::ShadowValue uses is twice the Skia/CSS value.
  // Skia counts the number of pixels outside the mask area whereas
  // gfx::ShadowValue counts together the number of pixels inside and outside
  // the mask bounds.
  shadows.push_back(gfx::ShadowValue(gfx::Vector2d(0, kYOffset),
                                     2 * kSkiaBlurRadius,
                                     SkColorSetA(SK_ColorBLACK, shadow_alpha)));
  const SkColor fill_color =
      SkColorSetA(SK_ColorWHITE, is_prominent_ ? 0x0D : 0x05);
  return base::MakeUnique<InkDropHighlight>(
      gfx::RectF(GetLocalBounds()).CenterPoint(),
      base::WrapUnique(new BorderShadowLayerDelegate(
          shadows, GetLocalBounds(), fill_color, kInkDropSmallCornerRadius)));
}

void MdTextButton::SetEnabledTextColors(SkColor color) {
  LabelButton::SetEnabledTextColors(color);
  UpdateColors();
}

void MdTextButton::SetText(const base::string16& text) {
  LabelButton::SetText(text);
  UpdatePadding();
}

void MdTextButton::UpdateStyleToIndicateDefaultStatus() {
  is_prominent_ = is_prominent_ || is_default();
  UpdateColors();
}

MdTextButton::MdTextButton(ButtonListener* listener, int button_context)
    : LabelButton(listener, base::string16(), button_context),
      is_prominent_(false) {
  SetInkDropMode(InkDropMode::ON);
  set_has_ink_drop_action_on_click(true);
  SetHorizontalAlignment(gfx::ALIGN_CENTER);
  SetFocusForPlatform();
  const int minimum_width = LayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_DIALOG_BUTTON_MINIMUM_WIDTH);
  SetMinSize(gfx::Size(minimum_width, 0));
  SetFocusPainter(nullptr);
  label()->SetAutoColorReadabilityEnabled(false);
  set_request_focus_on_press(false);

  set_animate_on_state_change(true);

  // Paint to a layer so that the canvas is snapped to pixel boundaries (useful
  // for fractional DSF).
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
}

void MdTextButton::UpdatePadding() {
  // Don't use font-based padding when there's no text visible.
  if (GetText().empty()) {
    SetBorder(NullBorder());
    return;
  }

  // Text buttons default to 28dp in height on all platforms when the base font
  // is in use, but should grow or shrink if the font size is adjusted up or
  // down. When the system font size has been adjusted, the base font will be
  // larger than normal such that 28dp might not be enough, so also enforce a
  // minimum height of twice the font size.
  // Example 1:
  // * Normal button on ChromeOS, 12pt Roboto. Button height of 28dp.
  // * Button on ChromeOS that has been adjusted to 14pt Roboto. Button height
  // of 28 + 2 * 2 = 32dp.
  // * Linux user sets base system font size to 17dp. For a normal button, the
  // |size_delta| will be zero, so to adjust upwards we double 17 to get 34.
  int size_delta =
      label()->font_list().GetFontSize() -
      style::GetFont(style::CONTEXT_BUTTON_MD, style::STYLE_PRIMARY)
          .GetFontSize();
  const int kBaseHeight = 28;
  int target_height = std::max(kBaseHeight + size_delta * 2,
                               label()->font_list().GetFontSize() * 2);

  int label_height = label()->GetPreferredSize().height();
  int top_padding = (target_height - label_height) / 2;
  int bottom_padding = (target_height - label_height + 1) / 2;
  DCHECK_EQ(target_height, label_height + top_padding + bottom_padding);

  // TODO(estade): can we get rid of the platform style border hoopla if
  // we apply the MD treatment to all buttons, even GTK buttons?
  const int horizontal_padding = LayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_BUTTON_HORIZONTAL_PADDING);
  SetBorder(CreateEmptyBorder(top_padding, horizontal_padding, bottom_padding,
                              horizontal_padding));
}

void MdTextButton::UpdateColors() {
  ui::NativeTheme::ColorId fg_color_id =
      is_prominent_ ? ui::NativeTheme::kColorId_TextOnProminentButtonColor
                    : ui::NativeTheme::kColorId_ButtonEnabledColor;

  ui::NativeTheme* theme = GetNativeTheme();
  if (!explicitly_set_normal_color()) {
    const auto colors = explicitly_set_colors();
    LabelButton::SetEnabledTextColors(theme->GetSystemColor(fg_color_id));
    set_explicitly_set_colors(colors);
  }

  // Prominent buttons keep their enabled text color; disabled state is conveyed
  // by shading the background instead.
  if (is_prominent_)
    SetTextColor(STATE_DISABLED, theme->GetSystemColor(fg_color_id));

  SkColor text_color = label()->enabled_color();
  SkColor bg_color =
      theme->GetSystemColor(ui::NativeTheme::kColorId_DialogBackground);

  if (bg_color_override_) {
    bg_color = *bg_color_override_;
  } else if (is_prominent_) {
    bg_color = theme->GetSystemColor(
        ui::NativeTheme::kColorId_ProminentButtonColor);
    if (state() == STATE_DISABLED)
      bg_color = color_utils::BlendTowardOppositeLuma(
          bg_color, gfx::kDisabledControlAlpha);
  }

  if (state() == STATE_PRESSED) {
    SkColor shade =
        theme->GetSystemColor(ui::NativeTheme::kColorId_ButtonPressedShade);
    bg_color = color_utils::GetResultingPaintColor(shade, bg_color);
  }

  // Specified text color: 5a5a5a @ 1.0 alpha
  // Specified stroke color: 000000 @ 0.2 alpha
  // 000000 @ 0.2 is very close to 5a5a5a @ 0.308 (== 0x4e); both are cccccc @
  // 1.0, and this way if NativeTheme changes the button color, the button
  // stroke will also change colors to match.
  SkColor stroke_color =
      is_prominent_ ? SK_ColorTRANSPARENT : SkColorSetA(text_color, 0x4e);

  // Disabled, non-prominent buttons need their stroke lightened. Prominent
  // buttons need it left at SK_ColorTRANSPARENT from above.
  if (state() == STATE_DISABLED && !is_prominent_) {
    stroke_color = color_utils::BlendTowardOppositeLuma(
        stroke_color, gfx::kDisabledControlAlpha);
  }

  DCHECK_EQ(SK_AlphaOPAQUE, static_cast<int>(SkColorGetA(bg_color)));
  SetBackground(
      CreateBackgroundFromPainter(Painter::CreateRoundRectWith1PxBorderPainter(
          bg_color, stroke_color, kInkDropSmallCornerRadius)));
  SchedulePaint();
}

}  // namespace views
