// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/ambient_animation_view.h"

#include <algorithm>
#include <cstdlib>
#include <utility>

#include "ash/ambient/model/ambient_animation_attribution_provider.h"
#include "ash/ambient/model/ambient_backend_model.h"
#include "ash/ambient/model/ambient_photo_config.h"
#include "ash/ambient/resources/ambient_animation_static_resources.h"
#include "ash/ambient/ui/ambient_animation_player.h"
#include "ash/ambient/ui/ambient_animation_resizer.h"
#include "ash/ambient/ui/ambient_animation_shield_controller.h"
#include "ash/ambient/ui/ambient_view_delegate.h"
#include "ash/ambient/ui/ambient_view_ids.h"
#include "ash/ambient/ui/glanceable_info_view.h"
#include "ash/ambient/ui/media_string_view.h"
#include "ash/ambient/util/ambient_util.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "cc/metrics/frame_sequence_tracker.h"
#include "cc/paint/skottie_color_map.h"
#include "cc/paint/skottie_wrapper.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/shadow_value.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/animated_image_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// How often to shift the animation slightly to prevent screen burn.
constexpr base::TimeDelta kAnimationJitterPeriod = base::Minutes(2);

constexpr JitterCalculator::Config kAnimationJitterConfig = {
    /*step_size=*/2,
    /*x_min_translation=*/-10,
    /*x_max_translation=*/10,
    /*y_min_translation=*/-10,
    /*y_max_translation=*/10};

constexpr base::TimeDelta kThroughputTrackerRestartPeriod = base::Seconds(30);

// Amount of x and y padding there should be from the top-left of the
// AmbientAnimationView to the top-left of the weather/time content views.
constexpr int kWeatherTimeBorderPaddingDip = 28;

// Amount of padding from the left and bottom of the AmbientAnimationView's
// bounds to the bottom-left of the media string content views.
constexpr int kMediaStringPaddingFromLeftDip = 28;
constexpr int kMediaStringPaddingFromBottomDip = 24;

constexpr int kMediaStringTextElevation = 1;

constexpr int kTimeFontSizeDip = 32;

// Google Grey 500 with 10% opacity.
constexpr SkColor kDarkModeShieldColor =
    SkColorSetA(gfx::kGoogleGrey900, SK_AlphaOPAQUE / 10);

// TODO(esum): Record throughput metrics to track animation performance in the
// field. We can use ash::metrics_util::CalculateSmoothness().
void LogCompositorThroughput(
    base::TimeTicks logging_start_time,
    const cc::FrameSequenceMetrics::CustomReportData& data) {
  base::TimeDelta duration = base::TimeTicks::Now() - logging_start_time;
  float duration_sec = duration.InSecondsF();
  // Use VLOG instead of DVLOG since this log is performance-related and
  // developers will almost certainly only care about this log on non-debug
  // builds. The overhead of "--vmodule" regex matching is very minor so far to
  // performance/CPU.
  VLOG(1) << "Compositor throughput report: frames_expected="
          << data.frames_expected << " frames_produced=" << data.frames_produced
          << " jank_count=" << data.jank_count
          << " expected_fps=" << data.frames_expected / duration_sec
          << " actual_fps=" << data.frames_produced / duration_sec
          << " duration=" << duration;
}

// Returns the maximum possible displacement in either dimension from the
// original unshifted position when jitter is applied.
int GetPaddingForAnimationJitter() {
  return std::max({abs(kAnimationJitterConfig.x_min_translation),
                   abs(kAnimationJitterConfig.x_max_translation),
                   abs(kAnimationJitterConfig.y_min_translation),
                   abs(kAnimationJitterConfig.y_max_translation)});
}

// The border serves as padding between the GlanceableInfoView and its
// parent view's bounds.
std::unique_ptr<views::Border> CreateGlanceableInfoBorder(
    const gfx::Vector2d& jitter = gfx::Vector2d()) {
  int top_padding = kWeatherTimeBorderPaddingDip + jitter.y();
  int left_padding = kWeatherTimeBorderPaddingDip + jitter.x();
  DCHECK_GE(top_padding, 0);
  DCHECK_GE(left_padding, 0);
  return views::CreateEmptyBorder(
      gfx::Insets::TLBR(top_padding, left_padding, 0, 0));
}

// The border serves as padding between the MediaStringView and its
// parent view's bounds.
std::unique_ptr<views::Border> CreateMediaStringBorder(
    const gfx::Vector2d& jitter = gfx::Vector2d()) {
  gfx::Insets shadow_insets = gfx::ShadowValue::GetMargin(
      ambient::util::GetTextShadowValues(nullptr, kMediaStringTextElevation));
  int bottom_padding =
      kMediaStringPaddingFromBottomDip + shadow_insets.bottom() + jitter.y();
  int left_padding =
      kMediaStringPaddingFromLeftDip + shadow_insets.left() + jitter.x();
  DCHECK_GE(bottom_padding, 0);
  DCHECK_GE(left_padding, 0);
  return views::CreateEmptyBorder(
      gfx::Insets::TLBR(0, left_padding, bottom_padding, 0));
}

}  // namespace

