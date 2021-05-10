// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_settings_view.h"

#include "ash/capture_mode/capture_mode_bar_view.h"
#include "ash/capture_mode/capture_mode_constants.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_settings_entry_view.h"
#include "ash/capture_mode/capture_mode_toggle_button.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "base/bind.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/style/platform_style.h"

namespace ash {

namespace {

constexpr gfx::Size kSettingsSize{256, 52};

constexpr gfx::Insets kSettingsPadding{/*vertical=*/16, /*horizontal=*/16};

constexpr gfx::RoundedCornersF kBorderRadius{10.f};

}  // namespace

CaptureModeSettingsView::CaptureModeSettingsView()
    : microphone_view_(
          AddChildView(std::make_unique<CaptureModeSettingsEntryView>(
              base::BindRepeating(&CaptureModeSettingsView::OnMicrophoneToggled,
                                  base::Unretained(this)),
              kCaptureModeMicOffIcon,
              IDS_ASH_SCREEN_CAPTURE_LABEL_MICROPHONE))) {
  SetPaintToLayer();
  auto* color_provider = AshColorProvider::Get();
  SkColor background_color = color_provider->GetBaseLayerColor(
      AshColorProvider::BaseLayerType::kTransparent80);
  SetBackground(views::CreateSolidBackground(background_color));
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetRoundedCornerRadius(kBorderRadius);
  layer()->SetBackgroundBlur(
      static_cast<float>(AshColorProvider::LayerBlurSigma::kBlurDefault));
  layer()->SetBackdropFilterQuality(capture_mode::kBlurQuality);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, kSettingsPadding,
      capture_mode::kBetweenChildSpacing));

  OnMicrophoneChanged(CaptureModeController::Get()->enable_audio_recording());
}

CaptureModeSettingsView::~CaptureModeSettingsView() = default;

// static
gfx::Rect CaptureModeSettingsView::GetBounds(
    CaptureModeBarView* capture_mode_bar_view) {
  DCHECK(capture_mode_bar_view);

  return gfx::Rect(
      capture_mode_bar_view->settings_button()->GetBoundsInScreen().right() -
          kSettingsSize.width(),
      capture_mode_bar_view->GetBoundsInScreen().y() -
          capture_mode::kSpaceBetweenCaptureBarAndSettingsMenu -
          kSettingsSize.height(),
      kSettingsSize.width(), kSettingsSize.height());
}

void CaptureModeSettingsView::OnMicrophoneChanged(bool microphone_enabled) {
  microphone_view_->toggle_button_view()->SetIsOn(microphone_enabled);
  microphone_view_->SetIcon(microphone_enabled ? kCaptureModeMicIcon
                                               : kCaptureModeMicOffIcon);
}

void CaptureModeSettingsView::OnMicrophoneToggled() {
  CaptureModeController::Get()->EnableAudioRecording(
      microphone_view_->toggle_button_view()->GetIsOn());
}

BEGIN_METADATA(CaptureModeSettingsView, views::View)
END_METADATA

}  // namespace ash
