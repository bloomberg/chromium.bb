// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/sharesheet/sharesheet_bubble_view.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/public/cpp/ash_typography.h"
#include "ash/public/cpp/tablet_mode.h"
#include "base/cxx17_backports.h"
#include "base/i18n/rtl.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharesheet/sharesheet_metrics.h"
#include "chrome/browser/sharesheet/sharesheet_service_delegate.h"
#include "chrome/browser/ui/ash/sharesheet/sharesheet_constants.h"
#include "chrome/browser/ui/ash/sharesheet/sharesheet_expand_button.h"
#include "chrome/browser/ui/ash/sharesheet/sharesheet_header_view.h"
#include "chrome/browser/ui/ash/sharesheet/sharesheet_target_button.h"
#include "chrome/browser/ui/ash/sharesheet/sharesheet_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/closure_animation_observer.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/transform_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace {

// TODO(crbug.com/1097623) Many of below values are sums of each other and
// can be removed.

// Sizes are in px.
constexpr int kButtonPadding = 8;
constexpr int kButtonWidth = 92;
constexpr int kCornerRadius = 12;
constexpr int kBubbleTopPaddingFromWindow = 28;
constexpr int kDefaultBubbleWidth = 416;

// kDefaultBubbleBodyHeight = kTargetViewHeight + kShortSpacing
constexpr int kDefaultBubbleBodyHeight = 236;

// kExpandedBubbleBodyHeight = kTargetViewHeight + kShortSpacing +
// kExpandViewPaddingTop + kSubtitleTextLineHeight + kExpandViewPaddingBottom
// SharesheetTargetButton.kButtonHeight
constexpr int kExpandedBubbleBodyHeight = 378;

constexpr int kMaxTargetsPerRow = 4;
constexpr int kMaxRowsForDefaultView = 2;

// TargetViewHeight is 2*kButtonHeight + kButtonPadding
constexpr int kTargetViewHeight = 216;
constexpr int kTargetViewExpandedHeight = 382;

constexpr int kExpandViewPaddingTop = 16;
constexpr int kExpandViewPaddingBottom = 8;

constexpr int kShortSpacing = 20;

constexpr auto kAnimateDelay = base::TimeDelta::FromMilliseconds(100);
constexpr auto kQuickAnimateTime = base::TimeDelta::FromMilliseconds(100);
constexpr auto kSlowAnimateTime = base::TimeDelta::FromMilliseconds(200);

// Resize Percentage.
constexpr int kStretchy = 1.0;

enum { kColumnSetIdTitle, kColumnSetIdTargets, kColumnSetIdZeroState };

void SetUpTargetColumnSet(views::GridLayout* layout) {
  views::ColumnSet* cs = layout->AddColumnSet(kColumnSetIdTargets);
  for (int i = 0; i < kMaxTargetsPerRow; i++) {
    cs->AddColumn(views::GridLayout::CENTER, views::GridLayout::CENTER, 0,
                  views::GridLayout::ColumnSize::kFixed, kButtonWidth, 0);
  }
}

bool IsKeyboardCodeArrow(ui::KeyboardCode key_code) {
  return key_code == ui::VKEY_UP || key_code == ui::VKEY_DOWN ||
         key_code == ui::VKEY_RIGHT || key_code == ui::VKEY_LEFT;
}

}  // namespace

namespace ash {
namespace sharesheet {

class SharesheetBubbleView::SharesheetParentWidgetObserver
    : public views::WidgetObserver {
 public:
  SharesheetParentWidgetObserver(SharesheetBubbleView* owner,
                                 views::Widget* widget)
      : owner_(owner) {
    observer_.Observe(widget);
  }
  ~SharesheetParentWidgetObserver() override = default;

  // WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override {
    DCHECK(observer_.IsObservingSource(widget));
    observer_.Reset();
    // |this| may be destroyed here!

    // TODO(crbug.com/1188938) Code clean up.
    // There should be something here telling SharesheetBubbleView
    // that its parent widget is closing and therefore it should
    // also close. Or we should try to inherit the widget changes from
    // BubbleDialogDelegate and not have this class here at all.
  }

  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& bounds) override {
    owner_->UpdateAnchorPosition();
  }

 private:
  SharesheetBubbleView* owner_;
  base::ScopedObservation<views::Widget, views::WidgetObserver> observer_{this};
};

