// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shortcut_viewer/views/ksv_search_box_view.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_color_provider.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/search_box/search_box_constants.h"
#include "ash/search_box/search_box_view_base.h"
#include "ash/search_box/search_box_view_delegate.h"
#include "ash/shortcut_viewer/strings/grit/shortcut_viewer_strings.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/textfield/textfield.h"

namespace keyboard_shortcut_viewer {

namespace {

constexpr int kIconSize = 20;

// Border corner radius of the search box.
constexpr int kBorderCornerRadius = 32;
constexpr int kBorderThichness = 2;

}  // namespace

KSVSearchBoxView::KSVSearchBoxView(ash::SearchBoxViewDelegate* delegate)
    : ash::SearchBoxViewBase(delegate) {
  SetSearchBoxBackgroundCornerRadius(kBorderCornerRadius);
  UpdateBackgroundColor(GetBackgroundColor());
  search_box()->SetBackgroundColor(SK_ColorTRANSPARENT);
  search_box()->SetColor(GetPrimaryTextColor());
  SetPlaceholderTextAttributes();
  const std::u16string search_box_name(
      l10n_util::GetStringUTF16(IDS_KSV_SEARCH_BOX_ACCESSIBILITY_NAME));
  search_box()->SetPlaceholderText(search_box_name);
  search_box()->SetAccessibleName(search_box_name);
  SetSearchIconImage(
      gfx::CreateVectorIcon(ash::kKsvSearchBarIcon, GetPrimaryIconColor()));
}

gfx::Size KSVSearchBoxView::CalculatePreferredSize() const {
  return gfx::Size(740, 32);
}

void KSVSearchBoxView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kSearchBox;
  node_data->SetValue(accessible_value_);
}

void KSVSearchBoxView::OnKeyEvent(ui::KeyEvent* event) {
  const ui::KeyboardCode key = event->key_code();
  if ((key != ui::VKEY_ESCAPE && key != ui::VKEY_BROWSER_BACK) ||
      event->type() != ui::ET_KEY_PRESSED) {
    return;
  }

  event->SetHandled();
  // |VKEY_BROWSER_BACK| will only clear all the text.
  ClearSearch();
  // |VKEY_ESCAPE| will clear text and exit search mode directly.
  if (key == ui::VKEY_ESCAPE)
    SetSearchBoxActive(false, event->type());
}

void KSVSearchBoxView::OnThemeChanged() {
  ash::SearchBoxViewBase::OnThemeChanged();

  back_button()->SetImage(
      views::ImageButton::STATE_NORMAL,
      gfx::CreateVectorIcon(ash::kKsvSearchBackIcon, GetBackButtonColor()));
  close_button()->SetImage(
      views::ImageButton::STATE_NORMAL,
      gfx::CreateVectorIcon(ash::kKsvSearchCloseIcon, GetCloseButtonColor()));
  search_box()->SetBackgroundColor(SK_ColorTRANSPARENT);
  search_box()->SetColor(GetPrimaryTextColor());
  search_box()->set_placeholder_text_color(GetPlaceholderTextColor());
  SetBorder(views::CreateRoundedRectBorder(
      kBorderThichness, kBorderCornerRadius, GetBorderColor()));
  SetSearchIconImage(
      gfx::CreateVectorIcon(ash::kKsvSearchBarIcon, GetPrimaryIconColor()));
  UpdateBackgroundColor(GetBackgroundColor());
}

void KSVSearchBoxView::SetAccessibleValue(const std::u16string& value) {
  accessible_value_ = value;
  NotifyAccessibilityEvent(ax::mojom::Event::kValueChanged, true);
}

void KSVSearchBoxView::UpdateSearchBoxBorder() {
  // TODO(wutao): Rename this function or create another function in base class.
  // It updates many things in addition to the border.
  if (!search_box()->HasFocus() && search_box()->GetText().empty())
    SetSearchBoxActive(false, ui::ET_UNKNOWN);

  if (ShouldUseFocusedColors()) {
    SetBorder(views::CreateRoundedRectBorder(
        kBorderThichness, kBorderCornerRadius, GetBorderColor()));
    UpdateBackgroundColor(GetBackgroundColor());
    return;
  }
  SetBorder(views::CreateRoundedRectBorder(
      kBorderThichness, kBorderCornerRadius, GetBorderColor()));
  UpdateBackgroundColor(GetBackgroundColor());
}

