// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_background_view.h"

#include <algorithm>
#include <memory>

#include "ash/focus_cycler.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/login_shelf_view.h"
#include "ash/shelf/shelf_focus_cycler.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/tray/tray_event_filter.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/bind.h"
#include "base/scoped_multi_source_observation.h"
#include "base/time/time.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/aura/window.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/interpolated_transform.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/background.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/painter.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_animations.h"

namespace ash {
namespace {

const int kAnimationDurationForBubblePopupMs = 200;

// Duration of opacity animation for visibility changes.
constexpr base::TimeDelta kAnimationDurationForVisibilityMs =
    base::Milliseconds(250);

// Duration of opacity animation for hide animation.
constexpr base::TimeDelta kAnimationDurationForHideMs = base::Milliseconds(100);

// Bounce animation constants
const base::TimeDelta kAnimationDurationForBounceElement =
    base::Milliseconds(250);
const int kAnimationBounceUpDistance = 16;
const int kAnimationBounceDownDistance = 8;
const float kAnimationBounceScaleFactor = 0.5;

// When becoming visible delay the animation so that StatusAreaWidgetDelegate
// can animate sibling views out of the position to be occupied by the
// TrayBackgroundView.
const base::TimeDelta kShowAnimationDelayMs = base::Milliseconds(100);

// Switches left and right insets if RTL mode is active.
void MirrorInsetsIfNecessary(gfx::Insets* insets) {
  if (base::i18n::IsRTL()) {
    insets->Set(insets->top(), insets->right(), insets->bottom(),
                insets->left());
  }
}

// Returns background insets relative to the contents bounds of the view and
// mirrored if RTL mode is active.
gfx::Insets GetMirroredBackgroundInsets(bool is_shelf_horizontal) {
  gfx::Insets insets;
  // "Primary" is the same direction as the shelf, "secondary" is orthogonal.
  const int primary_padding = 0;
  const int secondary_padding =
      -ash::ShelfConfig::Get()->status_area_hit_region_padding();

  if (is_shelf_horizontal) {
    insets.Set(secondary_padding, primary_padding, secondary_padding,
               primary_padding + ash::kTraySeparatorWidth);
  } else {
    insets.Set(primary_padding, secondary_padding,
               primary_padding + ash::kTraySeparatorWidth, secondary_padding);
  }
  MirrorInsetsIfNecessary(&insets);
  return insets;
}

class HighlightPathGenerator : public views::HighlightPathGenerator {
 public:
  explicit HighlightPathGenerator(TrayBackgroundView* tray_background_view)
      : tray_background_view_(tray_background_view), insets_(gfx::Insets()) {}

  HighlightPathGenerator(TrayBackgroundView* tray_background_view,
                         gfx::Insets insets)
      : tray_background_view_(tray_background_view), insets_(insets) {}

  HighlightPathGenerator(const HighlightPathGenerator&) = delete;
  HighlightPathGenerator& operator=(const HighlightPathGenerator&) = delete;

  // HighlightPathGenerator:
  absl::optional<gfx::RRectF> GetRoundRect(const gfx::RectF& rect) override {
    gfx::RectF bounds(tray_background_view_->GetBackgroundBounds());
    bounds.Inset(insets_);
    return gfx::RRectF(bounds, ShelfConfig::Get()->control_border_radius());
  }

 private:
  TrayBackgroundView* const tray_background_view_;
  const gfx::Insets insets_;
};

}  // namespace

// Used to track when the anchor widget changes position on screen so that the
// bubble position can be updated.
class TrayBackgroundView::TrayWidgetObserver : public views::WidgetObserver {
 public:
  explicit TrayWidgetObserver(TrayBackgroundView* host) : host_(host) {}

  TrayWidgetObserver(const TrayWidgetObserver&) = delete;
  TrayWidgetObserver& operator=(const TrayWidgetObserver&) = delete;

  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override {
    host_->AnchorUpdated();
  }

  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override {
    host_->AnchorUpdated();
  }

