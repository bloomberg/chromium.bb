// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_CHROME_APP_LIST_MODEL_UPDATER_H_
#define CHROME_BROWSER_UI_APP_LIST_CHROME_APP_LIST_MODEL_UPDATER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/model/app_list_model_observer.h"
#include "ash/app_list/model/search/search_model.h"
#include "ash/public/cpp/app_list/app_list_model_delegate.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/app_list/app_list_model_updater.h"

namespace app_list {
namespace reorder {
class AppListReorderDelegate;
struct ReorderParam;
}  // namespace reorder
}  // namespace app_list

class ChromeAppListItem;
class ChromeAppListItemManager;

class ChromeAppListModelUpdater : public AppListModelUpdater,
                                  public ash::AppListModelDelegate,
                                  public ash::AppListModelObserver {
 public:
  ChromeAppListModelUpdater(
      Profile* profile,
      app_list::reorder::AppListReorderDelegate* order_delegate,
      app_list::AppListSyncModelSanitizer* sync_model_sanitizer);
  ChromeAppListModelUpdater(const ChromeAppListModelUpdater&) = delete;
  ChromeAppListModelUpdater& operator=(const ChromeAppListModelUpdater&) =
      delete;
  ~ChromeAppListModelUpdater() override;

  void SetActive(bool active) override;

  // AppListModelUpdater:
  void AddItem(std::unique_ptr<ChromeAppListItem> app_item) override;
  void AddAppItemToFolder(std::unique_ptr<ChromeAppListItem> app_item,
                          const std::string& folder_id,
                          bool add_from_local) override;
  void RemoveItem(const std::string& id, bool is_uninstall) override;
  void SetStatus(ash::AppListModelStatus status) override;
  void SetSearchEngineIsGoogle(bool is_google) override;
  void UpdateSearchBox(const std::u16string& text,
                       bool initiated_by_user) override;
  void PublishSearchResults(
      const std::vector<ChromeSearchResult*>& results,
      const std::vector<ash::AppListSearchResultCategory>& categories) override;
  void ClearSearchResults() override;
  std::vector<ChromeSearchResult*> GetPublishedSearchResultsForTest() override;

  // Methods only used by ChromeAppListItem that talk to ash directly.
  void SetItemIconVersion(const std::string& id, int icon_version) override;
  void SetItemIcon(const std::string& id, const gfx::ImageSkia& icon) override;
  void SetItemName(const std::string& id, const std::string& name) override;
  void SetAppStatus(const std::string& id, ash::AppStatus app_status) override;
  void SetItemPosition(const std::string& id,
                       const syncer::StringOrdinal& new_position) override;
  void SetItemIsPersistent(const std::string& id, bool is_persistent) override;
  void SetIsNewInstall(const std::string& id, bool is_new_install) override;
  void SetItemFolderId(const std::string& id,
                       const std::string& folder_id) override;
  void SetNotificationBadgeColor(const std::string& id,
                                 const SkColor color) override;
  void SetIconColor(const std::string& id,
                    const ash::IconColor icon_color) override;

  // Methods only used by ChromeSearchResult that talk to ash directly.
  void SetSearchResultMetadata(
      const std::string& id,
      std::unique_ptr<ash::SearchResultMetadata> metadata) override;

  void ActivateChromeItem(const std::string& id, int event_flags) override;
  void LoadAppIcon(const std::string& id) override;

  // Methods for item querying.
  ChromeAppListItem* FindItem(const std::string& id) override;
  std::vector<const ChromeAppListItem*> GetItems() const override;
  std::set<std::string> GetTopLevelItemIds() const override;
  size_t ItemCount() override;
  std::vector<ChromeAppListItem*> GetTopLevelItems() const override;
  ChromeAppListItem* ItemAtForTest(size_t index) override;
  ChromeAppListItem* FindFolderItem(const std::string& folder_id) override;
  bool FindItemIndexForTest(const std::string& id, size_t* index) override;
  bool SearchEngineIsGoogle() override;
  void GetIdToAppListIndexMap(GetIdToAppListIndexMapCallback callback) override;
  size_t BadgedItemCount() override;
  void GetContextMenuModel(const std::string& id,
                           bool add_sort_options,
                           GetMenuModelCallback callback) override;
  syncer::StringOrdinal GetPositionBeforeFirstItem() const override;

  // Methods for AppListSyncableService:
  void UpdateAppItemFromSyncItem(
      app_list::AppListSyncableService::SyncItem* sync_item,
      bool update_name,
      bool update_folder) override;
  void NotifyProcessSyncChangesFinished() override;

  void OnAppListHidden() override;

  void AddObserver(AppListModelUpdaterObserver* observer) override;
  void RemoveObserver(AppListModelUpdaterObserver* observer) override;

  // AppListModelObserver:
  void OnAppListItemAdded(ash::AppListItem* item) override;
  void OnAppListItemUpdated(ash::AppListItem* item) override;
  void OnAppListItemWillBeDeleted(ash::AppListItem* item) override;

  // AppListModelDelegate:
  void RequestPositionUpdate(std::string id,
                             const syncer::StringOrdinal& new_position,
                             ash::RequestPositionUpdateReason reason) override;
  void RequestMoveItemToFolder(std::string id,
                               const std::string& folder_id) override;
  void RequestMoveItemToRoot(std::string id,
                             syncer::StringOrdinal target_position) override;
  std::string RequestFolderCreation(std::string target_merge_id,
                                    std::string item_to_merge_id) override;
  void RequestFolderRename(std::string folder_id,
                           const std::string& new_name) override;
  void RequestAppListSort(ash::AppListSortOrder order) override;
  void RequestAppListSortRevert() override;

  // Returns the temporary sort order.
  ash::AppListSortOrder GetTemporarySortOrderForTest() const;

  // Returns true if the app list is under temporary sort.
  bool is_under_temporary_sort() const { return !!temporary_sort_manager_; }

  ash::AppListModel* model_for_test() { return &model_; }

 private:
  friend class TemporaryAppListSortTest;

  class TemporarySortManager;

  enum class ItemChangeType {
    // An item is added.
    kAdd,

    // An item is updated.
    kUpdate,

    // An item will be deleted.
    kDelete
  };

  // Notifies observers of the change on `chrome_item` when temporary app list
  // sort is not active.
  void MaybeNotifyObserversOfItemChange(ChromeAppListItem* chrome_item,
                                        ItemChangeType type);

  // Lists the action that can be performed when app list exits the temporary
  // sort status.
  enum class EndAction {
    // Commit temporary positions and update the permanent order with the
    // temporary order.
    kCommit,

    // Revert temporary positions and the permanent order does not change.
    kRevert,

    // Commit temporary positions and clear the permanent order.
    kCommitAndClearSort
  };

  // Ends temporary sort status and performs the specified action.
  void EndTemporarySortAndTakeAction(EndAction action);

  // Commits item positions under the temporary sort.
  void CommitTemporaryPositions();

  // Calculates the reorder params for reverting the temporary order.
  std::vector<app_list::reorder::ReorderParam>
  CalculateReorderParamsForRevertOrder() const;

  // If folder with the provided ID has a single child, it reparents the child
  // to the root app list.
  void ClearFolderIfItHasSingleChild(const std::string& folder_id);

  // Updates the item positions in the ash side. `reorder_params` specifies
  // target positions.
  void UpdateItemPositionWithReorderParam(
      const std::vector<app_list::reorder::ReorderParam>& reorder_params);

  // Resets the pref sort order to be kCustom when the app list is not under
  // temporary sorting. `event` indicates the reason leading to reset.
  void ResetPrefSortOrderInNonTemporaryMode(ash::AppListOrderUpdateEvent event);

  // Indicates the profile that the model updater is associated with.
  Profile* const profile_ = nullptr;

  // Provides the access to the methods for ordering app list items.
  app_list::reorder::AppListReorderDelegate* const order_delegate_;
  app_list::AppListSyncModelSanitizer* const sync_model_sanitizer_;

  // A helper class to manage app list items. It never talks to ash.
  std::unique_ptr<ChromeAppListItemManager> item_manager_;

  ash::AppListModel model_;
  ash::SearchModel search_model_;

  bool is_active_ = false;

  // The most recently list of search results.
  std::vector<ChromeSearchResult*> published_results_;
  base::ObserverList<AppListModelUpdaterObserver> observers_;
  bool search_engine_is_google_ = false;

  // Set when sort is triggered and reset when exiting the temporary sort
  // status.
  std::unique_ptr<TemporarySortManager> temporary_sort_manager_;

  base::WeakPtrFactory<ChromeAppListModelUpdater> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_APP_LIST_CHROME_APP_LIST_MODEL_UPDATER_H_
