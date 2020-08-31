// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/animating_layout_manager.h"

#include <algorithm>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/stl_util.h"
#include "ui/gfx/animation/animation_container.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/layout/normalized_geometry.h"
#include "ui/views/view.h"

namespace views {

namespace {

// Returns the ChildLayout data for the child view in the proposed layout, or
// nullptr if not found.
const ChildLayout* FindChildViewInLayout(const ProposedLayout& layout,
                                         const View* view) {
  if (!view)
    return nullptr;

  // The number of children should be small enough that this is more efficient
  // than caching a lookup set.
  for (auto& child_layout : layout.child_layouts) {
    if (child_layout.child_view == view)
      return &child_layout;
  }
  return nullptr;
}

ChildLayout* FindChildViewInLayout(ProposedLayout* layout, const View* view) {
  return const_cast<ChildLayout*>(FindChildViewInLayout(*layout, view));
}

// Describes the type of fade, used by LayoutFadeInfo (see below).
enum class LayoutFadeType {
  // This view is fading in as part of the current animation.
  kFadingIn,
  // This view is fading out as part of the current animation.
  kFadingOut,
  // This view was fading as part of a previous animation that was interrupted
  // and redirected. No child views in the current animation should base their
  // position off of it.
  kContinuingFade
};

}  // namespace

// Holds data about a view that is fading in or out as part of an animation.
struct AnimatingLayoutManager::LayoutFadeInfo {
  // How the child view is fading.
  LayoutFadeType fade_type;
  // The child view which is fading.
  View* child_view = nullptr;
  // The view previous (leading side) to the fading view which is in both the
  // starting and target layout, or null if none.
  View* prev_view = nullptr;
  // The view next (trailing side) to the fading view which is in both the
  // starting and target layout, or null if none.
  View* next_view = nullptr;
  // The full-size bounds, normalized to the orientation of the layout manaer,
  // that |child_view| starts with, if fading out, or ends with, if fading in.
  NormalizedRect reference_bounds;
  // The offset from the end of |prev_view| and the start of |next_view|. Insets
  // may be negative if the views overlap.
  Inset1D offsets;
};

// Manages the animation and various callbacks from the animation system that
// are required to update the layout during animations.
class AnimatingLayoutManager::AnimationDelegate
    : public AnimationDelegateViews {
 public:
  explicit AnimationDelegate(AnimatingLayoutManager* layout_manager);
  ~AnimationDelegate() override = default;

  // Returns true after the host view is added to a widget or animation has been
  // enabled by a unit test.
  //
  // Before that, animation is not possible, so all changes to the host view
  // should result in the host view's layout being snapped directly to the
  // target layout.
  bool ready_to_animate() const { return ready_to_animate_; }

  // Pushes animation configuration (tween type, duration) through to the
  // animation itself.
  void UpdateAnimationParameters();

  // Starts the animation.
  void Animate();

  // Cancels and resets the current animation (if any).
  void Reset();

  // If the current layout is not yet ready to animate, transitions into the
  // ready-to-animate state, possibly resetting the current layout and
  // invalidating the host to make sure the layout is up to date.
  void MakeReadyForAnimation();

 private:
  // Observer used to watch for the host view being parented to a widget.
  class ViewWidgetObserver : public ViewObserver {
   public:
    explicit ViewWidgetObserver(AnimationDelegate* animation_delegate)
        : animation_delegate_(animation_delegate) {}

    void OnViewAddedToWidget(View* observed_view) override {
      animation_delegate_->MakeReadyForAnimation();
    }

    void OnViewIsDeleting(View* observed_view) override {
      if (animation_delegate_->scoped_observer_.IsObserving(observed_view))
        animation_delegate_->scoped_observer_.Remove(observed_view);
    }

   private:
    AnimationDelegate* const animation_delegate_;
  };
  friend class Observer;

  // AnimationDelegateViews:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

  bool ready_to_animate_ = false;
  bool resetting_animation_ = false;
  AnimatingLayoutManager* const target_layout_manager_;
  std::unique_ptr<gfx::SlideAnimation> animation_;
  ViewWidgetObserver view_widget_observer_{this};
  ScopedObserver<View, ViewObserver> scoped_observer_{&view_widget_observer_};
};

AnimatingLayoutManager::AnimationDelegate::AnimationDelegate(
    AnimatingLayoutManager* layout_manager)
    : AnimationDelegateViews(layout_manager->host_view()),
      target_layout_manager_(layout_manager),
      animation_(std::make_unique<gfx::SlideAnimation>(this)) {
  animation_->SetContainer(new gfx::AnimationContainer());
  View* const host_view = layout_manager->host_view();
  DCHECK(host_view);
  if (host_view->GetWidget())
    MakeReadyForAnimation();
  else
    scoped_observer_.Add(host_view);
  UpdateAnimationParameters();
}

void AnimatingLayoutManager::AnimationDelegate::UpdateAnimationParameters() {
  animation_->SetTweenType(target_layout_manager_->tween_type());
  animation_->SetSlideDuration(target_layout_manager_->animation_duration());
}

void AnimatingLayoutManager::AnimationDelegate::Animate() {
  DCHECK(ready_to_animate_);
  Reset();
  animation_->Show();
}

void AnimatingLayoutManager::AnimationDelegate::Reset() {
  if (!ready_to_animate_)
    return;
  base::AutoReset<bool> setter(&resetting_animation_, true);
  animation_->Reset();
}

void AnimatingLayoutManager::AnimationDelegate::MakeReadyForAnimation() {
  if (!ready_to_animate_) {
    target_layout_manager_->ResetLayout();
    ready_to_animate_ = true;
    if (scoped_observer_.IsObserving(target_layout_manager_->host_view()))
      scoped_observer_.Remove(target_layout_manager_->host_view());
  }
}

void AnimatingLayoutManager::AnimationDelegate::AnimationProgressed(
    const gfx::Animation* animation) {
  DCHECK(animation_.get() == animation);
  target_layout_manager_->AnimateTo(animation->GetCurrentValue());
}

void AnimatingLayoutManager::AnimationDelegate::AnimationCanceled(
    const gfx::Animation* animation) {
  AnimationEnded(animation);
}

void AnimatingLayoutManager::AnimationDelegate::AnimationEnded(
    const gfx::Animation* animation) {
  if (resetting_animation_)
    return;
  DCHECK(animation_.get() == animation);
  target_layout_manager_->AnimateTo(1.0);
}

// AnimatingLayoutManager:

AnimatingLayoutManager::AnimatingLayoutManager() = default;
AnimatingLayoutManager::~AnimatingLayoutManager() = default;

AnimatingLayoutManager& AnimatingLayoutManager::SetBoundsAnimationMode(
    BoundsAnimationMode bounds_animation_mode) {
  if (bounds_animation_mode_ != bounds_animation_mode) {
    bounds_animation_mode_ = bounds_animation_mode;
    ResetLayout();
  }
  return *this;
}

AnimatingLayoutManager& AnimatingLayoutManager::SetAnimationDuration(
    base::TimeDelta animation_duration) {
  DCHECK_GE(animation_duration, base::TimeDelta());
  animation_duration_ = animation_duration;
  if (animation_delegate_)
    animation_delegate_->UpdateAnimationParameters();
  return *this;
}

AnimatingLayoutManager& AnimatingLayoutManager::SetTweenType(
    gfx::Tween::Type tween_type) {
  tween_type_ = tween_type;
  if (animation_delegate_)
    animation_delegate_->UpdateAnimationParameters();
  return *this;
}

AnimatingLayoutManager& AnimatingLayoutManager::SetOrientation(
    LayoutOrientation orientation) {
  if (orientation_ != orientation) {
    orientation_ = orientation;
    ResetLayout();
  }
  return *this;
}

AnimatingLayoutManager& AnimatingLayoutManager::SetDefaultFadeMode(
    FadeInOutMode default_fade_mode) {
  default_fade_mode_ = default_fade_mode;
  return *this;
}

void AnimatingLayoutManager::ResetLayout() {
  if (!target_layout_manager())
    return;
  ResetLayoutToTargetSize();
  InvalidateHost(false);
}

void AnimatingLayoutManager::FadeOut(View* child_view) {
  DCHECK(child_view);
  DCHECK(child_view->parent());
  DCHECK_EQ(host_view(), child_view->parent());

  // If the view in question is already incapable of being visible, either:
  // 1. the view wasn't capable of being visible in the first place
  // 2. the view is already invisible because the layout has chosen to hide it
  // In either case, it is generally useful to recalculate the layout just in
  // case the caller has made other changes that won't directly cause a layout -
  // for example, the user has changed a layout-affecting class property. Worst
  // case this ends up being a slightly costly no-op but we don't expect this
  // method to be called very often.
  if (!CanBeVisible(child_view)) {
    InvalidateHost(true);
    return;
  }

  // Indicate that the view should become hidden in the layout without
  // immediately changing its visibility. Instead, this triggers an animation
  // which results in the view being hidden.
  //
  // This method is typically only called from View and has a private final
  // implementation in LayoutManagerBase so we have to cast to call it.
  static_cast<LayoutManager*>(this)->ViewVisibilitySet(
      host_view(), child_view, child_view->GetVisible(), false);
}

void AnimatingLayoutManager::FadeIn(View* child_view) {
  DCHECK(child_view);
  DCHECK(child_view->parent());
  DCHECK_EQ(host_view(), child_view->parent());

  // If the view in question is already capable of being visible, either:
  // 1. the view is already visible so this is a no-op
  // 2. the view is not visible because the target layout has chosen to hide it
  // In either case, it is generally useful to recalculate the layout just in
  // case the caller has made other changes that won't directly cause a layout -
  // for example, the user has changed a layout-affecting class property. Worst
  // case this ends up being a slightly costly no-op but we don't expect this
  // method to be called very often.
  if (CanBeVisible(child_view)) {
    InvalidateHost(true);
    return;
  }

  // Indicate that the view should become visible in the layout without
  // immediately changing its visibility. Instead, this triggers an animation
  // which results in the view being shown.
  //
  // This method is typically only called from View and has a private final
  // implementation in LayoutManagerBase so we have to cast to call it.
  static_cast<LayoutManager*>(this)->ViewVisibilitySet(
      host_view(), child_view, child_view->GetVisible(), true);
}

void AnimatingLayoutManager::AddObserver(Observer* observer) {
  if (!observers_.HasObserver(observer))
    observers_.AddObserver(observer);
}

void AnimatingLayoutManager::RemoveObserver(Observer* observer) {
  if (observers_.HasObserver(observer))
    observers_.RemoveObserver(observer);
}

bool AnimatingLayoutManager::HasObserver(Observer* observer) const {
  return observers_.HasObserver(observer);
}

gfx::Size AnimatingLayoutManager::GetPreferredSize(const View* host) const {
  if (!target_layout_manager())
    return gfx::Size();

  switch (bounds_animation_mode_) {
    case BoundsAnimationMode::kUseHostBounds:
      return target_layout_manager()->GetPreferredSize(host);
    case BoundsAnimationMode::kAnimateMainAxis: {
      // Animating only main axis, so cross axis is preferred size.
      gfx::Size result = current_layout_.host_size;
      SetCrossAxis(
          &result, orientation(),
          GetCrossAxis(orientation(),
                       target_layout_manager()->GetPreferredSize(host)));
      return result;
    }
    case BoundsAnimationMode::kAnimateBothAxes:
      return current_layout_.host_size;
  }
}

gfx::Size AnimatingLayoutManager::GetMinimumSize(const View* host) const {
  if (!target_layout_manager())
    return gfx::Size();
  // TODO(dfried): consider cases where the minimum size might not be just the
  // minimum size of the embedded layout.
  gfx::Size minimum_size = target_layout_manager()->GetMinimumSize(host);
  switch (bounds_animation_mode_) {
    case BoundsAnimationMode::kUseHostBounds:
      // No modification required.
      break;
    case BoundsAnimationMode::kAnimateMainAxis:
      SetMainAxis(
          &minimum_size, orientation(),
          std::min(GetMainAxis(orientation(), minimum_size),
                   GetMainAxis(orientation(), current_layout_.host_size)));
      break;
    case BoundsAnimationMode::kAnimateBothAxes:
      minimum_size.SetToMin(current_layout_.host_size);
      break;
  }
  return minimum_size;
}

int AnimatingLayoutManager::GetPreferredHeightForWidth(const View* host,
                                                       int width) const {
  if (!target_layout_manager())
    return 0;

  // TODO(dfried): revisit this computation.
  if (bounds_animation_mode_ == BoundsAnimationMode::kAnimateBothAxes ||
      (bounds_animation_mode_ == BoundsAnimationMode::kAnimateMainAxis &&
       orientation() == LayoutOrientation::kVertical)) {
    return current_layout_.host_size.height();
  }
  return target_layout_manager()->GetPreferredHeightForWidth(host, width);
}

std::vector<View*> AnimatingLayoutManager::GetChildViewsInPaintOrder(
    const View* host) const {
  DCHECK_EQ(host_view(), host);

  if (!is_animating())
    return LayoutManagerBase::GetChildViewsInPaintOrder(host);

  std::vector<View*> result;
  std::set<View*> fading;

  // Put all fading views to the front of the list (back of the Z-order).
  for (const LayoutFadeInfo& fade_info : fade_infos_) {
    result.push_back(fade_info.child_view);
    fading.insert(fade_info.child_view);
  }

  // Add the result of the views.
  for (View* child : host->children()) {
    if (!base::Contains(fading, child))
      result.push_back(child);
  }

  return result;
}

bool AnimatingLayoutManager::OnViewRemoved(View* host, View* view) {
  // Remove any fade infos corresponding to the removed view.
  base::EraseIf(fade_infos_, [view](const LayoutFadeInfo& fade_info) {
    return fade_info.child_view == view;
  });

  // Remove any elements in the current layout corresponding to the removed
  // view.
  base::EraseIf(current_layout_.child_layouts,
                [view](const ChildLayout& child_layout) {
                  return child_layout.child_view == view;
                });

  return LayoutManagerBase::OnViewRemoved(host, view);
}

void AnimatingLayoutManager::PostOrQueueAction(base::OnceClosure action) {
  queued_actions_.push_back(std::move(action));
  if (!is_animating())
    PostQueuedActions();
}

FlexRule AnimatingLayoutManager::GetDefaultFlexRule() const {
  return base::BindRepeating(&AnimatingLayoutManager::DefaultFlexRuleImpl,
                             base::Unretained(this));
}

gfx::AnimationContainer*
AnimatingLayoutManager::GetAnimationContainerForTesting() {
  DCHECK(animation_delegate_);
  animation_delegate_->MakeReadyForAnimation();
  DCHECK(animation_delegate_->ready_to_animate());
  return animation_delegate_->container();
}

void AnimatingLayoutManager::EnableAnimationForTesting() {
  DCHECK(animation_delegate_);
  animation_delegate_->MakeReadyForAnimation();
  DCHECK(animation_delegate_->ready_to_animate());
}

ProposedLayout AnimatingLayoutManager::CalculateProposedLayout(
    const SizeBounds& size_bounds) const {
  // This class directly overrides Layout() so GetProposedLayout() and
  // CalculateProposedLayout() are not called.
  NOTREACHED();
  return ProposedLayout();
}

void AnimatingLayoutManager::OnInstalled(View* host) {
  DCHECK(!animation_delegate_);
  animation_delegate_ = std::make_unique<AnimationDelegate>(this);
}

void AnimatingLayoutManager::OnLayoutChanged() {
  // This replaces the normal behavior of clearing cached layouts.
  RecalculateTarget();
}

void AnimatingLayoutManager::LayoutImpl() {
  // Changing the size of a view directly will lead to a layout call rather
  // than an invalidation. This should reset the layout (but see the note in
  // RecalculateTarget() below).
  const gfx::Size host_size = host_view()->size();

  if (bounds_animation_mode_ == BoundsAnimationMode::kUseHostBounds) {
    if (!cached_layout_size() || host_size != *cached_layout_size()) {
      // Host size changed, so reset the layout.
      ResetLayoutToTargetSize();
    }

  } else {
    const SizeBounds available_size = GetAvailableHostSize();

    if (bounds_animation_mode_ == BoundsAnimationMode::kAnimateMainAxis &&
        (!cached_layout_size() ||
         GetCrossAxis(orientation(), host_size) !=
             GetCrossAxis(orientation(), *cached_layout_size()))) {
      // If we're fixed to the cross-axis size of the host and that size
      // changes, we need to reset the layout.
      last_available_host_size_ = available_size;
      ResetLayoutToSize(host_size);
    } else {
      // Either both axes are animating or only the main axis is animating or
      // the cross axis hasn't changed (because otherwise the previous condition
      // would have executed instead).
      const base::Optional<int> bounds_main =
          GetMainAxis(orientation(), available_size);
      const int host_main = GetMainAxis(orientation(), host_size);
      const int current_main =
          GetMainAxis(orientation(), current_layout_.host_size);
      if (current_main > host_main ||
          (bounds_main && current_main > *bounds_main)) {
        // Reset the layout immediately if the current or target layout exceeds
        // the host size or the available space.
        last_available_host_size_ = available_size;
        ResetLayoutToSize(host_size);
      } else if (available_size != last_available_host_size_) {
        // May need to re-trigger animation if our bounds were relaxed; let us
        // expand into the new available space.
        RecalculateTarget();
      }
    }

    // Verify that the last available size has been updated.
    DCHECK_EQ(available_size, last_available_host_size_);
  }

  ApplyLayout(current_layout_);

  // Send animating stopped events on layout so the current layout during the
  // event represents the final state instead of an intermediate state.
  if (is_animating_ && current_offset_ == 1.0)
    OnAnimationEnded();
}

void AnimatingLayoutManager::OnAnimationEnded() {
  DCHECK(is_animating_);
  is_animating_ = false;
  fade_infos_.clear();
  PostQueuedActions();
  NotifyIsAnimatingChanged();
}

void AnimatingLayoutManager::ResetLayoutToTargetSize() {
  ResetLayoutToSize(GetAvailableTargetLayoutSize());
}

void AnimatingLayoutManager::ResetLayoutToSize(const gfx::Size& target_size) {
  if (animation_delegate_)
    animation_delegate_->Reset();

  ResolveFades();

  target_layout_ = target_layout_manager()->GetProposedLayout(target_size);
  current_layout_ = target_layout_;
  starting_layout_ = current_layout_;
  fade_infos_.clear();
  current_offset_ = 1.0;
  set_cached_layout_size(target_size);

  if (is_animating_)
    OnAnimationEnded();
}

bool AnimatingLayoutManager::RecalculateTarget() {
  constexpr double kResetAnimationThreshold = 0.8;

  if (!target_layout_manager())
    return false;

  if (!cached_layout_size() || !animation_delegate_ ||
      !animation_delegate_->ready_to_animate()) {
    ResetLayoutToTargetSize();
    return true;
  }

  const gfx::Size target_size = GetAvailableTargetLayoutSize();

  // For layouts that are confined to available space, changing the available
  // space causes a fresh layout, not an animation.
  // TODO(dfried): define a way for views to animate into and out of empty
  // space as adjacent child views appear/disappear. This will be useful in
  // animating tab titles, which currently slide over when the favicon
  // disappears.
  if (bounds_animation_mode_ == BoundsAnimationMode::kUseHostBounds &&
      *cached_layout_size() != target_size) {
    ResetLayoutToSize(target_size);
    return true;
  }

  set_cached_layout_size(target_size);

  // If there has been no appreciable change in layout, there's no reason to
  // start or update an animation.
  const ProposedLayout proposed_layout =
      target_layout_manager()->GetProposedLayout(target_size);
  if (target_layout_ == proposed_layout)
    return false;

  target_layout_ = proposed_layout;
  if (current_offset_ > kResetAnimationThreshold) {
    starting_layout_ = current_layout_;
    starting_offset_ = 0.0;
    current_offset_ = 0.0;
    animation_delegate_->Animate();
    if (!is_animating_) {
      is_animating_ = true;
      NotifyIsAnimatingChanged();
    }
  } else if (current_offset_ > starting_offset_) {
    // Only update the starting layout if the animation has progressed. This has
    // the effect of "batching up" changes that all happen on the same frame,
    // keeping the same starting point. (A common example of this is multiple
    // child views' visibility changing.)
    starting_layout_ = current_layout_;
    starting_offset_ = current_offset_;
  }
  CalculateFadeInfos();
  UpdateCurrentLayout(0.0);

  return true;
}

void AnimatingLayoutManager::AnimateTo(double value) {
  DCHECK_GE(value, 0.0);
  DCHECK_LE(value, 1.0);
  DCHECK_GE(value, starting_offset_);
  DCHECK_GE(value, current_offset_);
  if (current_offset_ == value)
    return;
  current_offset_ = value;
  const double percent =
      (current_offset_ - starting_offset_) / (1.0 - starting_offset_);
  UpdateCurrentLayout(percent);
  InvalidateHost(false);
}

void AnimatingLayoutManager::NotifyIsAnimatingChanged() {
  for (auto& observer : observers_)
    observer.OnLayoutIsAnimatingChanged(this, is_animating());
}

void AnimatingLayoutManager::RunQueuedActions() {
  run_queued_actions_is_pending_ = false;
  std::vector<base::OnceClosure> actions = std::move(queued_actions_to_run_);
  for (auto& action : actions)
    std::move(action).Run();
}

void AnimatingLayoutManager::PostQueuedActions() {
  // Move queued actions over to actions that should run during the next
  // PostTask(). This prevents a race between old PostTask() calls and new
  // delayed actions. See the header for more detail.
  for (auto& action : queued_actions_)
    queued_actions_to_run_.push_back(std::move(action));
  queued_actions_.clear();

  // Early return to prevent multiple RunQueuedAction() tasks.
  if (run_queued_actions_is_pending_)
    return;

  // Post to self (instead of posting the queued actions directly) which lets
  // us:
  // * Keep "AnimatingLayoutManager::RunDelayedActions" in the stack frame.
  // * Tie the task lifetimes to AnimatingLayoutManager.
  run_queued_actions_is_pending_ =
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(&AnimatingLayoutManager::RunQueuedActions,
                                    weak_ptr_factory_.GetWeakPtr()));
}