SharesheetBubbleView::SharesheetBubbleView(
    gfx::NativeWindow native_window,
    ::sharesheet::SharesheetServiceDelegate* delegate)
    : delegate_(delegate) {
  set_parent_window(native_window);
  parent_widget_observer_ = std::make_unique<SharesheetParentWidgetObserver>(
      this, views::Widget::GetWidgetForNativeWindow(native_window));
  parent_view_ =
      views::Widget::GetWidgetForNativeWindow(native_window)->GetRootView();
  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
  CreateBubble();
}

SharesheetBubbleView::~SharesheetBubbleView() = default;

void SharesheetBubbleView::ShowBubble(
    std::vector<TargetInfo> targets,
    apps::mojom::IntentPtr intent,
    ::sharesheet::DeliveredCallback delivered_callback,
    ::sharesheet::CloseCallback close_callback) {
  intent_ = std::move(intent);
  delivered_callback_ = std::move(delivered_callback);
  close_callback_ = std::move(close_callback);

  main_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  // When there are no targets, don't show any previews. Otherwise, show
  // previews if the flag is enabled.
  bool show_content_previews =
      !targets.empty() &&
      base::FeatureList::IsEnabled(features::kSharesheetContentPreviews);
  header_view_ =
      main_view_->AddChildView(std::make_unique<SharesheetHeaderView>(
          intent_->Clone(), delegate_->GetProfile(), show_content_previews));
  auto* body_view = main_view_->AddChildView(std::make_unique<views::View>());
  body_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  footer_view_ = main_view_->AddChildView(std::make_unique<views::View>());
  auto* footer_layout =
      footer_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets(kFooterDefaultVerticalPadding, 0)));
  footer_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  footer_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  if (targets.empty()) {
    auto* image = body_view->AddChildView(std::make_unique<views::ImageView>());
    image->SetImage(*ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
        IDR_SHARESHEET_EMPTY));
    image->SetProperty(views::kMarginsKey, gfx::Insets(0, 0, kSpacing, 0));
    body_view->AddChildView(CreateShareLabel(
        l10n_util::GetStringUTF16(IDS_SHARESHEET_ZERO_STATE_PRIMARY_LABEL),
        CONTEXT_SHARESHEET_BUBBLE_BODY, kPrimaryTextLineHeight,
        kPrimaryTextColor, gfx::ALIGN_CENTER));
    body_view->AddChildView(CreateShareLabel(
        l10n_util::GetStringUTF16(IDS_SHARESHEET_ZERO_STATE_SECONDARY_LABEL),
        CONTEXT_SHARESHEET_BUBBLE_BODY_SECONDARY, kPrimaryTextLineHeight,
        kSecondaryTextColor, gfx::ALIGN_CENTER, views::style::STYLE_PRIMARY));
  } else {
    if (show_content_previews) {
      header_body_separator_ =
          body_view->AddChildView(std::make_unique<views::Separator>());
    }

    const size_t targets_size = targets.size();
    auto scroll_view = std::make_unique<views::ScrollView>();
    scroll_view->SetContents(MakeScrollableTargetView(std::move(targets)));
    scroll_view->ClipHeightTo(kTargetViewHeight, kTargetViewExpandedHeight);
    body_view->AddChildView(std::move(scroll_view));

    if (expanded_view_) {
      body_footer_separator_ =
          body_view->AddChildView(std::make_unique<views::Separator>());
      expand_button_ =
          footer_view_->AddChildView(std::make_unique<SharesheetExpandButton>(
              base::BindRepeating(&SharesheetBubbleView::ExpandButtonPressed,
                                  base::Unretained(this))));
    } else if (targets_size <= kMaxTargetsPerRow * kMaxRowsForDefaultView) {
      // When we have between 1 and 8 targets inclusive. Update |footer_layout|
      // padding.
      footer_layout->set_inside_border_insets(
          gfx::Insets(kFooterNoExtensionVerticalPadding, 0));
    }
  }

  main_view_->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  main_view_->RequestFocus();
  main_view_->GetViewAccessibility().OverrideName(
      l10n_util::GetStringUTF16(IDS_SHARESHEET_TITLE_LABEL));
  views::BubbleDialogDelegateView::CreateBubble(this);
  GetWidget()->GetRootView()->Layout();
  RecordFormFactorMetric();
  ShowWidgetWithAnimateFadeIn();

  UpdateAnchorPosition();
}

