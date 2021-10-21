// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_navigation_widget.h"

#include "ash/focus_cycler.h"
#include "ash/public/cpp/metrics_util.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/shelf/back_button.h"
#include "ash/shelf/home_button.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_focus_cycler.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_layout_manager_observer.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/status_area_widget.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/bind.h"
#include "base/i18n/rtl.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/animation_throughput_reporter.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/compositor/throughput_tracker.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/background.h"
#include "ui/views/view.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {
namespace {

// The duration of the back button opacity animation.
constexpr base::TimeDelta kButtonOpacityAnimationDuration =
    base::Milliseconds(50);

// Returns the bounds for the first button shown in this view (the back
// button in tablet mode, the home button otherwise).
gfx::Rect GetFirstButtonBounds(bool is_shelf_horizontal) {
  // ShelfNavigationWidget is larger than the buttons in order to enable child
  // views to capture events nearby.
  return gfx::Rect(
      ShelfConfig::Get()->control_button_edge_spacing(is_shelf_horizontal),
      ShelfConfig::Get()->control_button_edge_spacing(!is_shelf_horizontal),
      ShelfConfig::Get()->control_size(), ShelfConfig::Get()->control_size());
}

// Returns the bounds for the second button shown in this view (which is
// always the home button and only in tablet mode, which implies a horizontal
// shelf).
gfx::Rect GetSecondButtonBounds() {
  // Second button only shows for horizontal shelf.
  return gfx::Rect(ShelfConfig::Get()->control_button_edge_spacing(
                       true /* is_primary_axis_edge */) +
                       ShelfConfig::Get()->control_size() +
                       ShelfConfig::Get()->button_spacing(),
                   ShelfConfig::Get()->control_button_edge_spacing(
                       false /* is_primary_axis_edge */),
                   ShelfConfig::Get()->control_size(),
                   ShelfConfig::Get()->control_size());
}

bool IsBackButtonShown(bool horizontal_alignment) {
  // Back button should only be shown in horizontal shelf.
  // TODO(https://crbug.com/1102648): Horizontal shelf should be implied by
  // tablet mode, but this may not be the case during tablet mode transition as
  // shelf layout may get updated before the correct shelf alignment is set.
  // Remove this when the linked bug is resolved.
  if (!horizontal_alignment)
    return false;

  // TODO(https://crbug.com/1058205): Test this behavior.
  if (ShelfConfig::Get()->is_virtual_keyboard_shown())
    return true;

  if (!ShelfConfig::Get()->shelf_controls_shown())
    return false;

  return Shell::Get()->IsInTabletMode() && ShelfConfig::Get()->is_in_app();
}

bool IsHomeButtonShown() {
  return ShelfConfig::Get()->shelf_controls_shown();
}

// An implicit animation observer that hides a view once the view's opacity
// animation finishes.
// It deletes itself when the animation is done.
class AnimationObserverToHideView : public ui::ImplicitAnimationObserver {
 public:
  explicit AnimationObserverToHideView(views::View* view) : view_(view) {}
  ~AnimationObserverToHideView() override = default;

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override {
    if (view_->layer()->GetTargetOpacity() == 0.0f)
      view_->SetVisible(false);
    delete this;
  }

 private:
  views::View* const view_;
};

// Tracks the animation smoothness of a view's bounds animation using
// ui::ThroughputTracker.
class BoundsAnimationReporter : public gfx::AnimationDelegate {
 public:
  BoundsAnimationReporter(views::View* view,
                          metrics_util::ReportCallback report_callback)
      : tracker_(
            view->GetWidget()->GetCompositor()->RequestNewThroughputTracker()) {
    tracker_.Start(std::move(report_callback));
  }
  BoundsAnimationReporter(const BoundsAnimationReporter& other) = delete;
  BoundsAnimationReporter& operator=(const BoundsAnimationReporter& other) =
      delete;
  ~BoundsAnimationReporter() override = default;

  // gfx::AnimationDelegate:
  void AnimationEnded(const gfx::Animation* animation) override {
    tracker_.Stop();
  }
  void AnimationCanceled(const gfx::Animation* animation) override {
    tracker_.Cancel();
  }