  void Add(views::Widget* widget) { observations_.AddObservation(widget); }

 private:
  TrayBackgroundView* host_;
  base::ScopedMultiSourceObservation<views::Widget, views::WidgetObserver>
      observations_{this};
};

// Handles `TrayBackgroundView`'s animation on session changed.
class TrayBackgroundView::TrayBackgroundViewSessionChangeHandler
    : public SessionObserver {
 public:
  explicit TrayBackgroundViewSessionChangeHandler(
      TrayBackgroundView* tray_background_view)
      : tray_(tray_background_view) {
    DCHECK(tray_);
  }
  TrayBackgroundViewSessionChangeHandler(
      const TrayBackgroundViewSessionChangeHandler&) = delete;
  TrayBackgroundViewSessionChangeHandler& operator=(
      const TrayBackgroundViewSessionChangeHandler&) = delete;
  ~TrayBackgroundViewSessionChangeHandler() override = default;

 private:  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override {
    DisableShowAnimationInSequence();
  }
  void OnActiveUserSessionChanged(const AccountId& account_id) override {
    DisableShowAnimationInSequence();
  }

  // Disables the `TrayBackgroundView`'s show animation until all queued tasks
  // in the current task sequence are run.
  void DisableShowAnimationInSequence() {
    base::ScopedClosureRunner callback = tray_->DisableShowAnimation();
    base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                     callback.Release());
  }

  TrayBackgroundView* const tray_;
  ScopedSessionObserver session_observer_{this};
};

////////////////////////////////////////////////////////////////////////////////
// TrayBackgroundView

TrayBackgroundView::TrayBackgroundView(Shelf* shelf)
    // Note the ink drop style is ignored.
    : ActionableView(TrayPopupInkDropStyle::FILL_BOUNDS),
      shelf_(shelf),
      tray_container_(new TrayContainer(shelf)),
      is_active_(false),
      separator_visible_(true),
      visible_preferred_(false),
      show_with_virtual_keyboard_(false),
      show_when_collapsed_(true),
      widget_observer_(new TrayWidgetObserver(this)),
      handler_(new TrayBackgroundViewSessionChangeHandler(this)) {
  DCHECK(shelf_);
  SetNotifyEnterExitOnChild(true);

  auto ripple_attributes = AshColorProvider::Get()->GetRippleAttributes();
  views::InkDrop::Get(this)->SetBaseColor(ripple_attributes.base_color);
  views::InkDrop::Get(this)->SetVisibleOpacity(
      ripple_attributes.inkdrop_opacity);

  views::InkDrop::Get(this)->SetMode(
      views::InkDropHost::InkDropMode::ON_NO_GESTURE_HANDLER);
  views::InkDrop::Get(this)->SetCreateHighlightCallback(base::BindRepeating(
      [](TrayBackgroundView* host) {
        gfx::Rect bounds = host->GetBackgroundBounds();
        // Currently, we don't handle view resize. To compensate for that,
        // enlarge the bounds by two tray icons so that the highlight looks good
        // even if two more icons are added when it is visible. Note that ink
        // drop mask handles resize correctly, so the extra highlight would be
        // clipped.
        // TODO(mohsen): Remove this extra size when resize is handled properly
        // (see https://crbug.com/669253).
        const int icon_size = kTrayIconSize + 2 * kTrayImageItemPadding;
        bounds.set_width(bounds.width() + 2 * icon_size);
        bounds.set_height(bounds.height() + 2 * icon_size);
        const AshColorProvider::RippleAttributes ripple_attributes =
            AshColorProvider::Get()->GetRippleAttributes();
        auto highlight = std::make_unique<views::InkDropHighlight>(
            gfx::SizeF(bounds.size()), ripple_attributes.base_color);
        highlight->set_visible_opacity(ripple_attributes.highlight_opacity);
        return highlight;
      },
      this));
  views::InkDrop::Get(this)->SetCreateRippleCallback(base::BindRepeating(
      [](TrayBackgroundView* host) -> std::unique_ptr<views::InkDropRipple> {
        const AshColorProvider::RippleAttributes ripple_attributes =
            AshColorProvider::Get()->GetRippleAttributes();
        return std::make_unique<views::FloodFillInkDropRipple>(
            host->size(), host->GetBackgroundInsets(),
            views::InkDrop::Get(host)->GetInkDropCenterBasedOnLastEvent(),
            ripple_attributes.base_color, ripple_attributes.inkdrop_opacity);
      },
      this));

  SetLayoutManager(std::make_unique<views::FillLayout>());
  SetInstallFocusRingOnFocus(true);

  views::FocusRing* const focus_ring = views::FocusRing::Get(this);
  focus_ring->SetColor(AshColorProvider::Get()->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kFocusRingColor));
  focus_ring->SetPathGenerator(std::make_unique<HighlightPathGenerator>(
      this, kTrayBackgroundFocusPadding));
  SetFocusPainter(nullptr);

  views::HighlightPathGenerator::Install(
      this, std::make_unique<HighlightPathGenerator>(this));

  AddChildView(tray_container_);

  tray_event_filter_ = std::make_unique<TrayEventFilter>();

  // Use layer color to provide background color. Note that children views
  // need to have their own layers to be visible.
  SetPaintToLayer(ui::LAYER_SOLID_COLOR);
  layer()->SetFillsBoundsOpaquely(false);

  // Start the tray items not visible, because visibility changes are animated.
  views::View::SetVisible(false);
}

