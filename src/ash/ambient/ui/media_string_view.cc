// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/media_string_view.h"

#include <memory>
#include <string>

#include "ash/ambient/ambient_constants.h"
#include "ash/ambient/ui/ambient_view_ids.h"
#include "ash/ambient/util/ambient_util.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/prefs/pref_service.h"
#include "services/media_session/public/cpp/media_session_service.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/font.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/shadow_value.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/transform.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/metadata/metadata_impl_macros.h"

namespace ash {

namespace {

// A layer delegate used for mask layer, with left and right gradient fading out
// zones.
class FadeoutLayerDelegate : public ui::LayerDelegate {
 public:
  FadeoutLayerDelegate() : layer_(ui::LAYER_TEXTURED) {
    layer_.set_delegate(this);
    layer_.SetFillsBoundsOpaquely(false);
  }

  ~FadeoutLayerDelegate() override { layer_.set_delegate(nullptr); }

  ui::Layer* layer() { return &layer_; }

 private:
  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override {
    const gfx::Size& size = layer()->size();
    gfx::Rect left_rect(0, 0, kMediaStringGradientWidthDip, size.height());
    gfx::Rect right_rect(size.width() - kMediaStringGradientWidthDip, 0,
                         kMediaStringGradientWidthDip, size.height());

    views::PaintInfo paint_info =
        views::PaintInfo::CreateRootPaintInfo(context, size);
    const auto& prs = paint_info.paint_recording_size();

    // Pass the scale factor when constructing PaintRecorder so the MaskLayer
    // size is not incorrectly rounded (see https://crbug.com/921274).
    ui::PaintRecorder recorder(context, paint_info.paint_recording_size(),
                               static_cast<float>(prs.width()) / size.width(),
                               static_cast<float>(prs.height()) / size.height(),
                               nullptr);

    gfx::Canvas* canvas = recorder.canvas();
    // Clear the canvas.
    canvas->DrawColor(SK_ColorBLACK, SkBlendMode::kSrc);
    // Draw left gradient zone.
    cc::PaintFlags flags;
    flags.setBlendMode(SkBlendMode::kSrc);
    flags.setAntiAlias(false);
    flags.setShader(gfx::CreateGradientShader(
        gfx::Point(), gfx::Point(kMediaStringGradientWidthDip, 0),
        SK_ColorTRANSPARENT, SK_ColorBLACK));
    canvas->DrawRect(left_rect, flags);

    // Draw right gradient zone.
    flags.setShader(gfx::CreateGradientShader(
        gfx::Point(size.width() - kMediaStringGradientWidthDip, 0),
        gfx::Point(size.width(), 0), SK_ColorBLACK, SK_ColorTRANSPARENT));
    canvas->DrawRect(right_rect, flags);
  }

  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}

  ui::Layer layer_;
};

// Typography.
constexpr char kMiddleDotSeparator[] = " \u2022 ";

constexpr int kMusicNoteIconSizeDip = 20;

// Returns true if we should show media string for ambient mode on lock-screen
// based on user pref. We should keep the same user policy here as the
// lock-screen media controls to avoid exposing user data on lock-screen without
// consent.
bool ShouldShowOnLockScreen() {
  PrefService* pref =
      Shell::Get()->session_controller()->GetPrimaryUserPrefService();
  DCHECK(pref);

  return pref->GetBoolean(prefs::kLockScreenMediaControlsEnabled);
}

}  // namespace

MediaStringView::MediaStringView() {
  SetID(AmbientViewID::kAmbientMediaStringView);
  InitLayout();
}

MediaStringView::~MediaStringView() = default;

void MediaStringView::OnViewBoundsChanged(views::View* observed_view) {
  UpdateMaskLayer();
}

