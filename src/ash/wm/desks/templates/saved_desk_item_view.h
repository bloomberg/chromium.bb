// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_TEMPLATES_SAVED_DESK_ITEM_VIEW_H_
#define ASH_WM_DESKS_TEMPLATES_SAVED_DESK_ITEM_VIEW_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/desk_template.h"
#include "ash/wm/overview/overview_highlightable_view.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/view_observer.h"
#include "ui/views/view_targeter_delegate.h"

namespace views {
class Label;
class Textfield;
}  // namespace views

namespace ash {

class CloseButton;
class PillButton;
class SavedDeskIconContainer;
class SavedDeskNameView;
class ViewShadow;

// A view that represents each individual saved desk item in the saved desk
// grid. The view has different shown contents depending on whether the mouse is
// hovered over it.
//   _________________________          _________________________
//   |  _______________  _   |          |                    _  |
//   |  |_____________| |_|  |          |                   |_| |
//   |  |_______|            |          |     ______________    |
//   |   _________________   |          |     |            |    |
//   |  |                 |  |          |     |____________|    |
//   |  |_________________|  |          |                       |
//   |_______________________|          |_______________________|
//            regular                             hover
//
// In the regular view we have the:
// `name_view_`: top-left: SavedDeskNameView: It's an editable textbox that
// contains the name of the saved desk.
// `time_view_`: middle-left: Label: A label that lets the user know when the
// saved desk was created.
// `icon_container_view_`: bottom-center: SavedDeskIconContainer: A
// container that houses a couple icons/text that give an indication of which
// apps are part of the saved desk.
// `managed_status_indicator`: top-right: ImageView: A icon that is visible if
// the saved desk was created by an admin.
//
// In the hover view we have the:
// `delete_button_`: top-right: Button: Shows a confirmation for deleting the
// saved desk when clicked.
// `launch_button_`: bottom-center: Button: Launches the apps associated with
// the saved desk when clicked.
//
// The whole view is also a button which does the same thing as `launch_button_`
// when clicked.
class ASH_EXPORT SavedDeskItemView : public views::Button,
                                     public OverviewHighlightableView,
                                     public views::ViewTargeterDelegate,
                                     public views::TextfieldController {
 public:
  METADATA_HEADER(SavedDeskItemView);

  explicit SavedDeskItemView(const DeskTemplate* desk_template);
  SavedDeskItemView(const SavedDeskItemView&) = delete;
  SavedDeskItemView& operator=(const SavedDeskItemView&) = delete;
  ~SavedDeskItemView() override;

  // The preferred size of the whole SavedDeskItemView.
  static constexpr gfx::Size kPreferredSize = {220, 120};

  DeskTemplate* desk_template() const { return desk_template_.get(); }
  SavedDeskNameView* name_view() const { return name_view_; }
  const base::GUID uuid() const { return desk_template_->uuid(); }

  // Updates the visibility state of the delete and launch buttons depending on
  // the current mouse or touch event location, or if switch access is enabled.
  void UpdateHoverButtonsVisibility(const gfx::Point& screen_location,
                                    bool is_touch);

  // Returns true if the saved desk's name is being modified (i.e. the
  // `SavedDeskNameView` has the focus).
  bool IsNameBeingModified() const;

  // To prevent duplications when saving a desk multiple times, check if there's
  // an existing saved desk that shares the same name as current active desk, if
  // so, remove auto added number.
  void MaybeRemoveNameNumber();
  // Show replace dialog when found a name duplication.
  void MaybeShowReplaceDialog(SavedDeskItemView* saved_desk_to_replace);
  // Rename current saved desk with new name, delete old saved desk with same
  // name by uuid. Used for callback functions for Replace Dialog.
  void ReplaceTemplate(const std::string& uuid);
  void RevertTemplateName();

  // This allows us to update an existing template view. Currently, this
  // function will only update the name. We will need to update this once we
  // allow the user to make more changes to a template. If the text field is
  // blurred when there is an update, we intentionally leave it blurred in order
  // to align this behavior with other similar cases.
  void UpdateTemplate(const DeskTemplate& updated_template);

  // views::Button:
  void Layout() override;
  void OnThemeChanged() override;
  void OnViewFocused(views::View* observed_view) override;
  void OnViewBlurred(views::View* observed_view) override;
  KeyClickAction GetKeyClickActionForEvent(const ui::KeyEvent& event) override;

  // views::TextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const std::u16string& new_contents) override;
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;
  bool HandleMouseEvent(views::Textfield* sender,
                        const ui::MouseEvent& mouse_event) override;

  // views::ViewTargeterDelegate:
  views::View* TargetForRect(views::View* root, const gfx::Rect& rect) override;

 private:
  friend class SavedDeskItemViewTestApi;

  // Return the duplicated saved desk item if there is a name duplication in
  // saved desks.
  SavedDeskItemView* FindOtherTemplateWithName(
      const std::u16string& name) const;

  void OnDeleteTemplate();
  void OnDeleteButtonPressed();

  void OnGridItemPressed(const ui::Event& event);

  // Launches the apps associated with the template unless editing the desk
  // template name is underway. Adds a 3 second delay between each app launch if
  // `should_delay` is true.
  void MaybeLaunchTemplate(bool should_delay);

  // Called when we want to update `name_view_` when the template's name
  // changes.
  void OnTemplateNameChanged(const std::u16string& new_name);

  // Update template name based on `name_view_` string.
  void UpdateTemplateName();

  // OverviewHighlightableView:
  views::View* GetView() override;
  void MaybeActivateHighlightedView() override;
  void MaybeCloseHighlightedView(bool primary_action) override;
  void MaybeSwapHighlightedView(bool right) override;
  void OnViewHighlighted() override;
  void OnViewUnhighlighted() override;

  // A copy of the associated saved desk.
  std::unique_ptr<DeskTemplate> desk_template_;

  // Owned by the views hierarchy.
  SavedDeskNameView* name_view_ = nullptr;
  // When template is managed by admin, `time_view_` will display management
  // description instead.
  views::Label* time_view_ = nullptr;
  SavedDeskIconContainer* icon_container_view_ = nullptr;
  CloseButton* delete_button_ = nullptr;
  PillButton* launch_button_ = nullptr;
  // Container used for holding all the views that appear on hover.
  views::View* hover_container_ = nullptr;

  std::unique_ptr<ViewShadow> shadow_;

  // When the `name_view_` is focused, we select all its text. However, if it is
  // focused via a mouse press event, on mouse release will clear the selection.
  // Therefore, we defer selecting all text until we receive that mouse release.
  bool defer_select_all_ = false;

  // This is set when `name_view_` is focused or blurred to indicate whether
  // this template's name is being modified or not. This is used instead of
  // `HasFocus()` to defer text selection, since the first mouse press event is
  // triggered before the `name_view_` is actually focused.
  bool is_template_name_being_modified_ = false;

  // This is initialized to true and tells the `OnViewBlurred` function if the
  // user wants to set a new template name. We set this to false if the
  // `HandleKeyEvent` function detects that the escape key was pressed so that
  // `OnViewBlurred` does not update the template name.
  bool should_commit_name_changes_ = true;

  base::ScopedObservation<views::View, views::ViewObserver>
      name_view_observation_{this};

  base::WeakPtrFactory<SavedDeskItemView> weak_ptr_factory_{this};
};

BEGIN_VIEW_BUILDER(/* no export */, SavedDeskItemView, views::Button)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(/* no export */, ash::SavedDeskItemView)

#endif  // ASH_WM_DESKS_TEMPLATES_SAVED_DESK_ITEM_VIEW_H_
