// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/browser_tab_strip_controller.h"

#include <limits>
#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_menu_model.h"
#include "chrome/browser/ui/tabs/tab_network_state.h"
#include "chrome/browser/ui/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/user_education/reopen_tab_in_product_help.h"
#include "chrome/browser/ui/user_education/reopen_tab_in_product_help_factory.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_drag_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/user_education/feature_promo_controller_views.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_delegate.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/peak_gpu_memory_tracker.h"
#include "content/public/browser/web_contents.h"
#include "ipc/ipc_message.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/base/models/menu_model.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/range/range.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/widget/widget.h"
#include "url/origin.h"

using base::UserMetricsAction;
using content::WebContents;

namespace {

// Gets the source browser view during a tab dragging. Returns nullptr if there
// is none.
BrowserView* GetSourceBrowserViewInTabDragging() {
  auto* source_context = TabDragController::GetSourceContext();
  if (source_context) {
    gfx::NativeWindow source_window =
        source_context->AsView()->GetWidget()->GetNativeWindow();
    if (source_window)
      return BrowserView::GetBrowserViewForNativeWindow(source_window);
  }
  return nullptr;
}

}  // namespace

class BrowserTabStripController::TabContextMenuContents
    : public ui::SimpleMenuModel::Delegate {
 public:
  TabContextMenuContents(Tab* tab,
                         BrowserTabStripController* controller,
                         FeaturePromoController* feature_promo_controller)
      : tab_(tab),
        controller_(controller),
        feature_promo_controller_(feature_promo_controller) {
    model_ = controller_->menu_model_factory_->Create(
        this, controller->browser()->tab_menu_model_delegate(),
        controller->model_, controller->tabstrip_->GetModelIndexOf(tab));

    // If IPH is showing, continue into the menu. IsCommandIdAlerted()
    // is called on |menu_runner_| construction, and we check
    // |tab_groups_promo_handle_| there. So we must do this first.
    if (feature_promo_controller_->BubbleIsShowing(
            feature_engagement::kIPHDesktopTabGroupsNewGroupFeature)) {
      tab_groups_promo_handle_ =
          feature_promo_controller_->CloseBubbleAndContinuePromo(
              feature_engagement::kIPHDesktopTabGroupsNewGroupFeature);
    }

    // Because we use "new" badging for feature promos, we cannot use system-
    // native context menus. (See crbug.com/1109256.)
    const int run_flags =
        views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU;
    menu_runner_ = std::make_unique<views::MenuRunner>(model_.get(), run_flags);
  }
  TabContextMenuContents(const TabContextMenuContents&) = delete;
  TabContextMenuContents& operator=(const TabContextMenuContents&) = delete;

  void Cancel() { controller_ = nullptr; }

  void RunMenuAt(const gfx::Point& point, ui::MenuSourceType source_type) {
    menu_runner_->RunMenuAt(tab_->GetWidget(), nullptr,
                            gfx::Rect(point, gfx::Size()),
                            views::MenuAnchorPosition::kTopLeft, source_type);
  }

  // Overridden from ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override { return false; }
  bool IsCommandIdEnabled(int command_id) const override {
    return controller_->IsCommandEnabledForTab(
        static_cast<TabStripModel::ContextMenuCommand>(command_id),
        tab_);
  }

  bool IsCommandIdAlerted(int command_id) const override {
    return command_id == TabStripModel::CommandAddToNewGroup &&
           tab_groups_promo_handle_;
  }

  void MenuClosed(ui::SimpleMenuModel*) override {
    tab_groups_promo_handle_.reset();
  }

  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const override {
    auto* browser = controller_->browser_view_->browser();
    auto* system_app = browser->app_controller()
                           ? browser->app_controller()->system_app()
                           : nullptr;
    if (system_app && !system_app->ShouldShowTabContextMenuShortcut(
                          browser->profile(), command_id)) {
      return false;
    }

    int browser_cmd;
    return TabStripModel::ContextMenuCommandToBrowserCommand(command_id,
                                                             &browser_cmd) &&
           controller_->tabstrip_->GetWidget()->GetAccelerator(browser_cmd,
                                                               accelerator);
  }
  void ExecuteCommand(int command_id, int event_flags) override {
    // Executing the command destroys |this|, and can also end up destroying
    // |controller_|. So stop the highlights before executing the command.
    controller_->ExecuteCommandForTab(
        static_cast<TabStripModel::ContextMenuCommand>(command_id),
        tab_);
  }

 private:
  std::unique_ptr<ui::SimpleMenuModel> model_;
  std::unique_ptr<views::MenuRunner> menu_runner_;

  // The tab we're showing a menu for.
  raw_ptr<Tab> tab_;

  // A pointer back to our hosting controller, for command state information.
  raw_ptr<BrowserTabStripController> controller_;

  const raw_ptr<FeaturePromoController> feature_promo_controller_;

  // Handle we keep if showing menu IPH for tab groups.
  absl::optional<FeaturePromoController::PromoHandle> tab_groups_promo_handle_;
};

