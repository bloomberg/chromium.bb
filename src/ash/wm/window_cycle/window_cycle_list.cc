// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_cycle/window_cycle_list.h"

#include <map>
#include <string>
#include <utility>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/frame_throttler/frame_throttling_controller.h"
#include "ash/public/cpp/metrics_util.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/default_colors.h"
#include "ash/wm/gestures/wm_fling_handler.h"
#include "ash/wm/window_cycle/window_cycle_tab_slider.h"
#include "ash/wm/window_cycle/window_cycle_tab_slider_button.h"
#include "ash/wm/window_mini_view.h"
#include "ash/wm/window_preview_view.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/ranges.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/aura/scoped_window_targeter.h"
#include "ui/aura/window.h"
#include "ui/aura/window_targeter.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/animation_throughput_reporter.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/window_animations.h"

namespace ash {

namespace {

bool g_disable_initial_delay = false;

// Shield rounded corner radius
constexpr gfx::RoundedCornersF kBackgroundCornerRadius{16.f};

// Shield horizontal inset.
constexpr int kBackgroundHorizontalInsetDp = 8;

// Shield background blur sigma.
constexpr float kBackgroundBlurSigma =
    static_cast<float>(AshColorProvider::LayerBlurSigma::kBlurDefault);

// Quality of the shield background blur.
constexpr float kBackgroundBlurQuality = 0.33f;

// All previews are the same height (this is achieved via a combination of
// scaling and padding).
constexpr int kFixedPreviewHeightDp = 256;

// The min and max width for preview size are in relation to the fixed height.
constexpr int kMinPreviewWidthDp = kFixedPreviewHeightDp / 2;
constexpr int kMaxPreviewWidthDp = kFixedPreviewHeightDp * 2;

// Vertical padding between the alt-tab bandshield and the window previews.
constexpr int kInsideBorderVerticalPaddingDp = 60;

// Padding between the alt-tab bandshield and the tab slider container.
constexpr int kMirrorContainerVerticalPaddingDp = 24;

// Padding between the window previews within the alt-tab bandshield.
constexpr int kBetweenChildPaddingDp = 10;

// Padding between the tab slider button and the tab slider container.
constexpr int kTabSliderContainerVerticalPaddingDp = 32;

// The font size of "No recent items" string when there's no window in the
// window cycle list.
constexpr int kNoRecentItemsLabelFontSizeDp = 14;

// The UMA histogram that logs smoothness of the fade-in animation.
constexpr char kShowAnimationSmoothness[] =
    "Ash.WindowCycleView.AnimationSmoothness.Show";
// The UMA histogram that logs smoothness of the window container animation.
constexpr char kContainerAnimationSmoothness[] =
    "Ash.WindowCycleView.AnimationSmoothness.Container";

// Delay before the UI fade in animation starts. This is so users can switch
// quickly between windows without bringing up the UI.
constexpr base::TimeDelta kShowDelayDuration =
    base::TimeDelta::FromMilliseconds(150);

// Duration of the window cycle UI fade in animation.
constexpr base::TimeDelta kFadeInDuration =
    base::TimeDelta::FromMilliseconds(100);

// Duration of the window cycle elements slide animation.
constexpr base::TimeDelta kContainerSlideDuration =
    base::TimeDelta::FromMilliseconds(120);

// Duration of the window cycle scale animation when a user toggles alt-tab
// modes.
constexpr base::TimeDelta kToggleModeScaleDuration =
    base::TimeDelta::FromMilliseconds(150);

// The alt-tab cycler widget is not activatable (except when ChromeVox is on),
// so we use WindowTargeter to send input events to the widget.
class CustomWindowTargeter : public aura::WindowTargeter {
 public:
  explicit CustomWindowTargeter(aura::Window* tab_cycler)
      : tab_cycler_(tab_cycler) {}
  CustomWindowTargeter(const CustomWindowTargeter&) = delete;
  CustomWindowTargeter& operator=(const CustomWindowTargeter&) = delete;
  ~CustomWindowTargeter() override = default;

  // aura::WindowTargeter:
  ui::EventTarget* FindTargetForEvent(ui::EventTarget* root,
                                      ui::Event* event) override {
    if (event->IsLocatedEvent())
      return aura::WindowTargeter::FindTargetForEvent(root, event);
    return tab_cycler_;
  }

 private:
  aura::Window* tab_cycler_;
};

gfx::Point ConvertEventToScreen(const ui::LocatedEvent* event) {
  aura::Window* target = static_cast<aura::Window*>(event->target());
  aura::Window* event_root = target->GetRootWindow();
  gfx::Point event_screen_point = event->root_location();
  wm::ConvertPointToScreen(event_root, &event_screen_point);
  return event_screen_point;
}

aura::Window* GetRootWindowForCycleView() {
  // Returns the root window for initializing cycle view if tablet mode is
  // enabled, or if the feature for alt-tab to follow the cursor is disabled.
  if (Shell::Get()->tablet_mode_controller()->InTabletMode() ||
      !features::DoWindowsFollowCursor()) {
    return Shell::GetRootWindowForNewWindows();
  }

  // Return the root window the cursor is currently on.
  return Shell::GetRootWindowForDisplayId(
      Shell::Get()->cursor_manager()->GetDisplay().id());
}

}  // namespace

// This view represents a single aura::Window by displaying a title and a
// thumbnail of the window's contents.
class WindowCycleItemView : public WindowMiniView {
 public:
  METADATA_HEADER(WindowCycleItemView);

  explicit WindowCycleItemView(aura::Window* window) : WindowMiniView(window) {
    SetFocusBehavior(FocusBehavior::ALWAYS);
    SetNotifyEnterExitOnChild(true);
  }
  WindowCycleItemView(const WindowCycleItemView&) = delete;
  WindowCycleItemView& operator=(const WindowCycleItemView&) = delete;
  ~WindowCycleItemView() override = default;

