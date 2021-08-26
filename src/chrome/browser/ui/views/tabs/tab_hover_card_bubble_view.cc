// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_hover_card_bubble_view.h"

#include <algorithm>
#include <memory>
#include <string>

#include "base/containers/mru_cache.h"
#include "base/cxx17_backports.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/thumbnails/thumbnail_image.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_hover_card_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/url_formatter/url_formatter.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/theme_provider.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/native_theme/native_theme.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

#if defined(OS_WIN)
#include "ui/base/win/shell.h"
#endif

namespace {
// Maximum number of lines that a title label occupies.
constexpr int kHoverCardTitleMaxLines = 2;

constexpr int kHorizontalMargin = 18;
constexpr int kVerticalMargin = 10;
constexpr int kFootnoteVerticalMargin = 8;
constexpr gfx::Insets kTitleMargins(kVerticalMargin, kHorizontalMargin);
constexpr gfx::Insets kAlertMargins(kFootnoteVerticalMargin, kHorizontalMargin);

bool CustomShadowsSupported() {
#if defined(OS_WIN)
  return ui::win::IsAeroGlassEnabled();
#else
  return true;
#endif
}

std::unique_ptr<views::Label> CreateAlertView(const TabAlertState& state) {
  auto alert_state_label = std::make_unique<views::Label>(
      std::u16string(), views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_PRIMARY);
  alert_state_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  alert_state_label->SetMultiLine(true);
  alert_state_label->SetVisible(true);
  alert_state_label->SetText(chrome::GetTabAlertStateText(state));
  return alert_state_label;
}

// Calculates an appropriate size to display a preview image in the hover card.
// For the vast majority of images, the |preferred_size| is used, but extremely
// tall or wide images use the image size instead, centering in the available
// space.
gfx::Size GetPreviewImageSize(gfx::Size preview_size,
                              gfx::Size preferred_size) {
  DCHECK(!preferred_size.IsEmpty());
  if (preview_size.IsEmpty())
    return preview_size;
  const float preview_aspect_ratio =
      static_cast<float>(preview_size.width()) / preview_size.height();
  const float preferred_aspect_ratio =
      static_cast<float>(preferred_size.width()) / preferred_size.height();
  const float ratio = preview_aspect_ratio / preferred_aspect_ratio;
  // Images between 2/3 and 3/2 of the target aspect ratio use the preferred
  // size, stretching the image. Only images outside this range get centered.
  // Since this is a corner case most users will never see, the specific cutoffs
  // just need to be reasonable and don't need to be precise values (that is,
  // there is no "correct" value; if the results are not aesthetic they can be
  // tuned).
  constexpr float kMinStretchRatio = 0.667f;
  constexpr float kMaxStretchRatio = 1.5f;
  if (ratio >= kMinStretchRatio && ratio <= kMaxStretchRatio)
    return preferred_size;
  return preview_size;
}

bool UseAlternateHoverCardFormat() {
  static const int use_alternate_format =
      base::GetFieldTrialParamByFeatureAsInt(
          features::kTabHoverCardImages, features::kTabHoverCardAlternateFormat,
          0);
  return use_alternate_format != 0;
}

// Label that renders its background in a solid color. Placed in front of a
// normal label either by being later in the draw order or on a layer, it can
// be used to animate a fade-out.
class SolidLabel : public views::Label {
 public:
  METADATA_HEADER(SolidLabel);
  using Label::Label;
  SolidLabel() = default;
  ~SolidLabel() override = default;

 protected:
  // views::Label:
  void OnPaintBackground(gfx::Canvas* canvas) override {
    canvas->DrawColor(GetBackgroundColor());
  }
};

BEGIN_METADATA(SolidLabel, views::Label)
END_METADATA

}  // namespace

// This view overlays and fades out an old version of the text of a label,
// while displaying the new text underneath. It is used to fade out the old
// value of the title and domain labels on the hover card when the tab switches
// or the tab title changes.
class TabHoverCardBubbleView::FadeLabel : public views::View {
 public:
  FadeLabel(int context, int num_lines) {
    primary_label_ = AddChildView(std::make_unique<views::Label>(
        std::u16string(), context, views::style::STYLE_PRIMARY));
    primary_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    primary_label_->SetVerticalAlignment(gfx::ALIGN_TOP);
    primary_label_->SetMultiLine(num_lines > 1);
    if (num_lines > 1)
      primary_label_->SetMaxLines(num_lines);

    label_fading_out_ = AddChildView(std::make_unique<SolidLabel>(
        std::u16string(), context, views::style::STYLE_PRIMARY));
    label_fading_out_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    label_fading_out_->SetVerticalAlignment(gfx::ALIGN_TOP);
    label_fading_out_->SetMultiLine(num_lines > 1);
    if (num_lines > 1)
      label_fading_out_->SetMaxLines(num_lines);
    label_fading_out_->GetViewAccessibility().OverrideIsIgnored(true);

    SetLayoutManager(std::make_unique<views::FillLayout>());
  }