////////////////////////////////////////////////////////////////////////////////
// BrowserTabStripController, public:

BrowserTabStripController::BrowserTabStripController(
    TabStripModel* model,
    BrowserView* browser_view,
    std::unique_ptr<TabMenuModelFactory> menu_model_factory_override)
    : model_(model),
      tabstrip_(nullptr),
      browser_view_(browser_view),
      feature_engagement_tracker_(
          feature_engagement::TrackerFactory::GetForBrowserContext(
              browser_view_->browser()->profile())),
      hover_tab_selector_(model),
      menu_model_factory_(std::move(menu_model_factory_override)) {
  if (!menu_model_factory_) {
    // Use the default one.
    menu_model_factory_ = std::make_unique<TabMenuModelFactory>();
  }
  model_->SetTabStripUI(this);
}

BrowserTabStripController::~BrowserTabStripController() {
  // When we get here the TabStrip is being deleted. We need to explicitly
  // cancel the menu, otherwise it may try to invoke something on the tabstrip
  // from its destructor.
  if (context_menu_contents_.get())
    context_menu_contents_->Cancel();

  model_->RemoveObserver(this);
}

void BrowserTabStripController::InitFromModel(TabStrip* tabstrip) {
  tabstrip_ = tabstrip;

  // Walk the model, calling our insertion observer method for each item within
  // it.
  for (int i = 0; i < model_->count(); ++i)
    AddTab(model_->GetWebContentsAt(i), i, model_->active_index() == i);
}

bool BrowserTabStripController::IsCommandEnabledForTab(
    TabStripModel::ContextMenuCommand command_id,
    Tab* tab) const {
  int model_index = tabstrip_->GetModelIndexOf(tab);
  return model_->ContainsIndex(model_index) ?
      model_->IsContextMenuCommandEnabled(model_index, command_id) : false;
}

void BrowserTabStripController::ExecuteCommandForTab(
    TabStripModel::ContextMenuCommand command_id,
    Tab* tab) {
  int model_index = tabstrip_->GetModelIndexOf(tab);
  if (model_->ContainsIndex(model_index))
    model_->ExecuteContextMenuCommand(model_index, command_id);
}

bool BrowserTabStripController::IsTabPinned(Tab* tab) const {
  return IsTabPinned(tabstrip_->GetModelIndexOf(tab));
}

const ui::ListSelectionModel&
BrowserTabStripController::GetSelectionModel() const {
  return model_->selection_model();
}

int BrowserTabStripController::GetCount() const {
  return model_->count();
}

bool BrowserTabStripController::IsValidIndex(int index) const {
  return model_->ContainsIndex(index);
}

bool BrowserTabStripController::IsActiveTab(int model_index) const {
  return model_->active_index() == model_index;
}

int BrowserTabStripController::GetActiveIndex() const {
  return model_->active_index();
}

bool BrowserTabStripController::IsTabSelected(int model_index) const {
  return model_->IsTabSelected(model_index);
}