void SharesheetBubbleView::ShowNearbyShareBubbleForArc(
    apps::mojom::IntentPtr intent,
    ::sharesheet::DeliveredCallback delivered_callback,
    ::sharesheet::CloseCallback close_callback) {
  user_selection_made_ = true;  // Disable close when clicking outside bubble.
  ShowBubble({}, std::move(intent), std::move(delivered_callback),
             std::move(close_callback));
  if (delivered_callback_) {
    std::move(delivered_callback_)
        .Run(::sharesheet::SharesheetResult::kSuccess);
  }
  delegate_->OnTargetSelected(
      l10n_util::GetStringUTF16(IDS_NEARBY_SHARE_FEATURE_NAME),
      ::sharesheet::TargetType::kAction, std::move(intent_),
      share_action_view_);
}

std::unique_ptr<views::View> SharesheetBubbleView::MakeScrollableTargetView(
    std::vector<TargetInfo> targets) {
  // Set up default and expanded views.
  auto default_view = std::make_unique<views::View>();
  default_view->SetProperty(views::kMarginsKey, gfx::Insets(0, kSpacing));
  auto* default_layout =
      default_view->SetLayoutManager(std::make_unique<views::GridLayout>());
  SetUpTargetColumnSet(default_layout);

  views::GridLayout* expanded_layout = nullptr;
  std::unique_ptr<views::View> expanded_view;
  if (targets.size() > kMaxTargetsPerRow * kMaxRowsForDefaultView) {
    expanded_view = std::make_unique<views::View>();
    expanded_view->SetProperty(views::kMarginsKey, gfx::Insets(0, kSpacing));
    expanded_layout =
        expanded_view->SetLayoutManager(std::make_unique<views::GridLayout>());
    SetUpTargetColumnSet(expanded_layout);
    views::ColumnSet* cs_expanded_view =
        expanded_layout->AddColumnSet(kColumnSetIdTitle);
    cs_expanded_view->AddColumn(/* h_align */ views::GridLayout::FILL,
                                /* v_align */ views::GridLayout::CENTER,
                                /* resize_percent */ kStretchy,
                                views::GridLayout::ColumnSize::kUsePreferred,
                                /* fixed_width */ 0, /* min_width */ 0);
    // Add Extended View Title.
    expanded_layout->AddPaddingRow(views::GridLayout::kFixedSize,
                                   kExpandViewPaddingTop);
    expanded_layout->StartRow(views::GridLayout::kFixedSize, kColumnSetIdTitle,
                              kSubtitleTextLineHeight);
    expanded_layout->AddView(CreateShareLabel(
        l10n_util::GetStringUTF16(IDS_SHARESHEET_APPS_LIST_LABEL),
        CONTEXT_SHARESHEET_BUBBLE_BODY, kSubtitleTextLineHeight,
        kPrimaryTextColor, gfx::ALIGN_CENTER));
    expanded_layout->AddPaddingRow(views::GridLayout::kFixedSize,
                                   kExpandViewPaddingBottom);
  }

  PopulateLayoutsWithTargets(std::move(targets), default_layout,
                             expanded_layout);
  default_layout->AddPaddingRow(views::GridLayout::kFixedSize, kShortSpacing);

  auto scrollable_view = std::make_unique<views::View>();
  auto* layout =
      scrollable_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);
  default_view_ = scrollable_view->AddChildView(std::move(default_view));
  if (expanded_layout) {
    expanded_view_separator_ =
        scrollable_view->AddChildView(std::make_unique<views::Separator>());
    expanded_view_separator_->SetProperty(views::kMarginsKey,
                                          gfx::Insets(0, kSpacing));
    expanded_view_ = scrollable_view->AddChildView(std::move(expanded_view));
    // |expanded_view_| is not visible by default.
    expanded_view_->SetVisible(false);
    expanded_view_separator_->SetVisible(false);
  }

  return scrollable_view;
}