  ~FadeLabel() override = default;

  void SetText(std::u16string text, absl::optional<bool> is_filename) {
    if (was_filename_.has_value())
      SetMultilineParams(label_fading_out_, was_filename_.value());
    label_fading_out_->SetText(primary_label_->GetText());
    if (is_filename.has_value())
      SetMultilineParams(primary_label_, is_filename.value());
    was_filename_ = is_filename;
    primary_label_->SetText(text);
  }

  // Sets the fade-out of the label as |percent| in the range [0, 1]. Since
  // FadeLabel is designed to mask new text with the old and then fade away, the
  // higher the percentage the less opaque the label.
  void SetFade(double percent) {
    percent_ = std::min(1.0, percent);
    if (percent_ == 1.0)
      label_fading_out_->SetText(std::u16string());
    const SkAlpha alpha = base::saturated_cast<SkAlpha>(
        std::numeric_limits<SkAlpha>::max() * (1.0 - percent_));
    label_fading_out_->SetBackgroundColor(
        SkColorSetA(label_fading_out_->GetBackgroundColor(), alpha));
    label_fading_out_->SetEnabledColor(
        SkColorSetA(label_fading_out_->GetEnabledColor(), alpha));
  }

  std::u16string GetText() const { return primary_label_->GetText(); }

 protected:
  // views::View:
  gfx::Size GetMaximumSize() const override {
    return gfx::Tween::SizeValueBetween(percent_,
                                        label_fading_out_->GetPreferredSize(),
                                        primary_label_->GetPreferredSize());
  }

  gfx::Size CalculatePreferredSize() const override {
    return primary_label_->GetPreferredSize();
  }

  gfx::Size GetMinimumSize() const override {
    return primary_label_->GetMinimumSize();
  }

  int GetHeightForWidth(int width) const override {
    return primary_label_->GetHeightForWidth(width);
  }

 private:
  static void SetMultilineParams(views::Label* label, bool is_filename) {
    if (is_filename) {
      label->SetMultiLine(false);
      label->SetElideBehavior(gfx::ELIDE_MIDDLE);
    } else {
      label->SetElideBehavior(gfx::ELIDE_TAIL);
      label->SetMultiLine(true);
    }
  }

  views::Label* primary_label_;
  SolidLabel* label_fading_out_;
  absl::optional<bool> was_filename_;
  double percent_ = 1.0;
};