void MediaStringView::MediaSessionInfoChanged(
    media_session::mojom::MediaSessionInfoPtr session_info) {
  if (ambient::util::IsShowing(LockScreen::ScreenType::kLock) &&
      !ShouldShowOnLockScreen()) {
    return;
  }

  // Don't show the media string if session info is unavailable, or the active
  // session is marked as sensitive.
  if (!session_info || session_info->is_sensitive) {
    SetVisible(false);
    return;
  }

  bool is_paused = session_info->playback_state ==
                   media_session::mojom::MediaPlaybackState::kPaused;

  // Don't show the media string if paused.
  SetVisible(!is_paused);
}

void MediaStringView::MediaSessionMetadataChanged(
    const base::Optional<media_session::MediaMetadata>& metadata) {
  media_session::MediaMetadata session_metadata =
      metadata.value_or(media_session::MediaMetadata());

  std::u16string media_string;
  std::u16string middle_dot = base::UTF8ToUTF16(kMiddleDotSeparator);
  if (!session_metadata.title.empty() && !session_metadata.artist.empty()) {
    media_string =
        session_metadata.title + middle_dot + session_metadata.artist;
  } else if (!session_metadata.title.empty()) {
    media_string = session_metadata.title;
  } else {
    media_string = session_metadata.artist;
  }

  // Reset text and stop any ongoing animation.
  media_text_->SetText(std::u16string());
  media_text_->layer()->GetAnimator()->StopAnimating();

  media_text_->SetText(media_string);
  media_text_->layer()->SetTransform(gfx::Transform());
  const auto& text_size = media_text_->GetPreferredSize();
  const int text_width = text_size.width();
  media_text_container_->SetPreferredSize(gfx::Size(
      std::min(kMediaStringMaxWidthDip, text_width), text_size.height()));

  if (NeedToAnimate()) {
    media_text_->SetText(media_string + middle_dot + media_string + middle_dot);
    ScheduleScrolling(/*is_initial=*/true);
  }
}

void MediaStringView::OnImplicitAnimationsCompleted() {
  if (!NeedToAnimate())
    return;

  ScheduleScrolling(/*is_initial=*/false);
}

void MediaStringView::InitLayout() {
  // This view will be drawn on its own layer instead of the layer of
  // |PhotoView| which has a solid black background.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  constexpr int kChildSpacingDip = 8;
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  layout->set_between_child_spacing(kChildSpacingDip);

  icon_ = AddChildView(std::make_unique<views::ImageView>());
  icon_->SetPreferredSize(
      gfx::Size(kMusicNoteIconSizeDip, kMusicNoteIconSizeDip));
  icon_->SetImage(gfx::CreateVectorIcon(
      kMusicNoteIcon, kMusicNoteIconSizeDip,
      ambient::util::GetContentLayerColor(
          AshColorProvider::ContentLayerType::kIconColorPrimary)));

  media_text_container_ = AddChildView(std::make_unique<views::View>());
  media_text_container_->SetPaintToLayer();
  media_text_container_->layer()->SetFillsBoundsOpaquely(false);
  media_text_container_->layer()->SetMasksToBounds(true);
  auto* text_layout = media_text_container_->SetLayoutManager(
      std::make_unique<views::FlexLayout>());
  text_layout->SetOrientation(views::LayoutOrientation::kHorizontal);
  text_layout->SetMainAxisAlignment(views::LayoutAlignment::kStart);
  text_layout->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
  observed_view_.Observe(media_text_container_);

  media_text_ =
      media_text_container_->AddChildView(std::make_unique<views::Label>());
  media_text_->SetPaintToLayer();
  media_text_->layer()->SetFillsBoundsOpaquely(false);

  // Defines the appearance.
  constexpr int kDefaultFontSizeDip = 64;
  constexpr int kMediaStringFontSizeDip = 18;
  media_text_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_TO_HEAD);
  media_text_->SetVerticalAlignment(gfx::VerticalAlignment::ALIGN_MIDDLE);
  media_text_->SetAutoColorReadabilityEnabled(false);
  media_text_->SetEnabledColor(ambient::util::GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary));
  media_text_->SetFontList(
      ambient::util::GetDefaultFontlist()
          .DeriveWithSizeDelta(kMediaStringFontSizeDip - kDefaultFontSizeDip)
          .DeriveWithWeight(gfx::Font::Weight::MEDIUM));
  media_text_->SetElideBehavior(gfx::ElideBehavior::NO_ELIDE);

  auto shadow_values = ambient::util::GetTextShadowValues();
  media_text_->SetShadows(shadow_values);
  gfx::Insets shadow_insets = gfx::ShadowValue::GetMargin(shadow_values);
  // Compensate the shadow insets to put the text middle align with the icon.
  media_text_->SetBorder(views::CreateEmptyBorder(
      /*top=*/-shadow_insets.bottom(),
      /*left=*/0,
      /*bottom=*/-shadow_insets.top(),
      /*right=*/0));

  BindMediaControllerObserver();
}