void SharesheetBubbleView::PopulateLayoutsWithTargets(
    std::vector<TargetInfo> targets,
    views::GridLayout* default_layout,
    views::GridLayout* expanded_layout) {
  // Add first kMaxRowsForDefaultView*kMaxTargetsPerRow targets to
  // |default_view| and subsequent targets to |expanded_view|.
  size_t row_count = 0;
  size_t target_counter = 0;
  auto* layout_for_target = default_layout;
  for (auto& target : targets) {
    if (target_counter % kMaxTargetsPerRow == 0) {
      // When we've reached kMaxRowsForDefaultView switch to populating
      // |expanded_layout|.
      if (row_count == kMaxRowsForDefaultView) {
        DCHECK(expanded_layout);
        layout_for_target = expanded_layout;
        // Do not add a padding row if we are at the first row of
        // |default_layout| or |expanded_layout|.
      } else if (row_count != 0) {
        layout_for_target->AddPaddingRow(views::GridLayout::kFixedSize,
                                         kButtonPadding);
      }
      ++row_count;
      layout_for_target->StartRow(views::GridLayout::kFixedSize,
                                  kColumnSetIdTargets);
    }
    ++target_counter;

    // Make a copy because value is needed after target is std::moved below.
    std::u16string display_name = target.display_name;
    std::u16string secondary_display_name =
        target.secondary_display_name.value_or(std::u16string());
    absl::optional<gfx::ImageSkia> icon = target.icon;

    auto target_view = std::make_unique<SharesheetTargetButton>(
        base::BindRepeating(&SharesheetBubbleView::TargetButtonPressed,
                            base::Unretained(this),
                            base::Passed(std::move(target))),
        display_name, secondary_display_name, icon,
        delegate_->GetVectorIcon(display_name));

    layout_for_target->AddView(std::move(target_view));
  }
}

void SharesheetBubbleView::ShowActionView() {
  constexpr float kShareActionScaleUpFactor = 0.9f;

  main_view_->SetPaintToLayer();
  ui::Layer* main_view_layer = main_view_->layer();
  main_view_layer->SetFillsBoundsOpaquely(false);
  main_view_layer->SetRoundedCornerRadius(gfx::RoundedCornersF(kCornerRadius));
  // |main_view_| opacity fade out.
  auto scoped_settings = std::make_unique<ui::ScopedLayerAnimationSettings>(
      main_view_layer->GetAnimator());
  scoped_settings->SetTransitionDuration(kQuickAnimateTime);
  scoped_settings->SetTweenType(gfx::Tween::Type::LINEAR);
  main_view_layer->SetOpacity(0.0f);
  main_view_->SetVisible(false);

  share_action_view_->SetPaintToLayer();
  ui::Layer* share_action_view_layer = share_action_view_->layer();
  share_action_view_layer->SetFillsBoundsOpaquely(false);
  share_action_view_layer->SetRoundedCornerRadius(
      gfx::RoundedCornersF(kCornerRadius));

  share_action_view_->SetVisible(true);
  share_action_view_layer->SetOpacity(0.0f);
  gfx::Transform transform = gfx::GetScaleTransform(
      gfx::Rect(share_action_view_layer->size()).CenterPoint(),
      kShareActionScaleUpFactor);
  share_action_view_layer->SetTransform(transform);
  auto share_action_scoped_settings =
      std::make_unique<ui::ScopedLayerAnimationSettings>(
          share_action_view_layer->GetAnimator());
  share_action_scoped_settings->SetPreemptionStrategy(
      ui::LayerAnimator::ENQUEUE_NEW_ANIMATION);

  // |share_action_view_| scale fade in.
  share_action_scoped_settings->SetTransitionDuration(kSlowAnimateTime);
  share_action_scoped_settings->SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN_2);
  // Set##name kicks off the animation with the TransitionDuration and
  // TweenType currently set. See ui/compositor/layer_animator.cc Set##name.
  share_action_view_layer->SetTransform(gfx::Transform());
  // |share_action_view_| opacity fade in.
  share_action_scoped_settings->SetTransitionDuration(kQuickAnimateTime);
  share_action_scoped_settings->SetTweenType(gfx::Tween::Type::LINEAR);
  share_action_view_layer->SetOpacity(1.0f);

  // Delay |share_action_view_| animate so that we can see |main_view_| fade out
  // first.
  share_action_view_layer->GetAnimator()->SchedulePauseForProperties(
      kAnimateDelay, ui::LayerAnimationElement::TRANSFORM |
                         ui::LayerAnimationElement::OPACITY);
}

