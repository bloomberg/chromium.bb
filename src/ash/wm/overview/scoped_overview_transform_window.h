// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_SCOPED_OVERVIEW_TRANSFORM_WINDOW_H_
#define ASH_WM_OVERVIEW_SCOPED_OVERVIEW_TRANSFORM_WINDOW_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/wm/overview/overview_animation_type.h"
#include "ash/wm/overview/overview_session.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/transform.h"

namespace aura {
class Window;
enum class EventTargetingPolicy;
}

namespace ui {
class Layer;
}

namespace ash {
class OverviewItem;
class ScopedOverviewAnimationSettings;
class ScopedOverviewHideWindows;

// Manages a window, and its transient children, in the overview mode. This
// class allows transforming the windows with a helper to determine the best
// fit in certain bounds. The window's state is restored when this object is
// destroyed.
class ASH_EXPORT ScopedOverviewTransformWindow
    : public ui::ImplicitAnimationObserver {
 public:
  // Overview windows have certain properties if their aspect ratio exceedes a
  // threshold. This enum keeps track of which category the window falls into,
  // based on its aspect ratio.
  enum class GridWindowFillMode {
    kNormal = 0,
    kLetterBoxed,
    kPillarBoxed,
  };

  using ScopedAnimationSettings =
      std::vector<std::unique_ptr<ScopedOverviewAnimationSettings>>;

  // Windows whose aspect ratio surpass this (width twice as large as height or
  // vice versa) will be classified as too wide or too tall and will be handled
  // slightly differently in overview mode.
  static constexpr float kExtremeWindowRatioThreshold = 2.f;

  // Calculates and returns an optimal scale ratio. This is only taking into
  // account |size.height()| as the width can vary.
  static float GetItemScale(const gfx::SizeF& source,
                            const gfx::SizeF& target,
                            int top_view_inset,
                            int title_height);

  // Returns the transform turning |src_rect| into |dst_rect|.
  static gfx::Transform GetTransformForRect(const gfx::RectF& src_rect,
                                            const gfx::RectF& dst_rect);

  ScopedOverviewTransformWindow(OverviewItem* overview_item,
                                aura::Window* window);
  ~ScopedOverviewTransformWindow() override;

  // Starts an animation sequence which will use animation settings specified by
  // |animation_type|. The |animation_settings| container is populated with
  // scoped entities and the container should be destroyed at the end of the
  // animation sequence.
  //
  // Example:
  //  ScopedOverviewTransformWindow overview_window(window);
  //  ScopedOverviewTransformWindow::ScopedAnimationSettings animation_settings;
  //  overview_window.BeginScopedAnimation(
  //      OVERVIEW_ANIMATION_SELECTOR_ITEM_SCROLL_CANCEL,
  //      &animation_settings);
  //  // Calls to SetTransform & SetOpacity will use the same animation settings
  //  // until animation_settings is destroyed.
  //  overview_window.SetTransform(root_window, new_transform);
  //  overview_window.SetOpacity(1);
  void BeginScopedAnimation(OverviewAnimationType animation_type,
                            ScopedAnimationSettings* animation_settings);

  // Returns true if this overview window contains the |target|.
  bool Contains(const aura::Window* target) const;

  // Returns transformed bounds of the overview window. See
  // OverviewUtil::GetTransformedBounds for more details.
  gfx::RectF GetTransformedBounds() const;

  // Returns the kTopViewInset property of |window_| unless there are transient
  // ancestors, in which case returns 0.
  int GetTopInset() const;

  // Restores and animates the managed window to its non overview mode state.
  // If |reset_transform| equals false, the window's transform will not be reset
  // to identity transform when exiting the overview mode. See
  // OverviewItem::RestoreWindow() for details why we need this.
  void RestoreWindow(bool reset_transform);

  // Informs the ScopedOverviewTransformWindow that the window being watched was
  // destroyed. This resets the internal window pointer.
  void OnWindowDestroyed();

  // Prepares for overview mode by doing any necessary actions before entering.
  void PrepareForOverview();

  // Sets the opacity of the managed windows.
  void SetOpacity(float opacity);

  // Returns |rect| having been shrunk to fit within |bounds| (preserving the
  // aspect ratio). Takes into account a window header that is |top_view_inset|
  // tall in the original window getting replaced by a window caption that is
  // |title_height| tall in the transformed window. If |type_| is not normal,
  // write |overview_bounds_|, which would differ than the return bounds.
  gfx::RectF ShrinkRectToFitPreservingAspectRatio(const gfx::RectF& rect,
                                                  const gfx::RectF& bounds,
                                                  int top_view_inset,
                                                  int title_height);

  // Returns the window used to show the content in overview mode.
  // For minimized window this will be a window that hosts mirrored layers.
  aura::Window* GetOverviewWindow() const;

  // Closes the transient root of the window managed by |this|.
  void Close();

  bool IsMinimized() const;

  // Ensures that a window is visible by setting its opacity to 1.
  void EnsureVisible();

  // Called via OverviewItem from OverviewGrid when |window_|'s bounds
  // change. Must be called before PositionWindows in OverviewGrid.
  void UpdateWindowDimensionsType();

  // Updates the mask which gives rounded corners on the windows. Shows the mask
  // if |show| is true, otherwise removes it.
  void UpdateMask(bool show);

  // Stop listening to any animations to finish.
  void CancelAnimationsListener();

  // ui::ImplicitAnimationObserver:
  void OnLayerAnimationStarted(ui::LayerAnimationSequence* sequence) override;
  void OnImplicitAnimationsCompleted() override;

  aura::Window* window() const { return window_; }

  GridWindowFillMode type() const { return type_; }

  base::Optional<gfx::RectF> overview_bounds() const {
    return overview_bounds_;
  }

  gfx::Rect GetMaskBoundsForTesting() const;

 private:
  friend class OverviewSessionTest;
  class LayerCachingAndFilteringObserver;
  class WindowMask;
  FRIEND_TEST_ALL_PREFIXES(ScopedOverviewTransformWindowWithMaskTest,
                           WindowBoundsChangeTest);

  // Closes the window managed by |this|.
  void CloseWidget();

  // Makes Close() execute synchronously when used in tests.
  static void SetImmediateCloseForTests();

  // A weak pointer to the overview item that owns |this|. Guaranteed to be not
  // null for the lifetime of |this|.
  OverviewItem* overview_item_;

  // A weak pointer to the real window in the overview.
  aura::Window* window_;

  // True if the window has been transformed for overview mode.
  bool overview_started_ = false;

  // The original opacity of the window before entering overview mode.
  float original_opacity_;

  // For the duration of this object |window_| event targeting policy will be
  // sent to NONE. Store the original so we can change it back when destroying
  // this object.
  aura::EventTargetingPolicy original_event_targeting_policy_;

  // Specifies how the window is laid out in the grid.
  GridWindowFillMode type_ = GridWindowFillMode::kNormal;

  // Empty if window is of type normal. Contains the bounds the overview item
  // should be if the window is too wide or too tall.
  base::Optional<gfx::RectF> overview_bounds_;

  // The observers associated with the layers we requested caching render
  // surface and trilinear filtering. The requests will be removed in dtor if
  // the layer has not been destroyed.
  std::vector<std::unique_ptr<LayerCachingAndFilteringObserver>>
      cached_and_filtered_layer_observers_;

  // A mask to be applied on |window_|. This will give |window_| rounded edges
  // while in overview.
  std::unique_ptr<WindowMask> mask_;

  // The original mask layer of the window before entering overview mode.
  ui::Layer* original_mask_layer_ = nullptr;

  std::unique_ptr<ScopedOverviewHideWindows> hidden_transient_children_;

  base::WeakPtrFactory<ScopedOverviewTransformWindow> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ScopedOverviewTransformWindow);
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_SCOPED_OVERVIEW_TRANSFORM_WINDOW_H_
