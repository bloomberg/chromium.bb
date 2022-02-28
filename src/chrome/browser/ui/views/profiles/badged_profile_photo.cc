// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/badged_profile_photo.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"

namespace {

constexpr int kBadgeIconSize = 16;
constexpr int kBadgeBorderWidth = 2;
// The space between the right/bottom edge of the profile badge and the
// right/bottom edge of the profile icon.
constexpr int kBadgeSpacing = 4;
// Width and Height of the badged profile photo. Doesn't include the badge
// border to the right and to the bottom.
constexpr int kBadgedProfilePhotoWidth =
    BadgedProfilePhoto::kImageSize + kBadgeSpacing;
constexpr int kBadgedProfilePhotoHeight = BadgedProfilePhoto::kImageSize;

// A custom ImageView that removes the part where the badge will be placed
// including the (transparent) border.
class CustomImageView : public views::ImageView {
 public:
  METADATA_HEADER(CustomImageView);
  CustomImageView() = default;
  CustomImageView(const CustomImageView&) = delete;
  CustomImageView& operator=(const CustomImageView&) = delete;

 private:
  // views::ImageView:
  void OnPaint(gfx::Canvas* canvas) override;
};

BEGIN_METADATA(CustomImageView, views::ImageView)
END_METADATA

void CustomImageView::OnPaint(gfx::Canvas* canvas) {
  // Remove the part of the ImageView that contains the badge.
  SkPath mask;
  mask.addCircle(
      GetMirroredXInView(kBadgedProfilePhotoWidth - kBadgeIconSize / 2),
      kBadgedProfilePhotoHeight - kBadgeIconSize / 2,
      kBadgeIconSize / 2 + kBadgeBorderWidth);
  mask.toggleInverseFillType();
  canvas->ClipPath(mask, true);
  ImageView::OnPaint(canvas);
}

class BadgeView : public ::views::ImageView {
 public:
  explicit BadgeView(BadgedProfilePhoto::BadgeType badge_type)
      : badge_type_(badge_type) {
    SetPosition(gfx::Point(kBadgedProfilePhotoWidth - kBadgeIconSize,
                           kBadgedProfilePhotoHeight - kBadgeIconSize));
  }

  // views::View
  void OnThemeChanged() override {
    ::views::ImageView::OnThemeChanged();
    switch (badge_type_) {
      case BadgedProfilePhoto::BADGE_TYPE_SUPERVISOR:
        SetImage(gfx::CreateVectorIcon(
            kSupervisorAccountCircleIcon, kBadgeIconSize,
            GetColorProvider()->GetColor(ui::kColorIcon)));
        break;
      case BadgedProfilePhoto::BADGE_TYPE_CHILD:
        SetImage(gfx::CreateVectorIcon(
            kAccountChildCircleIcon, kBadgeIconSize,
            GetColorProvider()->GetColor(ui::kColorIcon)));
        break;
      case BadgedProfilePhoto::BADGE_TYPE_SYNC_COMPLETE:
        SetImage(gfx::CreateVectorIcon(
            kSyncCircleIcon, kBadgeIconSize,
            GetColorProvider()->GetColor(ui::kColorAlertLowSeverity)));
        break;
      case BadgedProfilePhoto::BADGE_TYPE_SYNC_ERROR:
        SetImage(gfx::CreateVectorIcon(
            kSyncErrorCircleIcon, kBadgeIconSize,
            GetColorProvider()->GetColor(ui::kColorAlertHighSeverity)));
        break;
      case BadgedProfilePhoto::BADGE_TYPE_SYNC_PAUSED:
        SetImage(gfx::CreateVectorIcon(
            kSyncPausedCircleIcon, kBadgeIconSize,
            GetColorProvider()->GetColor(ui::kColorButtonBackgroundProminent)));
        break;
      case BadgedProfilePhoto::BADGE_TYPE_SYNC_DISABLED:
        SetImage(gfx::CreateVectorIcon(kSyncCircleIcon, kBadgeIconSize,
                                       gfx::kGoogleGrey400));
        break;
      case BadgedProfilePhoto::BADGE_TYPE_SYNC_OFF:
        SetImage(gfx::CreateVectorIcon(kSyncPausedCircleIcon, kBadgeIconSize,
                                       gfx::kGoogleGrey600));
        break;
      case BadgedProfilePhoto::BADGE_TYPE_NONE:
        NOTREACHED();
        break;
    }
    SizeToPreferredSize();
  }

 private:
  const BadgedProfilePhoto::BadgeType badge_type_;
};

}  // namespace

// BadgedProfilePhoto -------------------------------------------------

BadgedProfilePhoto::BadgedProfilePhoto(BadgeType badge_type,
                                       const gfx::Image& profile_photo) {
  SetCanProcessEventsWithinSubtree(false);

  // Create and add image view for profile icon.
  gfx::Image profile_photo_circular = profiles::GetSizedAvatarIcon(
      profile_photo, true, kImageSize, kImageSize, profiles::SHAPE_CIRCLE);
  views::ImageView* profile_photo_view = badge_type == BADGE_TYPE_NONE
                                             ? new views::ImageView()
                                             : new CustomImageView();
  profile_photo_view->SetImage(*profile_photo_circular.ToImageSkia());
  profile_photo_view->SizeToPreferredSize();
  AddChildView(profile_photo_view);

  if (badge_type != BADGE_TYPE_NONE)
    AddChildView(std::make_unique<BadgeView>(badge_type));

  SetPreferredSize(
      gfx::Size(kBadgedProfilePhotoWidth, kBadgedProfilePhotoHeight));
}

BEGIN_METADATA(BadgedProfilePhoto, views::View)
END_METADATA
