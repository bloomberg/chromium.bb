// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/ash_notification_expand_button.h"

#include "ash/public/cpp/metrics_util.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/message_center/message_center_constants.h"
#include "ash/system/message_center/message_center_utils.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "base/metrics/histogram_functions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/animation_throughput_reporter.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/animation/animation_sequence_block.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"

namespace ash {

BEGIN_METADATA(AshNotificationExpandButton, views::Button)
END_METADATA

AshNotificationExpandButton::AshNotificationExpandButton(
    PressedCallback callback)
    : Button(std::move(callback)) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetMainAxisAlignment(views::LayoutAlignment::kEnd);

  auto label = std::make_unique<views::Label>();
  label->SetFontList(gfx::FontList({kGoogleSansFont}, gfx::Font::NORMAL,
                                   kNotificationExpandButtonLabelFontSize,
                                   gfx::Font::Weight::MEDIUM));
  label->SetProperty(views::kMarginsKey, kNotificationExpandButtonLabelInsets);
  label->SetElideBehavior(gfx::ElideBehavior::NO_ELIDE);
  label->SetText(base::NumberToString16(total_grouped_notifications_));
  label->SetVisible(ShouldShowLabel());
  label_ = AddChildView(std::move(label));

  UpdateIcons();

  auto image = std::make_unique<views::ImageView>();
  image->SetProperty(views::kMarginsKey, kNotificationExpandButtonImageInsets);
  image->SetImage(expanded_ ? expanded_image_ : collapsed_image_);
  image_ = AddChildView(std::move(image));

  views::InstallRoundRectHighlightPathGenerator(
      this, gfx::Insets(), kNotificationExpandButtonCornerRadius);

  SetAccessibleName(l10n_util::GetStringUTF16(
      expanded_ ? IDS_ASH_NOTIFICATION_COLLAPSE_TOOLTIP
                : IDS_ASH_NOTIFICATION_EXPAND_TOOLTIP));

  message_center_utils::InitLayerForAnimations(label_);
  message_center_utils::InitLayerForAnimations(image_);

  SetPaintToLayer(ui::LAYER_SOLID_COLOR);
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetRoundedCornerRadius(gfx::RoundedCornersF{kTrayItemCornerRadius});
  layer()->SetIsFastRoundedCorner(true);
}

AshNotificationExpandButton::~AshNotificationExpandButton() = default;

void AshNotificationExpandButton::SetExpanded(bool expanded) {
  if (expanded_ == expanded)
    return;

  previous_bounds_ = GetContentsBounds();

  expanded_ = expanded;

  label_->SetText(base::NumberToString16(total_grouped_notifications_));
  label_->SetVisible(ShouldShowLabel());

  image_->SetImage(expanded_ ? expanded_image_ : collapsed_image_);
  image_->SetTooltipText(l10n_util::GetStringUTF16(
      expanded_ ? IDS_ASH_NOTIFICATION_COLLAPSE_TOOLTIP
                : IDS_ASH_NOTIFICATION_EXPAND_TOOLTIP));

  SetAccessibleName(l10n_util::GetStringUTF16(
      expanded_ ? IDS_ASH_NOTIFICATION_COLLAPSE_TOOLTIP
                : IDS_ASH_NOTIFICATION_EXPAND_TOOLTIP));
}

bool AshNotificationExpandButton::ShouldShowLabel() const {
  return !expanded_ && total_grouped_notifications_;
}

void AshNotificationExpandButton::UpdateGroupedNotificationsCount(int count) {
  total_grouped_notifications_ = count;
  label_->SetText(base::NumberToString16(total_grouped_notifications_));
  label_->SetVisible(ShouldShowLabel());
}

void AshNotificationExpandButton::UpdateIcons() {
  expanded_image_ = gfx::CreateVectorIcon(
      kUnifiedMenuExpandIcon, kNotificationExpandButtonChevronIconSize,
      AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kIconColorPrimary));

  collapsed_image_ = gfx::ImageSkiaOperations::CreateRotatedImage(
      gfx::CreateVectorIcon(
          kUnifiedMenuExpandIcon, kNotificationExpandButtonChevronIconSize,
          AshColorProvider::Get()->GetContentLayerColor(
              AshColorProvider::ContentLayerType::kIconColorPrimary)),
      SkBitmapOperations::ROTATION_180_CW);
}