void SharesheetBubbleView::ResizeBubble(const int& width, const int& height) {
  auto old_bounds = gfx::RectF(width_, height_);
  width_ = width;
  height_ = height;

  // Animate from the old bubble to the new bubble.
  ui::Layer* layer = View::GetWidget()->GetLayer();
  const gfx::Transform transform =
      gfx::TransformBetweenRects(old_bounds, gfx::RectF(width, height));
  layer->SetTransform(transform);
  auto scoped_settings =
      std::make_unique<ui::ScopedLayerAnimationSettings>(layer->GetAnimator());
  scoped_settings->SetTransitionDuration(kSlowAnimateTime);
  scoped_settings->SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN_2);
  layer->GetAnimator()->SchedulePauseForProperties(
      kAnimateDelay, ui::LayerAnimationElement::TRANSFORM);

  UpdateAnchorPosition();

  layer->SetTransform(gfx::Transform());
}

// CloseBubble is called from a ShareAction or after an app launches.
void SharesheetBubbleView::CloseBubble(views::Widget::ClosedReason reason) {
  if (!is_bubble_closing_) {
    CloseWidgetWithAnimateFadeOut(reason);
  }
}

bool SharesheetBubbleView::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  // We override this because when this is handled by the base class,
  // OnKeyPressed is not invoked when a user presses |VKEY_ESCAPE| if they have
  // not pressed |VKEY_TAB| first to focus the SharesheetBubbleView.
  DCHECK_EQ(accelerator.key_code(), ui::VKEY_ESCAPE);
  if (share_action_view_->GetVisible() &&
      delegate_->OnAcceleratorPressed(accelerator, active_target_)) {
    return true;
  }
  // If delivered_callback_ is not null at this point, then the sharesheet was
  // closed before a target was selected.
  if (delivered_callback_) {
    std::move(delivered_callback_).Run(::sharesheet::SharesheetResult::kCancel);
  }
  escape_pressed_ = true;
  ::sharesheet::SharesheetMetrics::RecordSharesheetActionMetrics(
      ::sharesheet::SharesheetMetrics::UserAction::kCancelledThroughEscPress);
  CloseWidgetWithAnimateFadeOut(views::Widget::ClosedReason::kEscKeyPressed);

  return true;
}

bool SharesheetBubbleView::OnKeyPressed(const ui::KeyEvent& event) {
  // Ignore key press if it's not an arrow or bubble is closing.
  if (!IsKeyboardCodeArrow(event.key_code()) || default_view_ == nullptr ||
      is_bubble_closing_) {
    if (event.key_code() == ui::VKEY_ESCAPE && !is_bubble_closing_) {
      escape_pressed_ = true;
    }
    return false;
  }

  int delta = 0;
  switch (event.key_code()) {
    case ui::VKEY_UP:
      delta = -kMaxTargetsPerRow;
      break;
    case ui::VKEY_DOWN:
      delta = kMaxTargetsPerRow;
      break;
    case ui::VKEY_LEFT:
      delta = base::i18n::IsRTL() ? 1 : -1;
      break;
    case ui::VKEY_RIGHT:
      delta = base::i18n::IsRTL() ? -1 : 1;
      break;
    default:
      NOTREACHED();
      break;
  }

  const size_t default_views = default_view_->children().size();
  // The -1 here and +1 below account for the app list label.
  const size_t targets =
      default_views +
      (show_expanded_view_ ? (expanded_view_->children().size() - 1) : 0);
  const int new_target = static_cast<int>(keyboard_highlighted_target_) + delta;
  keyboard_highlighted_target_ = static_cast<size_t>(
      base::clamp(new_target, 0, static_cast<int>(targets) - 1));

  if (keyboard_highlighted_target_ < default_views) {
    default_view_->children()[keyboard_highlighted_target_]->RequestFocus();
  } else {
    expanded_view_->children()[keyboard_highlighted_target_ + 1 - default_views]
        ->RequestFocus();
  }
  return true;
}

ax::mojom::Role SharesheetBubbleView::GetAccessibleWindowRole() {
  // We override the role because the base class sets it to alert dialog.
  // This would make screen readers repeatedly announce the whole of the
  // |sharesheet_bubble_view| which is undesirable.
  return ax::mojom::Role::kDialog;
}

std::unique_ptr<views::NonClientFrameView>
SharesheetBubbleView::CreateNonClientFrameView(views::Widget* widget) {
  // TODO(crbug.com/1097623) Replace this with layer->SetRoundedCornerRadius.
  auto bubble_border =
      std::make_unique<views::BubbleBorder>(arrow(), GetShadow(), color());
  bubble_border->SetCornerRadius(kCornerRadius);
  auto frame =
      views::BubbleDialogDelegateView::CreateNonClientFrameView(widget);
  static_cast<views::BubbleFrameView*>(frame.get())
      ->SetBubbleBorder(std::move(bubble_border));
  return frame;
}