bool BrowserTabStripController::IsTabPinned(int model_index) const {
  return model_->ContainsIndex(model_index) && model_->IsTabPinned(model_index);
}

void BrowserTabStripController::SelectTab(int model_index,
                                          const ui::Event& event) {
  std::unique_ptr<content::PeakGpuMemoryTracker> tracker =
      content::PeakGpuMemoryTracker::Create(
          content::PeakGpuMemoryTracker::Usage::CHANGE_TAB);

  TabStripModel::UserGestureDetails gesture_detail(
      TabStripModel::GestureType::kOther, event.time_stamp());
  TabStripModel::GestureType type = TabStripModel::GestureType::kOther;
  if (event.type() == ui::ET_MOUSE_PRESSED)
    type = TabStripModel::GestureType::kMouse;
  else if (event.type() == ui::ET_GESTURE_TAP_DOWN)
    type = TabStripModel::GestureType::kTouch;
  gesture_detail.type = type;
  model_->ActivateTabAt(model_index, gesture_detail);

  tabstrip_->GetWidget()->GetCompositor()->RequestPresentationTimeForNextFrame(
      base::BindOnce(
          [](std::unique_ptr<content::PeakGpuMemoryTracker> tracker,
             const gfx::PresentationFeedback& feedback) {
            // This callback will be ran once the ui::Compositor presents the
            // next frame for the |tabstrip_|. The destruction of |tracker| will
            // get the peak GPU memory and record a histogram.
          },
          std::move(tracker)));
}

void BrowserTabStripController::ExtendSelectionTo(int model_index) {
  model_->ExtendSelectionTo(model_index);
}

void BrowserTabStripController::ToggleSelected(int model_index) {
  model_->ToggleSelectionAt(model_index);
}

void BrowserTabStripController::AddSelectionFromAnchorTo(int model_index) {
  model_->AddSelectionFromAnchorTo(model_index);
}

bool BrowserTabStripController::BeforeCloseTab(int model_index,
                                               CloseTabSource source) {
  // Only consider pausing the close operation if this is the last remaining
  // tab (since otherwise closing it won't close the browser window).
  if (GetCount() > 1)
    return true;

  // Closing this tab will close the current window. See if the browser wants to
  // prompt the user before the browser is allowed to close.
  const Browser::WarnBeforeClosingResult result =
      browser_view_->browser()->MaybeWarnBeforeClosing(base::BindOnce(
          [](TabStrip* tab_strip, int model_index, CloseTabSource source,
             Browser::WarnBeforeClosingResult result) {
            if (result == Browser::WarnBeforeClosingResult::kOkToClose)
              tab_strip->CloseTab(tab_strip->tab_at(model_index), source);
          },
          base::Unretained(tabstrip_), model_index, source));

  return result == Browser::WarnBeforeClosingResult::kOkToClose;
}

void BrowserTabStripController::CloseTab(int model_index) {
  // Cancel any pending tab transition.
  hover_tab_selector_.CancelTabTransition();

  model_->CloseWebContentsAt(model_index,
                             TabStripModel::CLOSE_USER_GESTURE |
                             TabStripModel::CLOSE_CREATE_HISTORICAL_TAB);

  // Try to show reading list IPH if needed.
  if (tabstrip_->GetTabCount() >= 7) {
    feature_engagement_tracker_->NotifyEvent(
        feature_engagement::events::kClosedTabWithEightOrMore);

    browser_view_->feature_promo_controller()->MaybeShowPromo(
        feature_engagement::kIPHReadingListEntryPointFeature);
  }
}

void BrowserTabStripController::AddTabToGroup(
    int model_index,
    const tab_groups::TabGroupId& group) {
  model_->AddToExistingGroup({model_index}, group);
}

void BrowserTabStripController::RemoveTabFromGroup(int model_index) {
  model_->RemoveFromGroup({model_index});
}

void BrowserTabStripController::MoveTab(int start_index, int final_index) {
  model_->MoveWebContentsAt(start_index, final_index, false);
}

void BrowserTabStripController::MoveGroup(const tab_groups::TabGroupId& group,
                                          int final_index) {
  model_->MoveGroupTo(group, final_index);
}