void KSVSearchBoxView::SetupCloseButton() {
  views::ImageButton* close = close_button();
  close->SetHasInkDropActionOnClick(true);
  close->SetImage(
      views::ImageButton::STATE_NORMAL,
      gfx::CreateVectorIcon(ash::kKsvSearchCloseIcon, GetCloseButtonColor()));
  close->SetPreferredSize(gfx::Size(kIconSize, kIconSize));
  close->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  close->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  const std::u16string close_button_label(
      l10n_util::GetStringUTF16(IDS_KSV_CLEAR_SEARCHBOX_ACCESSIBILITY_NAME));
  close->SetAccessibleName(close_button_label);
  close->SetTooltipText(close_button_label);
  close->SetVisible(false);
}

void KSVSearchBoxView::SetupBackButton() {
  views::ImageButton* back = back_button();
  back->SetHasInkDropActionOnClick(true);
  back->SetImage(
      views::ImageButton::STATE_NORMAL,
      gfx::CreateVectorIcon(ash::kKsvSearchBackIcon, GetBackButtonColor()));
  back->SetPreferredSize(gfx::Size(kIconSize, kIconSize));
  back->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  back->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  const std::u16string back_button_label(
      l10n_util::GetStringUTF16(IDS_KSV_BACK_ACCESSIBILITY_NAME));
  back->SetAccessibleName(back_button_label);
  back->SetTooltipText(back_button_label);
  back->SetVisible(false);
}

void KSVSearchBoxView::UpdatePlaceholderTextStyle() {
  SetPlaceholderTextAttributes();
}

void KSVSearchBoxView::SetPlaceholderTextAttributes() {
  search_box()->set_placeholder_text_color(GetPlaceholderTextColor());
  search_box()->set_placeholder_text_draw_flags(
      base::i18n::IsRTL() ? gfx::Canvas::TEXT_ALIGN_RIGHT
                          : gfx::Canvas::TEXT_ALIGN_LEFT);
}

SkColor KSVSearchBoxView::GetBackgroundColor() {
  constexpr SkColor kBackgroundDarkColor =
      SkColorSetARGB(0xFF, 0x32, 0x33, 0x34);

  if (ShouldUseDarkThemeColors()) {
    return kBackgroundDarkColor;
  }

  return ShouldUseFocusedColors()
             ? gfx::kGoogleGrey100
             : ash::AppListColorProvider::Get()->GetSearchBoxBackgroundColor();
}

SkColor KSVSearchBoxView::GetBackButtonColor() {
  return ShouldUseDarkThemeColors() ? gfx::kGoogleBlue300 : gfx::kGoogleBlue500;
}

SkColor KSVSearchBoxView::GetBorderColor() {
  if (!ShouldUseFocusedColors()) {
    return SK_ColorTRANSPARENT;
  }

  constexpr SkColor kActiveBorderLightColor =
      SkColorSetARGB(0x7F, 0x1A, 0x73, 0xE8);
  const SkColor kActiveBorderDarkColor =
      ash::AppListColorProvider::Get()->GetFocusRingColor();

  return ShouldUseDarkThemeColors() ? kActiveBorderDarkColor
                                    : kActiveBorderLightColor;
}

SkColor KSVSearchBoxView::GetCloseButtonColor() {
  return ShouldUseDarkThemeColors() ? gfx::kGoogleGrey400 : gfx::kGoogleGrey700;
}

SkColor KSVSearchBoxView::GetPlaceholderTextColor() {
  return ShouldUseDarkThemeColors() ? gfx::kGoogleGrey400
                                    : ash::kZeroQuerySearchboxColor;
}

SkColor KSVSearchBoxView::GetPrimaryIconColor() {
  return ShouldUseDarkThemeColors() ? gfx::kGoogleGrey200 : gfx::kGoogleGrey900;
}

SkColor KSVSearchBoxView::GetPrimaryTextColor() {
  return ShouldUseDarkThemeColors() ? gfx::kGoogleGrey200 : gfx::kGoogleGrey900;
}

bool KSVSearchBoxView::ShouldUseFocusedColors() {
  return search_box()->HasFocus() || is_search_box_active();
}

bool KSVSearchBoxView::ShouldUseDarkThemeColors() {
  return ash::features::IsDarkLightModeEnabled() &&
         ash::ColorProvider::Get()->IsDarkModeEnabled();
}

}  // namespace keyboard_shortcut_viewer