AmbientAnimationView::AmbientAnimationView(
    AmbientViewDelegate* view_delegate,
    std::unique_ptr<const AmbientAnimationStaticResources> static_resources)
    : event_handler_(view_delegate->GetAmbientViewEventHandler()),
      static_resources_(std::move(static_resources)),
      animation_photo_provider_(static_resources_.get(),
                                view_delegate->GetAmbientBackendModel()),
      animation_jitter_calculator_(kAnimationJitterConfig) {
  SetID(AmbientViewID::kAmbientAnimationView);
  Init(view_delegate);
}

AmbientAnimationView::~AmbientAnimationView() = default;

void AmbientAnimationView::Init(AmbientViewDelegate* view_delegate) {
  SetUseDefaultFillLayout(true);

  views::View* animation_container_view =
      AddChildView(std::make_unique<views::View>());
  animation_container_view->SetUseDefaultFillLayout(true);
  // Purely for performance reasons. Gains 3-4 fps.
  animation_container_view->SetPaintToLayer();

  animated_image_view_ = animation_container_view->AddChildView(
      std::make_unique<views::AnimatedImageView>());
  auto animation = std::make_unique<lottie::Animation>(
      static_resources_->GetSkottieWrapper(), cc::SkottieColorMap(),
      &animation_photo_provider_);
  animation_observer_.Observe(animation.get());
  animated_image_view_->SetAnimatedImage(std::move(animation));
  animated_image_view_observer_.Observe(animated_image_view_);
  animation_attribution_provider_ =
      std::make_unique<AmbientAnimationAttributionProvider>(
          &animation_photo_provider_, animated_image_view_->animated_image());

  // SetPaintToLayer() causes a view to be painted above its non-layer-backed
  // siblings, irrespective of the order they were added in. Using an
  // intermediate layer-backed |animation_container_view| ensures the shield is
  // painted on top of the animation, while still getting performance benefits.
  auto shield_view = std::make_unique<views::View>();
  shield_view->SetID(kAmbientShieldView);
  shield_view->SetBackground(
      views::CreateSolidBackground(kDarkModeShieldColor));
  shield_view_controller_ = std::make_unique<AmbientAnimationShieldController>(
      std::move(shield_view), /*parent_view=*/animation_container_view);

  // The set of weather/time views embedded within GlanceableInfoView should
  // appear in the top-left of the the AmbientAnimationView's boundaries with
  // |kWeatherTimeBorderPaddingDip| from the top-left corner. However, the
  // weather/time components must be bottom-aligned like so:
  // +-------------------------------------------------------------------------+
  // |                                                                         |
  // |  +----+     +--+                                                        |
  // |  |    |+---+|  |                                                        |
  // |  |    ||   ||  |                                                        |
  // |  +----++---++--+                                                        |
  // |                                                                         |
  // |                                                                         |
  // |                                                                         |
  // |                                                                         |
  // |                                                                         |
  // |                                                                         |
  // |                                                                         |
  // |                                                                         |
  // +-------------------------------------------------------------------------+
  // As opposed to top-aligned :
  // +-------------------------------------------------------------------------+
  // |                                                                         |
  // |  +----++---++--+                                                        |
  // |  |    ||   ||  |                                                        |
  // |  |    |+---+|  |                                                        |
  // |  +----+     +--+                                                        |
  // |                                                                         |
  // |                                                                         |
  // |                                                                         |
  // |                                                                         |
  // |                                                                         |
  // |                                                                         |
  // |                                                                         |
  // |                                                                         |
  // +-------------------------------------------------------------------------+
  //
  // To accomplish this, a "container" view is first created that is top-aligned
  // and has no actual content. GlanceableInfoView is then added as a child of
  // the container view and bottom-aligns its contents within the container.
  glanceable_info_container_ =
      AddChildView(std::make_unique<views::BoxLayoutView>());
  glanceable_info_container_->SetOrientation(
      views::BoxLayout::Orientation::kVertical);
  glanceable_info_container_->SetMainAxisAlignment(
      views::BoxLayout::MainAxisAlignment::kStart);
  glanceable_info_container_->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  glanceable_info_container_->SetBorder(CreateGlanceableInfoBorder());
  glanceable_info_container_->AddChildView(std::make_unique<GlanceableInfoView>(
      view_delegate, kTimeFontSizeDip,
      /*time_temperature_font_color=*/gfx::kGoogleGrey900));

  // Media string should appear in the bottom-left corner of the
  // AmbientAnimationView's bounds.
  media_string_container_ =
      AddChildView(std::make_unique<views::BoxLayoutView>());
  media_string_container_->SetOrientation(
      views::BoxLayout::Orientation::kVertical);
  media_string_container_->SetMainAxisAlignment(
      views::BoxLayout::MainAxisAlignment::kEnd);
  media_string_container_->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  media_string_container_->SetBorder(CreateMediaStringBorder());
  MediaStringView* media_string_view = media_string_container_->AddChildView(
      std::make_unique<MediaStringView>(MediaStringView::Settings(
          {/*icon_light_mode_color=*/gfx::kGoogleGrey600,
           /*icon_dark_mode_color=*/gfx::kGoogleGrey500,
           /*text_light_mode_color=*/gfx::kGoogleGrey600,
           /*text_dark_mode_color=*/gfx::kGoogleGrey500,
           kMediaStringTextElevation})));
  media_string_view->SetVisible(false);
}