void AshNotificationExpandButton::PerformExpandCollapseAnimation() {
  // If the button is not used for grouped notification, there's no animation to
  // perform here.
  if (!total_grouped_notifications_)
    return;

  // This value is used to add extra width to the view's bounds. We will animate
  // the view with this extra width to its target state.
  int extra_width;
  int bounds_animation_duration;
  gfx::Tween::Type bounds_animation_tween_type;

  if (label_->GetVisible()) {
    if (label_->layer()->GetAnimator()->is_animating()) {
      // Label's fade out animation might still be running. If that's the case,
      // we need to abort this and reset visibility for fade in animation.
      label_->layer()->GetAnimator()->AbortAllAnimations();
      label_->SetVisible(true);
    }

    // Fade in animation when label is visible.
    message_center_utils::FadeInView(
        label_, kExpandButtonFadeInLabelDelayMs,
        kExpandButtonFadeInLabelDurationMs, gfx::Tween::LINEAR,
        "Ash.NotificationView.ExpandButtonLabel.FadeIn.AnimationSmoothness");

    bounds_animation_duration = kExpandButtonShowLabelBoundsChangeDurationMs;
    bounds_animation_tween_type = gfx::Tween::LINEAR_OUT_SLOW_IN;
  } else {
    // In this case, `total_grouped_notifications_` is not zero and label is not
    // visible. This means the label switch from visible to invisible and we
    // should do fade out animation.
    label_fading_out_ = true;
    message_center_utils::FadeOutView(
        label_,
        base::BindRepeating(
            [](base::WeakPtr<AshNotificationExpandButton> parent,
               views::Label* label) {
              if (parent) {
                label->layer()->SetOpacity(1.0f);
                label->SetVisible(false);
                parent->set_label_fading_out(false);
              }
            },
            weak_factory_.GetWeakPtr(), label_),
        0, kExpandButtonFadeOutLabelDurationMs);

    bounds_animation_duration = kExpandButtonHideLabelBoundsChangeDurationMs;
    bounds_animation_tween_type = gfx::Tween::ACCEL_20_DECEL_100;
  }

  // Perform size change animation with layer bounds animation, setting the
  // bounds to its previous state and then animating to current state. At the
  // same time, we move `image_` in the opposite direction so that it appears to
  // stay in the same location when the parent's bounds is moving.
  const gfx::Rect target_bounds = layer()->GetTargetBounds();
  const gfx::Rect image_target_bounds = image_->layer()->GetTargetBounds();

  extra_width = previous_bounds_.width() - target_bounds.width();

  ui::AnimationThroughputReporter reporter(
      layer()->GetAnimator(),
      metrics_util::ForSmoothness(base::BindRepeating([](int value) {
        base::UmaHistogramPercentage(
            "Ash.NotificationView.ExpandButton.BoundsChange."
            "AnimationSmoothness",
            value);
      })));

  layer()->SetBounds(
      gfx::Rect(target_bounds.x() - extra_width, target_bounds.y(),
                target_bounds.width() + extra_width, target_bounds.height()));
  image_->layer()->SetBounds(
      gfx::Rect(image_target_bounds.x() + extra_width, image_target_bounds.y(),
                image_target_bounds.width(), image_target_bounds.height()));

  views::AnimationBuilder()
      .Once()
      .SetDuration(base::Milliseconds(bounds_animation_duration))
      .SetBounds(this, target_bounds, bounds_animation_tween_type)
      .SetBounds(image_, image_target_bounds, bounds_animation_tween_type);
}

void AshNotificationExpandButton::OnThemeChanged() {
  views::Button::OnThemeChanged();

  UpdateIcons();
  image_->SetImage(expanded_ ? expanded_image_ : collapsed_image_);

  views::FocusRing::Get(this)->SetColor(
      AshColorProvider::Get()->GetControlsLayerColor(
          AshColorProvider::ControlsLayerType::kFocusRingColor));

  SkColor background_color = AshColorProvider::Get()->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive);
  layer()->SetColor(background_color);
}

gfx::Size AshNotificationExpandButton::CalculatePreferredSize() const {
  gfx::Size size = Button::CalculatePreferredSize();

  // When label is fading out, it is still visible but we should not consider
  // its size in our calculation here, so that size change animation can be
  // performed correctly.
  if (label_fading_out_) {
    return gfx::Size(size.width() - label_->GetPreferredSize().width() -
                         kNotificationExpandButtonLabelInsets.width(),
                     size.height());
  }

  return size;
}

}  // namespace ash