  // Shows the preview and icon. For performance reasons, these are not created
  // on construction. This should be called at most one time during the lifetime
  // of |this|.
  void ShowPreview() {
    DCHECK(!preview_view());

    UpdateIconView();
    SetShowPreview(/*show=*/true);
    UpdatePreviewRoundedCorners(/*show=*/true);
  }

  // views::View:
  void OnMouseEntered(const ui::MouseEvent& event) override {
    Shell::Get()->window_cycle_controller()->SetFocusedWindow(source_window());
  }

  bool OnMousePressed(const ui::MouseEvent& event) override {
    Shell::Get()->window_cycle_controller()->SetFocusedWindow(source_window());
    Shell::Get()->window_cycle_controller()->CompleteCycling();
    return true;
  }

 private:
  // WindowMiniView:
  gfx::Size GetPreviewViewSize() const override {
    // When the preview is not shown, do an estimate of the expected size.
    // |this| will not be visible anyways, and will get corrected once
    // ShowPreview() is called.
    if (!preview_view()) {
      gfx::SizeF source_size(source_window()->bounds().size());
      // Windows may have no size in tests.
      if (source_size.IsEmpty())
        return gfx::Size();
      const float aspect_ratio = source_size.width() / source_size.height();
      return gfx::Size(kFixedPreviewHeightDp * aspect_ratio,
                       kFixedPreviewHeightDp);
    }

    // Returns the size for the preview view, scaled to fit within the max
    // bounds. Scaling is always 1:1 and we only scale down, never up.
    gfx::Size preview_pref_size = preview_view()->GetPreferredSize();
    if (preview_pref_size.width() > kMaxPreviewWidthDp ||
        preview_pref_size.height() > kFixedPreviewHeightDp) {
      const float scale = std::min(
          kMaxPreviewWidthDp / static_cast<float>(preview_pref_size.width()),
          kFixedPreviewHeightDp /
              static_cast<float>(preview_pref_size.height()));
      preview_pref_size =
          gfx::ScaleToFlooredSize(preview_pref_size, scale, scale);
    }

    return preview_pref_size;
  }

  // views::View:
  void Layout() override {
    WindowMiniView::Layout();

    if (!preview_view())
      return;

    // Show the backdrop if the preview view does not take up all the bounds
    // allocated for it.
    gfx::Rect preview_max_bounds = GetContentsBounds();
    preview_max_bounds.Subtract(GetHeaderBounds());
    const gfx::Rect preview_area_bounds = preview_view()->bounds();
    SetBackdropVisibility(preview_max_bounds.size() !=
                          preview_area_bounds.size());
  }