TrayBackgroundView::~TrayBackgroundView() {
  Shell::Get()->system_tray_model()->virtual_keyboard()->RemoveObserver(this);
  widget_observer_.reset();
  handler_.reset();
}

void TrayBackgroundView::Initialize() {
  widget_observer_->Add(GetWidget());
  Shell::Get()->system_tray_model()->virtual_keyboard()->AddObserver(this);

  UpdateBackground();
}

// static
void TrayBackgroundView::InitializeBubbleAnimations(
    views::Widget* bubble_widget) {
  aura::Window* window = bubble_widget->GetNativeWindow();
  ::wm::SetWindowVisibilityAnimationType(
      window, ::wm::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE);
  ::wm::SetWindowVisibilityAnimationTransition(window, ::wm::ANIMATE_HIDE);
  ::wm::SetWindowVisibilityAnimationDuration(
      window, base::Milliseconds(kAnimationDurationForBubblePopupMs));
}

void TrayBackgroundView::SetVisiblePreferred(bool visible_preferred) {
  if (visible_preferred_ == visible_preferred)
    return;
  visible_preferred_ = visible_preferred;
  StartVisibilityAnimation(GetEffectiveVisibility());

  // We need to update which trays overflow after showing or hiding a tray.
  // If the hide animation is still playing, we do the `UpdateStatusArea(bool
  // should_log_visible_pod_count)` when the animation is finished.
  if (!layer()->GetAnimator()->is_animating() || visible_preferred_)
    UpdateStatusArea(true /*should_log_visible_pod_count*/);
}

bool TrayBackgroundView::IsShowingMenu() const {
  return context_menu_runner_ && context_menu_runner_->IsRunning();
}

void TrayBackgroundView::StartVisibilityAnimation(bool visible) {
  if (visible == layer()->GetTargetVisibility())
    return;

  base::AutoReset<bool> is_starting_animation(&is_starting_animation_, true);

  if (visible) {
    views::View::SetVisible(true);
    // If SetVisible(true) is called while animating to not visible, then
    // views::View::SetVisible(true) is a no-op. When the previous animation
    // ends layer->SetVisible(false) is called. To prevent this
    // layer->SetVisible(true) immediately interrupts the animation of this
    // property, and keeps the layer visible.
    layer()->SetVisible(true);

    // We only show visible animation when `IsShowAnimationEnabled()`.
    if (IsShowAnimationEnabled()) {
      if (use_bounce_in_animation_)
        BounceInAnimation();
      else
        FadeInAnimation();
    } else {
      // The opacity and scale of the `layer()` may have been manipulated, so
      // reset it before it is shown.
      layer()->SetOpacity(1.0f);
      layer()->SetTransform(gfx::Transform());
    }
  } else {
    HideAnimation();
  }
}

