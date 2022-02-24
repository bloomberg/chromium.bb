// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/desks_templates_icon_view.h"

#include "ash/public/cpp/desks_templates_delegate.h"
#include "ash/public/cpp/rounded_image_view.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ash/wm/desks/templates/desks_templates_icon_container.h"
#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/native_theme/native_theme.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "url/gurl.h"

namespace ash {

namespace {

// The size of the insets for the `count_label_`.
constexpr int kCountLabelInsetSize = 4;

// When fetching images from the app service, request one of size 64, which is a
// common cached image size. With a higher resolution it will look better after
// resizing.
constexpr int kAppIdImageSize = 64;

// Return the formatted string for `count`. If `count` is <=9, the string will
// be "+<count>". If `count` is >9, the string will be "9+".
std::u16string GetCountString(int count) {
  return base::UTF8ToUTF16(count > 9 ? "9+" : base::StringPrintf("+%i", count));
}

gfx::ImageSkia CreateResizedImageToIconSize(const gfx::ImageSkia& icon) {
  return gfx::ImageSkiaOperations::CreateResizedImage(
      icon, skia::ImageOperations::RESIZE_BEST,
      gfx::Size(DesksTemplatesIconView::kIconSize,
                DesksTemplatesIconView::kIconSize));
}

}  // namespace

DesksTemplatesIconView::DesksTemplatesIconView() = default;

DesksTemplatesIconView::~DesksTemplatesIconView() = default;

void DesksTemplatesIconView::SetIconIdentifierAndCount(
    const std::string& icon_identifier,
    int count) {
  icon_identifier_ = icon_identifier;
  count_ = count;

  if (count_ > 1 || icon_identifier_.empty()) {
    DCHECK(!count_label_);
    count_label_ = AddChildView(
        views::Builder<views::Label>()
            .SetText(GetCountString(count_))
            .SetBorder(views::CreateEmptyBorder(gfx::Insets(
                kCountLabelInsetSize, kCountLabelInsetSize,
                kCountLabelInsetSize,
                icon_view_ ? 2 * kCountLabelInsetSize : kCountLabelInsetSize)))
            .SetBackgroundColor(AshColorProvider::Get()->GetControlsLayerColor(
                AshColorProvider::ControlsLayerType::
                    kControlBackgroundColorInactive))
            .SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
                AshColorProvider::ContentLayerType::kTextColorPrimary))
            .Build());
  }

  if (icon_identifier_.empty())
    return;

  DCHECK(!icon_view_);
  icon_view_ =
      AddChildView(views::Builder<RoundedImageView>()
                       .SetCornerRadius(DesksTemplatesIconView::kIconSize / 2)
                       .Build());

  // First check if the `icon_identifier_` is a special value, i.e. NTP url or
  // incognito window. If it is, use the corresponding icon for the special
  // value.
  auto* delegate = Shell::Get()->desks_templates_delegate();
  absl::optional<gfx::ImageSkia> chrome_icon =
      delegate->MaybeRetrieveIconForSpecialIdentifier(
          icon_identifier_, static_cast<DesksTemplatesIconContainer*>(parent())
                                ->incognito_window_color_provider());
  if (chrome_icon.has_value()) {
    icon_view_->SetImage(CreateResizedImageToIconSize(chrome_icon.value()));
    return;
  }

  // It's not a special value so `icon_identifier_` is either a favicon or an
  // app id. If `icon_identifier_` is not a valid url then it's an app id.
  GURL potential_url{icon_identifier_};
  if (!potential_url.is_valid()) {
    delegate->GetIconForAppId(
        icon_identifier_, kAppIdImageSize,
        base::BindOnce(&DesksTemplatesIconView::OnIconLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  delegate->GetFaviconForUrl(
      icon_identifier_,
      base::BindOnce(&DesksTemplatesIconView::OnIconLoaded,
                     weak_ptr_factory_.GetWeakPtr()),
      &cancelable_task_tracker_);
}

void DesksTemplatesIconView::UpdateCount(int count) {
  count_ = count;
  DCHECK(count_label_);
  count_label_->SetText(GetCountString(count_));
}

gfx::Size DesksTemplatesIconView::CalculatePreferredSize() const {
  return gfx::Size(count_ > 1 && icon_view_ ? 2 * kIconSize : kIconSize,
                   kIconSize);
}

void DesksTemplatesIconView::Layout() {
  if (icon_view_)
    icon_view_->SetBoundsRect(gfx::Rect(kIconSize, kIconSize));

  if (count_label_) {
    count_label_->SetBoundsRect(
        gfx::Rect(icon_view_ ? kIconSize : 0, 0, kIconSize, kIconSize));
  }
}

void DesksTemplatesIconView::OnIconLoaded(const gfx::ImageSkia& icon) {
  if (!icon.isNull()) {
    icon_view_->SetImage(CreateResizedImageToIconSize(icon));
    return;
  }
  LoadDefaultIcon();
}

void DesksTemplatesIconView::LoadDefaultIcon() {
  const ui::NativeTheme* native_theme =
      ui::NativeTheme::GetInstanceForNativeUi();
  // Use a higher resolution image as it will look better after resizing.
  const int resource_id = native_theme && native_theme->ShouldUseDarkColors()
                              ? IDR_DEFAULT_FAVICON_DARK_64
                              : IDR_DEFAULT_FAVICON_64;
  icon_view_->SetImage(
      CreateResizedImageToIconSize(ui::ResourceBundle::GetSharedInstance()
                                       .GetImageNamed(resource_id)
                                       .AsImageSkia()));

  // Move `this` to the back of the visible icons, i.e. before any invisible
  // siblings and before the overflow counter,
  auto siblings = parent()->children();
  if (siblings.size() >= 2) {
    size_t i = 0;
    while (i < siblings.size() - 2 && siblings[i]->GetVisible())
      ++i;
    parent()->ReorderChildView(this, i);
  }
}

BEGIN_METADATA(DesksTemplatesIconView, views::View)
END_METADATA

}  // namespace ash