 private:
  ui::ThroughputTracker tracker_;
};

}  // namespace

// An animation metrics reporter for the shelf navigation buttons.
class ASH_EXPORT NavigationButtonAnimationMetricsReporter {
 public:
  // The different kinds of navigation buttons.
  enum class NavigationButtonType {
    // The Navigation Widget's back button.
    kBackButton,
    // The Navigation Widget's home button.
    kHomeButton
  };
  explicit NavigationButtonAnimationMetricsReporter(
      NavigationButtonType navigation_button_type)
      : navigation_button_type_(navigation_button_type) {}

  ~NavigationButtonAnimationMetricsReporter() = default;

  NavigationButtonAnimationMetricsReporter(
      const NavigationButtonAnimationMetricsReporter&) = delete;
  NavigationButtonAnimationMetricsReporter& operator=(
      const NavigationButtonAnimationMetricsReporter&) = delete;

  void ReportSmoothness(HotseatState target_hotseat_state, int smoothness) {
    switch (target_hotseat_state) {
      case HotseatState::kShownClamshell:
      case HotseatState::kShownHomeLauncher:
        switch (navigation_button_type_) {
          case NavigationButtonType::kBackButton:
            UMA_HISTOGRAM_PERCENTAGE(
                "Ash.NavigationWidget.BackButton.AnimationSmoothness."
                "TransitionToShownHotseat",
                smoothness);
            break;
          case NavigationButtonType::kHomeButton:
            UMA_HISTOGRAM_PERCENTAGE(
                "Ash.NavigationWidget.HomeButton.AnimationSmoothness."
                "TransitionToShownHotseat",
                smoothness);
            break;
          default:
            NOTREACHED();
            break;
        }
        break;
      case HotseatState::kExtended:
        switch (navigation_button_type_) {
          case NavigationButtonType::kBackButton:
            UMA_HISTOGRAM_PERCENTAGE(
                "Ash.NavigationWidget.BackButton.AnimationSmoothness."
                "TransitionToExtendedHotseat",
                smoothness);
            break;
          case NavigationButtonType::kHomeButton:
            UMA_HISTOGRAM_PERCENTAGE(
                "Ash.NavigationWidget.HomeButton.AnimationSmoothness."
                "TransitionToExtendedHotseat",
                smoothness);
            break;
          default:
            NOTREACHED();
            break;
        }
        break;
      case HotseatState::kHidden:
        switch (navigation_button_type_) {
          case NavigationButtonType::kBackButton:
            UMA_HISTOGRAM_PERCENTAGE(
                "Ash.NavigationWidget.BackButton.AnimationSmoothness."
                "TransitionToHiddenHotseat",
                smoothness);
            break;
          case NavigationButtonType::kHomeButton:
            UMA_HISTOGRAM_PERCENTAGE(
                "Ash.NavigationWidget.HomeButton.AnimationSmoothness."
                "TransitionToHiddenHotseat",
                smoothness);
            break;
          default:
            NOTREACHED();
            break;
        }
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  metrics_util::ReportCallback GetReportCallback(
      HotseatState target_hotseat_state) {
    DCHECK_NE(target_hotseat_state, HotseatState::kNone);
    return metrics_util::ForSmoothness(base::BindRepeating(
        &NavigationButtonAnimationMetricsReporter::ReportSmoothness,
        weak_ptr_factory_.GetWeakPtr(), target_hotseat_state));
  }

 private:
  // The type of navigation button that is animated.
  const NavigationButtonType navigation_button_type_;

  base::WeakPtrFactory<NavigationButtonAnimationMetricsReporter>
      weak_ptr_factory_{this};
};

class ShelfNavigationWidget::Delegate : public views::AccessiblePaneView,
                                        public views::WidgetDelegate {
 public:
  Delegate(Shelf* shelf, ShelfView* shelf_view);

  Delegate(const Delegate&) = delete;
  Delegate& operator=(const Delegate&) = delete;

  ~Delegate() override;

  // Initializes the view.
  void Init(ui::Layer* parent_layer);

  void UpdateOpaqueBackground();

  // views::View:
  FocusTraversable* GetPaneFocusTraversable() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void ReorderChildLayers(ui::Layer* parent_layer) override;
  void OnBoundsChanged(const gfx::Rect& old_bounds) override;

  // views::AccessiblePaneView:
  View* GetDefaultFocusableChild() override;

  // views::WidgetDelegate:
  bool CanActivate() const override;
  views::Widget* GetWidget() override { return View::GetWidget(); }
  const views::Widget* GetWidget() const override { return View::GetWidget(); }

  BackButton* back_button() const { return back_button_; }
  HomeButton* home_button() const { return home_button_; }

  void set_default_last_focusable_child(bool default_last_focusable_child) {
    default_last_focusable_child_ = default_last_focusable_child;
  }

 private:
  void SetParentLayer(ui::Layer* layer);

  BackButton* back_button_ = nullptr;
  HomeButton* home_button_ = nullptr;
  // When true, the default focus of the navigation widget is the last
  // focusable child.
  bool default_last_focusable_child_ = false;

  // A background layer that may be visible depending on shelf state.
  ui::Layer opaque_background_;

  Shelf* shelf_ = nullptr;
};

ShelfNavigationWidget::Delegate::Delegate(Shelf* shelf, ShelfView* shelf_view)
    : opaque_background_(ui::LAYER_SOLID_COLOR), shelf_(shelf) {
  SetOwnedByWidget(true);

  set_allow_deactivate_on_esc(true);

  const int control_size = ShelfConfig::Get()->control_size();
  std::unique_ptr<BackButton> back_button_ptr =
      std::make_unique<BackButton>(shelf);
  back_button_ = AddChildView(std::move(back_button_ptr));
  back_button_->SetSize(gfx::Size(control_size, control_size));

  std::unique_ptr<HomeButton> home_button_ptr =
      std::make_unique<HomeButton>(shelf);
  home_button_ = AddChildView(std::move(home_button_ptr));
  home_button_->set_context_menu_controller(shelf_view);
  home_button_->SetSize(gfx::Size(control_size, control_size));

  GetViewAccessibility().OverrideNextFocus(shelf->hotseat_widget());
  GetViewAccessibility().OverridePreviousFocus(shelf->GetStatusAreaWidget());
  opaque_background_.SetName("shelfNavigation/Background");
}

ShelfNavigationWidget::Delegate::~Delegate() = default;

void ShelfNavigationWidget::Delegate::Init(ui::Layer* parent_layer) {
  SetParentLayer(parent_layer);
  UpdateOpaqueBackground();
}

void ShelfNavigationWidget::Delegate::UpdateOpaqueBackground() {
  opaque_background_.SetColor(ShelfConfig::Get()->GetShelfControlButtonColor());

  // Hide background if no buttons should be shown.
  if (!IsHomeButtonShown() &&
      !IsBackButtonShown(shelf_->IsHorizontalAlignment())) {
    opaque_background_.SetVisible(false);
    return;
  }

  if (Shell::Get()->IsInTabletMode() && ShelfConfig::Get()->is_in_app()) {
    opaque_background_.SetVisible(false);
    return;
  }

  opaque_background_.SetVisible(true);

  float radius = ShelfConfig::Get()->control_border_radius();
  gfx::RoundedCornersF rounded_corners = {radius, radius, radius, radius};
  if (opaque_background_.rounded_corner_radii() != rounded_corners)
    opaque_background_.SetRoundedCornerRadius(rounded_corners);

  // The opaque background does not show up when there are two buttons.
  gfx::Rect opaque_background_bounds =
      GetFirstButtonBounds(shelf_->IsHorizontalAlignment());
  opaque_background_.SetBounds(opaque_background_bounds);
  opaque_background_.SetBackgroundBlur(
      ShelfConfig::Get()->GetShelfControlButtonBlurRadius());
}

bool ShelfNavigationWidget::Delegate::CanActivate() const {
  // We don't want mouse clicks to activate us, but we need to allow
  // activation when the user is using the keyboard (FocusCycler).
  return Shell::Get()->focus_cycler()->widget_activating() == GetWidget();
}

views::FocusTraversable*
ShelfNavigationWidget::Delegate::GetPaneFocusTraversable() {
  return this;
}

void ShelfNavigationWidget::Delegate::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kToolbar;
  node_data->SetName(l10n_util::GetStringUTF8(IDS_ASH_SHELF_ACCESSIBLE_NAME));

  ShelfWidget* shelf_widget =
      Shelf::ForWindow(GetWidget()->GetNativeWindow())->shelf_widget();
  GetViewAccessibility().OverrideNextFocus(shelf_widget->hotseat_widget());
  GetViewAccessibility().OverridePreviousFocus(
      shelf_widget->status_area_widget());
}

void ShelfNavigationWidget::Delegate::ReorderChildLayers(
    ui::Layer* parent_layer) {
  views::View::ReorderChildLayers(parent_layer);
  parent_layer->StackAtBottom(&opaque_background_);
}

void ShelfNavigationWidget::Delegate::OnBoundsChanged(
    const gfx::Rect& old_bounds) {
  UpdateOpaqueBackground();
}

views::View* ShelfNavigationWidget::Delegate::GetDefaultFocusableChild() {
  return default_last_focusable_child_ ? GetLastFocusableChild()
                                       : GetFirstFocusableChild();
}

void ShelfNavigationWidget::Delegate::SetParentLayer(ui::Layer* layer) {
  layer->Add(&opaque_background_);
  ReorderLayers();
}

ShelfNavigationWidget::TestApi::TestApi(ShelfNavigationWidget* widget)
    : navigation_widget_(widget) {}

ShelfNavigationWidget::TestApi::~TestApi() = default;

bool ShelfNavigationWidget::TestApi::IsHomeButtonVisible() const {
  const HomeButton* button = navigation_widget_->delegate_->home_button();
  const float opacity = button->layer()->GetTargetOpacity();
  DCHECK(opacity == 0.0f || opacity == 1.0f)
      << "Unexpected opacity " << opacity;
  return opacity > 0.0f && button->GetVisible();
}

bool ShelfNavigationWidget::TestApi::IsBackButtonVisible() const {
  const BackButton* button = navigation_widget_->delegate_->back_button();
  const float opacity = button->layer()->GetTargetOpacity();
  DCHECK(opacity == 0.0f || opacity == 1.0f)
      << "Unexpected opacity " << opacity;
  return opacity > 0.0f && button->GetVisible();
}

views::BoundsAnimator* ShelfNavigationWidget::TestApi::GetBoundsAnimator() {
  return navigation_widget_->bounds_animator_.get();
}

ShelfNavigationWidget::ShelfNavigationWidget(Shelf* shelf,
                                             ShelfView* shelf_view)
    : shelf_(shelf),
      delegate_(new ShelfNavigationWidget::Delegate(shelf, shelf_view)),
      bounds_animator_(
          std::make_unique<views::BoundsAnimator>(delegate_,
                                                  /*use_transforms=*/true)),
      back_button_metrics_reporter_(
          std::make_unique<NavigationButtonAnimationMetricsReporter>(
              NavigationButtonAnimationMetricsReporter::NavigationButtonType::
                  kBackButton)),
      home_button_metrics_reporter_(
          std::make_unique<NavigationButtonAnimationMetricsReporter>(
              NavigationButtonAnimationMetricsReporter::NavigationButtonType::
                  kHomeButton)) {
  DCHECK(shelf_);
}

ShelfNavigationWidget::~ShelfNavigationWidget() {
  // Cancel animations now so the BoundsAnimator doesn't outlive the metrics
  // reporter associated to it.
  bounds_animator_->Cancel();
}

void ShelfNavigationWidget::Initialize(aura::Window* container) {
  DCHECK(container);
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.name = "ShelfNavigationWidget";
  params.delegate = delegate_;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = container;
  Init(std::move(params));
  delegate_->Init(GetLayer());
  set_focus_on_creation(false);
  delegate_->SetEnableArrowKeyTraversal(true);
  SetContentsView(delegate_);
  SetSize(CalculateIdealSize(/*only_visible_area=*/false));
  UpdateLayout(/*animate=*/false);
}

void ShelfNavigationWidget::OnMouseEvent(ui::MouseEvent* event) {
  if (event->IsMouseWheelEvent()) {
    ui::MouseWheelEvent* mouse_wheel_event = event->AsMouseWheelEvent();
    shelf_->ProcessMouseWheelEvent(mouse_wheel_event);
    return;
  }

  views::Widget::OnMouseEvent(event);
}

void ShelfNavigationWidget::OnScrollEvent(ui::ScrollEvent* event) {
  shelf_->ProcessScrollEvent(event);
  if (!event->handled())
    views::Widget::OnScrollEvent(event);
}

bool ShelfNavigationWidget::OnNativeWidgetActivationChanged(bool active) {
  if (!Widget::OnNativeWidgetActivationChanged(active))
    return false;
  if (active)
    delegate_->SetPaneFocusAndFocusDefault();
  return true;
}

void ShelfNavigationWidget::OnGestureEvent(ui::GestureEvent* event) {
  // Shelf::ProcessGestureEvent expects an event whose location is in screen
  // coordinates - create a copy of the event with the location in screen
  // coordinate system.
  ui::GestureEvent copy_event(*event);
  gfx::Point location_in_screen(copy_event.location());
  wm::ConvertPointToScreen(GetNativeWindow(), &location_in_screen);
  copy_event.set_location(location_in_screen);

  if (shelf_->ProcessGestureEvent(copy_event)) {
    event->StopPropagation();
    return;
  }
  views::Widget::OnGestureEvent(event);
}

BackButton* ShelfNavigationWidget::GetBackButton() const {
  return IsBackButtonShown(shelf_->IsHorizontalAlignment())
             ? delegate_->back_button()
             : nullptr;
}

HomeButton* ShelfNavigationWidget::GetHomeButton() const {
  return IsHomeButtonShown() ? delegate_->home_button() : nullptr;
}

void ShelfNavigationWidget::SetDefaultLastFocusableChild(
    bool default_last_focusable_child) {
  delegate_->set_default_last_focusable_child(default_last_focusable_child);
}

void ShelfNavigationWidget::CalculateTargetBounds() {
  const gfx::Point shelf_origin =
      shelf_->shelf_widget()->GetTargetBounds().origin();

  gfx::Point nav_origin = gfx::Point(shelf_origin.x(), shelf_origin.y());
  gfx::Size nav_size = CalculateIdealSize(/*only_visible_area=*/false);

  if (shelf_->IsHorizontalAlignment() && base::i18n::IsRTL()) {
    nav_origin.set_x(shelf_->shelf_widget()->GetTargetBounds().size().width() -
                     nav_size.width());
  }
  target_bounds_ = gfx::Rect(nav_origin, nav_size);
  clip_rect_after_rtl_ = CalculateClipRectAfterRTL();
}

gfx::Rect ShelfNavigationWidget::GetTargetBounds() const {
  return target_bounds_;
}

void ShelfNavigationWidget::UpdateLayout(bool animate) {
  const bool back_button_shown =
      IsBackButtonShown(shelf_->IsHorizontalAlignment());
  const bool home_button_shown = IsHomeButtonShown();

  const ShelfLayoutManager* layout_manager = shelf_->shelf_layout_manager();
  // Having a window which is visible but does not have an opacity is an
  // illegal state. Also, never show this widget outside of an active session.
  if (layout_manager->GetOpacity() &&
      layout_manager->is_active_session_state()) {
    ShowInactive();
  } else {
    Hide();
  }

  // If the widget is currently active, and all the buttons will be hidden,
  // focus out to the status area (the widget's focus manager does not properly
  // handle the case where the widget does not have another view to focus - it
  // would clear the focus, and hit a DCHECK trying to cycle focus within the
  // widget).
  if (IsActive() && !back_button_shown && !home_button_shown) {
    Shelf::ForWindow(GetNativeWindow())
        ->shelf_focus_cycler()
        ->FocusOut(true /*reverse*/, SourceView::kShelfNavigationView);
  }

  // Use the same duration for all parts of the upcoming animation.
  const base::TimeDelta animation_duration =
      animate ? ShelfConfig::Get()->shelf_animation_duration()
              : base::Milliseconds(0);

  const HotseatState target_hotseat_state =
      layout_manager->CalculateHotseatState(layout_manager->visibility_state(),
                                            layout_manager->auto_hide_state());

  const bool update_opacity = !animate || GetLayer()->GetTargetOpacity() !=
                                              layout_manager->GetOpacity();
  const bool update_bounds =
      !animate || GetLayer()->GetTargetBounds() != target_bounds_;

  if (update_opacity || update_bounds) {
    ui::ScopedLayerAnimationSettings nav_animation_setter(
        GetNativeView()->layer()->GetAnimator());
    nav_animation_setter.SetTransitionDuration(animation_duration);
    nav_animation_setter.SetTweenType(gfx::Tween::EASE_OUT);
    nav_animation_setter.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);

    absl::optional<ui::AnimationThroughputReporter> reporter;
    if (animate) {
      reporter.emplace(nav_animation_setter.GetAnimator(),
                       shelf_->GetNavigationWidgetAnimationReportCallback(
                           target_hotseat_state));
    }
    if (update_opacity)
      GetLayer()->SetOpacity(layout_manager->GetOpacity());
    if (update_bounds)
      SetBounds(target_bounds_);
  }

