// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_SYNC_WEB_VIEW_SYNC_CLIENT_H_
#define IOS_WEB_VIEW_INTERNAL_SYNC_WEB_VIEW_SYNC_CLIENT_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/browser_sync/browser_sync_client.h"
#include "components/browser_sync/profile_sync_components_factory_impl.h"
#include "components/password_manager/core/browser/password_store.h"
#include "ios/web_view/internal/web_view_browser_state.h"

namespace ios_web_view {

class WebViewSyncClient : public browser_sync::BrowserSyncClient {
 public:
  static std::unique_ptr<WebViewSyncClient> Create(
      WebViewBrowserState* browser_state);

  explicit WebViewSyncClient(
      autofill::AutofillWebDataService* profile_web_data_service,
      autofill::AutofillWebDataService* account_web_data_service,
      password_manager::PasswordStore* profile_password_store,
      password_manager::PasswordStore* account_password_store,
      PrefService* pref_service,
      signin::IdentityManager* identity_manager,
      syncer::ModelTypeStoreService* model_type_store_service,
      syncer::DeviceInfoSyncService* device_info_sync_service,
      invalidation::InvalidationService* invalidation_service);
  ~WebViewSyncClient() override;

  // BrowserSyncClient implementation.
  PrefService* GetPrefService() override;
  signin::IdentityManager* GetIdentityManager() override;
  base::FilePath GetLocalSyncBackendFolder() override;
  syncer::ModelTypeStoreService* GetModelTypeStoreService() override;
  syncer::DeviceInfoSyncService* GetDeviceInfoSyncService() override;
  bookmarks::BookmarkModel* GetBookmarkModel() override;
  favicon::FaviconService* GetFaviconService() override;
  history::HistoryService* GetHistoryService() override;
  send_tab_to_self::SendTabToSelfSyncService* GetSendTabToSelfSyncService()
      override;
  sync_sessions::SessionSyncService* GetSessionSyncService() override;
  base::RepeatingClosure GetPasswordStateChangedCallback() override;
  syncer::DataTypeController::TypeVector CreateDataTypeControllers(
      syncer::SyncService* sync_service) override;
  invalidation::InvalidationService* GetInvalidationService() override;
  syncer::TrustedVaultClient* GetTrustedVaultClient() override;
  BookmarkUndoService* GetBookmarkUndoService() override;
  scoped_refptr<syncer::ExtensionsActivity> GetExtensionsActivity() override;
  base::WeakPtr<syncer::SyncableService> GetSyncableServiceForType(
      syncer::ModelType type) override;
  base::WeakPtr<syncer::ModelTypeControllerDelegate>
  GetControllerDelegateForModelType(syncer::ModelType type) override;
  scoped_refptr<syncer::ModelSafeWorker> CreateModelWorkerForGroup(
      syncer::ModelSafeGroup group) override;
  syncer::SyncApiComponentFactory* GetSyncApiComponentFactory() override;
  syncer::SyncTypePreferenceProvider* GetPreferenceProvider() override;

 private:
  autofill::AutofillWebDataService* profile_web_data_service_;
  autofill::AutofillWebDataService* account_web_data_service_;
  password_manager::PasswordStore* profile_password_store_;
  password_manager::PasswordStore* account_password_store_;
  PrefService* pref_service_;
  signin::IdentityManager* identity_manager_;
  syncer::ModelTypeStoreService* model_type_store_service_;
  syncer::DeviceInfoSyncService* device_info_sync_service_;
  invalidation::InvalidationService* invalidation_service_;

  // TODO(crbug.com/915154): Revert to SyncApiComponentFactory once common
  // controller creation is moved elsewhere.
  std::unique_ptr<browser_sync::ProfileSyncComponentsFactoryImpl>
      component_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebViewSyncClient);
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_SYNC_WEB_VIEW_SYNC_CLIENT_H_
