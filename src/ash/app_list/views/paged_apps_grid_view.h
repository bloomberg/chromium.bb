// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_PAGED_APPS_GRID_VIEW_H_
#define ASH_APP_LIST_VIEWS_PAGED_APPS_GRID_VIEW_H_

#include <memory>
#include <vector>

#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/views/apps_grid_view.h"
#include "ash/ash_export.h"
#include "ash/public/cpp/pagination/pagination_model.h"
#include "ash/public/cpp/pagination/pagination_model_observer.h"
#include "ash/public/cpp/presentation_time_recorder.h"
#include "base/memory/ref_counted.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/throughput_tracker.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point_f.h"

namespace gfx {
class Vector2d;
}  // namespace gfx

namespace ui {
class Layer;
}  // namespace ui

namespace ash {

class ContentsView;
class PaginationController;

// An apps grid that shows the apps on a series of fixed-size pages.
// Used for the peeking/fullscreen launcher, home launcher and folders.
// Created by and is a child of AppsContainerView. Observes layer animations
// for the transition into and out of the "cardified" state.
class ASH_EXPORT PagedAppsGridView : public AppsGridView,
                                     public PaginationModelObserver,
                                     public ui::ImplicitAnimationObserver {
 public:
  class ContainerDelegate {
   public:
    virtual ~ContainerDelegate() = default;

    // Returns true if |point| lies within the bounds of this grid view plus a
    // buffer area surrounding it that can trigger page flip.
    virtual bool IsPointWithinPageFlipBuffer(const gfx::Point& point) const = 0;

    // Returns whether |point| is in the bottom drag buffer, and not over the
    // shelf.
    virtual bool IsPointWithinBottomDragBuffer(
        const gfx::Point& point,
        int page_flip_zone_size) const = 0;
  };

  PagedAppsGridView(ContentsView* contents_view,
                    AppListA11yAnnouncer* a11y_announcer,
                    AppsGridViewFolderDelegate* folder_delegate,
                    AppListFolderController* folder_controller,
                    ContainerDelegate* container_delegate);
  PagedAppsGridView(const PagedAppsGridView&) = delete;
  PagedAppsGridView& operator=(const PagedAppsGridView&) = delete;
  ~PagedAppsGridView() override;

  // Called when tablet mode starts and ends.
  void OnTabletModeChanged(bool started);

  // Updates the opacity of all the items in the grid when the grid itself is
  // being dragged. The app icons fade out as the launcher slides off the bottom
  // of the screen.
  // `apps_opacity_change_start` and `apps_opacity_change_end` define the range
  // of height of centerline above screen bottom in which apps should change
  // opacity (from 0 to 1).
  void UpdateOpacity(bool restore_opacity,
                     float apps_opacity_change_start,
                     float apps_opacity_change_end);

  // Sets the number of max rows and columns in grid pages. Special-cases the
  // first page, which may allow smaller number of rows in certain cases (to
  // make room for other UI elements like continue section).
  // For non-folder item grid, this generally describes the number of slots
  // shown in the page. For folders, the number of displayed slots will also
  // depend on number of items in the grid (e.g. folder with 4 items will have
  // 2x2 grid).
  void SetMaxColumnsAndRows(int max_columns,
                            int max_rows_on_first_page,
                            int max_rows);

  // ui::EventHandler:
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;

  // views::View:
  void Layout() override;

  // AppsGridView:
  gfx::Size GetTileViewSize() const override;
  gfx::Insets GetTilePadding(int page) const override;
  gfx::Size GetTileGridSize() const override;
  int GetPaddingBetweenPages() const override;
  int GetTotalPages() const override;
  int GetSelectedPage() const override;
  bool IsScrollAxisVertical() const override;
  void UpdateBorder() override;
  void MaybeStartCardifiedView() override;
  void MaybeEndCardifiedView() override;
  void MaybeStartPageFlip() override;
  void MaybeStopPageFlip() override;
  bool MaybeAutoScroll() override;
  void StopAutoScroll() override {}
  void HandleScrollFromParentView(const gfx::Vector2d& offset,
                                  ui::EventType type) override;
  void SetFocusAfterEndDrag() override;
  void CalculateIdealBoundsForNonFolder() override;
  void RecordAppMovingTypeMetrics(AppListAppMovingType type) override;
  int GetMaxRowsInPage(int page) const override;
  gfx::Vector2d GetGridCenteringOffset(int page) const override;
  void UpdatePaging() override;
  void RecordPageMetrics() override;
  const gfx::Vector2d CalculateTransitionOffset(
      int page_of_view) const override;
  void EnsureViewVisible(const GridIndex& index) override;

  // PaginationModelObserver:
  void TotalPagesChanged(int previous_page_count, int new_page_count) override;
  void SelectedPageChanged(int old_selected, int new_selected) override;
  void TransitionStarting() override;
  void TransitionStarted() override;
  void TransitionChanged() override;
  void TransitionEnded() override;
  void ScrollStarted() override;
  void ScrollEnded() override;

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override;

  bool FirePageFlipTimerForTest();
  bool cardified_state_for_testing() const { return cardified_state_; }
  int BackgroundCardCountForTesting() const { return background_cards_.size(); }
  // Returns bounds within the apps grid view for the background card layer
  // with provided card index.
  gfx::Rect GetBackgroundCardBoundsForTesting(size_t card_index);
  void set_page_flip_delay_for_testing(base::TimeDelta page_flip_delay) {
    page_flip_delay_ = page_flip_delay;
  }

  // Gets the PaginationModel used for the grid view.
  PaginationModel* pagination_model() { return &pagination_model_; }

  void set_first_page_offset(int offset) { first_page_offset_ = offset; }

  // Calculates the maximum number of rows on the first page. Relies on tile
  // size, `first_page_offset_`, and the bounds of the apps grid.
  int CalculateFirstPageMaxRows(int available_height, int preferred_rows);

  // Calculates the maximum number of rows. Relies on tile size and the bounds
  // of the apps grid.
  int CalculateMaxRows(int available_height, int preferred_rows);

  int GetFirstPageRowsForTesting() const { return max_rows_on_first_page_; }
  int GetRowsForTesting() const { return max_rows_; }

 private:
  friend class test::AppsGridViewTest;

  class FadeoutLayerDelegate;

  // Returns the size reserved for a single apps grid page. May not match the
  // tile grid size when the first page selected, as the first page may have
  // reduced number of tiles.
  gfx::Size GetPageSize() const;

  // Gets the tile grid size on the provided apps grid page.
  gfx::Size GetTileGridSizeForPage(int page) const;

  // Indicates whether the drag event (from the gesture or mouse) should be
  // handled by PagedAppsGridView.
  bool ShouldHandleDragEvent(const ui::LocatedEvent& event);

  // Creates a layer mask for gradient alpha when the feature is enabled. The
  // gradient appears at the top and bottom of the apps grid to create a
  // "fade out" effect when dragging the whole page.
  void MaybeCreateGradientMask();

  // Returns true if the page is the right target to flip to.
  bool IsValidPageFlipTarget(int page) const;

  // Obtains the target page to flip for |drag_point|.
  int GetPageFlipTargetForDrag(const gfx::Point& drag_point);

  // Starts the page flip timer if |drag_point| is in left/right side page flip
  // zone or is over page switcher.
  void MaybeStartPageFlipTimer(const gfx::Point& drag_point);

  // Invoked when |page_flip_timer_| fires.
  void OnPageFlipTimer();

  // Stops the timer that triggers a page flip during a drag.
  void StopPageFlipTimer();

  // Helper functions to start the Apps Grid Cardified state.
  // The cardified state scales down apps and is shown when the user drags an
  // app in the AppList.
  void StartAppsGridCardifiedView();
  // Ends the Apps Grid Cardified state and sets it to normal.
  void EndAppsGridCardifiedView();
  // Animates individual elements of the apps grid to and from cardified state.
  void AnimateCardifiedState();
  // Call OnBoundsAnimatorDone when all layer animations finish.
  void MaybeCallOnBoundsAnimatorDone();
  // Translates the items container view to center the current page in the apps
  // grid.
  void RecenterItemsContainer();
  // Calculates the background bounds for the grid depending on the value of
  // |cardified_state_|
  gfx::Rect BackgroundCardBounds(int new_page_index);
  // Appends a background card to the back of |background_cards_|.
  void AppendBackgroundCard();
  // Removes the background card at the end of |background_cards_|.
  void RemoveBackgroundCard();
  // Masks the apps grid container to background cards bounds.
  void MaskContainerToBackgroundBounds();
  // Removes all background cards from |background_cards_|.
  void RemoveAllBackgroundCards();
  // Updates the highlighted background card. Used only for cardified state.
  void SetHighlightedBackgroundCard(int new_highlighted_page);

  // Update the padding of tile view based on the contents bounds.
  void UpdateTilePadding();

  // Created by AppListMainView, owned by views hierarchy.
  ContentsView* const contents_view_;

  // Used to get information about whether a point is within the page flip drag
  // buffer area around this view.
  ContainerDelegate* const container_delegate_;

  // Depends on |pagination_model_|.
  std::unique_ptr<PaginationController> pagination_controller_;

  // Timer to auto flip page when dragging an item near the left/right edges.
  base::OneShotTimer page_flip_timer_;

  // Target page to switch to when |page_flip_timer_| fires.
  int page_flip_target_ = -1;

  // Delay for when |page_flip_timer_| should fire after user drags an item near
  // the edge.
  base::TimeDelta page_flip_delay_;

  // Whether the grid is in mouse drag. Used for between-item drags that move
  // the entire grid, not for app icon drags.
  bool is_in_mouse_drag_ = false;

  // The initial mouse drag location in root window coordinate. Updates when the
  // drag on PagedAppsGridView starts. Used for between-item drags that move the
  // entire grid, not for app icon drags.
  gfx::PointF mouse_drag_start_point_;

  // The last mouse drag location in root window coordinate. Used for
  // between-item drags that move the entire grid, not for app icon drags.
  gfx::PointF last_mouse_drag_point_;

  // Implements a "fade out" gradient at the top and bottom of the grid. Used
  // during page flip transitions and for cardified drags.
  std::unique_ptr<FadeoutLayerDelegate> fadeout_layer_delegate_;

  // Records smoothness of pagination animation.
  absl::optional<ui::ThroughputTracker> pagination_metrics_tracker_;

  // Records the presentation time for apps grid dragging.
  std::unique_ptr<PresentationTimeRecorder> presentation_time_recorder_;

  // The highlighted page during cardified state.
  int highlighted_page_ = -1;

  // Layer array for apps grid background cards. Used to display the background
  // card during cardified state.
  std::vector<std::unique_ptr<ui::Layer>> background_cards_;

  // Whether the feature ProductivityLauncher is enabled.
  const bool is_productivity_launcher_enabled_;

  // Maximum number of rows on the first grid page.
  int max_rows_on_first_page_ = 0;

  // Maximum number of rows allowed in apps grid pages.
  int max_rows_ = 0;

  PaginationModel pagination_model_{this};

  // The amount that tiles need to be offset on the y-axis to avoid overlap
  // with the recent apps and continue section.
  int first_page_offset_ = 0;

  // Vertical tile spacing between the tile views on the first page.
  int first_page_vertical_tile_padding_ = 0;

  base::WeakPtrFactory<PagedAppsGridView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_PAGED_APPS_GRID_VIEW_H_
