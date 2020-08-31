// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_LIST_SYNCABLE_SERVICE_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_LIST_SYNCABLE_SERVICE_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/one_shot_event.h"
#include "build/build_config.h"
#include "chrome/browser/sync/glue/sync_start_util.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/model/string_ordinal.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/model/sync_change_processor.h"
#include "components/sync/model/sync_error_factory.h"
#include "components/sync/model/syncable_service.h"
#include "components/sync/protocol/app_list_specifics.pb.h"

class AppListModelUpdater;
class AppServiceAppModelBuilder;
class ChromeAppListItem;
class Profile;

namespace extensions {
class ExtensionRegistry;
class ExtensionSystem;
}

namespace sync_pb {
class AppListSpecifics;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace app_list {

// Keyed Service that owns, stores, and syncs an AppListModel for a profile.
class AppListSyncableService : public syncer::SyncableService,
                               public KeyedService {
 public:
  struct SyncItem {
    SyncItem(const std::string& id,
             sync_pb::AppListSpecifics::AppListItemType type);
    ~SyncItem();
    const std::string item_id;
    sync_pb::AppListSpecifics::AppListItemType item_type;
    std::string item_name;
    std::string parent_id;
    syncer::StringOrdinal item_ordinal;
    syncer::StringOrdinal item_pin_ordinal;

    std::string ToString() const;
  };

  class Observer {
   public:
    // Notifies that sync model was updated.
    virtual void OnSyncModelUpdated() = 0;

   protected:
    virtual ~Observer() = default;
  };

  // An app list model updater factory function used by tests.
  using ModelUpdaterFactoryCallback =
      base::Callback<std::unique_ptr<AppListModelUpdater>()>;

  // Sets and resets an app list model updater factory function for tests.
  class ScopedModelUpdaterFactoryForTest {
   public:
    explicit ScopedModelUpdaterFactoryForTest(
        const ModelUpdaterFactoryCallback& factory);
    ~ScopedModelUpdaterFactoryForTest();

   private:
    ModelUpdaterFactoryCallback factory_;

    DISALLOW_COPY_AND_ASSIGN(ScopedModelUpdaterFactoryForTest);
  };

  using SyncItemMap = std::map<std::string, std::unique_ptr<SyncItem>>;

  // Populates the model when |profile|'s extension system is ready.
  explicit AppListSyncableService(Profile* profile);

  ~AppListSyncableService() override;

  // Registers prefs to support local storage.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Some sync behavior depends on whether or not an app was installed by
  // default (as opposed to e.g. installed via explicit user action). Some
  // tests want the AppListSyncableService to consider an app to be installed
  // by default, without going through the heavyweight process of completely
  // installing an app. These functions facilitate that.
  static bool AppIsDefaultForTest(Profile* profile, const std::string& id);
  static void SetAppIsDefaultForTest(Profile* profile, const std::string& id);

  // Adds |item| to |sync_items_| and |model_|. If a sync item already exists,
  // updates the existing sync item instead.
  void AddItem(std::unique_ptr<ChromeAppListItem> app_item);

  // Removes sync item matching |id|.
  void RemoveItem(const std::string& id);

  // Removes sync item matching |id| after item uninstall.
  void RemoveUninstalledItem(const std::string& id);

  // Called when properties of an item may have changed, e.g. default/oem state.
  void UpdateItem(const ChromeAppListItem* app_item);

  // Returns the existing sync item matching |id| or NULL.
  const SyncItem* GetSyncItem(const std::string& id) const;

  // Transfers app attributes, such as parent folder id, position in App
  // Launcher and pin position on the shelf from one app to another app. Target
  // app defined by |to_app_id| is not required to be present at call time. In
  // which case attributes would be applied once the target app appears on the
  // device. Note, pending attributes are not preserved between the user
  // sessions. This functionality is primarily used for migrating app in case
  // app id is changed but it is required to preserve position in App Launcher
  // and in shelf.
  // Returns true on success and false in case app defined by |from_app_id|
  // does not exist.
  bool TransferItemAttributes(const std::string& from_app_id,
                              const std::string& to_app_id);

  // Sets the name of the folder for OEM apps.
  void SetOemFolderName(const std::string& name);

  // Returns optional pin position for the app specified by |app_id|. If app is
  // not synced or does not have associated pin position then empty ordinal is
  // returned.
  syncer::StringOrdinal GetPinPosition(const std::string& app_id);

  // Sets pin position and how it is pinned for the app specified by |app_id|.
  // Empty |item_pin_ordinal| indicates that the app has no pin.
  void SetPinPosition(const std::string& app_id,
                      const syncer::StringOrdinal& item_pin_ordinal);

  // Gets the app list model updater.
  AppListModelUpdater* GetModelUpdater();

  // Returns true if this service was initialized.
  bool IsInitialized() const;

  // Signalled when AppListSyncableService is Initialized.
  const base::OneShotEvent& on_initialized() const { return on_initialized_; }

  // Returns true if sync was started.
  bool IsSyncing() const;

  // Registers new observers and makes sure that service is started.
  void AddObserverAndStart(Observer* observer);
  void RemoveObserver(Observer* observer);

  Profile* profile() { return profile_; }
  size_t GetNumSyncItemsForTest();
  const std::string& GetOemFolderNameForTest() const {
    return oem_folder_name_;
  }

  void InstallDefaultPageBreaksForTest();

  const SyncItemMap& sync_items() const { return sync_items_; }

  // syncer::SyncableService
  void WaitUntilReadyToSync(base::OnceClosure done) override;
  base::Optional<syncer::ModelError> MergeDataAndStartSyncing(
      syncer::ModelType type,
      const syncer::SyncDataList& initial_sync_data,
      std::unique_ptr<syncer::SyncChangeProcessor> sync_processor,
      std::unique_ptr<syncer::SyncErrorFactory> error_handler) override;
  void StopSyncing(syncer::ModelType type) override;
  syncer::SyncDataList GetAllSyncDataForTesting() const;
  base::Optional<syncer::ModelError> ProcessSyncChanges(
      const base::Location& from_here,
      const syncer::SyncChangeList& change_list) override;

  // KeyedService
  void Shutdown() override;

 private:
  class ModelUpdaterObserver;

  // Builds the model once ExtensionService is ready.
  void BuildModel();

  // Returns true if sync has restarted, otherwise runs |flare_|.
  bool SyncStarted();

  // If |app_item| matches an existing sync item, returns it. Otherwise adds
  // |app_item| to |sync_items_| and returns the new item. If |app_item| is
  // invalid returns NULL.
  SyncItem* FindOrAddSyncItem(const ChromeAppListItem* app_item);

  // Creates a sync item for |app_item| and sends an ADD SyncChange event.
  SyncItem* CreateSyncItemFromAppItem(const ChromeAppListItem* app_item);

  // If a sync item for |app_item| already exists, update |app_item| from the
  // sync item, otherwise create a new sync item from |app_item|.
  void AddOrUpdateFromSyncItem(const ChromeAppListItem* app_item);

  // Either uninstalling a default app or remove the REMOVE_DEFAULT sync item.
  // Returns true if the app is removed. Otherwise deletes the existing sync
  // item and returns false.
  bool RemoveDefaultApp(const ChromeAppListItem* item, SyncItem* sync_item);

  // Returns whether the delete-sync-item request was for a default app. If
  // true, the |sync_item| is set to REMOVE_DEFAULT and bounced back to the
  // sync server. The caller should abort deleting the |sync_item|.
  bool InterceptDeleteDefaultApp(SyncItem* sync_item);

  // Deletes a sync item from |sync_items_| and sends a DELETE action.
  void DeleteSyncItem(const std::string& item_id);

  // Updates existing entry in |sync_items_| from |app_item|.
  void UpdateSyncItem(const ChromeAppListItem* app_item);

  // Removes sync item matching |id|.
  void RemoveSyncItem(const std::string& id);

  // Updates folder items that may get created during initial sync.
  void ResolveFolderPositions();

  // Removes any empty SyncItem folders and deletes them from sync. Called
  // after a sync item is removed (which may result in an empty folder).
  void PruneEmptySyncFolders();

  // Creates or updates a SyncItem from |specifics|. Returns true if a new item
  // was created.
  // TODO(crbug.com/1057577): Change return type to void.
  bool ProcessSyncItemSpecifics(const sync_pb::AppListSpecifics& specifics);

  // Handles a newly created sync item (e.g. creates a new AppItem and adds it
  // to the model or uninstalls a deleted default item.
  void ProcessNewSyncItem(SyncItem* sync_item);

  // Handles an existing sync item.
  void ProcessExistingSyncItem(SyncItem* sync_item);

  // Sends ADD or CHANGED for sync item.
  void SendSyncChange(SyncItem* sync_item,
                      syncer::SyncChange::SyncChangeType sync_change_type);

  // Returns an existing SyncItem corresponding to |item_id| or NULL.
  SyncItem* FindSyncItem(const std::string& item_id);

  // Creates a new sync item for |item_id|.
  SyncItem* CreateSyncItem(
      const std::string& item_id,
      sync_pb::AppListSpecifics::AppListItemType item_type);

  // Deletes a SyncItem matching |specifics|.
  void DeleteSyncItemSpecifics(const sync_pb::AppListSpecifics& specifics);

  // Gets the preferred location for the OEM folder. It may return an invalid
  // position and the final OEM folder position will be determined in the
  // AppListModel.
  syncer::StringOrdinal GetPreferredOemFolderPos();

  // Returns true if an extension matching |id| exists and was installed by
  // an OEM (extension->was_installed_by_oem() is true).
  bool AppIsOem(const std::string& id);

  // Initializes sync items from the local storage while sync service is not
  // enabled.
  void InitFromLocalStorage();

  // Helper that notifies observers that sync model has been updated.
  void NotifyObserversSyncUpdated();

  // Handles model update start/finish.
  void HandleUpdateStarted();
  void HandleUpdateFinished();

  // Returns true if extension service is ready.
  bool IsExtensionServiceReady() const;

  // Returns a list of top level sync items sorted by item ordinal.
  std::vector<SyncItem*> GetSortedTopLevelSyncItems() const;

  // Remove leading, trailing and duplicate "page break" items in sorted top
  // level item list.
  void PruneRedundantPageBreakItems();

  // Installs the default page break items. This is only called for first time
  // users.
  void InstallDefaultPageBreaks();

  // Applies sync changes to the local item.
  void UpdateSyncItemFromSync(const sync_pb::AppListSpecifics& specifics,
                              AppListSyncableService::SyncItem* item);

  // Applies changes from the local item to sync item.
  bool UpdateSyncItemFromAppItem(const ChromeAppListItem* app_item,
                                 AppListSyncableService::SyncItem* sync_item);

  // Sets position, folder id and pin position for the app |app_id|. Attributes
  // are taken from the sync item |attributes|. This generates sync update and
  // notifies app models and Chrome shelf controller that are automatically
  // refreshed.
  void ApplyAppAttributes(const std::string& app_id,
                          std::unique_ptr<SyncItem> attributes);

  Profile* profile_;
  extensions::ExtensionSystem* extension_system_;
  extensions::ExtensionRegistry* extension_registry_;
  std::unique_ptr<AppListModelUpdater> model_updater_;
  std::unique_ptr<ModelUpdaterObserver> model_updater_observer_;

  std::unique_ptr<AppServiceAppModelBuilder> app_service_apps_builder_;
  std::unique_ptr<syncer::SyncChangeProcessor> sync_processor_;
  std::unique_ptr<syncer::SyncErrorFactory> sync_error_handler_;
  SyncItemMap sync_items_;
  // Map that keeps pending request to transfer attributes from one app to
  // another.
  SyncItemMap pending_transfer_map_;
  syncer::SyncableService::StartSyncFlare flare_;
  bool initial_sync_data_processed_;
  bool first_app_list_sync_;
  std::string oem_folder_name_;
  // Callback to install default page breaks.
  // Only set for first time user for tablet form devices.
  base::OnceClosure install_default_page_breaks_;
  base::OnceClosure wait_until_ready_to_sync_cb_;

  // List of observers.
  base::ObserverList<Observer>::Unchecked observer_list_;
  base::OneShotEvent on_initialized_;

  base::WeakPtrFactory<AppListSyncableService> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AppListSyncableService);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_LIST_SYNCABLE_SERVICE_H_