base::ScopedClosureRunner TrayBackgroundView::DisableShowAnimation() {
  if (layer()->GetAnimator()->is_animating())
    layer()->GetAnimator()->StopAnimating();

  ++disable_show_animation_count_;
  if (disable_show_animation_count_ == 1u)
    OnShouldShowAnimationChanged(false);

  return base::ScopedClosureRunner(base::BindOnce(
      [](const base::WeakPtr<TrayBackgroundView>& ptr) {
        if (ptr) {
          --ptr->disable_show_animation_count_;
          if (ptr->IsShowAnimationEnabled())
            ptr->OnShouldShowAnimationChanged(true);
        }
      },
      weak_factory_.GetWeakPtr()));
}

void TrayBackgroundView::UpdateStatusArea(bool should_log_visible_pod_count) {
  auto* status_area_widget = shelf_->GetStatusAreaWidget();
  if (status_area_widget) {
    status_area_widget->UpdateCollapseState();
    if (should_log_visible_pod_count)
      status_area_widget->LogVisiblePodCountMetric();
  }
}

void TrayBackgroundView::OnVisibilityAnimationFinished(
    bool should_log_visible_pod_count,
    bool aborted) {
  if (aborted && is_starting_animation_)
    return;
  if (!visible_preferred_) {
    views::View::SetVisible(false);
    UpdateStatusArea(should_log_visible_pod_count);
  }
}

void TrayBackgroundView::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  context_menu_model_ = CreateContextMenuModel();
  if (!context_menu_model_)
    return;

  const int run_types = views::MenuRunner::USE_TOUCHABLE_LAYOUT |
                        views::MenuRunner::CONTEXT_MENU |
                        views::MenuRunner::FIXED_ANCHOR;
  context_menu_runner_ = std::make_unique<views::MenuRunner>(
      context_menu_model_.get(), run_types,
      base::BindRepeating(&Shelf::UpdateAutoHideState,
                          base::Unretained(shelf_)));
  views::MenuAnchorPosition anchor;
  switch (shelf_->alignment()) {
    case ShelfAlignment::kBottom:
    case ShelfAlignment::kBottomLocked:
      anchor = views::MenuAnchorPosition::kBubbleTopRight;
      break;
    case ShelfAlignment::kLeft:
      anchor = views::MenuAnchorPosition::kBubbleRight;
      break;
    case ShelfAlignment::kRight:
      anchor = views::MenuAnchorPosition::kBubbleLeft;
      break;
  }

  context_menu_runner_->RunMenuAt(
      source->GetWidget(), /*button_controller=*/nullptr,
      source->GetBoundsInScreen(), anchor, source_type);
}

void TrayBackgroundView::AboutToRequestFocusFromTabTraversal(bool reverse) {
  Shelf* shelf = Shelf::ForWindow(GetWidget()->GetNativeWindow());
  StatusAreaWidgetDelegate* delegate =
      shelf->GetStatusAreaWidget()->status_area_widget_delegate();
  if (!delegate || !delegate->ShouldFocusOut(reverse))
    return;

  shelf_->shelf_focus_cycler()->FocusOut(reverse, SourceView::kStatusAreaView);
}

void TrayBackgroundView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  ActionableView::GetAccessibleNodeData(node_data);
  node_data->SetName(GetAccessibleNameForTray());

  if (LockScreen::HasInstance()) {
    GetViewAccessibility().OverrideNextFocus(LockScreen::Get()->widget());
  }

  Shelf* shelf = Shelf::ForWindow(GetWidget()->GetNativeWindow());
  ShelfWidget* shelf_widget = shelf->shelf_widget();
  GetViewAccessibility().OverridePreviousFocus(shelf_widget->hotseat_widget());
  GetViewAccessibility().OverrideNextFocus(shelf_widget->navigation_widget());
}