gfx::Size SharesheetBubbleView::CalculatePreferredSize() const {
  return gfx::Size(width_, height_);
}

void SharesheetBubbleView::OnWidgetActivationChanged(views::Widget* widget,
                                                     bool active) {
  // Catch widgets that are closing due to the user clicking out of the bubble.
  // If |user_selection_made_| we should not close the bubble here as it will be
  // closed in a different code path.
  if (!active && !user_selection_made_ && !is_bubble_closing_) {
    if (delivered_callback_) {
      std::move(delivered_callback_)
          .Run(::sharesheet::SharesheetResult::kCancel);
    }
    auto user_action = ::sharesheet::SharesheetMetrics::UserAction::
        kCancelledThroughClickingOut;
    auto closed_reason = views::Widget::ClosedReason::kLostFocus;
    if (escape_pressed_) {
      user_action = ::sharesheet::SharesheetMetrics::UserAction::
          kCancelledThroughEscPress;
      closed_reason = views::Widget::ClosedReason::kEscKeyPressed;
    }
    ::sharesheet::SharesheetMetrics::RecordSharesheetActionMetrics(user_action);
    CloseWidgetWithAnimateFadeOut(closed_reason);
  }
}

void SharesheetBubbleView::CreateBubble() {
  set_close_on_deactivate(false);
  SetButtons(ui::DIALOG_BUTTON_NONE);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  // Margins must be set to 0 or share_action_view will have undesired margins.
  set_margins(gfx::Insets());

  auto main_view = std::make_unique<views::View>();
  main_view_ = AddChildView(std::move(main_view));

  auto share_action_view = std::make_unique<views::View>();
  share_action_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  share_action_view_ = AddChildView(std::move(share_action_view));
  share_action_view_->SetVisible(false);
}

void SharesheetBubbleView::ExpandButtonPressed() {
  show_expanded_view_ = !show_expanded_view_;
  ResizeBubble(kDefaultBubbleWidth, GetBubbleHeight());

  // Scrollview has separators that overlaps with |header_body_separator_| and
  // |body_footer_separator_| to create a double line when both are visible, so
  // when scrollview is expanded we hide our separators.
  if (header_body_separator_)
    header_body_separator_->SetVisible(!show_expanded_view_);
  body_footer_separator_->SetVisible(!show_expanded_view_);

  expanded_view_->SetVisible(show_expanded_view_);
  expanded_view_separator_->SetVisible(show_expanded_view_);

  if (show_expanded_view_) {
    expand_button_->SetToExpandedState();
    AnimateToExpandedState();
  } else {
    expand_button_->SetToDefaultState();
  }
}

void SharesheetBubbleView::AnimateToExpandedState() {
  expanded_view_->SetVisible(true);
  expanded_view_->SetPaintToLayer();
  ui::Layer* expanded_view_layer = expanded_view_->layer();
  expanded_view_layer->SetFillsBoundsOpaquely(false);
  expanded_view_layer->SetRoundedCornerRadius(
      gfx::RoundedCornersF(kCornerRadius));
  expanded_view_layer->SetOpacity(0.0f);
  // |expanded_view_| opacity fade in.
  auto scoped_settings = std::make_unique<ui::ScopedLayerAnimationSettings>(
      expanded_view_layer->GetAnimator());
  scoped_settings->SetTransitionDuration(kQuickAnimateTime);
  scoped_settings->SetTweenType(gfx::Tween::Type::LINEAR);

  expanded_view_layer->SetOpacity(1.0f);
}

void SharesheetBubbleView::TargetButtonPressed(TargetInfo target) {
  user_selection_made_ = true;
  auto type = target.type;
  if (type == ::sharesheet::TargetType::kAction) {
    active_target_ = target.launch_name;
  } else {
    intent_->activity_name = target.activity_name;
  }
  delegate_->OnTargetSelected(target.launch_name, type, std::move(intent_),
                              share_action_view_);
  if (delivered_callback_) {
    std::move(delivered_callback_)
        .Run(::sharesheet::SharesheetResult::kSuccess);
  }
  intent_.reset();
}