  gfx::Size CalculatePreferredSize() const override {
    // Previews can range in width from half to double of
    // |kFixedPreviewHeightDp|. Padding will be added to the sides to achieve
    // this if the preview is too narrow.
    gfx::Size preview_size = GetPreviewViewSize();

    // All previews are the same height (this may add padding on top and
    // bottom).
    preview_size.set_height(kFixedPreviewHeightDp);

    // Previews should never be narrower than half or wider than double their
    // fixed height.
    preview_size.set_width(base::ClampToRange(
        preview_size.width(), kMinPreviewWidthDp, kMaxPreviewWidthDp));

    const int margin = GetInsets().width();
    preview_size.Enlarge(margin, margin + WindowMiniView::kHeaderHeightDp);
    return preview_size;
  }
};

BEGIN_METADATA(WindowCycleItemView, WindowMiniView)
END_METADATA

// A view that shows a collection of windows the user can tab through.
class WindowCycleView : public views::WidgetDelegateView,
                        public ui::ImplicitAnimationObserver {
 public:
  METADATA_HEADER(WindowCycleView);

  WindowCycleView(aura::Window* root_window,
                  const WindowCycleList::WindowList& windows)
      : root_window_(root_window) {
    const bool is_interactive_alt_tab_mode_allowed =
        Shell::Get()
            ->window_cycle_controller()
            ->IsInteractiveAltTabModeAllowed();

    DCHECK(!windows.empty() || is_interactive_alt_tab_mode_allowed);
    // Start the occlusion tracker pauser. It's used to increase smoothness for
    // the fade in but we also create windows here which may occlude other
    // windows.
    occlusion_tracker_pauser_ =
        std::make_unique<aura::WindowOcclusionTracker::ScopedPause>();

    // The layer for |this| is responsible for showing color, background blur
    // and fading in.
    SetPaintToLayer(ui::LAYER_SOLID_COLOR);
    ui::Layer* layer = this->layer();
    SkColor background_color = AshColorProvider::Get()->GetBaseLayerColor(
        AshColorProvider::BaseLayerType::kTransparent80);
    layer->SetColor(background_color);
    layer->SetBackgroundBlur(kBackgroundBlurSigma);
    layer->SetBackdropFilterQuality(kBackgroundBlurQuality);
    layer->SetName("WindowCycleView");
    layer->SetMasksToBounds(true);

    // |mirror_container_| may be larger than |this|. In this case, it will be
    // shifted along the x-axis when the user tabs through. It is a container
    // for the previews and has no rendered content.
    mirror_container_ = AddChildView(std::make_unique<views::View>());
    mirror_container_->SetPaintToLayer(ui::LAYER_NOT_DRAWN);
    mirror_container_->layer()->SetName("WindowCycleView/MirrorContainer");
    views::BoxLayout* layout =
        mirror_container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal,
            gfx::Insets(is_interactive_alt_tab_mode_allowed
                            ? kMirrorContainerVerticalPaddingDp
                            : kInsideBorderVerticalPaddingDp,
                        WindowCycleList::kInsideBorderHorizontalPaddingDp,
                        kInsideBorderVerticalPaddingDp,
                        WindowCycleList::kInsideBorderHorizontalPaddingDp),
            kBetweenChildPaddingDp));
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kStart);

    if (is_interactive_alt_tab_mode_allowed) {
      tab_slider_container_ =
          AddChildView(std::make_unique<WindowCycleTabSlider>());

      no_recent_items_label_ = AddChildView(std::make_unique<views::Label>(
          l10n_util::GetStringUTF16(IDS_ASH_OVERVIEW_NO_RECENT_ITEMS)));
      no_recent_items_label_->SetPaintToLayer();
      no_recent_items_label_->layer()->SetFillsBoundsOpaquely(false);
      no_recent_items_label_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
      no_recent_items_label_->SetVerticalAlignment(gfx::ALIGN_MIDDLE);

      no_recent_items_label_->SetEnabledColor(
          AshColorProvider::Get()->GetContentLayerColor(
              AshColorProvider::ContentLayerType::kIconColorSecondary));
      no_recent_items_label_->SetFontList(
          no_recent_items_label_->font_list()
              .DeriveWithSizeDelta(
                  kNoRecentItemsLabelFontSizeDp -
                  no_recent_items_label_->font_list().GetFontSize())
              .DeriveWithWeight(gfx::Font::Weight::NORMAL));
      no_recent_items_label_->SetVisible(windows.empty());
      no_recent_items_label_->SetPreferredSize(
          gfx::Size(tab_slider_container_->GetPreferredSize().width() +
                        2 * WindowCycleList::kInsideBorderHorizontalPaddingDp,
                    kFixedPreviewHeightDp + WindowMiniView::kHeaderHeightDp +
                        kMirrorContainerVerticalPaddingDp +
                        kInsideBorderVerticalPaddingDp + 8));
    }

    for (auto* window : windows) {
      // |mirror_container_| owns |view|. The |preview_view_| in |view| will
      // use trilinear filtering in InitLayerOwner().
      auto* view = mirror_container_->AddChildView(
          std::make_unique<WindowCycleItemView>(window));
      window_view_map_[window] = view;

      no_previews_set_.insert(view);
    }

    // The insets in the WindowCycleItemView are coming from its border, which
    // paints the focus ring around the view when it is highlighted. Exclude the
    // insets such that the spacing between the contents of the views rather
    // than the views themselves is |kBetweenChildPaddingDp|.
    const gfx::Insets cycle_item_insets =
        window_view_map_.begin()->second->GetInsets();
    layout->set_between_child_spacing(kBetweenChildPaddingDp -
                                      cycle_item_insets.width());
  }
  WindowCycleView(const WindowCycleView&) = delete;
  WindowCycleView& operator=(const WindowCycleView&) = delete;
  ~WindowCycleView() override = default;

  // Scales the window cycle view by scaling its clip rect. If the widget is
  // growing, the widget's bounds are set to |screen_bounds| immediately then
  // its clipping rect is scaled. If the widget is shrinking, the widget's
  // cliping rect is scaled first then the widget's bounds are set to
  // |screen_bounds| upon completion/interruption of the clipping rect's
  // animation.
  void ScaleCycleView(const gfx::Rect& screen_bounds) {
    auto* layer_animator = layer()->GetAnimator();
    if (layer_animator->is_animating()) {
      // There is an existing scaling animation occurring. To accurately get the
      // new bounds for the next layout, we must abort the ongoing animation so
      // |this| will set the previous bounds of the widget and clear the clip
      // rect.
      // TODO(chinsenj): We may not want to abort the animation and rather just
      // animate from the current position.
      layer_animator->AbortAllAnimations();
    }

    // |screen_bounds| is in screen coords so store it in local coordinates in
    // |new_bounds|.
    gfx::Rect old_bounds = GetLocalBounds();
    gfx::Rect new_bounds = gfx::Rect(screen_bounds.size());

    if (old_bounds == new_bounds)
      return;

    if (new_bounds.width() >= old_bounds.width()) {
      // In this case, the cycle view is growing. To achieve the scaling
      // animation we set the widget bounds immediately and scale the clipping
      // rect of |this|'s layer from where the |old_bounds| would be in the
      // new local coordinates.
      GetWidget()->SetBounds(screen_bounds);
      old_bounds +=
          gfx::Vector2d((new_bounds.width() - old_bounds.width()) / 2, 0);
    } else {
      // In this case, the cycle view is shrinking. To achieve the scaling
      // animation, we first scale the clipping rect and defer updating the
      // widget's bounds to when the animation is complete. If we instantly
      // laid out, then it wouldn't appear as though the background is
      // shrinking.
      new_bounds +=
          gfx::Vector2d((old_bounds.width() - new_bounds.width()) / 2, 0);
      defer_widget_bounds_update_ = true;
    }

    layer()->SetClipRect(old_bounds);
    ui::ScopedLayerAnimationSettings settings(layer_animator);
    settings.SetTransitionDuration(kToggleModeScaleDuration);
    settings.SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN_2);
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    settings.AddObserver(this);
    layer()->SetClipRect(new_bounds);
  }

  gfx::Rect GetTargetBounds() const {
    // The widget is sized clamped to the screen bounds. Its child, the mirror
    // container which is parent to all the previews may be larger than the
    // widget as some previews will be offscreen. In Layout() of |cycle_view_|
    // the mirror container will be slid back and forth depending on the target
    // window.
    gfx::Rect widget_rect = root_window_->GetBoundsInScreen();
    widget_rect.ClampToCenteredSize(GetPreferredSize());
    return widget_rect;
  }

  void UpdateWindows(const WindowCycleList::WindowList& windows) {
    const bool no_windows = windows.empty();
    const bool is_interactive_alt_tab_mode_allowed =
        Shell::Get()
            ->window_cycle_controller()
            ->IsInteractiveAltTabModeAllowed();

    if (is_interactive_alt_tab_mode_allowed) {
      DCHECK(no_recent_items_label_);
      no_recent_items_label_->SetVisible(no_windows);
    }

    if (no_windows)
      return;

    for (auto* window : windows) {
      auto* view = mirror_container_->AddChildView(
          std::make_unique<WindowCycleItemView>(window));
      window_view_map_[window] = view;

      no_previews_set_.insert(view);
    }

    // If there was an ongoing drag session, it's now been completed so reset
    // |horizontal_distance_dragged_|.
    horizontal_distance_dragged_ = 0.f;

    gfx::Rect widget_rect = GetTargetBounds();
    if (is_interactive_alt_tab_mode_allowed)
      ScaleCycleView(widget_rect);
    else
      GetWidget()->SetBounds(widget_rect);

    SetTargetWindow(windows[0]);
    ScrollToWindow(windows[0]);
  }

  void FadeInLayer() {
    DCHECK(GetWidget());

    layer()->SetOpacity(0.f);
    ui::ScopedLayerAnimationSettings settings(layer()->GetAnimator());
    settings.SetTransitionDuration(kFadeInDuration);
    settings.AddObserver(this);
    settings.CacheRenderSurface();
    ui::AnimationThroughputReporter reporter(
        settings.GetAnimator(),
        metrics_util::ForSmoothness(base::BindRepeating([](int smoothness) {
          UMA_HISTOGRAM_PERCENTAGE(kShowAnimationSmoothness, smoothness);
        })));

    layer()->SetOpacity(1.f);
  }

  void ScrollToWindow(aura::Window* target) {
    current_window_ = target;

    // If there was an ongoing drag session, it's now been completed so reset
    // |horizontal_distance_dragged_|.
    horizontal_distance_dragged_ = 0.f;

    if (GetWidget())
      Layout();
  }

  void SetTargetWindow(aura::Window* target) {
    // Hide the focus border of the previous target window and show the focus
    // border of the new one.
    if (target_window_) {
      auto target_it = window_view_map_.find(target_window_);
      if (target_it != window_view_map_.end())
        target_it->second->UpdateBorderState(/*show=*/false);
    }
    target_window_ = target;
    auto target_it = window_view_map_.find(target_window_);
    if (target_it != window_view_map_.end())
      target_it->second->UpdateBorderState(/*show=*/true);

    // Focus the target window if the user is not currently switching the mode
    // while ChromeVox is on.
    // During the mode switch, we prevent ChromeVox auto-announce the window
    // title from the focus and send our custom string to announce both window
    // title and the selected mode together
    // (see `WindowCycleController::OnModeChanged`).
    auto* a11y_controller = Shell::Get()->accessibility_controller();
    auto* window_cycle_controller = Shell::Get()->window_cycle_controller();
    const bool chromevox_enabled = a11y_controller->spoken_feedback().enabled();
    const bool is_switching_mode = window_cycle_controller->IsSwitchingMode();
    if (target_window_ && !(chromevox_enabled && is_switching_mode)) {
      if (GetWidget()) {
        window_view_map_[target_window_]->RequestFocus();
      } else {
        SetInitiallyFocusedView(window_view_map_[target_window_]);
        // When alt-tab mode selection is available, announce via ChromeVox the
        // current mode and the directional cue for mode switching.
        if (window_cycle_controller->IsInteractiveAltTabModeAllowed()) {
          a11y_controller->TriggerAccessibilityAlertWithMessage(
              l10n_util::GetStringUTF8(
                  window_cycle_controller->IsAltTabPerActiveDesk()
                      ? IDS_ASH_ALT_TAB_FOCUS_CURRENT_DESK_MODE
                      : IDS_ASH_ALT_TAB_FOCUS_ALL_DESKS_MODE));
        }
      }
    }
  }

  void HandleWindowDestruction(aura::Window* destroying_window,
                               aura::Window* new_target) {
    auto view_iter = window_view_map_.find(destroying_window);
    WindowCycleItemView* preview = view_iter->second;
    views::View* parent = preview->parent();
    DCHECK_EQ(mirror_container_, parent);
    window_view_map_.erase(view_iter);
    no_previews_set_.erase(preview);
    delete preview;

    // With one of its children now gone, we must re-layout
    // |mirror_container_|. This must happen before ScrollToWindow() to make
    // sure our own Layout() works correctly when it's calculating highlight
    // bounds.
    parent->Layout();
    SetTargetWindow(new_target);
    ScrollToWindow(new_target);
  }

  void DestroyContents() {
    is_destroying_ = true;

    window_view_map_.clear();
    no_previews_set_.clear();
    target_window_ = nullptr;
    current_window_ = nullptr;
    defer_widget_bounds_update_ = false;
    RemoveAllChildViews(true);
    OnFlingEnd();
  }

  void Drag(float delta_x) {
    horizontal_distance_dragged_ += delta_x;
    Layout();
  }

  void StartFling(float velocity_x) {
    fling_handler_ = std::make_unique<WmFlingHandler>(
        gfx::Vector2dF(velocity_x, 0),
        GetWidget()->GetNativeWindow()->GetRootWindow(),
        base::BindRepeating(&WindowCycleView::OnFlingStep,
                            base::Unretained(this)),
        base::BindRepeating(&WindowCycleView::OnFlingEnd,
                            base::Unretained(this)));
  }

  bool OnFlingStep(float offset) {
    DCHECK(fling_handler_);
    horizontal_distance_dragged_ += offset;
    Layout();
    return true;
  }

  void OnFlingEnd() { fling_handler_.reset(); }

  // views::WidgetDelegateView:
  gfx::Size CalculatePreferredSize() const override {
    gfx::Size size = GetContentContainerBounds().size();
    // |mirror_container_| can have window list that overflow out of the
    // screen, but the window cycle view with a bandshield, cropping the
    // overflow window list, should remain within the specified horizontal
    // insets of the screen width.
    const int max_width = root_window_->GetBoundsInScreen().size().width() -
                          2 * kBackgroundHorizontalInsetDp;
    size.set_width(std::min(size.width(), max_width));
    if (Shell::Get()
            ->window_cycle_controller()
            ->IsInteractiveAltTabModeAllowed()) {
      DCHECK(tab_slider_container_);
      // |mirror_container_| can have window list with width smaller the tab
      // slider's width. The padding should be 64px from the tab slider.
      const int min_width =
          tab_slider_container_->GetPreferredSize().width() +
          2 * WindowCycleList::kInsideBorderHorizontalPaddingDp;
      size.set_width(std::max(size.width(), min_width));
      size.Enlarge(0, tab_slider_container_->GetPreferredSize().height() +
                          kTabSliderContainerVerticalPaddingDp);
    }
    return size;
  }

  void Layout() override {
    if (is_destroying_)
      return;

    const bool is_interactive_alt_tab_mode_allowed =
        Shell::Get()
            ->window_cycle_controller()
            ->IsInteractiveAltTabModeAllowed();
    if (bounds().IsEmpty() || (!is_interactive_alt_tab_mode_allowed &&
                               (!target_window_ || !current_window_))) {
      return;
    }

    const bool first_layout = mirror_container_->bounds().IsEmpty();
    // If |mirror_container_| has not yet been laid out, we must lay it and
    // its descendants out so that the calculations based on |target_view|
    // work properly.
    if (first_layout) {
      mirror_container_->SizeToPreferredSize();
      layer()->SetRoundedCornerRadius(kBackgroundCornerRadius);
    }

    gfx::RectF target_bounds;
    if (current_window_ || !is_interactive_alt_tab_mode_allowed) {
      views::View* target_view = window_view_map_[current_window_];
      target_bounds = gfx::RectF(target_view->GetLocalBounds());
      views::View::ConvertRectToTarget(target_view, mirror_container_,
                                       &target_bounds);
    } else {
      DCHECK(no_recent_items_label_);
      target_bounds = gfx::RectF(no_recent_items_label_->bounds());
    }

    // Content container represents the mirror container with >=1 windows or
    // no-recent-items label when there is no window to be shown.
    gfx::Rect content_container_bounds = GetContentContainerBounds();

    // Case one: the container is narrower than the screen. Center the
    // container.
    int x_offset = (width() - content_container_bounds.width()) / 2;
    if (x_offset < 0) {
      // Case two: the container is wider than the screen. Center the target
      // view by moving the list just enough to ensure the target view is in
      // the center. Additionally, offset by however much the user has dragged.
      x_offset = width() / 2 - mirror_container_->GetMirroredXInView(
                                   target_bounds.CenterPoint().x());

      // However, the container must span the screen, i.e. the maximum x is 0
      // and the minimum for its right boundary is the width of the screen.
      int minimum_x = width() - content_container_bounds.width();
      x_offset = base::ClampToRange(x_offset, minimum_x, 0);

      // If the user has dragged, offset the container based on how much they
      // have dragged. Cap |horizontal_distance_dragged_| based on the available
      // distance from the container to the left and right boundaries.
      float clamped_horizontal_distance_dragged =
          base::ClampToRange(horizontal_distance_dragged_,
                             static_cast<float>(minimum_x - x_offset),
                             static_cast<float>(-x_offset));
      if (horizontal_distance_dragged_ != clamped_horizontal_distance_dragged)
        OnFlingEnd();

      horizontal_distance_dragged_ = clamped_horizontal_distance_dragged;
      x_offset += horizontal_distance_dragged_;
    }
    content_container_bounds.set_x(x_offset);

    // Layout a tab slider if Bento is enabled.
    if (is_interactive_alt_tab_mode_allowed) {
      // TODO(crbug.com/1216238): Change these back to DCHECKs once the bug is
      // resolved.
      CHECK(tab_slider_container_);
      CHECK(no_recent_items_label_);
      // Layout the tab slider.
      const gfx::Size tab_slider_size =
          tab_slider_container_->GetPreferredSize();
      const gfx::Rect tab_slider_mirror_container_bounds(
          (width() - tab_slider_size.width()) / 2,
          kTabSliderContainerVerticalPaddingDp, tab_slider_size.width(),
          tab_slider_size.height());
      tab_slider_container_->SetBoundsRect(tab_slider_mirror_container_bounds);

      // Move window cycle container down.
      content_container_bounds.set_y(tab_slider_container_->y() +
                                     tab_slider_container_->height());

      // Unlike the bounds of scrollable mirror container, the bounds of label
      // should not overflow out of the screen.
      const gfx::Rect no_recent_item_bounds_(
          std::max(0, content_container_bounds.x()),
          content_container_bounds.y(),
          std::min(width(), content_container_bounds.width()),
          content_container_bounds.height());
      no_recent_items_label_->SetBoundsRect(no_recent_item_bounds_);
    }

    // Enable animations only after the first Layout() pass. If |this| is
    // animating or |defer_widget_bounds_update_|, don't animate as well since
    // the cycle view is already being animated or just finished animating for
    // mode switch.
    std::unique_ptr<ui::ScopedLayerAnimationSettings> settings;
    absl::optional<ui::AnimationThroughputReporter> reporter;
    if (!first_layout && !this->layer()->GetAnimator()->is_animating() &&
        !defer_widget_bounds_update_) {
      settings = std::make_unique<ui::ScopedLayerAnimationSettings>(
          mirror_container_->layer()->GetAnimator());
      settings->SetTransitionDuration(kContainerSlideDuration);
      reporter.emplace(
          settings->GetAnimator(),
          metrics_util::ForSmoothness(base::BindRepeating([](int smoothness) {
            // Reports animation metrics when the mirror container, which holds
            // all the preview views slides along the x-axis. This can happen
            // while tabbing through windows, if the window cycle ui spans the
            // length of the display.
            UMA_HISTOGRAM_PERCENTAGE(kContainerAnimationSmoothness, smoothness);
          })));
    }
    mirror_container_->SetBoundsRect(content_container_bounds);

    // If an element in |no_previews_set_| is no onscreen (its bounds in |this|
    // coordinates intersects |this|), create the rest of its elements and
    // remove it from the set.
    const gfx::RectF local_bounds(GetLocalBounds());
    for (auto it = no_previews_set_.begin(); it != no_previews_set_.end();) {
      WindowCycleItemView* view = *it;
      gfx::RectF bounds(view->GetLocalBounds());
      views::View::ConvertRectToTarget(view, this, &bounds);
      if (bounds.Intersects(local_bounds)) {
        view->ShowPreview();
        it = no_previews_set_.erase(it);
      } else {
        ++it;
      }
    }
  }

  aura::Window* GetTargetWindow() { return target_window_; }

  void SetFocusTabSlider(bool focus) { tab_slider_container_->SetFocus(focus); }

  bool IsTabSliderFocused() {
    DCHECK(tab_slider_container_);
    return tab_slider_container_->is_focused();
  }

  aura::Window* GetWindowAtPoint(const gfx::Point& screen_point) {
    for (const auto& entry : window_view_map_) {
      if (entry.second->GetBoundsInScreen().Contains(screen_point))
        return entry.first;
    }
    return nullptr;
  }

  const views::View::Views& GetPreviewViewsForTesting() const {
    return mirror_container_->children();
  }

  const views::View::Views& GetTabSliderButtonsForTesting() const {
    if (!tab_slider_container_) {
      static const views::View::Views empty;
      return empty;
    }
    return tab_slider_container_->GetTabSliderButtonsForTesting();
  }

  const views::Label* GetNoRecentItemsLabelForTesting() const {
    return no_recent_items_label_;
  }

  const aura::Window* GetTargetWindowForTesting() const {
    return target_window_;
  }

  bool IsCycleViewAnimatingForTesting() {
    return layer()->GetAnimator()->is_animating();
  }

  void OnModePrefsChanged() {
    if (tab_slider_container_)
      tab_slider_container_->OnModePrefsChanged();
  }

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override {
    occlusion_tracker_pauser_.reset();
    this->layer()->SetClipRect(gfx::Rect());
    if (defer_widget_bounds_update_) {
      // This triggers a Layout() so reset |defer_widget_bounds_update_| after
      // calling SetBounds() to prevent the mirror container from animating.
      GetWidget()->SetBounds(GetTargetBounds());
      defer_widget_bounds_update_ = false;
    }
  }

 private:
  // Returns a bound of alt-tab content container, which represents the mirror
  // container when there is at least one window and represents no-recent-items
  // label when there is no window to be shown.
  gfx::Rect GetContentContainerBounds() const {
    const bool empty_mirror_container = mirror_container_->children().empty();
    if (empty_mirror_container && no_recent_items_label_)
      return gfx::Rect(no_recent_items_label_->GetPreferredSize());
    return gfx::Rect(mirror_container_->GetPreferredSize());
  }

  aura::Window* const root_window_;
  std::map<aura::Window*, WindowCycleItemView*> window_view_map_;
  views::View* mirror_container_ = nullptr;

  // Used when the widget bounds update should be deferred during the cycle
  // view's scaling animation..
  bool defer_widget_bounds_update_ = false;

  // Tab slider and no recent items are only used when Bento is enabled.
  WindowCycleTabSlider* tab_slider_container_ = nullptr;
  views::Label* no_recent_items_label_ = nullptr;

  // The |target_window_| is the window that has the focus ring. When the user
  // completes cycling the |target_window_| is activated.
  aura::Window* target_window_ = nullptr;

  // The |current_window_| is the window that the window cycle list uses to
  // determine the layout and positioning of the list's items. If this window's
  // preview can equally divide the list it is centered, otherwise it is
  // off-center.
  aura::Window* current_window_ = nullptr;

  // Set which contains items which have been created but have some of their
  // performance heavy elements not created yet. These elements will be created
  // once onscreen to improve fade in performance, then removed from this set.
  base::flat_set<WindowCycleItemView*> no_previews_set_;

  // Used for preventng occlusion state computations for the duration of the
  // fade in animation.
  std::unique_ptr<aura::WindowOcclusionTracker::ScopedPause>
      occlusion_tracker_pauser_;

  // Tracks the distance that a user has dragged, offsetting the
  // |mirror_container_|. This should be reset only when a user cycles the
  // window cycle list or when the user switches alt-tab modes.
  float horizontal_distance_dragged_ = 0.f;

  // Fling handler of the current active fling. Nullptr while a fling is not
  // active.
  std::unique_ptr<WmFlingHandler> fling_handler_;

  // True once `DestroyContents` is called. Used to prevent `Layout` from being
  // called once all the child views have been removed. See
  // https://crbug.com/1223302 for more details.
  bool is_destroying_ = false;
};