  if (update_bounds)
    GetLayer()->SetClipRect(clip_rect_after_rtl_);

  views::View* const back_button = delegate_->back_button();
  UpdateButtonVisibility(back_button, back_button_shown, animate,
                         back_button_metrics_reporter_.get(),
                         target_hotseat_state);

  views::View* const home_button = delegate_->home_button();
  UpdateButtonVisibility(home_button, home_button_shown, animate,
                         home_button_metrics_reporter_.get(),
                         target_hotseat_state);

  if (back_button_shown) {
    // TODO(https://crbug.com/1058205): Test this behavior.
    gfx::Transform rotation;
    // If the IME virtual keyboard is visible, rotate the back button downwards,
    // this indicates it can be used to close the keyboard.
    if (ShelfConfig::Get()->is_virtual_keyboard_shown())
      rotation.Rotate(270.0);

    delegate_->back_button()->layer()->SetTransform(TransformAboutPivot(
        delegate_->back_button()->GetCenterPoint(), rotation));
  }

  gfx::Rect home_button_bounds =
      back_button_shown ? GetSecondButtonBounds()
                        : GetFirstButtonBounds(shelf_->IsHorizontalAlignment());

  if (animate) {
    if (bounds_animator_->GetTargetBounds(home_button) != home_button_bounds) {
      bounds_animator_->SetAnimationDuration(
          ui::ScopedAnimationDurationScaleMode::duration_multiplier() *
          animation_duration);
      bounds_animator_->AnimateViewTo(
          home_button, home_button_bounds,
          std::make_unique<BoundsAnimationReporter>(
              home_button, home_button_metrics_reporter_->GetReportCallback(
                               target_hotseat_state)));
    }
  } else {
    bounds_animator_->Cancel();
    home_button->SetBoundsRect(home_button_bounds);
  }

