// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/window_occlusion_tracker.h"

#include "base/containers/adapters.h"
#include "base/stl_util.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/transform.h"

namespace aura {

namespace {

// When one of these properties is animated, a window is considered non-occluded
// and cannot occlude other windows.
constexpr ui::LayerAnimationElement::AnimatableProperties
    kSkipWindowWhenPropertiesAnimated =
        ui::LayerAnimationElement::TRANSFORM |
        ui::LayerAnimationElement::BOUNDS | ui::LayerAnimationElement::OPACITY;

WindowOcclusionTracker* g_tracker = nullptr;

int g_num_pause_occlusion_tracking = 0;

// Returns true if |window| opaquely fills its bounds. |window| must be visible.
bool VisibleWindowIsOpaque(Window* window) {
  DCHECK(window->IsVisible());
  DCHECK(window->layer());
  return !window->transparent() &&
         window->layer()->type() != ui::LAYER_NOT_DRAWN &&
         window->layer()->GetCombinedOpacity() == 1.0f;
}

// Returns the transform of |window| relative to its root.
// |parent_transform_relative_to_root| is the transform of |window->parent()|
// relative to its root.
gfx::Transform GetWindowTransformRelativeToRoot(
    aura::Window* window,
    const gfx::Transform& parent_transform_relative_to_root) {
  if (window->IsRootWindow())
    return gfx::Transform();

  // Concatenate |parent_transform_relative_to_root| to the result of
  // GetTargetTransformRelativeTo(parent_layer) rather than simply calling
  // GetTargetTransformRelativeTo(window->GetRootWindow()->layer()) to avoid
  // doing the same computations multiple times when traversing a window
  // hierarchy.
  ui::Layer* parent_layer = window->parent()->layer();
  gfx::Transform transform_relative_to_root;
  bool success = window->layer()->GetTargetTransformRelativeTo(
      parent_layer, &transform_relative_to_root);
  DCHECK(success);
  transform_relative_to_root.ConcatTransform(parent_transform_relative_to_root);
  return transform_relative_to_root;
}

// Returns the bounds of |window| relative to its |root|.
// |transform_relative_to_root| is the transform of |window| relative to its
// root. If |clipped_bounds| is not null, the returned bounds are clipped by it.
SkIRect GetWindowBoundsInRootWindow(
    aura::Window* window,
    const gfx::Transform& transform_relative_to_root,
    const SkIRect* clipped_bounds) {
  DCHECK(transform_relative_to_root.Preserves2dAxisAlignment());
  // Compute the unclipped bounds of |window|.
  gfx::RectF bounds(0.0f, 0.0f, static_cast<float>(window->bounds().width()),
                    static_cast<float>(window->bounds().height()));
  transform_relative_to_root.TransformRect(&bounds);
  SkIRect skirect_bounds = SkIRect::MakeXYWH(
      gfx::ToFlooredInt(bounds.x()), gfx::ToFlooredInt(bounds.y()),
      gfx::ToFlooredInt(bounds.width()), gfx::ToFlooredInt(bounds.height()));
  // If necessary, clip the bounds.
  if (clipped_bounds && !skirect_bounds.intersect(*clipped_bounds))
    return SkIRect::MakeEmpty();
  return skirect_bounds;
}

}  // namespace

WindowOcclusionTracker::ScopedPauseOcclusionTracking::
    ScopedPauseOcclusionTracking() {
  ++g_num_pause_occlusion_tracking;
}

WindowOcclusionTracker::ScopedPauseOcclusionTracking::
    ~ScopedPauseOcclusionTracking() {
  --g_num_pause_occlusion_tracking;
  DCHECK_GE(g_num_pause_occlusion_tracking, 0);
  if (g_tracker)
    g_tracker->MaybeRecomputeOcclusion();
}

void WindowOcclusionTracker::Track(Window* window) {
  DCHECK(window);
  DCHECK(window != window->GetRootWindow());

  if (!g_tracker)
    g_tracker = new WindowOcclusionTracker();

  auto insert_result = g_tracker->tracked_windows_.insert(window);
  DCHECK(insert_result.second);
  if (!window->HasObserver(g_tracker))
    window->AddObserver(g_tracker);
  if (window->GetRootWindow())
    g_tracker->TrackedWindowAddedToRoot(window);
}

WindowOcclusionTracker::WindowOcclusionTracker() = default;

WindowOcclusionTracker::~WindowOcclusionTracker() = default;

void WindowOcclusionTracker::MaybeRecomputeOcclusion() {
  if (g_num_pause_occlusion_tracking)
    return;
  for (auto& root_window_pair : root_windows_) {
    RootWindowState& root_window_state = root_window_pair.second;
    if (root_window_state.dirty == true) {
      ScopedPauseOcclusionTracking scoped_pause_occlusion_tracking;
      root_window_state.dirty = false;
      SkRegion occluded_region;
      RecomputeOcclusionImpl(root_window_pair.first, gfx::Transform(), nullptr,
                             &occluded_region);
      // WindowDelegate::OnWindowOcclusionChanged() impls must not change any
      // Window.
      DCHECK(!root_window_state.dirty);
    }
  }
}

void WindowOcclusionTracker::RecomputeOcclusionImpl(
    Window* window,
    const gfx::Transform& parent_transform_relative_to_root,
    const SkIRect* clipped_bounds,
    SkRegion* occluded_region) {
  DCHECK(window);

  if (WindowIsAnimated(window)) {
    SetWindowAndDescendantsAreOccluded(window, false);
    return;
  }

  if (!window->IsVisible()) {
    SetWindowAndDescendantsAreOccluded(window, true);
    return;
  }

  // Compute window bounds.
  const gfx::Transform transform_relative_to_root =
      GetWindowTransformRelativeToRoot(window,
                                       parent_transform_relative_to_root);
  if (!transform_relative_to_root.Preserves2dAxisAlignment()) {
    // For simplicity, windows that are not axis-aligned are considered
    // unoccluded and do not occlude other windows.
    SetWindowAndDescendantsAreOccluded(window, false);
    return;
  }
  const SkIRect window_bounds = GetWindowBoundsInRootWindow(
      window, transform_relative_to_root, clipped_bounds);

  // Compute children occlusion states.
  const SkIRect* clipped_bounds_for_children =
      window->layer()->GetMasksToBounds() ? &window_bounds : clipped_bounds;
  for (auto* child : base::Reversed(window->children())) {
    RecomputeOcclusionImpl(child, transform_relative_to_root,
                           clipped_bounds_for_children, occluded_region);
  }

  // Compute window occlusion state.
  if (occluded_region->contains(window_bounds)) {
    SetOccluded(window, true);
  } else {
    SetOccluded(window, false);
    if (VisibleWindowIsOpaque(window))
      occluded_region->op(window_bounds, SkRegion::kUnion_Op);
  }
}

void WindowOcclusionTracker::CleanupAnimatedWindows() {
  base::EraseIf(animated_windows_, [=](Window* window) {
    ui::LayerAnimator* const animator = window->layer()->GetAnimator();
    if (animator->IsAnimatingOnePropertyOf(kSkipWindowWhenPropertiesAnimated))
      return false;
    animator->RemoveObserver(this);
    auto root_window_state_it = root_windows_.find(window->GetRootWindow());
    if (root_window_state_it != root_windows_.end())
      root_window_state_it->second.dirty = true;
    return true;
  });
}

bool WindowOcclusionTracker::MaybeObserveAnimatedWindow(Window* window) {
  // MaybeObserveAnimatedWindow() is called when OnWindowBoundsChanged(),
  // OnWindowTransformed() or OnWindowOpacitySet() is called with
  // ui::PropertyChangeReason::FROM_ANIMATION. Despite that, if the animation is
  // ending, the IsAnimatingOnePropertyOf() call below may return false. It is
  // important not to register an observer in that case because it would never
  // be notified.
  ui::LayerAnimator* const animator = window->layer()->GetAnimator();
  if (animator->IsAnimatingOnePropertyOf(kSkipWindowWhenPropertiesAnimated)) {
    const auto insert_result = animated_windows_.insert(window);
    if (insert_result.second) {
      animator->AddObserver(this);
      return true;
    }
  }
  return false;
}

void WindowOcclusionTracker::SetWindowAndDescendantsAreOccluded(
    Window* window,
    bool is_occluded) {
  SetOccluded(window, is_occluded);
  for (Window* child_window : window->children())
    SetWindowAndDescendantsAreOccluded(child_window, is_occluded);
}

void WindowOcclusionTracker::SetOccluded(Window* window, bool occluded) {
  if (WindowIsTracked(window))
    window->SetOccluded(occluded);
}

bool WindowOcclusionTracker::WindowIsTracked(Window* window) const {
  return base::ContainsKey(tracked_windows_, window);
}

bool WindowOcclusionTracker::WindowIsAnimated(Window* window) const {
  return base::ContainsKey(animated_windows_, window);
}

template <typename Predicate>
void WindowOcclusionTracker::MarkRootWindowAsDirtyAndMaybeRecomputeOcclusionIf(
    Window* window,
    Predicate predicate) {
  Window* root_window = window->GetRootWindow();
  if (!root_window)
    return;
  auto root_window_state_it = root_windows_.find(root_window);
  DCHECK(root_window_state_it != root_windows_.end());
  if (root_window_state_it->second.dirty)
    return;
  if (predicate()) {
    root_window_state_it->second.dirty = true;
    MaybeRecomputeOcclusion();
  }
}

bool WindowOcclusionTracker::WindowOrParentIsAnimated(Window* window) const {
  while (window && !WindowIsAnimated(window))
    window = window->parent();
  return window != nullptr;
}

bool WindowOcclusionTracker::WindowOrDescendantIsTrackedAndVisible(
    Window* window) const {
  if (!window->IsVisible())
    return false;
  if (WindowIsTracked(window))
    return true;
  for (Window* child_window : window->children()) {
    if (WindowOrDescendantIsTrackedAndVisible(child_window))
      return true;
  }
  return false;
}

bool WindowOcclusionTracker::WindowOrDescendantIsOpaque(
    Window* window,
    bool assume_parent_opaque,
    bool assume_window_opaque) const {
  const bool parent_window_is_opaque =
      assume_parent_opaque || !window->parent() ||
      window->parent()->layer()->GetCombinedOpacity() == 1.0f;
  const bool window_is_opaque =
      parent_window_is_opaque &&
      (assume_window_opaque || window->layer()->opacity() == 1.0f);

  if (!window->IsVisible() || !window->layer() || !window_is_opaque ||
      WindowIsAnimated(window)) {
    return false;
  }
  if (!window->transparent() && window->layer()->type() != ui::LAYER_NOT_DRAWN)
    return true;
  for (Window* child_window : window->children()) {
    if (WindowOrDescendantIsOpaque(child_window, true))
      return true;
  }
  return false;
}

bool WindowOcclusionTracker::WindowMoveMayAffectOcclusionStates(
    Window* window) const {
  return !WindowOrParentIsAnimated(window) &&
         (WindowOrDescendantIsOpaque(window) ||
          WindowOrDescendantIsTrackedAndVisible(window));
}

void WindowOcclusionTracker::TrackedWindowAddedToRoot(Window* window) {
  Window* const root_window = window->GetRootWindow();
  DCHECK(root_window);
  RootWindowState& root_window_state = root_windows_[root_window];
  ++root_window_state.num_tracked_windows;
  if (root_window_state.num_tracked_windows == 1)
    AddObserverToWindowAndDescendants(root_window);
  root_window_state.dirty = true;
  MaybeRecomputeOcclusion();
}

void WindowOcclusionTracker::TrackedWindowRemovedFromRoot(Window* window) {
  Window* const root_window = window->GetRootWindow();
  DCHECK(root_window);
  auto root_window_state_it = root_windows_.find(root_window);
  DCHECK(root_window_state_it != root_windows_.end());
  --root_window_state_it->second.num_tracked_windows;
  if (root_window_state_it->second.num_tracked_windows == 0) {
    RemoveObserverFromWindowAndDescendants(root_window);
    root_windows_.erase(root_window_state_it);
  }
}

void WindowOcclusionTracker::RemoveObserverFromWindowAndDescendants(
    Window* window) {
  if (WindowIsTracked(window))
    DCHECK(window->HasObserver(this));
  else
    window->RemoveObserver(this);
  for (Window* child_window : window->children())
    RemoveObserverFromWindowAndDescendants(child_window);
}

void WindowOcclusionTracker::AddObserverToWindowAndDescendants(Window* window) {
  if (WindowIsTracked(window))
    DCHECK(window->HasObserver(this));
  else
    window->AddObserver(this);
  for (Window* child_window : window->children())
    AddObserverToWindowAndDescendants(child_window);
}

void WindowOcclusionTracker::OnLayerAnimationEnded(
    ui::LayerAnimationSequence* sequence) {
  CleanupAnimatedWindows();
  MaybeRecomputeOcclusion();
}

void WindowOcclusionTracker::OnLayerAnimationAborted(
    ui::LayerAnimationSequence* sequence) {
  CleanupAnimatedWindows();
  MaybeRecomputeOcclusion();
}

void WindowOcclusionTracker::OnLayerAnimationScheduled(
    ui::LayerAnimationSequence* sequence) {}

void WindowOcclusionTracker::OnWindowHierarchyChanged(
    const HierarchyChangeParams& params) {
  Window* const window = params.target;
  Window* const root_window = window->GetRootWindow();
  if (root_window && base::ContainsKey(root_windows_, root_window) &&
      !window->HasObserver(this)) {
    AddObserverToWindowAndDescendants(window);
  }
}

void WindowOcclusionTracker::OnWindowAdded(Window* window) {
  MarkRootWindowAsDirtyAndMaybeRecomputeOcclusionIf(
      window, [=]() { return WindowMoveMayAffectOcclusionStates(window); });
}

void WindowOcclusionTracker::OnWillRemoveWindow(Window* window) {
  MarkRootWindowAsDirtyAndMaybeRecomputeOcclusionIf(window, [=]() {
    return !WindowOrParentIsAnimated(window) &&
           WindowOrDescendantIsOpaque(window);
  });
}

void WindowOcclusionTracker::OnWindowVisibilityChanged(Window* window,
                                                       bool visible) {
  MarkRootWindowAsDirtyAndMaybeRecomputeOcclusionIf(
      window, [=]() { return !WindowOrParentIsAnimated(window); });
}

void WindowOcclusionTracker::OnWindowBoundsChanged(
    Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  // Call MaybeObserveAnimatedWindow() outside the lambda so that the window can
  // be marked as animated even when its root is dirty.
  const bool animation_started =
      (reason == ui::PropertyChangeReason::FROM_ANIMATION) &&
      MaybeObserveAnimatedWindow(window);
  MarkRootWindowAsDirtyAndMaybeRecomputeOcclusionIf(window, [=]() {
    return animation_started || WindowMoveMayAffectOcclusionStates(window);
  });
}

void WindowOcclusionTracker::OnWindowOpacitySet(
    Window* window,
    ui::PropertyChangeReason reason) {
  // Call MaybeObserveAnimatedWindow() outside the lambda so that the window can
  // be marked as animated even when its root is dirty.
  const bool animation_started =
      (reason == ui::PropertyChangeReason::FROM_ANIMATION) &&
      MaybeObserveAnimatedWindow(window);
  MarkRootWindowAsDirtyAndMaybeRecomputeOcclusionIf(window, [=]() {
    return animation_started || !WindowOrParentIsAnimated(window);
  });
}

void WindowOcclusionTracker::OnWindowTransformed(
    Window* window,
    ui::PropertyChangeReason reason) {
  // Call MaybeObserveAnimatedWindow() outside the lambda so that the window can
  // be marked as animated even when its root is dirty.
  const bool animation_started =
      (reason == ui::PropertyChangeReason::FROM_ANIMATION) &&
      MaybeObserveAnimatedWindow(window);
  MarkRootWindowAsDirtyAndMaybeRecomputeOcclusionIf(window, [=]() {
    return animation_started || WindowMoveMayAffectOcclusionStates(window);
  });
}

void WindowOcclusionTracker::OnWindowStackingChanged(Window* window) {
  MarkRootWindowAsDirtyAndMaybeRecomputeOcclusionIf(
      window, [=]() { return WindowMoveMayAffectOcclusionStates(window); });
}

void WindowOcclusionTracker::OnWindowDestroyed(Window* window) {
  DCHECK(!window->GetRootWindow() || (window == window->GetRootWindow()));
  // Animations should be completed or aborted before a window is destroyed.
  DCHECK(!WindowIsAnimated(window));
  tracked_windows_.erase(window);
}

void WindowOcclusionTracker::OnWindowAddedToRootWindow(Window* window) {
  DCHECK(window->GetRootWindow());
  if (WindowIsTracked(window))
    TrackedWindowAddedToRoot(window);
}

void WindowOcclusionTracker::OnWindowRemovingFromRootWindow(Window* window,
                                                            Window* new_root) {
  DCHECK(window->GetRootWindow());
  if (WindowIsTracked(window))
    TrackedWindowRemovedFromRoot(window);
  RemoveObserverFromWindowAndDescendants(window);
}

void WindowOcclusionTracker::OnWindowLayerRecreated(Window* window) {
  ui::LayerAnimator* animator = window->layer()->GetAnimator();

  // Recreating the layer may have stopped animations.
  if (animator->IsAnimatingOnePropertyOf(kSkipWindowWhenPropertiesAnimated))
    return;

  size_t num_removed = animated_windows_.erase(window);
  if (num_removed == 0)
    return;

  animator->RemoveObserver(this);
  auto root_window_state_it = root_windows_.find(window->GetRootWindow());
  if (root_window_state_it != root_windows_.end()) {
    root_window_state_it->second.dirty = true;
    MaybeRecomputeOcclusion();
  }
}

}  // namespace aura
