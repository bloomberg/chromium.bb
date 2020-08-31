// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/app_service/public/cpp/publisher_base.h"

#include <vector>

#include "base/time/time.h"

namespace apps {

PublisherBase::PublisherBase() = default;

PublisherBase::~PublisherBase() = default;

// static
apps::mojom::AppPtr PublisherBase::MakeApp(
    apps::mojom::AppType app_type,
    std::string app_id,
    apps::mojom::Readiness readiness,
    const std::string& name,
    apps::mojom::InstallSource install_source) {
  apps::mojom::AppPtr app = apps::mojom::App::New();

  app->app_type = app_type;
  app->app_id = app_id;
  app->readiness = readiness;
  app->name = name;
  app->short_name = name;

  app->last_launch_time = base::Time();
  app->install_time = base::Time();

  app->install_source = install_source;

  app->is_platform_app = apps::mojom::OptionalBool::kFalse;
  app->recommendable = apps::mojom::OptionalBool::kTrue;
  app->searchable = apps::mojom::OptionalBool::kTrue;
  app->paused = apps::mojom::OptionalBool::kFalse;

  return app;
}

void PublisherBase::FlushMojoCallsForTesting() {
  if (receiver_.is_bound()) {
    receiver_.FlushForTesting();
  }
}

void PublisherBase::Initialize(
    const mojo::Remote<apps::mojom::AppService>& app_service,
    apps::mojom::AppType app_type) {
  app_service->RegisterPublisher(receiver_.BindNewPipeAndPassRemote(),
                                 app_type);
}

void PublisherBase::Publish(
    apps::mojom::AppPtr app,
    const mojo::RemoteSet<apps::mojom::Subscriber>& subscribers) {
  for (auto& subscriber : subscribers) {
    std::vector<apps::mojom::AppPtr> apps;
    apps.push_back(app.Clone());
    subscriber->OnApps(std::move(apps));
  }
}

void PublisherBase::LaunchAppWithFiles(const std::string& app_id,
                                       apps::mojom::LaunchContainer container,
                                       int32_t event_flags,
                                       apps::mojom::LaunchSource launch_source,
                                       apps::mojom::FilePathsPtr file_paths) {
  NOTIMPLEMENTED();
}

void PublisherBase::LaunchAppWithIntent(const std::string& app_id,
                                        int32_t event_flags,
                                        apps::mojom::IntentPtr intent,
                                        apps::mojom::LaunchSource launch_source,
                                        int64_t display_id) {
  NOTIMPLEMENTED();
}

void PublisherBase::SetPermission(const std::string& app_id,
                                  apps::mojom::PermissionPtr permission) {
  NOTIMPLEMENTED();
}

void PublisherBase::Uninstall(const std::string& app_id,
                              bool clear_site_data,
                              bool report_abuse) {
  LOG(ERROR) << "Uninstall failed, could not remove the app with id " << app_id;
}

void PublisherBase::PauseApp(const std::string& app_id) {
  NOTIMPLEMENTED();
}

void PublisherBase::UnpauseApps(const std::string& app_id) {
  NOTIMPLEMENTED();
}

void PublisherBase::GetMenuModel(const std::string& app_id,
                                 apps::mojom::MenuType menu_type,
                                 int64_t display_id,
                                 GetMenuModelCallback callback) {
  NOTIMPLEMENTED();
}

void PublisherBase::OpenNativeSettings(const std::string& app_id) {
  NOTIMPLEMENTED();
}

void PublisherBase::OnPreferredAppSet(
    const std::string& app_id,
    apps::mojom::IntentFilterPtr intent_filter,
    apps::mojom::IntentPtr intent,
    apps::mojom::ReplacedAppPreferencesPtr replaced_app_preferences) {
  NOTIMPLEMENTED();
}

}  // namespace apps