BEGIN_METADATA(WindowCycleView, views::WidgetDelegateView)
END_METADATA

WindowCycleList::WindowCycleList(const WindowList& windows)
    : windows_(windows) {
  if (!ShouldShowUi())
    Shell::Get()->mru_window_tracker()->SetIgnoreActivations(true);

  active_window_before_window_cycle_ = window_util::GetActiveWindow();

  for (auto* window : windows_)
    window->AddObserver(this);

  if (ShouldShowUi()) {
    // Disable the tab scrubber so three finger scrolling doesn't scrub tabs as
    // well.
    Shell::Get()->shell_delegate()->SetTabScrubberEnabled(false);

    if (g_disable_initial_delay) {
      InitWindowCycleView();
    } else {
      show_ui_timer_.Start(FROM_HERE, kShowDelayDuration, this,
                           &WindowCycleList::InitWindowCycleView);
    }
  }
}

WindowCycleList::~WindowCycleList() {
  if (!ShouldShowUi())
    Shell::Get()->mru_window_tracker()->SetIgnoreActivations(false);

  Shell::Get()->shell_delegate()->SetTabScrubberEnabled(true);

  for (auto* window : windows_)
    window->RemoveObserver(this);

  if (cycle_ui_widget_)
    cycle_ui_widget_->Close();

  // Store the target window before |cycle_view_| is destroyed.
  aura::Window* target_window = nullptr;

  // |this| is responsible for notifying |cycle_view_| when windows are
  // destroyed. Since |this| is going away, clobber |cycle_view_|. Otherwise
  // there will be a race where a window closes after now but before the
  // Widget::Close() call above actually destroys |cycle_view_|. See
  // crbug.com/681207
  if (cycle_view_) {
    target_window = cycle_view_->GetTargetWindow();
    cycle_view_->DestroyContents();
  }

  // While the cycler widget is shown, the windows listed in the cycler is
  // marked as force-visible and don't contribute to occlusion. In order to
  // work occlusion calculation properly, we need to activate a window after
  // the widget has been destroyed. See b/138914552.
  if (!windows_.empty() && user_did_accept_) {
    if (!target_window)
      target_window = windows_[current_index_];
    SelectWindow(target_window);
  }
  Shell::Get()->frame_throttling_controller()->EndThrottling();
}