  back_button->SetBoundsRect(
      GetFirstButtonBounds(shelf_->IsHorizontalAlignment()));

  delegate_->UpdateOpaqueBackground();
}

void ShelfNavigationWidget::UpdateTargetBoundsForGesture(int shelf_position) {
  if (shelf_->IsHorizontalAlignment())
    target_bounds_.set_y(shelf_position);
  else
    target_bounds_.set_x(shelf_position);
}

gfx::Rect ShelfNavigationWidget::GetVisibleBounds() const {
  return gfx::Rect(target_bounds_.origin(), clip_rect_after_rtl_.size());
}

void ShelfNavigationWidget::PrepareForGettingFocus(bool last_element) {
  SetDefaultLastFocusableChild(last_element);

  // The native view of the navigation widget is not activatable when its target
  // visibility is false. So show the widget before setting focus.

  // Layer opacity should be set first. Because it is not allowed that a window
  // is visible but the layers alpha is fully transparent.
  ui::Layer* layer = GetLayer();
  if (layer->GetTargetOpacity() != 1.f)
    GetLayer()->SetOpacity(1.f);
  if (!IsVisible())
    ShowInactive();
}

void ShelfNavigationWidget::HandleLocaleChange() {
  delegate_->home_button()->HandleLocaleChange();
  delegate_->back_button()->HandleLocaleChange();
}