void AnimatingLayoutManager::UpdateCurrentLayout(double percent) {
  // This drops out any child view elements that don't exist in the target
  // layout. We'll add them back in later.
  current_layout_ =
      ProposedLayoutBetween(percent, starting_layout_, target_layout_);

  for (const LayoutFadeInfo& fade_info : fade_infos_) {
    // This shouldn't happen but we should ensure that with a check.
    DCHECK_NE(-1, host_view()->GetIndexOf(fade_info.child_view));

    // Views that were previously fading are animated as normal, so nothing to
    // do here.
    if (fade_info.fade_type == LayoutFadeType::kContinuingFade)
      continue;

    ChildLayout child_layout;

    if (percent == 1.0) {
      // At the end of the animation snap to the final state of the child view.
      child_layout.child_view = fade_info.child_view;
      switch (fade_info.fade_type) {
        case LayoutFadeType::kFadingIn:
          child_layout.visible = true;
          child_layout.bounds =
              Denormalize(orientation(), fade_info.reference_bounds);
          break;
        case LayoutFadeType::kFadingOut:
          child_layout.visible = false;
          break;
        case LayoutFadeType::kContinuingFade:
          NOTREACHED();
          continue;
      }
    } else if (default_fade_mode_ == FadeInOutMode::kHide) {
      child_layout.child_view = fade_info.child_view;
      child_layout.visible = false;
    } else {
      const double scale_percent =
          fade_info.fade_type == LayoutFadeType::kFadingIn ? percent
                                                           : 1.0 - percent;

      switch (default_fade_mode_) {
        case FadeInOutMode::kHide:
          NOTREACHED();
          break;
        case FadeInOutMode::kScaleFromMinimum:
          child_layout = CalculateScaleFade(fade_info, scale_percent,
                                            /* scale_from_zero */ false);
          break;
        case FadeInOutMode::kScaleFromZero:
          child_layout = CalculateScaleFade(fade_info, scale_percent,
                                            /* scale_from_zero */ true);
          break;
        case FadeInOutMode::kSlideFromLeadingEdge:
          child_layout = CalculateSlideFade(fade_info, scale_percent,
                                            /* slide_from_leading */ true);
          break;
        case FadeInOutMode::kSlideFromTrailingEdge:
          child_layout = CalculateSlideFade(fade_info, scale_percent,
                                            /* slide_from_leading */ false);
          break;
      }
    }

    ChildLayout* const to_overwrite =
        FindChildViewInLayout(&current_layout_, fade_info.child_view);
    if (to_overwrite)
      *to_overwrite = child_layout;
    else
      current_layout_.child_layouts.push_back(child_layout);
  }
}