aura::Window* WindowCycleList::GetTargetWindow() {
  return cycle_view_->GetTargetWindow();
}

void WindowCycleList::ReplaceWindows(const WindowList& windows) {
  RemoveAllWindows();
  windows_ = windows;

  for (auto* new_window : windows_)
    new_window->AddObserver(this);

  if (cycle_view_)
    cycle_view_->UpdateWindows(windows_);
}

void WindowCycleList::Step(
    WindowCycleController::WindowCyclingDirection direction,
    bool starting_alt_tab_or_switching_mode) {
  if (windows_.empty())
    return;

  // If the position of the window cycle list is out-of-sync with the currently
  // selected item, scroll to the selected item and then step.
  if (cycle_view_) {
    aura::Window* selected_window = cycle_view_->GetTargetWindow();
    if (selected_window)
      Scroll(GetIndexOfWindow(selected_window) - current_index_);
  }

  int offset =
      direction == WindowCycleController::WindowCyclingDirection::kForward ? 1
                                                                           : -1;
  // When the window highlight should be reset and the first window in the MRU
  // cycle list is not the latest active one before entering alt-tab, highlight
  // it instead of the second window. This occurs when the user is in overview
  // mode, all windows are minimized, or all windows are in other desks.
  //
  // Note: Simply checking the active status of the first window won't work
  // because when the ChromeVox is enabled, the widget is activatable, so the
  // first window in MRU becomes inactive.
  if (starting_alt_tab_or_switching_mode &&
      direction == WindowCycleController::WindowCyclingDirection::kForward &&
      active_window_before_window_cycle_ != windows_[0]) {
    offset = 0;
    current_index_ = 0;
  }

  SetFocusedWindow(windows_[GetOffsettedWindowIndex(offset)]);
  Scroll(offset);
}