// Represents the preview image on the hover card. Allows for a new image to be
// faded in over the old image.
class TabHoverCardBubbleView::ThumbnailView
    : public views::View,
      public views::AnimationDelegateViews {
 public:
  // Specifies which (if any) of the corners of the preview image will be
  // rounded. See SetRoundedCorners() below for more information.
  enum class RoundedCorners { kNone, kTopCorners, kBottomCorners };

  explicit ThumbnailView(TabHoverCardBubbleView* bubble_view)
      : AnimationDelegateViews(this),
        bubble_view_(bubble_view),
        image_transition_animation_(this) {
    constexpr base::TimeDelta kImageTransitionDuration =
        kHoverCardSlideDuration;
    image_transition_animation_.SetDuration(kImageTransitionDuration);

    // Set a reasonable preview size so that ThumbnailView() is given an
    // appropriate amount of space in the layout.
    target_tab_image_ = AddChildView(CreateImageView());
    image_fading_out_ = AddChildView(CreateImageView());
    image_fading_out_->SetPaintToLayer();
    image_fading_out_->layer()->SetOpacity(0.0f);

    SetLayoutManager(std::make_unique<views::FillLayout>());
  }

  // Sets the appropriate rounded corners for the preview image, for platforms
  // where layers must be explicitly clipped (because they are not clipped by
  // the widget). Set `rounded_corners` to kTopCorners if the preview image is
  // the topmost view in the widget (including header); set kBottomCorners if
  // the preview is the bottom-most view (including footer). If neither, use
  // kNone.
  void SetRoundedCorners(RoundedCorners rounded_corners, float radius) {
    gfx::RoundedCornersF corners;
    switch (rounded_corners) {
      case RoundedCorners::kNone:
        corners = {0, 0, 0, 0};
        break;
      case RoundedCorners::kTopCorners:
        corners = {radius, radius, 0, 0};
        break;
      case RoundedCorners::kBottomCorners:
        corners = {0, 0, radius, radius};
        break;
    }
    image_fading_out_->layer()->SetRoundedCornerRadius(corners);
  }

  // Sets the new preview image. The old image will be faded out.
  void SetTargetTabImage(gfx::ImageSkia preview_image) {
    StartFadeOut();
    SetImage(target_tab_image_, preview_image, ImageType::kThumbnail);
    image_type_ = ImageType::kThumbnail;
  }

  // Clears the preview image and replaces it with a placeholder image. The old
  // image will be faded out.
  void SetPlaceholderImage() {
    if (image_type_ == ImageType::kPlaceholder)
      return;

    // Theme provider may be null if there is no associated widget. In that case
    // there is nothing to render, and we can't get theme default colors to
    // render with anyway, so bail out.
    const ui::ThemeProvider* const theme_provider = GetThemeProvider();
    if (!theme_provider)
      return;

    StartFadeOut();

    // Check the no-preview color and size to see if it needs to be
    // regenerated. DPI or theme change can cause a regeneration.
    const SkColor foreground_color = theme_provider->GetColor(
        ThemeProperties::COLOR_HOVER_CARD_NO_PREVIEW_FOREGROUND);

    // Set the no-preview placeholder image. All sizes are in DIPs.
    // gfx::CreateVectorIcon() caches its result so there's no need to store
    // images here; if a particular size/color combination has already been
    // requested it will be low-cost to request it again.
    constexpr gfx::Size kNoPreviewImageSize{64, 64};
    const gfx::ImageSkia no_preview_image = gfx::CreateVectorIcon(
        kGlobeIcon, kNoPreviewImageSize.width(), foreground_color);
    SetImage(target_tab_image_, no_preview_image, ImageType::kPlaceholder);
    image_type_ = ImageType::kPlaceholder;
  }

  void ClearImage() {
    if (image_type_ == ImageType::kNone)
      return;

    StartFadeOut();
    SetImage(target_tab_image_, gfx::ImageSkia(), ImageType::kNone);
    image_type_ = ImageType::kNone;
  }

  void SetWaitingForImage() {
    if (image_type_ == ImageType::kNone) {
      image_type_ = ImageType::kNoneButWaiting;
      InvalidateLayout();
    }
  }

 private:
  enum class ImageType { kNone, kNoneButWaiting, kPlaceholder, kThumbnail };

  // Creates an image view with the appropriate default properties.
  static std::unique_ptr<views::ImageView> CreateImageView() {
    auto image_view = std::make_unique<views::ImageView>();
    image_view->SetHorizontalAlignment(views::ImageView::Alignment::kCenter);
    return image_view;
  }

  // Sets `image` on `image_view_`, configuring the image appropriately based
  // on whether it's a placeholder or not.
  void SetImage(views::ImageView* image_view,
                gfx::ImageSkia image,
                ImageType image_type) {
    image_view->SetImage(image);
    switch (image_type) {
      case ImageType::kNone:
      case ImageType::kNoneButWaiting:
        image_view->SetBackground(
            views::CreateSolidBackground(bubble_view_->color()));
        break;
      case ImageType::kPlaceholder:
        image_view->SetVerticalAlignment(views::ImageView::Alignment::kCenter);
        image_view->SetImageSize(image.size());
        image_view->SetBackground(views::CreateSolidBackground(
            image_view->GetThemeProvider()->GetColor(
                ThemeProperties::COLOR_HOVER_CARD_NO_PREVIEW_BACKGROUND)));
        break;
      case ImageType::kThumbnail:
        image_view->SetVerticalAlignment(views::ImageView::Alignment::kLeading);
        image_view->SetImageSize(
            GetPreviewImageSize(image.size(), TabStyle::GetPreviewImageSize()));
        image_view->SetBackground(nullptr);
        break;
    }
  }

  // views::View:
  gfx::Size GetMinimumSize() const override { return gfx::Size(); }

  gfx::Size CalculatePreferredSize() const override {
    return image_type_ == ImageType::kNone ? gfx::Size()
                                           : TabStyle::GetPreviewImageSize();
  }

  gfx::Size GetMaximumSize() const override {
    return TabStyle::GetPreviewImageSize();
  }

  // views::AnimationDelegateViews:
  void AnimationProgressed(const gfx::Animation* animation) override {
    image_fading_out_->layer()->SetOpacity(1.0 - animation->GetCurrentValue());
  }

  void AnimationEnded(const gfx::Animation* animation) override {
    image_fading_out_->layer()->SetOpacity(0.0f);
    SetImage(image_fading_out_, gfx::ImageSkia(), ImageType::kNone);
  }

  void AnimationCanceled(const gfx::Animation* animation) override {
    AnimationEnded(animation);
  }

  // Begins fading out the existing image, which it reads from
  // `target_tab_image_`. Does a smart three-way crossfade if an image is
  // already fading out.
  void StartFadeOut() {
    if (!GetVisible())
      return;

    if (!GetPreviewImageCrossfadeStart().has_value())
      return;

    gfx::ImageSkia old_image = target_tab_image_->GetImage();

    if (image_transition_animation_.is_animating()) {
      // If we're already animating and we've barely faded out the previous old
      // image, keep fading out the old one and just swap the new one
      // underneath.
      const double current_value =
          image_transition_animation_.GetCurrentValue();
      if (current_value <= 0.5)
        return;

      // Currently we have:
      //  - old preview at `current_value` opacity
      //  - previous old preview at 1.0 - `current_value` opacity
      // We will discard the previous old preview and move the old preview into
      // the occluding view. However, since the opacity of this view is
      // inversely proportional to animation progress, to keep the same opacity
      // (while we swap the new image in behind) we have to rewind the
      // animation.
      image_transition_animation_.SetCurrentValue(1.0 - current_value);
      SetImage(image_fading_out_, old_image, image_type_);
      AnimationProgressed(&image_transition_animation_);

    } else {
      SetImage(image_fading_out_, old_image, image_type_);
      image_fading_out_->layer()->SetOpacity(1.0f);
      image_transition_animation_.Start();
    }
  }

  TabHoverCardBubbleView* const bubble_view_;

  // Displays the image that we are trying to display for the target/current
  // tab. Placed under `image_fading_out_` so that it is revealed as the
  // previous image fades out.
  views::ImageView* target_tab_image_ = nullptr;

  // Displays the previous image as it's fading out. Rendered over
  // `target_tab_image_` and has its alpha animated from 1 to 0.
  views::ImageView* image_fading_out_ = nullptr;

  // Provides a smooth fade out for `image_fading_out_`. We do not use a
  // LayerAnimation because we need to rewind the transparency at various
  // times (it's not necessarily a single smooth animation).
  gfx::LinearAnimation image_transition_animation_;

  // Records what type of image `target_tab_image_` is showing. Used to
  // configure `image_fading_out_` when the target image becomes the previous
  // image and fades out.
  ImageType image_type_ = ImageType::kNone;
};

