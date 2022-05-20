// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_TEMPLATES_SAVED_DESK_LIBRARY_VIEW_H_
#define ASH_WM_DESKS_TEMPLATES_SAVED_DESK_LIBRARY_VIEW_H_

#include <memory>
#include <string>
#include <vector>

#include "base/guid.h"
#include "ui/aura/window_observer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

class DeskTemplate;
class PillButton;
class SavedDeskGridView;
class SavedDeskItemView;
class SavedDeskLibraryEventHandler;
class SavedDeskLibraryWindowTargeter;

// This view is the content of the saved desk library widget. Depending on which
// saved desk features are enabled, it can show one or more `SavedDeskGridView`s
// that each hold a number of saved desks. It is owned by the `OverviewGrid`.
class SavedDeskLibraryView : public views::View, public aura::WindowObserver {
 public:
  METADATA_HEADER(SavedDeskLibraryView);

  SavedDeskLibraryView();
  SavedDeskLibraryView(const SavedDeskLibraryView&) = delete;
  SavedDeskLibraryView& operator=(const SavedDeskLibraryView&) = delete;
  ~SavedDeskLibraryView() override;

  // Creates and returns the widget that contains the SavedDeskLibraryView in
  // overview mode. This does not show the widget.
  static std::unique_ptr<views::Widget> CreateSavedDeskLibraryWidget(
      aura::Window* root);

  const std::vector<SavedDeskGridView*>& grid_views() { return grid_views_; }

  // Retrieve the item view for a given saved desk, or nullptr.
  SavedDeskItemView* GetItemForUUID(const base::GUID& uuid);

  // TODO(dandersson): Look into unifying this and `AddOrUpdateTemplates`.
  void PopulateGridUI(const std::vector<const DeskTemplate*>& entries,
                      const gfx::Rect& grid_bounds,
                      const base::GUID& last_saved_desk_uuid);

  void AddOrUpdateTemplates(const std::vector<const DeskTemplate*>& entries,
                            bool initializing_grid_view,
                            const base::GUID& last_saved_desk_uuid);

  void DeleteTemplates(const std::vector<std::string>& uuids);

 private:
  friend class SavedDeskLibraryEventHandler;
  friend class SavedDeskLibraryViewTestApi;
  friend class SavedDeskLibraryWindowTargeter;

  // Called when the feedback button is pressed. Shows the feedback dialog with
  // desks templates information.
  void OnFeedbackButtonPressed();

  bool IsAnimating();

  // Called from `SavedDeskLibraryWindowTargeter`. Returns true if
  // `screen_location` intersects with an interactive part of the library UI.
  // This includes saved desk items and the feedback button.
  bool IntersectsWithUi(const gfx::Point& screen_location);

  // If this view is attached to a widget, returns its window (or nullptr).
  aura::Window* GetWidgetWindow();

  void OnLocatedEvent(ui::LocatedEvent* event, bool is_touch);

  // views::View:
  void AddedToWidget() override;
  void Layout() override;
  void OnThemeChanged() override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  // Pointers to the grids with saved desks of specific types. These will be set
  // depending on which features are enabled.
  SavedDeskGridView* desk_template_grid_view_ = nullptr;
  SavedDeskGridView* save_and_recall_grid_view_ = nullptr;

  // Used for scroll functionality of the library page. Owned by views
  // hierarchy.
  views::ScrollView* scroll_view_ = nullptr;

  // Holds the active ones, for convenience.
  std::vector<SavedDeskGridView*> grid_views_;

  // Owned by views hierarchy. Section headers above grids. Will match size and
  // order of items in `grid_views_`.
  std::vector<views::Label*> grid_labels_;

  // Owned by views hierarchy. Temporary button to help users give feedback.
  // TODO(crbug.com/1289880): Remove this button when it is no longer needed.
  PillButton* feedback_button_ = nullptr;

  // Handles mouse/touch events on saved desk library widget.
  std::unique_ptr<SavedDeskLibraryEventHandler> event_handler_;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_TEMPLATES_SAVED_DESK_LIBRARY_VIEW_H_
