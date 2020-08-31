// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/authenticator_bio_enrollment_sheet_view.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/webauthn/ring_progress_bar.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/vector_icons.h"

#include <utility>

namespace {
static constexpr int kFingerprintSize = 120;
static constexpr SkColor kFingerprintColor = SkColorSetRGB(66, 133, 224);
static constexpr int kRingSize = 228;

double CalculateProgressFor(double samples_remaining, double max_samples) {
  return 1 - samples_remaining / (max_samples <= 0 ? 1 : max_samples);
}
}  // namespace

AuthenticatorBioEnrollmentSheetView::AuthenticatorBioEnrollmentSheetView(
    std::unique_ptr<AuthenticatorBioEnrollmentSheetModel> sheet_model)
    : AuthenticatorRequestSheetView(std::move(sheet_model)) {}

AuthenticatorBioEnrollmentSheetView::~AuthenticatorBioEnrollmentSheetView() =
    default;

std::unique_ptr<views::View>
AuthenticatorBioEnrollmentSheetView::BuildStepSpecificContent() {
  auto* bio_model = static_cast<AuthenticatorBioEnrollmentSheetModel*>(model());
  double target = CalculateProgressFor(bio_model->bio_samples_remaining(),
                                       bio_model->max_bio_samples());
  double initial;
  if (target <= 0) {
    initial = target;
  } else {
    initial = CalculateProgressFor(bio_model->bio_samples_remaining() + 1,
                                   bio_model->max_bio_samples());
  }

  auto animation_container = std::make_unique<views::View>();
  animation_container->SetLayoutManager(std::make_unique<views::FillLayout>());

  auto image_view = std::make_unique<NonAccessibleImageView>();
  image_view->SetVerticalAlignment(views::ImageView::Alignment::kCenter);
  gfx::IconDescription icon_description(
      target >= 1 ? views::kMenuCheckIcon : kFingerprintIcon, kFingerprintSize,
      kFingerprintColor);
  image_view->SetImage(gfx::CreateVectorIcon(icon_description));
  animation_container->AddChildView(std::move(image_view));

  auto ring_progress_bar = std::make_unique<RingProgressBar>();
  ring_progress_bar_ = ring_progress_bar.get();
  ring_progress_bar->SetPreferredSize(gfx::Size(kRingSize, kRingSize));
  ring_progress_bar->SetValue(initial, target);
  animation_container->AddChildView(std::move(ring_progress_bar));

  return animation_container;
}