bool BrowserTabStripController::ToggleTabGroupCollapsedState(
    const tab_groups::TabGroupId group,
    ToggleTabGroupCollapsedStateOrigin origin) {
  const bool is_currently_collapsed = IsGroupCollapsed(group);
  if (is_currently_collapsed) {
    if (origin != ToggleTabGroupCollapsedStateOrigin::kImplicitAction) {
      base::RecordAction(
          base::UserMetricsAction("TabGroups_TabGroupHeader_Expanded"));
    }
  } else {
    if (model_->GetTabGroupForTab(GetActiveIndex()) == group) {
      // If the active tab is in the group that is toggling to collapse, the
      // active tab should switch to the next available tab. If there are no
      // available tabs for the active tab to switch to, the group will not
      // toggle to collapse.
      const absl::optional<int> next_active =
          model_->GetNextExpandedActiveTab(GetActiveIndex(), group);
      if (!next_active.has_value()) {
        base::RecordAction(base::UserMetricsAction("TabGroups_CannotCollapse"));
        return false;
      }
      model_->ActivateTabAt(next_active.value(),
                            {TabStripModel::GestureType::kOther});
    } else {
      // If the active tab is not in the group that is toggling to collapse,
      // reactive the active tab to deselect any other potentially selected
      // tabs.
      model_->ActivateTabAt(GetActiveIndex(),
                            {TabStripModel::GestureType::kOther});
    }
    if (origin != ToggleTabGroupCollapsedStateOrigin::kImplicitAction) {
      base::RecordAction(
          base::UserMetricsAction("TabGroups_TabGroupHeader_Collapsed"));
    }
  }
  tabstrip_->ToggleTabGroup(group, !is_currently_collapsed, origin);

  tab_groups::TabGroupVisualData new_data(
      GetGroupTitle(group), GetGroupColorId(group), !is_currently_collapsed);
  model_->group_model()->GetTabGroup(group)->SetVisualData(new_data, true);
  return true;
}

void BrowserTabStripController::ShowContextMenuForTab(
    Tab* tab,
    const gfx::Point& p,
    ui::MenuSourceType source_type) {
  context_menu_contents_ = std::make_unique<TabContextMenuContents>(
      tab, this, browser_view_->feature_promo_controller());
  context_menu_contents_->RunMenuAt(p, source_type);
  base::UmaHistogramEnumeration(
      "TabStrip.Tab.Views.ActivationAction",
      TabStripModel::TabActivationTypes::kContextMenu);
}

int BrowserTabStripController::HasAvailableDragActions() const {
  return model_->delegate()->GetDragActions();
}

void BrowserTabStripController::OnDropIndexUpdate(int index,
                                                  bool drop_before) {
  // Perform a delayed tab transition if hovering directly over a tab.
  // Otherwise, cancel the pending one.
  if (index != -1 && !drop_before) {
    hover_tab_selector_.StartTabTransition(index);
  } else {
    hover_tab_selector_.CancelTabTransition();
  }
}

void BrowserTabStripController::CreateNewTab() {
  // This must be called before AddTabAt() so that OmniboxFocused is called
  // after NewTabOpened. TODO(collinbaker): remove omnibox focusing from
  // triggering conditions (since it is always focused for new tabs) and move
  // this after AddTabAt() call.
  auto* reopen_tab_iph = ReopenTabInProductHelpFactory::GetForProfile(
      browser_view_->browser()->profile());
  reopen_tab_iph->NewTabOpened();

  model_->delegate()->AddTabAt(GURL(), -1, true);
}

void BrowserTabStripController::CreateNewTabWithLocation(
    const std::u16string& location) {
  // Use autocomplete to clean up the text, going so far as to turn it into
  // a search query if necessary.
  AutocompleteMatch match;
  AutocompleteClassifierFactory::GetForProfile(GetProfile())
      ->Classify(location, false, false, metrics::OmniboxEventProto::BLANK,
                 &match, nullptr);
  if (match.destination_url.is_valid())
    model_->delegate()->AddTabAt(match.destination_url, -1, true);
}