// static
constexpr base::TimeDelta TabHoverCardBubbleView::kHoverCardSlideDuration;

TabHoverCardBubbleView::TabHoverCardBubbleView(Tab* tab)
    : BubbleDialogDelegateView(tab,
                               views::BubbleBorder::TOP_LEFT,
                               views::BubbleBorder::STANDARD_SHADOW) {
  if (CustomShadowsSupported()) {
    corner_radius_ = ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
        views::Emphasis::kHigh);
  }
  SetButtons(ui::DIALOG_BUTTON_NONE);

  // We'll do all of our own layout inside the bubble, so no need to inset this
  // view inside the client view.
  set_margins(gfx::Insets());

  // Set so that when hovering over a tab in a inactive window that window will
  // not become active. Setting this to false creates the need to explicitly
  // hide the hovercard on press, touch, and keyboard events.
  SetCanActivate(false);
#if defined(OS_MAC)
  set_accept_events(false);
#endif

  // Set so that the tab hover card is not focus traversable when keyboard
  // navigating through the tab strip.
  set_focus_traversable_from_anchor_view(false);

  title_label_ = AddChildView(std::make_unique<FadeLabel>(
      CONTEXT_TAB_HOVER_CARD_TITLE, kHoverCardTitleMaxLines));
  domain_label_ = AddChildView(
      std::make_unique<FadeLabel>(views::style::CONTEXT_DIALOG_BODY_TEXT, 1));

  if (TabHoverCardController::AreHoverCardImagesEnabled()) {
    if (UseAlternateHoverCardFormat()) {
      thumbnail_view_ =
          AddChildViewAt(std::make_unique<ThumbnailView>(this), 0);
      thumbnail_view_->SetRoundedCorners(
          ThumbnailView::RoundedCorners::kTopCorners,
          corner_radius_.value_or(0));
    } else {
      thumbnail_view_ = AddChildView(std::make_unique<ThumbnailView>(this));
      thumbnail_view_->SetRoundedCorners(
          ThumbnailView::RoundedCorners::kBottomCorners,
          corner_radius_.value_or(0));
    }
  }

  // Set up layout.

  views::FlexLayout* const layout =
      SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kVertical);
  layout->SetMainAxisAlignment(views::LayoutAlignment::kStart);
  layout->SetCrossAxisAlignment(views::LayoutAlignment::kStretch);
  layout->SetCollapseMargins(true);

  // In some browser types (e.g. ChromeOS terminal app) we hide the domain
  // label. In those cases, we need to adjust the bottom margin of the title
  // element because it is no longer above another text element and needs a
  // bottom margin.
  const bool show_domain = tab->controller()->ShowDomainInHoverCards();
  gfx::Insets title_margins = kTitleMargins;
  domain_label_->SetVisible(show_domain);
  if (show_domain) {
    title_margins.set_bottom(0);
    domain_label_->SetProperty(
        views::kMarginsKey,
        gfx::Insets(0, kHorizontalMargin, kVerticalMargin, kHorizontalMargin));
  }

  title_label_->SetProperty(views::kMarginsKey, title_margins);
  title_label_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kScaleToMaximum)
          .WithOrder(2));
  if (thumbnail_view_) {
    thumbnail_view_->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                                 views::MaximumFlexSizeRule::kScaleToMaximum)
            .WithOrder(1));
  }

  // Set up widget.

  views::BubbleDialogDelegateView::CreateBubble(this);
  set_adjust_if_offscreen(true);

  GetBubbleFrameView()->SetFootnoteMargins(kAlertMargins);
  GetBubbleFrameView()->SetPreferredArrowAdjustment(
      views::BubbleFrameView::PreferredArrowAdjustment::kOffset);
  GetBubbleFrameView()->set_hit_test_transparent(true);

  if (using_rounded_corners())
    GetBubbleFrameView()->SetCornerRadius(corner_radius_.value());

  // Placeholder image should be used when there is no image data for the
  // given tab. Otherwise don't flash the placeholder while we wait for the
  // existing thumbnail to be decompressed.
  //
  // Note that this code has to go after CreateBubble() above, since setting up
  // the placeholder image and background color require a ThemeProvider, which
  // is only available once this View has been added to its widget.
  if (thumbnail_view_ && !tab->data().thumbnail->has_data() &&
      !tab->IsActive()) {
    thumbnail_view_->SetPlaceholderImage();
  }

  // Start in the fully "faded-in" position so that whatever text we initially
  // display is visible.
  SetTextFade(1.0);
}