void WindowCycleList::Drag(float delta_x) {
  DCHECK(cycle_view_);
  cycle_view_->Drag(delta_x);
}

void WindowCycleList::StartFling(float velocity_x) {
  DCHECK(cycle_view_);
  cycle_view_->StartFling(velocity_x);
}

void WindowCycleList::SetFocusedWindow(aura::Window* window) {
  if (windows_.empty())
    return;

  if (ShouldShowUi() && cycle_view_)
    cycle_view_->SetTargetWindow(windows_[GetIndexOfWindow(window)]);
}

void WindowCycleList::SetFocusTabSlider(bool focus) {
  DCHECK(cycle_view_);
  cycle_view_->SetFocusTabSlider(focus);
}

bool WindowCycleList::IsTabSliderFocused() {
  DCHECK(cycle_view_);
  return cycle_view_->IsTabSliderFocused();
}

bool WindowCycleList::IsEventInCycleView(const ui::LocatedEvent* event) {
  return cycle_view_ &&
         cycle_view_->GetBoundsInScreen().Contains(ConvertEventToScreen(event));
}

aura::Window* WindowCycleList::GetWindowAtPoint(const ui::LocatedEvent* event) {
  return cycle_view_
             ? cycle_view_->GetWindowAtPoint(ConvertEventToScreen(event))
             : nullptr;
}

