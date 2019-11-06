// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TOOLBAR_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TOOLBAR_CONTAINER_H_

#include <map>
#include <memory>
#include <vector>

#include "chrome/browser/ui/extensions/extensions_container.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/views/toolbar/toolbar_action_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_icon_container_view.h"

class Browser;
class ExtensionsToolbarButton;
class ToolbarActionViewController;

// Container for extensions shown in the toolbar. These include pinned
// extensions and extensions that are 'popped out' transitively to show dialogs
// or be called out to the user.
// This container is used when the extension-menu experiment is active as a
// replacement for BrowserActionsContainer and ToolbarActionsBar which are
// intended to be removed.
// TODO(crbug.com/943702): Remove note related to extensions menu when cleaning
// up after the experiment.
class ExtensionsToolbarContainer : public ToolbarIconContainerView,
                                   public ExtensionsContainer,
                                   public ToolbarActionsModel::Observer,
                                   public ToolbarActionView::Delegate {
 public:
  explicit ExtensionsToolbarContainer(Browser* browser);
  ~ExtensionsToolbarContainer() override;

  ExtensionsToolbarButton* extensions_button() const {
    return extensions_button_;
  }

  // ToolbarIconContainerView:
  void UpdateAllIcons() override;

 private:
  // Creates toolbar actions and icons corresponding to the model. This is only
  // called in the constructor or when the model initializes and should not be
  // called for subsequent changes to the model.
  void CreateActions();

  // Creates an action and toolbar button for the corresponding ID.
  void CreateActionForId(const ToolbarActionsModel::ActionId& action_id);

  // Sorts child views to display them in the correct order (pinned actions,
  // popped out actions, extensions button).
  void ReorderViews();

  // ExtensionsContainer:
  ToolbarActionViewController* GetActionForId(
      const std::string& action_id) override;
  ToolbarActionViewController* GetPoppedOutAction() const override;
  bool IsActionVisibleOnToolbar(
      const ToolbarActionViewController* action) const override;
  void UndoPopOut() override;
  void SetPopupOwner(ToolbarActionViewController* popup_owner) override;
  void HideActivePopup() override;
  bool CloseOverflowMenuIfOpen() override;
  void PopOutAction(ToolbarActionViewController* action,
                    bool is_sticky,
                    const base::Closure& closure) override;

  // ToolbarActionsModel::Observer:
  void OnToolbarActionAdded(const ToolbarActionsModel::ActionId& action_id,
                            int index) override;
  void OnToolbarActionRemoved(
      const ToolbarActionsModel::ActionId& action_id) override;
  void OnToolbarActionMoved(const ToolbarActionsModel::ActionId& action_id,
                            int index) override;
  void OnToolbarActionLoadFailed() override;
  void OnToolbarActionUpdated(
      const ToolbarActionsModel::ActionId& action_id) override;
  void OnToolbarVisibleCountChanged() override;
  void OnToolbarHighlightModeChanged(bool is_highlighting) override;
  void OnToolbarModelInitialized() override;

  // ToolbarActionView::Delegate:
  content::WebContents* GetCurrentWebContents() override;
  bool ShownInsideMenu() const override;
  void OnToolbarActionViewDragDone() override;
  views::LabelButton* GetOverflowReferenceView() override;
  gfx::Size GetToolbarActionSize() override;
  void WriteDragDataForView(View* sender,
                            const gfx::Point& press_pt,
                            ui::OSExchangeData* data) override;
  int GetDragOperationsForView(View* sender, const gfx::Point& p) override;
  bool CanStartDragForView(View* sender,
                           const gfx::Point& press_pt,
                           const gfx::Point& p) override;

  Browser* const browser_;
  ToolbarActionsModel* const model_;
  ScopedObserver<ToolbarActionsModel, ToolbarActionsModel::Observer>
      model_observer_;
  ExtensionsToolbarButton* const extensions_button_;

  // TODO(pbos): Create actions and icons only for pinned pinned / popped out
  // actions (lazily). Currently code expects GetActionForId() to return
  // actions for extensions that aren't visible.
  // Actions for all extensions.
  std::vector<std::unique_ptr<ToolbarActionViewController>> actions_;
  // View for every action, does not imply pinned or currently shown.
  std::map<ToolbarActionsModel::ActionId, std::unique_ptr<ToolbarActionView>>
      icons_;
  // Popped-out extension, if any.
  ToolbarActionViewController* popped_out_action_ = nullptr;
  // The action that triggered the current popup, if any.
  ToolbarActionViewController* popup_owner_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ExtensionsToolbarContainer);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TOOLBAR_CONTAINER_H_