void BrowserTabStripController::OnStartedDragging(bool dragging_window) {
  if (!immersive_reveal_lock_.get()) {
    // The top-of-window views should be revealed while the user is dragging
    // tabs in immersive fullscreen. The top-of-window views may not be already
    // revealed if the user is attempting to attach a tab to a tabstrip
    // belonging to an immersive fullscreen window.
    immersive_reveal_lock_.reset(
        browser_view_->immersive_mode_controller()->GetRevealedLock(
            ImmersiveModeController::ANIMATE_REVEAL_NO));
  }

  browser_view_->frame()->SetTabDragKind(dragging_window ? TabDragKind::kAllTabs
                                                         : TabDragKind::kTab);
  // We also use fast resize for the source browser window as the source browser
  // window may also change bounds during dragging.
  BrowserView* source_browser_view = GetSourceBrowserViewInTabDragging();
  if (source_browser_view && source_browser_view != browser_view_)
    source_browser_view->frame()->SetTabDragKind(TabDragKind::kTab);
}

void BrowserTabStripController::OnStoppedDragging() {
  immersive_reveal_lock_.reset();

  BrowserView* source_browser_view = GetSourceBrowserViewInTabDragging();
  // Only reset the source window's fast resize bit after the entire drag
  // ends.
  if (browser_view_ != source_browser_view)
    browser_view_->frame()->SetTabDragKind(TabDragKind::kNone);
  if (source_browser_view && !TabDragController::IsActive())
    source_browser_view->frame()->SetTabDragKind(TabDragKind::kNone);
}

void BrowserTabStripController::OnKeyboardFocusedTabChanged(
    absl::optional<int> index) {
  browser_view_->browser()->command_controller()->TabKeyboardFocusChangedTo(
      index);
}

std::u16string BrowserTabStripController::GetGroupTitle(
    const tab_groups::TabGroupId& group) const {
  return model_->group_model()->GetTabGroup(group)->visual_data()->title();
}

std::u16string BrowserTabStripController::GetGroupContentString(
    const tab_groups::TabGroupId& group) const {
  return model_->group_model()->GetTabGroup(group)->GetContentString();
}

tab_groups::TabGroupColorId BrowserTabStripController::GetGroupColorId(
    const tab_groups::TabGroupId& group) const {
  return model_->group_model()->GetTabGroup(group)->visual_data()->color();
}

bool BrowserTabStripController::IsGroupCollapsed(
    const tab_groups::TabGroupId& group) const {
  return model_->group_model()->ContainsTabGroup(group) &&
         model_->group_model()
             ->GetTabGroup(group)
             ->visual_data()
             ->is_collapsed();
}

void BrowserTabStripController::SetVisualDataForGroup(
    const tab_groups::TabGroupId& group,
    const tab_groups::TabGroupVisualData& visual_data) {
  model_->group_model()->GetTabGroup(group)->SetVisualData(visual_data);
}

absl::optional<int> BrowserTabStripController::GetFirstTabInGroup(
    const tab_groups::TabGroupId& group) const {
  return model_->group_model()->GetTabGroup(group)->GetFirstTab();
}

absl::optional<int> BrowserTabStripController::GetLastTabInGroup(
    const tab_groups::TabGroupId& group) const {
  return model_->group_model()->GetTabGroup(group)->GetLastTab();
}

gfx::Range BrowserTabStripController::ListTabsInGroup(
    const tab_groups::TabGroupId& group) const {
  return model_->group_model()->GetTabGroup(group)->ListTabs();
}

bool BrowserTabStripController::IsFrameCondensed() const {
  return GetFrameView()->IsFrameCondensed();
}

bool BrowserTabStripController::HasVisibleBackgroundTabShapes() const {
  return GetFrameView()->HasVisibleBackgroundTabShapes(
      BrowserFrameActiveState::kUseCurrent);
}

