// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_WINDOW_OCCLUSION_TRACKER_H_
#define UI_AURA_WINDOW_OCCLUSION_TRACKER_H_

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/layer_animation_observer.h"

struct SkIRect;
class SkRegion;

namespace gfx {
class Transform;
}

namespace aura {

class Window;

// Notifies tracked Windows when their occlusion state change.
//
// To start tracking the occlusion state of a Window, call
// WindowOcclusionTracker::Track().
//
// A Window is occluded if its bounds and transform are not animated and one of
// these conditions is true:
// - The Window is hidden (Window::IsVisible() is true).
// - The bounds of the Window are completely covered by opaque and axis-aligned
//   Windows whose bounds and transform are not animated.
// Note that an occluded window may be drawn on the screen by window switching
// features such as "Alt-Tab" or "Overview".
class AURA_EXPORT WindowOcclusionTracker : public ui::LayerAnimationObserver,
                                           public WindowObserver {
 public:
  // Prevents window occlusion state computations within its scope. If an event
  // that could cause window occlusion states to change occurs within the scope
  // of a ScopedPauseOcclusionTracking, window occlusion state computations are
  // delayed until all ScopedPauseOcclusionTracking objects have been destroyed.
  class AURA_EXPORT ScopedPauseOcclusionTracking {
   public:
    ScopedPauseOcclusionTracking();
    ~ScopedPauseOcclusionTracking();

   private:
    DISALLOW_COPY_AND_ASSIGN(ScopedPauseOcclusionTracking);
  };

  // Start tracking the occlusion state of |window|.
  static void Track(Window* window);

 private:
  WindowOcclusionTracker();
  ~WindowOcclusionTracker() override;

  // Recomputes the occlusion state of tracked windows under roots marked as
  // dirty in |root_windows_| if there are no active
  // ScopedPauseOcclusionTracking instance.
  void MaybeRecomputeOcclusion();

  // Recomputes the occlusion state of |window| and its descendants.
  // |parent_transform_relative_to_root| is the transform of |window->parent()|
  // relative to the root window. |clipped_bounds| is an optional mask for the
  // bounds of |window| and its descendants. |occluded_region| is a region
  // covered by windows which are on top of |window|.
  void RecomputeOcclusionImpl(
      Window* window,
      const gfx::Transform& parent_transform_relative_to_root,
      const SkIRect* clipped_bounds,
      SkRegion* occluded_region);

  // Removes windows whose bounds and transform are not animated from
  // |animated_windows_|. Marks the root of those windows as dirty.
  void CleanupAnimatedWindows();

  // If the bounds or transform of |window| are animated and |window| is not in
  // |animated_windows_|, adds |window| to |animated_windows_| and returns true.
  bool MaybeObserveAnimatedWindow(Window* window);

  // Calls SetOccluded(|is_occluded|) on |window| and its descendants if they
  // are in |tracked_windows_|.
  void SetWindowAndDescendantsAreOccluded(Window* window, bool is_occluded);

  // Calls SetOccluded() on |window| with |occluded| as argument if |window| is
  // in |tracked_windows_|.
  void SetOccluded(Window* window, bool occluded);

  // Returns true if |window| is in |tracked_windows_|.
  bool WindowIsTracked(Window* window) const;

  // Returns true if |window| is in |animated_windows_|.
  bool WindowIsAnimated(Window* window) const;

  // If the root of |window| is not dirty and |predicate| is true, marks the
  // root of |window| as dirty. Then, calls MaybeRecomputeOcclusion().
  // |predicate| is not evaluated if the root of |window| is already dirty when
  // this is called.
  template <typename Predicate>
  void MarkRootWindowAsDirtyAndMaybeRecomputeOcclusionIf(Window* window,
                                                         Predicate predicate);

  // Returns true if |window| or one of its parents is in |animated_windows_|.
  bool WindowOrParentIsAnimated(Window* window) const;

  // Returns true if |window| or one of its descendants is in
  // |tracked_windows_| and visible.
  bool WindowOrDescendantIsTrackedAndVisible(Window* window) const;

  // Returns true if |window| or one of its descendants is visible, opaquely
  // fills its bounds and is not in |animated_windows_|. If
  // |assume_parent_opaque| is true, the function assumes that the combined
  // opacity of window->parent() is 1.0f. If |assume_window_opaque|, the
  // function assumes that the opacity of |window| is 1.0f.
  bool WindowOrDescendantIsOpaque(Window* window,
                                  bool assume_parent_opaque = false,
                                  bool assume_window_opaque = false) const;

  // Returns true if changing the transform, bounds or stacking order of
  // |window| could affect the occlusion state of a tracked window.
  bool WindowMoveMayAffectOcclusionStates(Window* window) const;

  // Called when a tracked |window| is added to a root window.
  void TrackedWindowAddedToRoot(Window* window);

  // Called when a tracked |window| is removed from a root window.
  void TrackedWindowRemovedFromRoot(Window* window);

  // Removes |this| from the observer list of |window| and its descendants,
  // except if they are in |tracked_windows_| or |windows_being_destroyed_|.
  void RemoveObserverFromWindowAndDescendants(Window* window);

  // Add |this| to the observer list of |window| and its descendants.
  void AddObserverToWindowAndDescendants(Window* window);

  // ui::LayerAnimationObserver:
  void OnLayerAnimationEnded(ui::LayerAnimationSequence* sequence) override;
  void OnLayerAnimationAborted(ui::LayerAnimationSequence* sequence) override;
  void OnLayerAnimationScheduled(ui::LayerAnimationSequence* sequence) override;

  // WindowObserver:
  void OnWindowHierarchyChanged(const HierarchyChangeParams& params) override;
  void OnWindowAdded(Window* window) override;
  void OnWillRemoveWindow(Window* window) override;
  void OnWindowVisibilityChanged(Window* window, bool visible) override;
  void OnWindowBoundsChanged(Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnWindowOpacitySet(Window* window,
                          ui::PropertyChangeReason reason) override;
  void OnWindowTransformed(Window* window,
                           ui::PropertyChangeReason reason) override;
  void OnWindowStackingChanged(Window* window) override;
  void OnWindowDestroyed(Window* window) override;
  void OnWindowAddedToRootWindow(Window* window) override;
  void OnWindowRemovingFromRootWindow(Window* window,
                                      Window* new_root) override;
  void OnWindowLayerRecreated(Window* window) override;

  struct RootWindowState {
    // Number of Windows whose occlusion state is tracked under this root
    // Window.
    int num_tracked_windows = 0;

    // Whether the occlusion state of tracked Windows under this root is stale.
    bool dirty = false;
  };

  // Windows whose occlusion state is tracked.
  base::flat_set<Window*> tracked_windows_;

  // Windows whose bounds or transform are animated.
  //
  // To reduce the overhead of the WindowOcclusionTracker, windows in this set
  // and their descendants are considered non-occluded and cannot occlude other
  // windows. A window is added to this set the first time that occlusion is
  // computed after it was animated. It is removed when the animation ends or is
  // aborted.
  base::flat_set<Window*> animated_windows_;

  // Root Windows of Windows in |tracked_windows_|.
  base::flat_map<Window*, RootWindowState> root_windows_;

  DISALLOW_COPY_AND_ASSIGN(WindowOcclusionTracker);
};

}  // namespace aura

#endif  // UI_AURA_WINDOW_OCCLUSION_TRACKER_H_
