// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_uninstall_job.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/web_applications/isolation_prefs_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "components/webapps/browser/installable/installable_metrics.h"

namespace web_app {

WebAppUninstallJob::WebAppUninstallJob(
    OsIntegrationManager* os_integration_manager,
    WebAppSyncBridge* sync_bridge,
    WebAppIconManager* icon_manager,
    WebAppRegistrar* registrar,
    WebAppInstallManager* install_manager,
    WebAppInstallFinalizer* install_finalizer,
    PrefService* profile_prefs)
    : os_integration_manager_(os_integration_manager),
      sync_bridge_(sync_bridge),
      icon_manager_(icon_manager),
      registrar_(registrar),
      install_manager_(install_manager),
      install_finalizer_(install_finalizer),
      profile_prefs_(profile_prefs) {}

WebAppUninstallJob::~WebAppUninstallJob() = default;

void WebAppUninstallJob::Start(const AppId& app_id,
                               url::Origin app_origin,
                               webapps::WebappUninstallSource source,
                               ModifyAppRegistry delete_option,
                               UninstallCallback callback) {
  DCHECK(install_manager_);

  app_id_ = app_id;
  source_ = source;
  delete_option_ = delete_option;
  callback_ = std::move(callback);
  DCHECK(state_ == State::kNotStarted);
  state_ = State::kPendingDataDeletion;

  // Note: It is supported to re-start an uninstall on startup, so
  // `is_uninstalling()` is not checked. It is a class invariant that there can
  // never be more than one uninstall task operating on the same web app at the
  // same time.
  if (delete_option_ == ModifyAppRegistry::kYes) {
    ScopedRegistryUpdate update(sync_bridge_);
    WebApp* app = update->UpdateApp(app_id);
    DCHECK(app);
    app->SetIsUninstalling(true);
  }
  install_manager_->NotifyWebAppWillBeUninstalled(app_id);

  RemoveAppIsolationState(profile_prefs_, app_origin);

  // Uninstall any sub-apps the app has.
  std::vector<AppId> sub_app_ids = registrar_->GetAllSubAppIds(app_id_);
  num_pending_sub_app_uninstalls_ = sub_app_ids.size();
  for (AppId sub_app_id : sub_app_ids) {
    if (registrar_->GetAppById(sub_app_id) == nullptr)
      continue;
    install_finalizer_->UninstallExternalWebApp(
        sub_app_id, webapps::WebappUninstallSource::kSubApp,
        base::BindOnce(&WebAppUninstallJob::OnSubAppUninstalled,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  os_integration_manager_->UninstallAllOsHooks(
      app_id_, base::BindOnce(&WebAppUninstallJob::OnOsHooksUninstalled,
                              weak_ptr_factory_.GetWeakPtr()));
  icon_manager_->DeleteData(
      app_id, base::BindOnce(&WebAppUninstallJob::OnIconDataDeleted,
                             weak_ptr_factory_.GetWeakPtr()));
}

void WebAppUninstallJob::StopAppRegistryModification() {
  delete_option_ = ModifyAppRegistry::kNo;
}

void WebAppUninstallJob::OnSubAppUninstalled(bool success) {
  errors_ = errors_ || !success;
  num_pending_sub_app_uninstalls_--;
  DCHECK_GE(num_pending_sub_app_uninstalls_, 0u);
  MaybeFinishUninstall();
}

void WebAppUninstallJob::OnOsHooksUninstalled(OsHooksErrors errors) {
  DCHECK(state_ == State::kPendingDataDeletion);
  hooks_uninstalled_ = true;
  // TODO(https://crbug.com/1293234): Remove after flakiness is solved.
  DLOG_IF(ERROR, errors.any())
      << "OS integration errors for " << app_id_ << ": " << errors.to_string();
  base::UmaHistogramBoolean("WebApp.Uninstall.OsHookSuccess", errors.none());
  errors_ = errors_ || errors.any();
  MaybeFinishUninstall();
}

void WebAppUninstallJob::OnIconDataDeleted(bool success) {
  DCHECK(state_ == State::kPendingDataDeletion);
  app_data_deleted_ = true;
  // TODO(https://crbug.com/1293234): Remove after flakiness is solved.
  DLOG_IF(ERROR, !success) << "Error deleting icon data for " << app_id_;
  base::UmaHistogramBoolean("WebApp.Uninstall.IconDataSuccess", success);
  errors_ = errors_ || !success;
  MaybeFinishUninstall();
}

void WebAppUninstallJob::MaybeFinishUninstall() {
  DCHECK(state_ == State::kPendingDataDeletion);
  if (!hooks_uninstalled_ || !app_data_deleted_ ||
      num_pending_sub_app_uninstalls_ > 0) {
    return;
  }
  DCHECK_EQ(num_pending_sub_app_uninstalls_, 0u);
  state_ = State::kDone;

  base::UmaHistogramBoolean("WebApp.Uninstall.Result", !errors_);

  webapps::InstallableMetrics::TrackUninstallEvent(source_);

  switch (delete_option_) {
    case ModifyAppRegistry::kYes: {
      DCHECK_NE(registrar_->GetAppById(app_id_), nullptr);
      ScopedRegistryUpdate update(sync_bridge_);
      update->DeleteApp(app_id_);
    } break;
    case ModifyAppRegistry::kNo:
      DCHECK_EQ(registrar_->GetAppById(app_id_), nullptr);
      break;
  }
  install_manager_->NotifyWebAppUninstalled(app_id_);
  std::move(callback_).Run(errors_ ? WebAppUninstallJobResult::kError
                                   : WebAppUninstallJobResult::kSuccess);
}

}  // namespace web_app
