// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/md_text_button.h"

#include "base/i18n/case_conversion.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_painted_layer_delegates.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/blue_button.h"
#include "ui/views/painter.h"

namespace views {

namespace {

// Minimum size to reserve for the button contents.
const int kMinWidth = 48;

// The stroke width of the focus border in normal and call to action mode.
const int kFocusBorderThickness = 1;
const int kFocusBorderThicknessCta = 2;

// The corner radius of the focus border roundrect.
const int kFocusBorderCornerRadius = 3;

LabelButton* CreateButton(ButtonListener* listener,
                          const base::string16& text,
                          bool md) {
  if (md)
    return MdTextButton::CreateMdButton(listener, text);

  LabelButton* button = new LabelButton(listener, text);
  button->SetStyle(CustomButton::STYLE_BUTTON);
  return button;
}

const gfx::FontList& GetMdFontList() {
  static base::LazyInstance<gfx::FontList>::Leaky font_list =
      LAZY_INSTANCE_INITIALIZER;
  const gfx::Font::Weight min_weight = gfx::Font::Weight::MEDIUM;
  if (font_list.Get().GetFontWeight() < min_weight)
    font_list.Get() = font_list.Get().DeriveWithWeight(min_weight);
  return font_list.Get();
}

}  // namespace

namespace internal {

class MdFocusRing : public View {
 public:
  MdFocusRing() : thickness_(kFocusBorderThickness) {
    SetPaintToLayer(true);
    layer()->SetFillsBoundsOpaquely(false);
  }
  ~MdFocusRing() override {}

  int thickness() const { return thickness_; }
  void set_thickness(int thickness) { thickness_ = thickness; }

  // View:
  bool CanProcessEventsWithinSubtree() const override { return false; }

  void OnPaint(gfx::Canvas* canvas) override {
    MdTextButton::PaintMdFocusRing(canvas, this, thickness_, 0x33);
  }

 private:
  int thickness_;

