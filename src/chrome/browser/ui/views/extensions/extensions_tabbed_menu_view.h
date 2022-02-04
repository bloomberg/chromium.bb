// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TABBED_MENU_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TABBED_MENU_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/extensions/extension_action_view_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace views {
class View;
class TabbedPane;
}  // namespace views

class ExtensionsMenuItemView;
class ExtensionsContainer;

// ExtensionsTabbedMenuView is the extensions menu dialog with a tabbed pane.
// TODO(crbug.com/1263311): Brief explanation of each tabs goal after
// implementing them.
class ExtensionsTabbedMenuView : public views::BubbleDialogDelegateView,
                                 public TabStripModelObserver,
                                 public ToolbarActionsModel::Observer {
 public:
  METADATA_HEADER(ExtensionsTabbedMenuView);
  ExtensionsTabbedMenuView(views::View* anchor_view,
                           Browser* browser,
                           ExtensionsContainer* extensions_container,
                           ExtensionsToolbarButton::ButtonType button_type,
                           bool allow_pinning);
  ~ExtensionsTabbedMenuView() override;
  ExtensionsTabbedMenuView(const ExtensionsTabbedMenuView&) = delete;
  const ExtensionsTabbedMenuView& operator=(const ExtensionsTabbedMenuView&) =
      delete;

  // Displays the ExtensionsTabbedMenu under `anchor_view` with the selected tab
  // open by the `selected_tab_index` given. Only one menu is allowed to be
  // shown at a time (outside of tests).
  static views::Widget* ShowBubble(
      views::View* anchor_view,
      Browser* browser,
      ExtensionsContainer* extensions_container_,
      ExtensionsToolbarButton::ButtonType button_type,
      bool allow_pinning);

  // Returns true if there is currently an ExtensionsTabbedMenuView showing
  // (across all browsers and profiles).
  static bool IsShowing();

  // Hides the currently-showing ExtensionsTabbedMenuView, if any exists.
  static void Hide();

  // Returns the currently-showing ExtensionsTabbedMenuView, if any exists.
  static ExtensionsTabbedMenuView* GetExtensionsTabbedMenuViewForTesting();

  // Returns the currently-showing extension items in the installed tab, if any
  // exists.
  std::vector<ExtensionsMenuItemView*> GetInstalledItemsForTesting() const;

  // Returns the currently-showing `has_access_` extension items in the site
  // access tab, if any exists.
  std::vector<ExtensionsMenuItemView*> GetHasAccessItemsForTesting() const;

  // Returns the currently-showing `requests_access_` extension items in the
  // site access tab, if any exists.
  std::vector<ExtensionsMenuItemView*> GetRequestsAccessItemsForTesting() const;

  // Returns the index of the currently selected tab.
  size_t GetSelectedTabIndex() const;

  // views::BubbleDialogDelegateView:
  std::u16string GetAccessibleWindowTitle() const override;

  // TabStripModelObserver:
  void TabChangedAt(content::WebContents* contents,
                    int index,
                    TabChangeType change_type) override;
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // ToolbarActionsModel::Observer:
  void OnToolbarActionAdded(
      const ToolbarActionsModel::ActionId& action_id) override;
  void OnToolbarActionRemoved(
      const ToolbarActionsModel::ActionId& action_id) override;
  void OnToolbarActionUpdated(
      const ToolbarActionsModel::ActionId& action_id) override;
  void OnToolbarModelInitialized() override;
  void OnToolbarPinnedActionsChanged() override;

 private:
  struct SiteAccessSection {
    // The root view for this section used to toggle the visibility of the
    // entire section (depending on whether there are any menu items).
    raw_ptr<views::View> container;

    // The view containing only the menu items for this section.
    raw_ptr<views::View> items;

    // The id of the string to use for the section heading. Does not include the
    // current site string.
    const int header_string_id;

    // The PageInteractionStatus that this section is handling.
    const ToolbarActionViewController::PageInteractionStatus page_status;
  };

  // Initially creates the tabs.
  void Populate();

  // Updates the menu.
  void Update();

  // Creates and returns the site access container with empty sections.
  std::unique_ptr<views::View> CreateSiteAccessContainer();

  // Creates and adds a menu item for `id` in the installed extensions for a
  // newly-added extension.
  void CreateAndInsertInstalledExtension(
      const ToolbarActionsModel::ActionId& id,
      int index);

  // Creates and adds a menu item for `id` in its corresponding site access
  // section if the associated extension has or requests access to the current
  // site.
  void MaybeCreateAndInsertSiteAccessItem(
      const ToolbarActionsModel::ActionId& id);

  // Adds `item` to the items list of `section`.
  void InsertSiteAccessItem(std::unique_ptr<ExtensionsMenuItemView> item,
                            SiteAccessSection* section);

  // Moves items between site access sections if their site access status
  // changed. Called when one or more items are updated.
  void MoveItemsBetweenSectionsIfNecessary();

  // Updates the visibility of the site access sections. A given section should
  // be visible if there are any extensions displayed in it.
  void UpdateSiteAccessSectionsVisibility();

  // Returns the section corresponding to `status`, or nullptr.
  SiteAccessSection* GetSiteAccessSectionForPageStatus(
      ToolbarActionViewController::PageInteractionStatus status);

  // Runs a set of consistency checks on the appearance of the menu. This is a
  // no-op if DCHECKs are disabled.
  void ConsistencyCheck();

  const raw_ptr<Browser> browser_;
  const raw_ptr<ExtensionsContainer> extensions_container_;
  const raw_ptr<ToolbarActionsModel> toolbar_model_;
  base::ScopedObservation<ToolbarActionsModel, ToolbarActionsModel::Observer>
      toolbar_model_observation_{this};
  bool const allow_pinning_;

  // The view containing the menu's two tabs.
  raw_ptr<views::TabbedPane> tabbed_pane_ = nullptr;

  // The view containing the installed extension menu items. This is
  // separated for easy insertion and iteration of menu items. The children are
  // guaranteed to only be ExtensionMenuItemViews.
  views::View* installed_items_ = nullptr;

  // The different sections in the site access tab.
  SiteAccessSection requests_access_;
  SiteAccessSection has_access_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TABBED_MENU_VIEW_H_