bool WindowCycleList::ShouldShowUi() {
  // Show alt-tab when there are at least two windows to pick from alt-tab, or
  // when there is at least a window to switch to by switching to the different
  // mode.
  if (!Shell::Get()
           ->window_cycle_controller()
           ->IsInteractiveAltTabModeAllowed()) {
    return windows_.size() > 1u;
  }

  int total_window_in_all_desks = GetNumberOfWindowsAllDesks();
  return windows_.size() > 1u ||
         (windows_.size() <= 1u &&
          static_cast<size_t>(total_window_in_all_desks) > windows_.size());
}

void WindowCycleList::OnModePrefsChanged() {
  if (cycle_view_)
    cycle_view_->OnModePrefsChanged();
}

// static
void WindowCycleList::DisableInitialDelayForTesting() {
  g_disable_initial_delay = true;
}

void WindowCycleList::OnWindowDestroying(aura::Window* window) {
  window->RemoveObserver(this);

  WindowList::iterator i = std::find(windows_.begin(), windows_.end(), window);
  // TODO(oshima): Change this back to DCHECK once crbug.com/483491 is fixed.
  CHECK(i != windows_.end());
  int removed_index = static_cast<int>(i - windows_.begin());
  windows_.erase(i);
  if (current_index_ > removed_index ||
      current_index_ == static_cast<int>(windows_.size())) {
    current_index_--;
  }

  // Reset |active_window_before_window_cycle_| to avoid a dangling pointer.
  if (window == active_window_before_window_cycle_)
    active_window_before_window_cycle_ = nullptr;

  if (cycle_view_) {
    auto* new_target_window =
        windows_.empty() ? nullptr : windows_[current_index_];
    cycle_view_->HandleWindowDestruction(window, new_target_window);

    if (windows_.empty()) {
      // This deletes us.
      Shell::Get()->window_cycle_controller()->CancelCycling();
      return;
    }
  }
}

void WindowCycleList::OnDisplayMetricsChanged(const display::Display& display,
                                              uint32_t changed_metrics) {
  if (cycle_ui_widget_ &&
      display.id() ==
          display::Screen::GetScreen()
              ->GetDisplayNearestWindow(cycle_ui_widget_->GetNativeWindow())
              .id() &&
      (changed_metrics & (DISPLAY_METRIC_BOUNDS | DISPLAY_METRIC_ROTATION))) {
    Shell::Get()->window_cycle_controller()->CancelCycling();
    // |this| is deleted.
    return;
  }
}

