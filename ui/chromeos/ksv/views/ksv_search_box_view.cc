// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/chromeos/ksv/views/ksv_search_box_view.h"

#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/ksv/vector_icons/vector_icons.h"
#include "ui/chromeos/search_box/search_box_view_delegate.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/textfield/textfield.h"

namespace {

constexpr SkColor kDefaultSearchBoxBackgroundColor =
    SkColorSetARGBMacro(0x1E, 0x80, 0x86, 0x8B);

constexpr int kIconSize = 20;

// Border corner radius of the search box.
constexpr int kBorderCornerRadius = 32;

}  // namespace

namespace keyboard_shortcut_viewer {

KSVSearchBoxView::KSVSearchBoxView(search_box::SearchBoxViewDelegate* delegate)
    : search_box::SearchBoxViewBase(delegate) {
  SetSearchBoxBackgroundCornerRadius(kBorderCornerRadius);
  SetSearchBoxBackgroundColor(kDefaultSearchBoxBackgroundColor);
  search_box()->SetBackgroundColor(SK_ColorTRANSPARENT);
  search_box()->set_placeholder_text(
      l10n_util::GetStringUTF16(IDS_KSV_SEARCH_BOX_PLACEHOLDER));
  search_box()->set_placeholder_text_draw_flags(gfx::Canvas::TEXT_ALIGN_CENTER);
  search_box()->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_KSV_SEARCH_BOX_ACCESSIBILITY_NAME));

  constexpr SkColor kSearchBarIconColor =
      SkColorSetARGBMacro(0xFF, 0x3C, 0x40, 0x43);
  SetSearchIconImage(
      gfx::CreateVectorIcon(kKsvSearchBarIcon, kSearchBarIconColor));
}

gfx::Size KSVSearchBoxView::CalculatePreferredSize() const {
  return gfx::Size(704, 32);
}

void KSVSearchBoxView::UpdateBackgroundColor(SkColor color) {
  SetSearchBoxBackgroundColor(color);
}

void KSVSearchBoxView::UpdateSearchBoxBorder() {
  constexpr int kBorderThichness = 2;
  constexpr SkColor kActiveBorderColor =
      SkColorSetARGBMacro(0x7F, 0x1A, 0x73, 0xE8);
  constexpr SkColor kActiveFillColor =
      SkColorSetARGBMacro(0xFF, 0xF1, 0xF3, 0xF4);

  if (search_box()->HasFocus() || is_search_box_active()) {
    SetBorder(views::CreateRoundedRectBorder(
        kBorderThichness, kBorderCornerRadius, kActiveBorderColor));
    SetSearchBoxBackgroundColor(kActiveFillColor);
    return;
  }
  SetBorder(views::CreateRoundedRectBorder(
      kBorderThichness, kBorderCornerRadius, SK_ColorTRANSPARENT));
  SetSearchBoxBackgroundColor(kDefaultSearchBoxBackgroundColor);
}

void KSVSearchBoxView::SetupCloseButton() {
  constexpr SkColor kCloseIconColor =
      SkColorSetARGBMacro(0xFF, 0x80, 0x86, 0x8B);
  views::ImageButton* close = close_button();
  close->SetImage(views::ImageButton::STATE_NORMAL,
                  gfx::CreateVectorIcon(kKsvSearchCloseIcon, kCloseIconColor));
  close->SetSize(gfx::Size(kIconSize, kIconSize));
  close->SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                           views::ImageButton::ALIGN_MIDDLE);
  close->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_KSV_CLEAR_SEARCHBOX_ACCESSIBILITY_NAME));
  close->SetVisible(false);
}

void KSVSearchBoxView::SetupBackButton() {
  constexpr SkColor kBackIconColor =
      SkColorSetARGBMacro(0xFF, 0x42, 0x85, 0xF4);
  views::ImageButton* back = back_button();
  back->SetImage(views::ImageButton::STATE_NORMAL,
                 gfx::CreateVectorIcon(kKsvSearchBackIcon, kBackIconColor));
  back->SetSize(gfx::Size(kIconSize, kIconSize));
  back->SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                          views::ImageButton::ALIGN_MIDDLE);
  back->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_KSV_BACK_ACCESSIBILITY_NAME));
  back->SetVisible(false);
}

}  // namespace keyboard_shortcut_viewer