void TrayBackgroundView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

std::unique_ptr<ui::Layer> TrayBackgroundView::RecreateLayer() {
  if (layer()->GetAnimator()->is_animating())
    OnVisibilityAnimationFinished(/*should_log_visible_pod_count=*/false,
                                  /*aborted=*/false);

  return views::View::RecreateLayer();
}

void TrayBackgroundView::OnThemeChanged() {
  ActionableView::OnThemeChanged();
  UpdateBackground();
}

void TrayBackgroundView::OnVirtualKeyboardVisibilityChanged() {
  // We call the base class' SetVisible to skip animations.
  if (GetVisible() != GetEffectiveVisibility())
    views::View::SetVisible(GetEffectiveVisibility());
}

TrayBubbleView* TrayBackgroundView::GetBubbleView() {
  return nullptr;
}

views::Widget* TrayBackgroundView::GetBubbleWidget() const {
  return nullptr;
}

void TrayBackgroundView::CloseBubble() {}

void TrayBackgroundView::ShowBubble() {}

void TrayBackgroundView::CalculateTargetBounds() {
  tray_container_->CalculateTargetBounds();
}

void TrayBackgroundView::UpdateLayout() {
  UpdateBackground();
  tray_container_->UpdateLayout();
}

void TrayBackgroundView::UpdateAfterLoginStatusChange() {
  // Handled in subclasses.
}

void TrayBackgroundView::UpdateAfterStatusAreaCollapseChange() {
  views::View::SetVisible(GetEffectiveVisibility());
}

void TrayBackgroundView::BubbleResized(const TrayBubbleView* bubble_view) {}

void TrayBackgroundView::UpdateBackground() {
  const float radius = ShelfConfig::Get()->control_border_radius();
  gfx::RoundedCornersF rounded_corners = {radius, radius, radius, radius};
  layer()->SetRoundedCornerRadius(rounded_corners);
  layer()->SetIsFastRoundedCorner(true);
  layer()->SetBackgroundBlur(
      ShelfConfig::Get()->GetShelfControlButtonBlurRadius());
  layer()->SetColor(ShelfConfig::Get()->GetShelfControlButtonColor());
  layer()->SetClipRect(GetBackgroundBounds());
}

void TrayBackgroundView::OnAnimationAborted() {
  OnVisibilityAnimationFinished(/*should_log_visible_pod_count=*/true,
                                /*aborted=*/true);
}
void TrayBackgroundView::OnAnimationEnded() {
  OnVisibilityAnimationFinished(/*should_log_visible_pod_count=*/true,
                                /*aborted=*/false);
}

void TrayBackgroundView::FadeInAnimation() {
  gfx::Transform transform;
  if (shelf_->IsHorizontalAlignment())
    transform.Translate(width(), 0.0f);
  else
    transform.Translate(0.0f, height());

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnAborted(base::BindOnce(
          [](base::WeakPtr<TrayBackgroundView> view) {
            if (view)
              view->OnAnimationAborted();
          },
          weak_factory_.GetWeakPtr()))
      .OnEnded(base::BindOnce(
          [](base::WeakPtr<TrayBackgroundView> view) {
            if (view)
              view->OnAnimationEnded();
          },
          weak_factory_.GetWeakPtr()))
      .Once()
      .SetDuration(kShowAnimationDelayMs)
      .Then()
      .SetDuration(base::TimeDelta())
      .SetOpacity(this, 0.0f)
      .SetTransform(this, transform)
      .Then()
      .SetDuration(kAnimationDurationForVisibilityMs)
      .SetOpacity(this, 1.0f)
      .SetTransform(this, gfx::Transform());
}