void AnimatingLayoutManager::CalculateFadeInfos() {
  // Save any views that were previously fading so we don't try to key off of
  // them when calculating leading/trailing edge.
  std::set<const View*> previously_fading;
  for (const auto& fade_info : fade_infos_)
    previously_fading.insert(fade_info.child_view);

  fade_infos_.clear();

  struct ChildInfo {
    base::Optional<size_t> start;
    NormalizedRect start_bounds;
    bool start_visible = false;
    base::Optional<size_t> target;
    NormalizedRect target_bounds;
    bool target_visible = false;
  };

  std::map<View*, ChildInfo> child_to_info;
  std::map<int, View*> start_leading_edges;
  std::map<int, View*> target_leading_edges;

  // Collect some bookkeping info to prevent linear searches later.

  for (View* child : host_view()->children()) {
    if (IsChildIncludedInLayout(child, /* include hidden */ true))
      child_to_info.emplace(child, ChildInfo());
  }

  for (size_t i = 0; i < starting_layout_.child_layouts.size(); ++i) {
    const auto& child_layout = starting_layout_.child_layouts[i];
    auto it = child_to_info.find(child_layout.child_view);
    if (it != child_to_info.end()) {
      it->second.start = i;
      it->second.start_bounds = Normalize(orientation(), child_layout.bounds);
      it->second.start_visible = child_layout.visible;
    }
  }

  for (size_t i = 0; i < target_layout_.child_layouts.size(); ++i) {
    const auto& child_layout = target_layout_.child_layouts[i];
    auto it = child_to_info.find(child_layout.child_view);
    if (it != child_to_info.end()) {
      it->second.target = i;
      it->second.target_bounds = Normalize(orientation(), child_layout.bounds);
      it->second.target_visible = child_layout.visible;
    }
  }

  for (View* child : host_view()->children()) {
    const auto& index = child_to_info[child];
    if (index.start_visible && index.target_visible &&
        !base::Contains(previously_fading, child)) {
      start_leading_edges.emplace(index.start_bounds.origin_main(), child);
      target_leading_edges.emplace(index.target_bounds.origin_main(), child);
    }
  }

  // Build the LayoutFadeInfo data.

  const NormalizedSize start_host_size =
      Normalize(orientation(), starting_layout_.host_size);
  const NormalizedSize target_host_size =
      Normalize(orientation(), target_layout_.host_size);

  for (View* child : host_view()->children()) {
    const auto& current = child_to_info[child];
    if (current.start_visible && !current.target_visible) {
      LayoutFadeInfo fade_info;
      fade_info.fade_type = LayoutFadeType::kFadingOut;
      fade_info.child_view = child;
      fade_info.reference_bounds = current.start_bounds;
      auto next =
          start_leading_edges.upper_bound(current.start_bounds.origin_main());
      if (next == start_leading_edges.end()) {
        fade_info.next_view = nullptr;
        fade_info.offsets.set_trailing(start_host_size.main() -
                                       current.start_bounds.max_main());
      } else {
        fade_info.next_view = next->second;
        fade_info.offsets.set_trailing(next->first -
                                       current.start_bounds.max_main());
      }
      if (next == start_leading_edges.begin()) {
        fade_info.prev_view = nullptr;
        fade_info.offsets.set_leading(current.start_bounds.origin_main());
      } else {
        auto prev = next;
        --prev;
        const auto& prev_info = child_to_info[prev->second];
        fade_info.prev_view = prev->second;
        fade_info.offsets.set_leading(current.start_bounds.origin_main() -
                                      prev_info.start_bounds.max_main());
      }
      fade_infos_.push_back(fade_info);
    } else if (!current.start_visible && current.target_visible) {
      LayoutFadeInfo fade_info;
      fade_info.fade_type = LayoutFadeType::kFadingIn;
      fade_info.child_view = child;
      fade_info.reference_bounds = current.target_bounds;
      auto next =
          target_leading_edges.upper_bound(current.target_bounds.origin_main());
      if (next == target_leading_edges.end()) {
        fade_info.next_view = nullptr;
        fade_info.offsets.set_trailing(target_host_size.main() -
                                       current.target_bounds.max_main());
      } else {
        fade_info.next_view = next->second;
        fade_info.offsets.set_trailing(next->first -
                                       current.target_bounds.max_main());
      }
      if (next == target_leading_edges.begin()) {
        fade_info.prev_view = nullptr;
        fade_info.offsets.set_leading(current.target_bounds.origin_main());
      } else {
        auto prev = next;
        --prev;
        const auto& prev_info = child_to_info[prev->second];
        fade_info.prev_view = prev->second;
        fade_info.offsets.set_leading(current.target_bounds.origin_main() -
                                      prev_info.target_bounds.max_main());
      }
      fade_infos_.push_back(fade_info);
    } else if (base::Contains(previously_fading, child)) {
      // Capture the fact that this view was fading as part of an animation that
      // was interrupted. (It is therefore technically still fading.) This
      // status goes away when the animation ends.
      LayoutFadeInfo fade_info;
      fade_info.fade_type = LayoutFadeType::kContinuingFade;
      fade_info.child_view = child;
      // No reference bounds or offsets since we'll use the normal animation
      // pathway for this view.
      fade_infos_.push_back(fade_info);
    }
  }
}