void SharesheetBubbleView::UpdateAnchorPosition() {
  // If |width_| is not set, set to default value.
  if (width_ == 0) {
    SetToDefaultBubbleSizing();
  }

  // Horizontally centered
  int x_within_parent_view = parent_view_->GetMirroredXInView(
      (parent_view_->bounds().width() - width_) / 2);
  // Get position in screen, taking parent view origin into account. This is
  // 0,0 in fullscreen on the primary display, but not on secondary displays, or
  // in Hosted App windows.
  gfx::Point origin = parent_view_->GetBoundsInScreen().origin();
  origin += gfx::Vector2d(x_within_parent_view, kBubbleTopPaddingFromWindow);

  // SetAnchorRect will CalculatePreferredSize when called.
  SetAnchorRect(gfx::Rect(origin, gfx::Size()));
}

void SharesheetBubbleView::SetToDefaultBubbleSizing() {
  width_ = kDefaultBubbleWidth;
  height_ = GetBubbleHeight();
}

void SharesheetBubbleView::ShowWidgetWithAnimateFadeIn() {
  constexpr float kSharesheetScaleUpFactor = 0.8f;
  constexpr auto kSharesheetScaleUpTime =
      base::TimeDelta::FromMilliseconds(150);

  views::Widget* widget = View::GetWidget();
  ui::Layer* layer = widget->GetLayer();

  layer->SetOpacity(0.0f);
  widget->ShowInactive();
  gfx::Transform transform = gfx::GetScaleTransform(
      gfx::Rect(layer->size()).CenterPoint(), kSharesheetScaleUpFactor);
  layer->SetTransform(transform);
  auto scoped_settings =
      std::make_unique<ui::ScopedLayerAnimationSettings>(layer->GetAnimator());

  scoped_settings->SetTransitionDuration(kSharesheetScaleUpTime);
  scoped_settings->SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
  layer->SetTransform(gfx::Transform());

  scoped_settings->SetTransitionDuration(kQuickAnimateTime);
  scoped_settings->SetTweenType(gfx::Tween::Type::LINEAR);
  layer->SetOpacity(1.0f);
  widget->Activate();
}

void SharesheetBubbleView::CloseWidgetWithAnimateFadeOut(
    views::Widget::ClosedReason closed_reason) {
  constexpr auto kSharesheetOpacityFadeOutTime =
      base::TimeDelta::FromMilliseconds(80);

  is_bubble_closing_ = true;
  if (close_callback_) {
    std::move(close_callback_).Run(closed_reason);
  }
  ui::Layer* layer = View::GetWidget()->GetLayer();

  auto scoped_settings =
      std::make_unique<ui::ScopedLayerAnimationSettings>(layer->GetAnimator());
  scoped_settings->SetTweenType(gfx::Tween::Type::LINEAR);
  scoped_settings->SetTransitionDuration(kSharesheetOpacityFadeOutTime);
  // This aborts any running animations and starts the current one.
  scoped_settings->SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  layer->SetOpacity(0.0f);
  // We are closing the native widget during the close animation which results
  // in destroying the layer and the animation and the observer not calling
  // back. Thus it is safe to use base::Unretained here.
  scoped_settings->AddObserver(new ui::ClosureAnimationObserver(
      base::BindOnce(&SharesheetBubbleView::CloseWidgetWithReason,
                     base::Unretained(this), closed_reason)));
}

void SharesheetBubbleView::CloseWidgetWithReason(
    views::Widget::ClosedReason closed_reason) {
  View::GetWidget()->CloseWithReason(closed_reason);

  // Bubble is deleted here.
  delegate_->OnBubbleClosed(active_target_);
}

// TODO(crbug.com/1097623): Rename this function.
int SharesheetBubbleView::GetBubbleHeight() {
  int height = (show_expanded_view_ ? kExpandedBubbleBodyHeight
                                    : kDefaultBubbleBodyHeight) +
               header_view_->GetPreferredSize().height() +
               footer_view_->GetPreferredSize().height();
  return height;
}

void SharesheetBubbleView::RecordFormFactorMetric() {
  auto form_factor =
      TabletMode::Get()->InTabletMode()
          ? ::sharesheet::SharesheetMetrics::FormFactor::kTablet
          : ::sharesheet::SharesheetMetrics::FormFactor::kClamshell;
  ::sharesheet::SharesheetMetrics::RecordSharesheetFormFactor(form_factor);
}

BEGIN_METADATA(SharesheetBubbleView, views::BubbleDialogDelegateView)
END_METADATA

}  // namespace sharesheet
}  // namespace ash