  DISALLOW_COPY_AND_ASSIGN(MdFocusRing);
};

}  // namespace internal

// static
LabelButton* MdTextButton::CreateStandardButton(ButtonListener* listener,
                                                const base::string16& text) {
  return CreateButton(listener, text,
                      ui::MaterialDesignController::IsModeMaterial());
}

// static
LabelButton* MdTextButton::CreateSecondaryUiButton(ButtonListener* listener,
                                                   const base::string16& text) {
  return CreateButton(listener, text,
                      ui::MaterialDesignController::IsSecondaryUiMaterial());
}

// static
LabelButton* MdTextButton::CreateSecondaryUiBlueButton(
    ButtonListener* listener,
    const base::string16& text) {
  if (ui::MaterialDesignController::IsSecondaryUiMaterial()) {
    MdTextButton* md_button = MdTextButton::CreateMdButton(listener, text);
    md_button->SetCallToAction(true);
    return md_button;
  }

  return new BlueButton(listener, text);
}

// static
MdTextButton* MdTextButton::CreateMdButton(ButtonListener* listener,
                                           const base::string16& text) {
  MdTextButton* button = new MdTextButton(listener);
  button->SetText(text);
  button->SetFocusForPlatform();
  return button;
}

// static
void MdTextButton::PaintMdFocusRing(gfx::Canvas* canvas,
                                    views::View* view,
                                    int thickness,
                                    SkAlpha alpha) {
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setColor(SkColorSetA(view->GetNativeTheme()->GetSystemColor(
                                 ui::NativeTheme::kColorId_CallToActionColor),
                             alpha));
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeWidth(thickness);
  gfx::RectF rect(view->GetLocalBounds());
  rect.Inset(gfx::InsetsF(thickness / 2.f));
  canvas->DrawRoundRect(rect, kFocusBorderCornerRadius, paint);
}

void MdTextButton::SetCallToAction(bool cta) {
  if (is_cta_ == cta)
    return;

  is_cta_ = cta;
  focus_ring_->set_thickness(cta ? kFocusBorderThicknessCta
                                 : kFocusBorderThickness);
  UpdateColors();
}

void MdTextButton::Layout() {
  LabelButton::Layout();
  gfx::Rect focus_bounds = GetLocalBounds();
  focus_bounds.Inset(gfx::Insets(-focus_ring_->thickness()));
  focus_ring_->SetBoundsRect(focus_bounds);
}

void MdTextButton::OnFocus() {
  LabelButton::OnFocus();
  focus_ring_->SetVisible(true);
}

void MdTextButton::OnBlur() {
  LabelButton::OnBlur();
  focus_ring_->SetVisible(false);
}

void MdTextButton::OnNativeThemeChanged(const ui::NativeTheme* theme) {
  LabelButton::OnNativeThemeChanged(theme);
  UpdateColors();
}

SkColor MdTextButton::GetInkDropBaseColor() const {
  return color_utils::DeriveDefaultIconColor(label()->enabled_color());
}

std::unique_ptr<views::InkDropRipple> MdTextButton::CreateInkDropRipple()
    const {
  return std::unique_ptr<views::InkDropRipple>(
      new views::FloodFillInkDropRipple(
          GetLocalBounds(), GetInkDropCenterBasedOnLastEvent(),
          GetInkDropBaseColor(), ink_drop_visible_opacity()));
}

std::unique_ptr<views::InkDropHighlight> MdTextButton::CreateInkDropHighlight()
    const {
  if (!ShouldShowInkDropHighlight())
    return nullptr;
  if (!is_cta_)
    return LabelButton::CreateInkDropHighlight();

  // The call to action hover effect is a shadow.
  const int kYOffset = 1;
  const int kSkiaBlurRadius = 1;
  std::vector<gfx::ShadowValue> shadows;
  // The notion of blur that gfx::ShadowValue uses is twice the Skia/CSS value.
  // Skia counts the number of pixels outside the mask area whereas
  // gfx::ShadowValue counts together the number of pixels inside and outside
  // the mask bounds.
  shadows.push_back(gfx::ShadowValue(gfx::Vector2d(0, kYOffset),
                                     2 * kSkiaBlurRadius,
                                     SkColorSetA(SK_ColorBLACK, 0x3D)));
  return base::MakeUnique<InkDropHighlight>(
      gfx::RectF(GetLocalBounds()).CenterPoint(),
      base::WrapUnique(new BorderShadowLayerDelegate(
          shadows, GetLocalBounds(), kInkDropSmallCornerRadius)));
}

bool MdTextButton::ShouldShowInkDropForFocus() const {
  // These types of button use |focus_ring_|.
  return false;
}

void MdTextButton::SetEnabledTextColors(SkColor color) {
  LabelButton::SetEnabledTextColors(color);
  UpdateColors();
}

void MdTextButton::SetText(const base::string16& text) {
  LabelButton::SetText(text);
  UpdatePadding();
}

void MdTextButton::AdjustFontSize(int size_delta) {
  LabelButton::AdjustFontSize(size_delta);
  UpdatePadding();
}

void MdTextButton::UpdateStyleToIndicateDefaultStatus() {
  UpdateColors();
}

void MdTextButton::SetFontList(const gfx::FontList& font_list) {
  NOTREACHED()
      << "Don't call MdTextButton::SetFontList (it will soon be protected)";
}

MdTextButton::MdTextButton(ButtonListener* listener)
    : LabelButton(listener, base::string16()),
      focus_ring_(new internal::MdFocusRing()),
      is_cta_(false) {
  SetInkDropMode(InkDropMode::ON);
  set_has_ink_drop_action_on_click(true);
  SetHorizontalAlignment(gfx::ALIGN_CENTER);
  SetFocusForPlatform();
  SetMinSize(gfx::Size(kMinWidth, 0));
  SetFocusPainter(nullptr);
  label()->SetAutoColorReadabilityEnabled(false);
  AddChildView(focus_ring_);
  focus_ring_->SetVisible(false);
  set_request_focus_on_press(false);
  LabelButton::SetFontList(GetMdFontList());
}

MdTextButton::~MdTextButton() {}

void MdTextButton::UpdatePadding() {
  // Don't use font-based padding when there's no text visible.
  if (GetText().empty()) {
    SetBorder(Border::NullBorder());
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
      label()->font_list().GetFontSize() - GetMdFontList().GetFontSize();
  const int kBaseHeight = 28;
  int target_height = std::max(kBaseHeight + size_delta * 2,
                               label()->font_list().GetFontSize() * 2);

  int label_height = label()->GetPreferredSize().height();
  int top_padding = (target_height - label_height) / 2;
  int bottom_padding = (target_height - label_height + 1) / 2;
  DCHECK_EQ(target_height, label_height + top_padding + bottom_padding);

  // TODO(estade): can we get rid of the platform style border hoopla if
  // we apply the MD treatment to all buttons, even GTK buttons?
  const int kHorizontalPadding = 16;
  SetBorder(Border::CreateEmptyBorder(top_padding, kHorizontalPadding,
                                      bottom_padding, kHorizontalPadding));
}

void MdTextButton::UpdateColors() {
  ui::NativeTheme::ColorId fg_color_id =
      is_cta_ ? ui::NativeTheme::kColorId_TextOnCallToActionColor
              : ui::NativeTheme::kColorId_ButtonEnabledColor;

  ui::NativeTheme* theme = GetNativeTheme();
  if (!explicitly_set_normal_color())
    LabelButton::SetEnabledTextColors(theme->GetSystemColor(fg_color_id));

  SkColor text_color = label()->enabled_color();
  SkColor bg_color =
      bg_color_override_
          ? *bg_color_override_
          : is_cta_
                ? theme->GetSystemColor(
                      ui::NativeTheme::kColorId_CallToActionColor)
                : is_default()
                      ? color_utils::BlendTowardOppositeLuma(text_color, 0xD8)
                      : SK_ColorTRANSPARENT;

  const SkAlpha kStrokeOpacity = 0x1A;
  SkColor stroke_color = (is_cta_ || color_utils::IsDark(text_color))
                             ? SkColorSetA(SK_ColorBLACK, kStrokeOpacity)
                             : SkColorSetA(SK_ColorWHITE, 2 * kStrokeOpacity);

  set_background(Background::CreateBackgroundPainter(
      true, Painter::CreateRoundRectWith1PxBorderPainter(
                bg_color, stroke_color, kInkDropSmallCornerRadius)));
}

}  // namespace views