void AnimatingLayoutManager::ResolveFades() {
  // Views that need faded out are views which were were fading out previously
  // because they were set to not be visible, either by calling SetVisible() or
  // FadeOut(). Those views will not be included in the new layout but may not
  // have been allowed to become invisible yet because of the fade-out
  // animation. Even in the case of FadeInOutMode::kHide, if no frames of the
  // animation have run, the relevant view may still be visible.
  for (const LayoutFadeInfo& fade_info : fade_infos_) {
    View* const child = fade_info.child_view;
    if (fade_info.fade_type == LayoutFadeType::kFadingOut &&
        host_view()->GetIndexOf(child) >= 0 &&
        !IsChildViewIgnoredByLayout(child) && !IsChildIncludedInLayout(child)) {
      SetViewVisibility(child, false);
    }
  }
}

ChildLayout AnimatingLayoutManager::CalculateScaleFade(
    const LayoutFadeInfo& fade_info,
    double scale_percent,
    bool scale_from_zero) const {
  ChildLayout child_layout;

  int leading_reference_point = 0;
  if (fade_info.prev_view) {
    // Since prev/next view is always a view in the start and target layouts, it
    // should also be in the current layout. Therefore this should never return
    // null.
    const ChildLayout* const prev_layout =
        FindChildViewInLayout(current_layout_, fade_info.prev_view);
    leading_reference_point =
        Normalize(orientation(), prev_layout->bounds).max_main();
  }
  leading_reference_point += fade_info.offsets.leading();

  int trailing_reference_point;
  if (fade_info.next_view) {
    // Since prev/next view is always a view in the start and target layouts, it
    // should also be in the current layout. Therefore this should never return
    // null.
    const ChildLayout* const next_layout =
        FindChildViewInLayout(current_layout_, fade_info.next_view);
    trailing_reference_point =
        Normalize(orientation(), next_layout->bounds).origin_main();
  } else {
    trailing_reference_point =
        Normalize(orientation(), current_layout_.host_size).main();
  }
  trailing_reference_point -= fade_info.offsets.trailing();

  const int new_size =
      std::min(int{scale_percent * fade_info.reference_bounds.size_main()},
               trailing_reference_point - leading_reference_point);

  child_layout.child_view = fade_info.child_view;
  if (new_size > 0 &&
      (scale_from_zero ||
       new_size >=
           Normalize(orientation(), fade_info.child_view->GetMinimumSize())
               .main())) {
    child_layout.visible = true;
    NormalizedRect new_bounds = fade_info.reference_bounds;
    switch (fade_info.fade_type) {
      case LayoutFadeType::kFadingIn:
        new_bounds.set_origin_main(leading_reference_point);
        break;
      case LayoutFadeType::kFadingOut:
        new_bounds.set_origin_main(trailing_reference_point - new_size);
        break;
      case LayoutFadeType::kContinuingFade:
        NOTREACHED();
        break;
    }
    new_bounds.set_size_main(new_size);
    child_layout.bounds = Denormalize(orientation(), new_bounds);
  }

  return child_layout;
}