TabHoverCardBubbleView::~TabHoverCardBubbleView() = default;

ax::mojom::Role TabHoverCardBubbleView::GetAccessibleWindowRole() {
  // Override the role so that hover cards are not read when they appear because
  // tabs handle accessibility text.
  return ax::mojom::Role::kNone;
}

void TabHoverCardBubbleView::UpdateCardContent(const Tab* tab) {
  // Preview image is never visible for the active tab.
  if (thumbnail_view_) {
    if (tab->IsActive())
      thumbnail_view_->ClearImage();
    else
      thumbnail_view_->SetWaitingForImage();
  }

  std::u16string title;
  absl::optional<TabAlertState> old_alert_state = alert_state_;
  GURL domain_url;
  // Use committed URL to determine if no page has yet loaded, since the title
  // can be blank for some web pages.
  if (tab->data().last_committed_url.is_empty()) {
    domain_url = tab->data().visible_url;
    title = tab->data().IsCrashed()
                ? l10n_util::GetStringUTF16(IDS_HOVER_CARD_CRASHED_TITLE)
                : l10n_util::GetStringUTF16(IDS_TAB_LOADING_TITLE);
    alert_state_ = absl::nullopt;
  } else {
    domain_url = tab->data().last_committed_url;
    title = tab->data().title;
    alert_state_ = Tab::GetAlertStateToShow(tab->data().alert_state);
  }
  std::u16string domain;
  bool is_filename = false;
  if (domain_url.SchemeIsFile()) {
    is_filename = true;
    domain = l10n_util::GetStringUTF16(IDS_HOVER_CARD_FILE_URL_SOURCE);
  } else {
    if (domain_url.SchemeIsBlob()) {
      domain = l10n_util::GetStringUTF16(IDS_HOVER_CARD_BLOB_URL_SOURCE);
    } else {
      domain = url_formatter::FormatUrl(
          domain_url,
          url_formatter::kFormatUrlOmitDefaults |
              url_formatter::kFormatUrlOmitHTTPS |
              url_formatter::kFormatUrlOmitTrivialSubdomains |
              url_formatter::kFormatUrlTrimAfterHost,
          net::UnescapeRule::NORMAL, nullptr, nullptr, nullptr);
    }
  }
  title_label_->SetText(title, is_filename);
  domain_label_->SetText(domain, absl::nullopt);

  const bool alternate_layout = UseAlternateHoverCardFormat();
  if (alert_state_ != old_alert_state) {
    std::unique_ptr<views::Label> alert_label =
        alert_state_.has_value() ? CreateAlertView(*alert_state_) : nullptr;
    if (alternate_layout) {
      if (alert_label) {
        // Simulate the same look as the footnote view.
        // TODO(dfried): should we add this as a variation of
        // FootnoteContainerView? Currently it's only used here.
        alert_label->SetBackground(
            views::CreateSolidBackground(GetNativeTheme()->GetSystemColor(
                ui::NativeTheme::kColorId_BubbleFooterBackground)));
        alert_label->SetBorder(views::CreatePaddedBorder(
            views::CreateSolidSidedBorder(
                0, 0, 1, 0,
                GetNativeTheme()->GetSystemColor(
                    ui::NativeTheme::kColorId_FootnoteContainerBorder)),
            kAlertMargins));
      }
      GetBubbleFrameView()->SetHeaderView(std::move(alert_label));
    } else {
      GetBubbleFrameView()->SetFootnoteView(std::move(alert_label));
    }

    if (thumbnail_view_) {
      // We only clip the corners of the fade image when there isn't a header
      // or footer.
      ThumbnailView::RoundedCorners corners =
          ThumbnailView::RoundedCorners::kNone;
      if (!alert_state_.has_value()) {
        corners = alternate_layout
                      ? ThumbnailView::RoundedCorners::kTopCorners
                      : ThumbnailView::RoundedCorners::kBottomCorners;
      }
      thumbnail_view_->SetRoundedCorners(corners, corner_radius_.value_or(0));
    }
  }
}