void ShelfNavigationWidget::UpdateButtonVisibility(
    views::View* button,
    bool visible,
    bool animate,
    NavigationButtonAnimationMetricsReporter* metrics_reporter,
    HotseatState target_hotseat_state) {
  if (animate && button->layer()->GetTargetOpacity() == visible)
    return;

  // Update visibility immediately only if making the button visible. When
  // hiding the button, the visibility will be updated when the animations
  // complete (by AnimationObserverToHideView).
  if (visible)
    button->SetVisible(true);
  button->SetFocusBehavior(visible ? views::View::FocusBehavior::ALWAYS
                                   : views::View::FocusBehavior::NEVER);

  ui::ScopedLayerAnimationSettings opacity_settings(
      button->layer()->GetAnimator());
  opacity_settings.SetTransitionDuration(
      animate ? kButtonOpacityAnimationDuration : base::TimeDelta());
  opacity_settings.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);

  absl::optional<ui::AnimationThroughputReporter> reporter;
  if (animate) {
    reporter.emplace(opacity_settings.GetAnimator(),
                     metrics_reporter->GetReportCallback(target_hotseat_state));
  }

  if (!visible)
    opacity_settings.AddObserver(new AnimationObserverToHideView(button));

  button->layer()->SetOpacity(visible ? 1.0f : 0.0f);
}

