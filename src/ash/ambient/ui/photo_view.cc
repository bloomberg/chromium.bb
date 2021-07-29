// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/photo_view.h"

#include <algorithm>
#include <iterator>
#include <memory>

#include "ash/ambient/ambient_constants.h"
#include "ash/ambient/model/ambient_backend_model.h"
#include "ash/ambient/ui/ambient_background_image_view.h"
#include "ash/ambient/ui/ambient_view_delegate.h"
#include "ash/ambient/ui/ambient_view_ids.h"
#include "ash/public/cpp/metrics_util.h"
#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/aura/window.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/animation_throughput_reporter.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

namespace {

constexpr char kPhotoTransitionSmoothness[] =
    "Ash.AmbientMode.AnimationSmoothness.PhotoTransition";

void ReportSmoothness(int value) {
  base::UmaHistogramPercentageObsoleteDoNotUse(kPhotoTransitionSmoothness,
                                               value);
}

}  // namespace

// PhotoView ------------------------------------------------------------------
PhotoView::PhotoView(AmbientViewDelegate* delegate) : delegate_(delegate) {
  DCHECK(delegate_);
  SetID(AmbientViewID::kAmbientPhotoView);
  Init();
}

PhotoView::~PhotoView() = default;

void PhotoView::OnImageAdded() {
  // If NeedToAnimate() is true, will start transition animation and
  // UpdateImages() when animation completes. Otherwise, update images
  // immediately.
  if (NeedToAnimateTransition()) {
    StartTransitionAnimation();
    return;
  }

  UpdateImage(delegate_->GetAmbientBackendModel()->GetNextImage());
}

void PhotoView::Init() {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  SetLayoutManager(std::make_unique<views::FillLayout>());

  for (auto*& image_view : image_views_) {
    // Creates image views.
    image_view =
        AddChildView(std::make_unique<AmbientBackgroundImageView>(delegate_));
    // Each image view will be animated on its own layer.
    image_view->SetPaintToLayer();
    image_view->layer()->SetFillsBoundsOpaquely(false);
  }

  // Hides one image view initially for fade in animation.
  image_views_.back()->layer()->SetOpacity(0.0f);

  auto* model = delegate_->GetAmbientBackendModel();
  scoped_backend_model_observer_.Observe(model);

  // |PhotoView::Init| is called after
  // |AmbientBackendModelObserver::OnImagesReady| has been called.
  // |AmbientBackendModel| has two images ready and views should be constructed
  // for each one.
  UpdateImage(model->GetCurrentImage());
  UpdateImage(model->GetNextImage());
}

void PhotoView::UpdateImage(const PhotoWithDetails& next_image) {
  if (next_image.photo.isNull())
    return;

  image_views_.at(image_index_)
      ->UpdateImage(next_image.photo, next_image.related_photo,
                    next_image.is_portrait, next_image.topic_type);
  image_views_.at(image_index_)
      ->UpdateImageDetails(base::UTF8ToUTF16(next_image.details),
                           base::UTF8ToUTF16(next_image.related_details));
  image_index_ = 1 - image_index_;
}

void PhotoView::StartTransitionAnimation() {
  ui::Layer* visible_layer = image_views_.at(image_index_)->layer();
  {
    ui::ScopedLayerAnimationSettings animation(visible_layer->GetAnimator());
    animation.SetTransitionDuration(kAnimationDuration);
    animation.SetTweenType(gfx::Tween::LINEAR);
    animation.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_SET_NEW_TARGET);
    animation.CacheRenderSurface();

    ui::AnimationThroughputReporter reporter(
        animation.GetAnimator(),
        metrics_util::ForSmoothness(base::BindRepeating(ReportSmoothness)));

    visible_layer->SetOpacity(0.0f);
  }

  ui::Layer* invisible_layer = image_views_.at(1 - image_index_)->layer();
  {
    ui::ScopedLayerAnimationSettings animation(invisible_layer->GetAnimator());
    animation.SetTransitionDuration(kAnimationDuration);
    animation.SetTweenType(gfx::Tween::LINEAR);
    animation.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_SET_NEW_TARGET);
    animation.CacheRenderSurface();
    // For simplicity, only observe one animation.
    animation.AddObserver(this);

    ui::AnimationThroughputReporter reporter(
        animation.GetAnimator(),
        metrics_util::ForSmoothness(base::BindRepeating(ReportSmoothness)));

    invisible_layer->SetOpacity(1.0f);
  }
}

void PhotoView::OnImplicitAnimationsCompleted() {
  UpdateImage(delegate_->GetAmbientBackendModel()->GetNextImage());
  delegate_->OnPhotoTransitionAnimationCompleted();
}

bool PhotoView::NeedToAnimateTransition() const {
  // Can do transition animation if both two images in |images_unscaled_| are
  // not nullptr. Check the image index 1 is enough.
  return !image_views_.back()->GetCurrentImage().isNull();
}

gfx::ImageSkia PhotoView::GetVisibleImageForTesting() {
  return image_views_.at(image_index_)->GetCurrentImage();
}

BEGIN_METADATA(PhotoView, views::View)
END_METADATA

}  // namespace ash