bool BrowserTabStripController::EverHasVisibleBackgroundTabShapes() const {
  return GetFrameView()->EverHasVisibleBackgroundTabShapes();
}

bool BrowserTabStripController::ShouldPaintAsActiveFrame() const {
  return GetFrameView()->ShouldPaintAsActive();
}

bool BrowserTabStripController::CanDrawStrokes() const {
  return GetFrameView()->CanDrawStrokes();
}

SkColor BrowserTabStripController::GetFrameColor(
    BrowserFrameActiveState active_state) const {
  return GetFrameView()->GetFrameColor(active_state);
}

SkColor BrowserTabStripController::GetToolbarTopSeparatorColor() const {
  return GetFrameView()->GetToolbarTopSeparatorColor();
}

absl::optional<int> BrowserTabStripController::GetCustomBackgroundId(
    BrowserFrameActiveState active_state) const {
  return GetFrameView()->GetCustomBackgroundId(active_state);
}

std::u16string BrowserTabStripController::GetAccessibleTabName(
    const Tab* tab) const {
  return browser_view_->GetAccessibleTabLabel(false /* include_app_name */,
                                              tabstrip_->GetModelIndexOf(tab));
}

Profile* BrowserTabStripController::GetProfile() const {
  return model_->profile();
}

const Browser* BrowserTabStripController::GetBrowser() const {
  return browser();
}
////////////////////////////////////////////////////////////////////////////////
// BrowserTabStripController, TabStripModelObserver implementation:

void BrowserTabStripController::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  switch (change.type()) {
    case TabStripModelChange::kInserted: {
      for (const auto& contents : change.GetInsert()->contents) {
        DCHECK(model_->ContainsIndex(contents.index));
        AddTab(contents.contents, contents.index,
               selection.new_contents == contents.contents);
      }
      break;
    }
    case TabStripModelChange::kRemoved: {
      for (const auto& contents : change.GetRemove()->contents) {
        hover_tab_selector_.CancelTabTransition();
        tabstrip_->RemoveTabAt(contents.contents, contents.index,
                               contents.contents == selection.old_contents);
      }
      break;
    }
    case TabStripModelChange::kMoved: {
      auto* move = change.GetMove();
      // Cancel any pending tab transition.
      hover_tab_selector_.CancelTabTransition();

      // A move may have resulted in the pinned state changing, so pass in a
      // TabRendererData.
      tabstrip_->MoveTab(
          move->from_index, move->to_index,
          TabRendererData::FromTabInModel(model_, move->to_index));
      break;
    }
    case TabStripModelChange::kReplaced: {
      auto* replace = change.GetReplace();
      SetTabDataAt(replace->new_contents, replace->index);
      break;
    }
    case TabStripModelChange::kSelectionOnly:
      break;
  }

  if (tab_strip_model->empty())
    return;

  if (selection.active_tab_changed()) {
    // It's possible for |new_contents| to be null when the final tab in a tab
    // strip is closed.
    content::WebContents* new_contents = selection.new_contents;
    int index = selection.new_model.active();
    if (new_contents && index != TabStripModel::kNoTab) {
      TabUIHelper::FromWebContents(new_contents)
          ->set_was_active_at_least_once();
      SetTabDataAt(new_contents, index);
    }
  }

  if (selection.selection_changed())
    tabstrip_->SetSelection(selection.new_model);
}