gfx::Rect ShelfNavigationWidget::CalculateClipRectAfterRTL() const {
  gfx::Rect bounds_before_rtl;
  if (Shell::Get()->IsInTabletMode()) {
    bounds_before_rtl =
        gfx::Rect(CalculateIdealSize(/*only_visible_area=*/true));
  } else {
    bounds_before_rtl = gfx::Rect(target_bounds_.size());
  }

  return GetRootView()->GetMirroredRect(bounds_before_rtl);
}

gfx::Size ShelfNavigationWidget::CalculateIdealSize(
    bool only_visible_area) const {
  if (!ShelfConfig::Get()->shelf_controls_shown())
    return gfx::Size();

  int control_button_number;
  if (Shell::Get()->IsInTabletMode() && !only_visible_area) {
    // There are home button and back button. So the maximum is 2.
    control_button_number = 2;
  } else {
    control_button_number = CalculateButtonCount();
  }

  const int control_size = ShelfConfig::Get()->control_size();
  int controls_space =
      control_button_number * control_size +
      (control_button_number - 1) * ShelfConfig::Get()->button_spacing();
  const int major_axis_spacing =
      2 * ShelfConfig::Get()->control_button_edge_spacing(
              shelf_->IsHorizontalAlignment());

  const int major_axis_length = controls_space + major_axis_spacing;

  // Calculate |minor_axis_spacing|.
  int minor_axis_spacing;
  if (only_visible_area) {
    minor_axis_spacing = 2 * ShelfConfig::Get()->control_button_edge_spacing(
                                 !shelf_->IsHorizontalAlignment());
  } else {
    // When calculating the minor axis length of the navigation widget, use
    // the larger edge spacing between the home launcher state and the in-app
    // state. It ensures that the widget' size is constant during the transition
    // between these two states.

    DCHECK_GT(ShelfConfig::Get()->system_shelf_size(),
              ShelfConfig::Get()->in_app_shelf_size());
    minor_axis_spacing = ShelfConfig::Get()->system_shelf_size() - control_size;
  }

  const int minor_axis_length = control_size + minor_axis_spacing;

  return shelf_->IsHorizontalAlignment()
             ? gfx::Size(major_axis_length, minor_axis_length)
             : gfx::Size(minor_axis_length, major_axis_length);
}

int ShelfNavigationWidget::CalculateButtonCount() const {
  return (IsBackButtonShown(shelf_->IsHorizontalAlignment()) ? 1 : 0) +
         (IsHomeButtonShown() ? 1 : 0);
}

}  // namespace ash