ChildLayout AnimatingLayoutManager::CalculateSlideFade(
    const LayoutFadeInfo& fade_info,
    double scale_percent,
    bool slide_from_leading) const {
  // Fall back to kScaleFromMinimum if there is no edge to slide out from.
  if (!fade_info.prev_view && !fade_info.next_view)
    return CalculateScaleFade(fade_info, scale_percent, false);

  // Slide from the other direction if against the edge of the host view.
  if (slide_from_leading && !fade_info.prev_view)
    slide_from_leading = false;
  else if (!slide_from_leading && !fade_info.next_view)
    slide_from_leading = true;

  NormalizedRect new_bounds = fade_info.reference_bounds;

  // Determine which layout the sliding view will be completely faded in.
  const ProposedLayout* fully_faded_layout;
  switch (fade_info.fade_type) {
    case LayoutFadeType::kFadingIn:
      fully_faded_layout = &starting_layout_;
      break;
    case LayoutFadeType::kFadingOut:
      fully_faded_layout = &target_layout_;
      break;
    case LayoutFadeType::kContinuingFade:
      NOTREACHED();
      break;
  }

  if (slide_from_leading) {
    // Get the layout info for the leading child.
    const ChildLayout* const leading_child =
        FindChildViewInLayout(*fully_faded_layout, fade_info.prev_view);

    // This is the right side of the leading control that will eclipse the
    // sliding view at the start/end of the animation.
    const int initial_trailing =
        Normalize(orientation(), leading_child->bounds).max_main();

    // Interpolate between initial and final trailing edge.
    const int new_trailing = gfx::Tween::IntValueBetween(
        scale_percent, initial_trailing, new_bounds.max_main());

    // Adjust the bounding rectangle of the view.
    new_bounds.Offset(new_trailing - new_bounds.max_main(), 0);

  } else {
    // Get the layout info for the trailing child.
    const ChildLayout* const trailing_child =
        FindChildViewInLayout(*fully_faded_layout, fade_info.next_view);

    // This is the left side of the trailing control that will eclipse the
    // sliding view at the start/end of the animation.
    const int initial_leading =
        Normalize(orientation(), trailing_child->bounds).origin_main();

    // Interpolate between initial and final leading edge.
    const int new_leading = gfx::Tween::IntValueBetween(
        scale_percent, initial_leading, new_bounds.origin_main());

    // Adjust the bounding rectangle of the view.
    new_bounds.Offset(new_leading - new_bounds.origin_main(), 0);
  }

  // Actual bounds are a linear interpolation between starting and reference
  // bounds.
  ChildLayout child_layout;
  child_layout.child_view = fade_info.child_view;
  child_layout.visible = true;
  child_layout.bounds = Denormalize(orientation(), new_bounds);

  return child_layout;
}