void TabHoverCardBubbleView::SetTextFade(double percent) {
  title_label_->SetFade(percent);
  domain_label_->SetFade(percent);
}

void TabHoverCardBubbleView::SetTargetTabImage(gfx::ImageSkia preview_image) {
  DCHECK(thumbnail_view_)
      << "This method should only be called when preview images are enabled.";
  thumbnail_view_->SetTargetTabImage(preview_image);
}

void TabHoverCardBubbleView::SetPlaceholderImage() {
  DCHECK(thumbnail_view_)
      << "This method should only be called when preview images are enabled.";
  thumbnail_view_->SetPlaceholderImage();
}

std::u16string TabHoverCardBubbleView::GetTitleTextForTesting() const {
  return title_label_->GetText();
}

std::u16string TabHoverCardBubbleView::GetDomainTextForTesting() const {
  return domain_label_->GetText();
}

// static
absl::optional<double> TabHoverCardBubbleView::GetPreviewImageCrossfadeStart() {
  static const double start_percent = base::GetFieldTrialParamByFeatureAsDouble(
      features::kTabHoverCardImages,
      features::kTabHoverCardImagesCrossfadePreviewAtParameterName, -1.0);
  return start_percent >= 0.0
             ? absl::make_optional(base::clamp(start_percent, 0.0, 1.0))
             : absl::nullopt;
}

gfx::Size TabHoverCardBubbleView::CalculatePreferredSize() const {
  gfx::Size preferred_size = GetLayoutManager()->GetPreferredSize(this);
  preferred_size.set_width(TabStyle::GetPreviewImageSize().width());
  DCHECK(!preferred_size.IsEmpty());
  return preferred_size;
}

void TabHoverCardBubbleView::OnThemeChanged() {
  BubbleDialogDelegateView::OnThemeChanged();

  // Bubble closes if the theme changes to the point where the border has to be
  // regenerated. See crbug.com/1140256
  if (using_rounded_corners() != CustomShadowsSupported()) {
    GetWidget()->Close();
    return;
  }
}

BEGIN_METADATA(TabHoverCardBubbleView, views::BubbleDialogDelegateView)
END_METADATA
