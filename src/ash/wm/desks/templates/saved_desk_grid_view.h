// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_TEMPLATES_SAVED_DESK_GRID_VIEW_H_
#define ASH_WM_DESKS_TEMPLATES_SAVED_DESK_GRID_VIEW_H_

#include <vector>

#include "base/guid.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/view.h"

namespace ash {

class DeskTemplate;
class SavedDeskItemView;

// A view that shows a grid of saved desks. Each saved desk is a
// `SavedDeskItemView`.
class SavedDeskGridView : public views::View {
 public:
  enum class LayoutMode {
    LANDSCAPE = 0,
    PORTRAIT,
  };

  METADATA_HEADER(SavedDeskGridView);

  SavedDeskGridView();
  SavedDeskGridView(const SavedDeskGridView&) = delete;
  SavedDeskGridView& operator=(const SavedDeskGridView&) = delete;
  ~SavedDeskGridView() override;

  const std::vector<SavedDeskItemView*>& grid_items() const {
    return grid_items_;
  }

  // Sets the grid to show items in landscape or portrait mode.
  void set_layout_mode(LayoutMode layout_mode) { layout_mode_ = layout_mode; }

  // Updates the UI by creating a grid layout and populating the grid with the
  // provided list of saved desks.
  void PopulateGridUI(const std::vector<const DeskTemplate*>& desk_templates,
                      const base::GUID& last_saved_template_uuid);

  // Updates `grid_items_` to ensure the saved desk grid is sorted.
  void SortTemplateGridItems(const base::GUID& last_saved_template_uuid);

  // Updates existing saved desks and adds new saved desks to the grid. Also
  // sorts `grid_items_` in alphabetical order. This will animate the
  // `grid_items_` to their final positions if `initializing_grid_view` is
  // false. Currently only allows a maximum of 6 saved desks to be shown in the
  // grid.
  void AddOrUpdateTemplates(const std::vector<const DeskTemplate*>& entries,
                            bool initializing_grid_view,
                            const base::GUID& last_saved_template_uuid);

  // Removes templates from the grid by UUID. Will trigger an animation to
  // shuffle `grid_items_` to their final positions.
  void DeleteTemplates(const std::vector<std::string>& uuids);

  // Returns true if a template name is being modified using an item view's
  // `SavedDeskNameView` in this grid.
  bool IsTemplateNameBeingModified() const;

  // Returns the item view associated with `uuid`.
  SavedDeskItemView* GetItemForUUID(const base::GUID& uuid);

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void Layout() override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

  bool IsAnimating() const;

  // Returns the size needed to lay out the grid in a given `width`.
  gfx::Size GetSizeForWidth(int width) const;

 private:
  friend class SavedDeskGridViewTestApi;

  // Returns the max columns that the grid can show based on `layout_mode_`.
  size_t GetMaxColumns() const;

  // Calculates the bounds for each grid item within the saved desks grid. The
  // indices of the returned vector directly correlate to those of `grid_items_`
  // (i.e. the Rect at index 1 of the returned vector should be applied to the
  // `SavedDeskItemView` found at index 1 of `grid_items_`).
  std::vector<gfx::Rect> CalculateGridItemPositions() const;

  // Animates the bounds for all the `grid_items_` (using `bounds_animator_`) to
  // their calculated position. `new_grid_items` contains a list of the
  // newly-created saved desk items and will be animated differently than the
  // existing views that are being shifted around.
  void AnimateGridItems(const std::vector<SavedDeskItemView*>& new_grid_items);

  // The views representing saved desks. They're owned by views hierarchy.
  std::vector<SavedDeskItemView*> grid_items_;

  // Controls how the grid items are laid out.
  LayoutMode layout_mode_ = LayoutMode::LANDSCAPE;

  // Used to animate individual view positions.
  views::BoundsAnimator bounds_animator_;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_TEMPLATES_SAVED_DESK_GRID_VIEW_H_