void BrowserTabStripController::OnTabGroupChanged(
    const TabGroupChange& change) {
  switch (change.type) {
    case TabGroupChange::kCreated: {
      tabstrip_->OnGroupCreated(change.group);
      feature_engagement_tracker_->NotifyEvent(
          feature_engagement::events::kTabGroupCreated);
      break;
    }
    case TabGroupChange::kEditorOpened: {
      tabstrip_->OnGroupEditorOpened(change.group);
      break;
    }
    case TabGroupChange::kContentsChanged: {
      tabstrip_->OnGroupContentsChanged(change.group);
      break;
    }
    case TabGroupChange::kVisualsChanged: {
      const TabGroupChange::VisualsChange* visuals_delta =
          change.GetVisualsChange();
      const tab_groups::TabGroupVisualData* old_visuals =
          visuals_delta->old_visuals;
      const tab_groups::TabGroupVisualData* new_visuals =
          visuals_delta->new_visuals;
      if (old_visuals &&
          old_visuals->is_collapsed() != new_visuals->is_collapsed()) {
        gfx::Range tabs_in_group = ListTabsInGroup(change.group);
        for (auto i = tabs_in_group.start(); i < tabs_in_group.end(); ++i) {
          tabstrip_->tab_at(i)->SetVisible(!new_visuals->is_collapsed());
          if (base::FeatureList::IsEnabled(
                  features::kTabGroupsCollapseFreezing)) {
            if (visuals_delta->new_visuals->is_collapsed()) {
              tabstrip_->tab_at(i)->SetFreezingVoteToken(
                  performance_manager::freezing::EmitFreezingVoteForWebContents(
                      model_->GetWebContentsAt(i),
                      performance_manager::freezing::FreezingVoteValue::
                          kCanFreeze,
                      "Collapsed Tab Group"));
            } else {
              tabstrip_->tab_at(i)->ReleaseFreezingVoteToken();
            }
          }
        }
      }

      tabstrip_->OnGroupVisualsChanged(change.group, old_visuals, new_visuals);
      break;
    }
    case TabGroupChange::kMoved: {
      tabstrip_->OnGroupMoved(change.group);
      break;
    }
    case TabGroupChange::kClosed: {
      tabstrip_->OnGroupClosed(change.group);
      break;
    }
  }
}

void BrowserTabStripController::TabChangedAt(WebContents* contents,
                                             int model_index,
                                             TabChangeType change_type) {
  SetTabDataAt(contents, model_index);
}

void BrowserTabStripController::TabPinnedStateChanged(
    TabStripModel* tab_strip_model,
    WebContents* contents,
    int model_index) {
  SetTabDataAt(contents, model_index);
}

void BrowserTabStripController::TabBlockedStateChanged(WebContents* contents,
                                                       int model_index) {
  SetTabDataAt(contents, model_index);
}

void BrowserTabStripController::TabGroupedStateChanged(
    absl::optional<tab_groups::TabGroupId> group,
    content::WebContents* contents,
    int index) {
  tabstrip_->AddTabToGroup(std::move(group), index);
}

void BrowserTabStripController::SetTabNeedsAttentionAt(int index,
                                                       bool attention) {
  tabstrip_->SetTabNeedsAttention(index, attention);
}

BrowserNonClientFrameView* BrowserTabStripController::GetFrameView() {
  return browser_view_->frame()->GetFrameView();
}

const BrowserNonClientFrameView* BrowserTabStripController::GetFrameView()
    const {
  return browser_view_->frame()->GetFrameView();
}

void BrowserTabStripController::SetTabDataAt(content::WebContents* web_contents,
                                             int model_index) {
  tabstrip_->SetTabData(model_index,
                        TabRendererData::FromTabInModel(model_, model_index));
}

void BrowserTabStripController::AddTab(WebContents* contents,
                                       int index,
                                       bool is_active) {
  // Cancel any pending tab transition.
  hover_tab_selector_.CancelTabTransition();

  tabstrip_->AddTabAt(index, TabRendererData::FromTabInModel(model_, index),
                      is_active);
  // Try to show tab groups IPH if needed.
  if (tabstrip_->GetTabCount() >= 6) {
    feature_engagement_tracker_->NotifyEvent(
        feature_engagement::events::kSixthTabOpened);

    browser_view_->feature_promo_controller()->MaybeShowPromo(
        feature_engagement::kIPHDesktopTabGroupsNewGroupFeature);
  }

  // Try to show tab search IPH if needed.
  constexpr int kTabSearchIPHTriggerThreshold = 8;
  if (tabstrip_->GetTabCount() >= kTabSearchIPHTriggerThreshold) {
    browser_view_->feature_promo_controller()->MaybeShowPromo(
        feature_engagement::kIPHTabSearchFeature);
  }
}