void MediaStringView::BindMediaControllerObserver() {
  media_session::MediaSessionService* service =
      Shell::Get()->shell_delegate()->GetMediaSessionService();
  // Service might be unavailable under some test environments.
  if (!service)
    return;

  // Binds to the MediaControllerManager and create a MediaController for the
  // current active media session so that we can observe it.
  mojo::Remote<media_session::mojom::MediaControllerManager>
      controller_manager_remote;
  service->BindMediaControllerManager(
      controller_manager_remote.BindNewPipeAndPassReceiver());
  controller_manager_remote->CreateActiveMediaController(
      media_controller_remote_.BindNewPipeAndPassReceiver());

  // Observe the active media controller for changes.
  media_controller_remote_->AddObserver(
      observer_receiver_.BindNewPipeAndPassRemote());
}

void MediaStringView::UpdateMaskLayer() {
  if (!NeedToAnimate()) {
    media_text_container_->layer()->SetMaskLayer(nullptr);
    return;
  }

  if (!fadeout_layer_delegate_) {
    fadeout_layer_delegate_ = std::make_unique<FadeoutLayerDelegate>();
    fadeout_layer_delegate_->layer()->SetBounds(
        media_text_container_->layer()->bounds());
  }
  media_text_container_->layer()->SetMaskLayer(
      fadeout_layer_delegate_->layer());
}

bool MediaStringView::NeedToAnimate() const {
  return media_text_->GetPreferredSize().width() >
         media_text_container_->GetPreferredSize().width();
}

gfx::Transform MediaStringView::GetMediaTextTransform(bool is_initial) {
  gfx::Transform transform;
  if (is_initial) {
    // Start animation half way of |media_text_container_|.
    transform.Translate(kMediaStringMaxWidthDip / 2, 0);
  }
  return transform;
}

void MediaStringView::ScheduleScrolling(bool is_initial) {
  if (!GetVisible())
    return;

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&MediaStringView::StartScrolling,
                                weak_factory_.GetWeakPtr(), is_initial));
}

void MediaStringView::StartScrolling(bool is_initial) {
  ui::Layer* text_layer = media_text_->layer();
  text_layer->SetTransform(GetMediaTextTransform(is_initial));
  {
    // Desired speed is 10 seconds for kMediaStringMaxWidthDip.
    const int text_width = media_text_->GetPreferredSize().width();
    const int shadow_width =
        gfx::ShadowValue::GetMargin(ambient::util::GetTextShadowValues())
            .width();
    const int start_x = text_layer->GetTargetTransform().To2dTranslation().x();
    const int end_x = -(text_width + shadow_width) / 2;
    const int transform_distance = start_x - end_x;
    const base::TimeDelta kScrollingDuration =
        base::TimeDelta::FromSeconds(10) * transform_distance /
        kMediaStringMaxWidthDip;

    ui::ScopedLayerAnimationSettings animation(text_layer->GetAnimator());
    animation.SetTransitionDuration(kScrollingDuration);
    animation.SetTweenType(gfx::Tween::LINEAR);
    animation.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_SET_NEW_TARGET);
    animation.AddObserver(this);

    gfx::Transform transform;
    transform.Translate(end_x, 0);
    text_layer->SetTransform(transform);
  }
}

BEGIN_METADATA(MediaStringView, views::View)
END_METADATA

}  // namespace ash