void TrayBackgroundView::BounceInAnimation() {
  gfx::Vector2dF bounce_up_location;
  gfx::Vector2dF bounce_down_location;

  switch (shelf_->alignment()) {
    case ShelfAlignment::kLeft:
      bounce_up_location = gfx::Vector2dF(kAnimationBounceUpDistance, 0);
      bounce_down_location = gfx::Vector2dF(-kAnimationBounceDownDistance, 0);
      break;
    case ShelfAlignment::kRight:
      bounce_up_location = gfx::Vector2dF(-kAnimationBounceUpDistance, 0);
      bounce_down_location = gfx::Vector2dF(kAnimationBounceDownDistance, 0);
      break;
    case ShelfAlignment::kBottom:
    case ShelfAlignment::kBottomLocked:
    default:
      bounce_up_location = gfx::Vector2dF(0, -kAnimationBounceUpDistance);
      bounce_down_location = gfx::Vector2dF(0, kAnimationBounceDownDistance);
  }

  gfx::Transform initial_scale;
  initial_scale.Scale3d(kAnimationBounceScaleFactor,
                        kAnimationBounceScaleFactor, 1);

  gfx::Transform initial_state =
      gfx::TransformAboutPivot(GetLocalBounds().CenterPoint(), initial_scale);

  gfx::Transform scale_about_pivot = gfx::TransformAboutPivot(
      GetLocalBounds().CenterPoint(), gfx::Transform());
  scale_about_pivot.Translate(bounce_up_location);

  gfx::Transform move_down;
  move_down.Translate(bounce_down_location);

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnAborted(base::BindOnce(
          [](base::WeakPtr<TrayBackgroundView> view) {
            if (view)
              view->OnAnimationAborted();
          },
          weak_factory_.GetWeakPtr()))
      .OnEnded(base::BindOnce(
          [](base::WeakPtr<TrayBackgroundView> view) {
            if (view)
              view->OnAnimationEnded();
          },
          weak_factory_.GetWeakPtr()))
      .Once()
      .SetDuration(base::TimeDelta())
      .SetOpacity(this, 1.0)
      .SetTransform(this, std::move(initial_state))
      .Then()
      .SetDuration(kAnimationDurationForBounceElement)
      .SetTransform(this, std::move(scale_about_pivot),
                    gfx::Tween::FAST_OUT_SLOW_IN_3)
      .Then()
      .SetDuration(kAnimationDurationForBounceElement)
      .SetTransform(this, std::move(move_down), gfx::Tween::EASE_OUT_4)
      .Then()
      .SetDuration(kAnimationDurationForBounceElement)
      .SetTransform(this, gfx::Transform(), gfx::Tween::FAST_OUT_SLOW_IN_3);
}

// Any visibility updates should be called after the hide animation is
// finished, otherwise the view will disappear immediately without animation
// once the view's visibility is set to false.
void TrayBackgroundView::HideAnimation() {
  gfx::Transform scale;
  scale.Scale3d(kAnimationBounceScaleFactor, kAnimationBounceScaleFactor, 1);

  gfx::Transform scale_about_pivot =
      gfx::TransformAboutPivot(GetLocalBounds().CenterPoint(), scale);

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnAborted(base::BindOnce(
          [](base::WeakPtr<TrayBackgroundView> view) {
            if (view)
              view->OnAnimationAborted();
          },
          weak_factory_.GetWeakPtr()))
      .OnEnded(base::BindOnce(
          [](base::WeakPtr<TrayBackgroundView> view) {
            if (view)
              view->OnAnimationEnded();
          },
          weak_factory_.GetWeakPtr()))
      .Once()
      .SetDuration(kAnimationDurationForHideMs)
      .SetVisibility(this, false)
      .SetTransform(this, std::move(scale_about_pivot))
      .SetOpacity(this, 0.0f);
}

void TrayBackgroundView::SetIsActive(bool is_active) {
  if (is_active_ == is_active)
    return;
  is_active_ = is_active;
  views::InkDrop::Get(this)->AnimateToState(
      is_active_ ? views::InkDropState::ACTIVATED
                 : views::InkDropState::DEACTIVATED,
      nullptr);
}

views::View* TrayBackgroundView::GetBubbleAnchor() const {
  return tray_container_;
}

