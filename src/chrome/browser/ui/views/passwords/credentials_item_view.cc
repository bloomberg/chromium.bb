// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/credentials_item_view.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/grit/theme_resources.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/bubble/tooltip_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

namespace {

class CircularImageView : public views::ImageView {
 public:
  CircularImageView() = default;

 private:
  // views::ImageView:
  void OnPaint(gfx::Canvas* canvas) override;

  DISALLOW_COPY_AND_ASSIGN(CircularImageView);
};

void CircularImageView::OnPaint(gfx::Canvas* canvas) {
  // Display the avatar picture as a circle.
  gfx::Rect bounds(GetImageBounds());
  SkPath circular_mask;
  circular_mask.addCircle(
      SkIntToScalar(bounds.x() + bounds.right()) / 2,
      SkIntToScalar(bounds.y() + bounds.bottom()) / 2,
      SkIntToScalar(std::min(bounds.height(), bounds.width())) / 2);
  canvas->ClipPath(circular_mask, true);
  ImageView::OnPaint(canvas);
}

}  // namespace

CredentialsItemView::CredentialsItemView(
    views::ButtonListener* button_listener,
    const base::string16& upper_text,
    const base::string16& lower_text,
    const autofill::PasswordForm* form,
    network::mojom::URLLoaderFactory* loader_factory,
    int upper_text_style,
    int lower_text_style)
    : Button(button_listener), form_(form) {
  set_notify_enter_exit_on_child(true);
  views::BoxLayout* layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  layout->set_between_child_spacing(
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_LABEL_HORIZONTAL));
  // Create an image-view for the avatar. Make sure it ignores events so that
  // the parent can receive the events instead.
  auto image_view = std::make_unique<CircularImageView>();
  image_view_ = image_view.get();
  image_view_->set_can_process_events_within_subtree(false);
  gfx::Image image = ui::ResourceBundle::GetSharedInstance().GetImageNamed(
      IDR_PROFILE_AVATAR_PLACEHOLDER_LARGE);
  DCHECK(image.Width() >= kAvatarImageSize &&
         image.Height() >= kAvatarImageSize);
  UpdateAvatar(image.AsImageSkia());
  if (form_->icon_url.is_valid()) {
    // Fetch the actual avatar.
    AccountAvatarFetcher* fetcher = new AccountAvatarFetcher(
        form_->icon_url, weak_ptr_factory_.GetWeakPtr());
    fetcher->Start(loader_factory);
  }
  AddChildView(std::move(image_view));

  // TODO(tapted): Check these (and the STYLE_ values below) against the spec on
  // http://crbug.com/651681.
  const int kLabelContext = CONTEXT_BODY_TEXT_SMALL;

  views::View* text_container = nullptr;
  if (!upper_text.empty() || !lower_text.empty()) {
    text_container = AddChildView(std::make_unique<views::View>());
    views::BoxLayout* text_layout =
        text_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kVertical));
    text_layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kStart);
    layout->SetFlexForView(text_container, 1);
  }

  if (!upper_text.empty()) {
    auto upper_label = std::make_unique<views::Label>(upper_text, kLabelContext,
                                                      upper_text_style);
    upper_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    upper_label_ = text_container->AddChildView(std::move(upper_label));
  }

  if (!lower_text.empty()) {
    auto lower_label = std::make_unique<views::Label>(lower_text, kLabelContext,
                                                      lower_text_style);
    lower_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    lower_label->SetMultiLine(true);
    lower_label_ = text_container->AddChildView(std::move(lower_label));
  }

  if (form_->is_public_suffix_match) {
    info_icon_ = AddChildView(std::make_unique<views::TooltipIcon>(
        base::UTF8ToUTF16(form_->origin.GetOrigin().spec())));
  }

  if (!upper_text.empty() && !lower_text.empty())
    SetAccessibleName(upper_text + base::ASCIIToUTF16("\n") + lower_text);
  else
    SetAccessibleName(upper_text + lower_text);

  SetFocusBehavior(FocusBehavior::ALWAYS);
}

CredentialsItemView::~CredentialsItemView() = default;

void CredentialsItemView::SetStoreIndicatorIcon(
    autofill::PasswordForm::Store store) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (store == autofill::PasswordForm::Store::kAccountStore &&
      !store_indicator_icon_view_) {
    store_indicator_icon_view_ =
        AddChildView(std::make_unique<views::ImageView>());
    store_indicator_icon_view_->set_can_process_events_within_subtree(false);
    store_indicator_icon_view_->SetImage(
        gfx::CreateVectorIcon(kGoogleGLogoIcon, gfx::kPlaceholderColor));
  } else if (store == autofill::PasswordForm::Store::kProfileStore &&
             store_indicator_icon_view_) {
    RemoveChildView(store_indicator_icon_view_);
    store_indicator_icon_view_ = nullptr;
  }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

void CredentialsItemView::UpdateAvatar(const gfx::ImageSkia& image) {
  image_view_->SetImage(ScaleImageForAccountAvatar(image));
}

int CredentialsItemView::GetPreferredHeight() const {
  return GetPreferredSize().height();
}

void CredentialsItemView::OnPaintBackground(gfx::Canvas* canvas) {
  if (state() == STATE_PRESSED || state() == STATE_HOVERED) {
    canvas->DrawColor(GetNativeTheme()->GetSystemColor(
        ui::NativeTheme::kColorId_FocusedMenuItemBackgroundColor));
  }
}