// Returns the space in which to calculate the target layout.
gfx::Size AnimatingLayoutManager::GetAvailableTargetLayoutSize() {
  if (bounds_animation_mode_ == BoundsAnimationMode::kUseHostBounds)
    return host_view()->size();

  const SizeBounds bounds = GetAvailableHostSize();
  last_available_host_size_ = bounds;
  const gfx::Size preferred_size =
      target_layout_manager()->GetPreferredSize(host_view());

  int width = preferred_size.width();

  if (orientation() == LayoutOrientation::kVertical &&
      bounds_animation_mode_ == BoundsAnimationMode::kAnimateMainAxis) {
    width = host_view()->width();
  } else if (bounds.width()) {
    width = std::min(width, *bounds.width());
  }

  int height;

  if (orientation() == LayoutOrientation::kHorizontal &&
      bounds_animation_mode_ == BoundsAnimationMode::kAnimateMainAxis) {
    height = host_view()->height();
  } else {
    height = width < preferred_size.width()
                 ? target_layout_manager()->GetPreferredHeightForWidth(
                       host_view(), width)
                 : preferred_size.height();
    if (bounds.height()) {
      height = std::min(height, *bounds.height());
    }
  }

  return gfx::Size(width, height);
}

// static
gfx::Size AnimatingLayoutManager::DefaultFlexRuleImpl(
    const AnimatingLayoutManager* animating_layout,
    const View* view,
    const SizeBounds& size_bounds) {
  DCHECK_EQ(view->GetLayoutManager(), animating_layout);

  // This is the current preferred size, which takes animation into account.
  const gfx::Size preferred_size = animating_layout->GetPreferredSize(view);

  // Does the preferred size fit in the bounds? If so, return the preferred
  // size. Note that the *target* size might not fit in the bounds, but we'll
  // recalculate that the next time we lay out.
  if (CanFitInBounds(preferred_size, size_bounds))
    return preferred_size;

  // Special case - if we're being asked for a zero-size layout we'll return the
  // minimum size of the layout. This is because we're being probed for how
  // small we can get, not being asked for an actual size.
  const base::Optional<int> bounds_main =
      GetMainAxis(animating_layout->orientation(), size_bounds);
  if (bounds_main && *bounds_main <= 0)
    return animating_layout->GetMinimumSize(view);

  // We know our current size does not fit into the bounds being given to us.
  // This is going to force a snap to a new size, which will be the ideal size
  // of the target layout in the provided space.
  const LayoutManagerBase* const target_layout =
      animating_layout->target_layout_manager();

  // Easiest case is that the target layout's preferred size *does* fit, in
  // which case we can use that.
  const gfx::Size target_preferred = target_layout->GetPreferredSize(view);
  if (CanFitInBounds(target_preferred, size_bounds))
    return target_preferred;

  // We know that at least one of the width and height are constrained, so we
  // need to ask the target layout how large it wants to be in the space
  // provided.
  gfx::Size size;
  if (size_bounds.width() && size_bounds.height()) {
    // Both width and height are specified.  Constraining the width may change
    // the desired height, so we can't just blindly return the minimum in both
    // dimensions.  Instead, query the target layout in the constrained space
    // and return its size.
    size = gfx::Size(*size_bounds.width(), *size_bounds.height());
  } else if (size_bounds.width()) {
    // The width is specified and too small.  Use the height-for-width
    // calculation.
    // TODO(dfried): This should be rare, but it is also inefficient. See if we
    // can't add an alternative to GetPreferredHeightForWidth() that actually
    // calculates the layout in this space so we don't have to do it twice.
    const int width = *size_bounds.width();
    size = gfx::Size(width,
                     target_layout->GetPreferredHeightForWidth(view, width));
  } else {
    DCHECK(size_bounds.height());
    // The height is specified and too small.  Fortunately the height of a
    // layout can't (shouldn't?) affect its width.
    size = gfx::Size(target_preferred.width(), *size_bounds.height());
  }

  return target_layout->GetProposedLayout(size).host_size;
}

}  // namespace views