gfx::Insets TrayBackgroundView::GetBubbleAnchorInsets() const {
  gfx::Insets anchor_insets = GetBubbleAnchor()->GetInsets();
  gfx::Insets tray_bg_insets = GetInsets();
  if (shelf_->alignment() == ShelfAlignment::kBottom ||
      shelf_->alignment() == ShelfAlignment::kBottomLocked) {
    return gfx::Insets(-tray_bg_insets.top(), anchor_insets.left(),
                       -tray_bg_insets.bottom(), anchor_insets.right());
  } else {
    return gfx::Insets(anchor_insets.top(), -tray_bg_insets.left(),
                       anchor_insets.bottom(), -tray_bg_insets.right());
  }
}

aura::Window* TrayBackgroundView::GetBubbleWindowContainer() {
  return Shell::GetContainer(
      tray_container()->GetWidget()->GetNativeWindow()->GetRootWindow(),
      kShellWindowId_SettingBubbleContainer);
}

gfx::Rect TrayBackgroundView::GetBackgroundBounds() const {
  gfx::Rect bounds = GetLocalBounds();
  bounds.Inset(GetBackgroundInsets());
  return bounds;
}

bool TrayBackgroundView::PerformAction(const ui::Event& event) {
  if (GetBubbleWidget())
    CloseBubble();
  else
    ShowBubble();
  return true;
}

void TrayBackgroundView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  UpdateBackground();

  ActionableView::OnBoundsChanged(previous_bounds);
}

bool TrayBackgroundView::ShouldEnterPushedState(const ui::Event& event) {
  if (is_active_)
    return false;

  return ActionableView::ShouldEnterPushedState(event);
}

void TrayBackgroundView::HandlePerformActionResult(bool action_performed,
                                                   const ui::Event& event) {
  // When an action is performed, ink drop ripple is handled in SetIsActive().
  if (action_performed)
    return;
  ActionableView::HandlePerformActionResult(action_performed, event);
}

std::unique_ptr<ui::SimpleMenuModel>
TrayBackgroundView::CreateContextMenuModel() {
  return nullptr;
}

views::PaintInfo::ScaleType TrayBackgroundView::GetPaintScaleType() const {
  return views::PaintInfo::ScaleType::kUniformScaling;
}

gfx::Insets TrayBackgroundView::GetBackgroundInsets() const {
  gfx::Insets insets =
      GetMirroredBackgroundInsets(shelf_->IsHorizontalAlignment());

  // |insets| are relative to contents bounds. Change them to be relative to
  // local bounds.
  gfx::Insets local_contents_insets =
      GetLocalBounds().InsetsFrom(GetContentsBounds());
  MirrorInsetsIfNecessary(&local_contents_insets);
  insets += local_contents_insets;

  if (Shell::Get()->IsInTabletMode() && ShelfConfig::Get()->is_in_app()) {
    insets += gfx::Insets(
        ShelfConfig::Get()->in_app_control_button_height_inset(), 0);
  }

  return insets;
}

bool TrayBackgroundView::GetEffectiveVisibility() {
  // When the virtual keyboard is visible, the effective visibility of the view
  // is solely determined by |show_with_virtual_keyboard_|.
  if (Shell::Get()->system_tray_model()->virtual_keyboard()->visible())
    return show_with_virtual_keyboard_;

  if (!visible_preferred_)
    return false;

  DCHECK(GetWidget());

  // When the status area is collapsed, the effective visibility of the view is
  // determined by |show_when_collapsed_|.
  StatusAreaWidget::CollapseState collapse_state =
      Shelf::ForWindow(GetWidget()->GetNativeWindow())
          ->GetStatusAreaWidget()
          ->collapse_state();
  if (collapse_state == StatusAreaWidget::CollapseState::COLLAPSED)
    return show_when_collapsed_;

  return true;
}

BEGIN_METADATA(TrayBackgroundView, ActionableView)
END_METADATA

}  // namespace ash