void WindowCycleList::RemoveAllWindows() {
  for (auto* window : windows_) {
    window->RemoveObserver(this);

    if (cycle_view_)
      cycle_view_->HandleWindowDestruction(window, nullptr);
  }

  windows_.clear();
  current_index_ = 0;
  window_selected_ = false;
}

void WindowCycleList::InitWindowCycleView() {
  if (cycle_view_)
    return;
  aura::Window* root_window = GetRootWindowForCycleView();
  cycle_view_ = new WindowCycleView(root_window, windows_);
  const bool is_interactive_alt_tab_mode_allowed =
      Shell::Get()->window_cycle_controller()->IsInteractiveAltTabModeAllowed();
  DCHECK(!windows_.empty() || is_interactive_alt_tab_mode_allowed);

  // Only set target window and scroll to the window when alt-tab is not empty.
  if (!windows_.empty()) {
    DCHECK(static_cast<int>(windows_.size()) > current_index_);
    cycle_view_->SetTargetWindow(windows_[current_index_]);
    cycle_view_->ScrollToWindow(windows_[current_index_]);
  }

  // We need to activate the widget if ChromeVox is enabled as ChromeVox
  // relies on activation.
  const bool spoken_feedback_enabled =
      Shell::Get()->accessibility_controller()->spoken_feedback().enabled();

  views::Widget* widget = new views::Widget();
  views::Widget::InitParams params;
  params.delegate = cycle_view_;
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.layer_type = ui::LAYER_NOT_DRAWN;

  // Don't let the alt-tab cycler be activatable. This lets the currently
  // activated window continue to be in the foreground. This may affect
  // things such as video automatically pausing/playing.
  if (!spoken_feedback_enabled)
    params.activatable = views::Widget::InitParams::Activatable::kNo;
  params.accept_events = true;
  params.name = "WindowCycleList (Alt+Tab)";
  // TODO(estade): make sure nothing untoward happens when the lock screen
  // or a system modal dialog is shown.
  params.parent = root_window->GetChildById(kShellWindowId_OverlayContainer);
  params.bounds = cycle_view_->GetTargetBounds();

  widget->Init(std::move(params));
  widget->Show();
  cycle_view_->FadeInLayer();
  cycle_ui_widget_ = widget;

  // Since this window is not activated, grab events.
  if (!spoken_feedback_enabled) {
    window_targeter_ = std::make_unique<aura::ScopedWindowTargeter>(
        widget->GetNativeWindow()->GetRootWindow(),
        std::make_unique<CustomWindowTargeter>(widget->GetNativeWindow()));
  }
  // Close the app list, if it's open in clamshell mode.
  if (!Shell::Get()->tablet_mode_controller()->InTabletMode())
    Shell::Get()->app_list_controller()->DismissAppList();

  Shell::Get()->frame_throttling_controller()->StartThrottling(windows_);
}

void WindowCycleList::SelectWindow(aura::Window* window) {
  // If the list has only one window, the window can be selected twice (in
  // Scroll() and the destructor). This causes ARC PIP windows to be restored
  // twice, which leads to a wrong window state.
  if (window_selected_)
    return;

  if (window->GetProperty(kPipOriginalWindowKey)) {
    window_util::ExpandArcPipWindow();
  } else {
    window->Show();
    WindowState::Get(window)->Activate();
  }

  window_selected_ = true;
}

void WindowCycleList::Scroll(int offset) {
  if (windows_.size() == 1)
    SelectWindow(windows_[0]);

  if (!ShouldShowUi()) {
    // When there is only one window, we should give feedback to the user. If
    // the window is minimized, we should also show it.
    if (windows_.size() == 1)
      ::wm::AnimateWindow(windows_[0], ::wm::WINDOW_ANIMATION_TYPE_BOUNCE);
    return;
  }

  DCHECK(static_cast<size_t>(current_index_) < windows_.size());
  current_index_ = GetOffsettedWindowIndex(offset);

  if (current_index_ > 1)
    InitWindowCycleView();

  if (cycle_view_)
    cycle_view_->ScrollToWindow(windows_[current_index_]);
}

int WindowCycleList::GetOffsettedWindowIndex(int offset) const {
  DCHECK(!windows_.empty());

  const int offsetted_index =
      (current_index_ + offset + windows_.size()) % windows_.size();
  DCHECK(windows_[offsetted_index]);

  return offsetted_index;
}

int WindowCycleList::GetIndexOfWindow(aura::Window* window) const {
  auto target_window = std::find(windows_.begin(), windows_.end(), window);
  DCHECK(target_window != windows_.end());
  return std::distance(windows_.begin(), target_window);
}

int WindowCycleList::GetNumberOfWindowsAllDesks() const {
  // If alt-tab mode is not available, the alt-tab defaults to all-desks mode
  // and can obtain the number of all windows easily from `windows_.size()`.
  DCHECK(Shell::Get()
             ->window_cycle_controller()
             ->IsInteractiveAltTabModeAllowed());
  return Shell::Get()
      ->mru_window_tracker()
      ->BuildWindowForCycleWithPipList(kAllDesks)
      .size();
}

const views::View::Views& WindowCycleList::GetWindowCycleItemViewsForTesting()
    const {
  return cycle_view_->GetPreviewViewsForTesting();  // IN-TEST
}

const views::View::Views&
WindowCycleList::GetWindowCycleTabSliderButtonsForTesting() const {
  return cycle_view_->GetTabSliderButtonsForTesting();  // IN-TEST
}

const views::Label*
WindowCycleList::GetWindowCycleNoRecentItemsLabelForTesting() const {
  return cycle_view_->GetNoRecentItemsLabelForTesting();  // IN-TEST
}

const aura::Window* WindowCycleList::GetTargetWindowForTesting() const {
  return cycle_view_->GetTargetWindowForTesting();  // IN-TEST
}

bool WindowCycleList::IsCycleViewAnimatingForTesting() const {
  return cycle_view_->IsCycleViewAnimatingForTesting();  // IN-TEST
}

}  // namespace ash