void AmbientAnimationView::AnimationCycleEnded(
    const lottie::Animation* animation) {
  event_handler_->OnMarkerHit(AmbientPhotoConfig::Marker::kUiCycleEnded);
  base::TimeTicks now = base::TimeTicks::Now();
  if (now - last_jitter_timestamp_ >= kAnimationJitterPeriod) {
    // AnimationCycleEnded() may be called while a ui "paint" operation is still
    // in progress. Changing translation properties of the UI while a paint
    // operation is in progress results in a fatal error deep in the UI stack.
    // Thus, post a task to apply jitter rather than invoking it synchronously.
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&AmbientAnimationView::ApplyJitter,
                                  weak_factory_.GetWeakPtr()));
    last_jitter_timestamp_ = now;
  }
}

void AmbientAnimationView::OnViewBoundsChanged(View* observed_view) {
  DCHECK_EQ(observed_view, static_cast<View*>(animated_image_view_));
  DVLOG(4) << __func__ << " to "
           << animated_image_view_->GetContentsBounds().ToString();
  if (animated_image_view_->GetContentsBounds().IsEmpty())
    return;

  // By default, the |animated_image_view_| will render the animation with the
  // fixed dimensions specified in the Lottie file. To render the animation
  // at the view's full bounds, wait for the view's initial layout to happen
  // so that its proper bounds become available (they are 0x0 initially) before
  // starting the animation playback.
  gfx::Rect previous_animation_bounds = animated_image_view_->GetImageBounds();
  AmbientAnimationResizer::Resize(*animated_image_view_,
                                  GetPaddingForAnimationJitter());
  DVLOG(4)
      << "View bounds available. Resized animation with native size "
      << animated_image_view_->animated_image()->GetOriginalSize().ToString()
      << " from " << previous_animation_bounds.ToString() << " to "
      << animated_image_view_->GetImageBounds().ToString();
  StartPlayingAnimation();
  if (!throughput_tracker_restart_timer_.IsRunning()) {
    RestartThroughputTracking();
    throughput_tracker_restart_timer_.Start(
        FROM_HERE, kThroughputTrackerRestartPeriod, this,
        &AmbientAnimationView::RestartThroughputTracking);
  }
}

void AmbientAnimationView::StartPlayingAnimation() {
  // There should only be one active AmbientAnimationPlayer at any given time,
  // otherwise multiple active players can lead to confusing simultaneous state
  // changes. So destroy the existing player first before creating a new one.
  animation_player_.reset();
  // |animated_image_view_| is owned by the base |View| class and outlives the
  // |animation_player_|, so it's safe to pass a raw ptr here.
  animation_player_ =
      std::make_unique<AmbientAnimationPlayer>(animated_image_view_);
  event_handler_->OnMarkerHit(AmbientPhotoConfig::Marker::kUiStartRendering);
  last_jitter_timestamp_ = base::TimeTicks::Now();
}

void AmbientAnimationView::RestartThroughputTracking() {
  // Stop() must be called to trigger throughput reporting.
  if (throughput_tracker_ && !throughput_tracker_->Stop()) {
    LOG(WARNING) << "Throughput will not be reported";
  }

  views::Widget* widget = GetWidget();
  DCHECK(widget);
  ui::Compositor* compositor = widget->GetCompositor();
  DCHECK(compositor);
  throughput_tracker_ = compositor->RequestNewThroughputTracker();
  throughput_tracker_->Start(base::BindOnce(
      &LogCompositorThroughput, /*logging_start_time=*/base::TimeTicks::Now()));
}

void AmbientAnimationView::ApplyJitter() {
  gfx::Vector2d jitter = animation_jitter_calculator_.Calculate();
  DVLOG(4) << "Applying jitter to animation: " << jitter.ToString();
  // Sharing the same jitter between the animation and other peripheral content
  // keeps the spacing between features consistent.
  animated_image_view_->SetAdditionalTranslation(jitter);
  glanceable_info_container_->SetBorder(CreateGlanceableInfoBorder(jitter));
  media_string_container_->SetBorder(CreateMediaStringBorder(jitter));
}

BEGIN_METADATA(AmbientAnimationView, views::View)
END_METADATA

}  // namespace ash
