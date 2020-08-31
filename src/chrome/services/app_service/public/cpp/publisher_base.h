// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_APP_SERVICE_PUBLIC_CPP_PUBLISHER_BASE_H_
#define CHROME_SERVICES_APP_SERVICE_PUBLIC_CPP_PUBLISHER_BASE_H_

#include <string>

#include "chrome/services/app_service/public/mojom/app_service.mojom.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace apps {

// An publisher parent class (in the App Service sense) for all app publishers.
// This class has NOTIMPLEMENTED() implementations of mandatory methods from the
// apps::mojom::Publisher class to simplify the process of adding a new
// publisher.
//
// See chrome/services/app_service/README.md.
class PublisherBase : public apps::mojom::Publisher {
 public:
  PublisherBase();
  ~PublisherBase() override;

  PublisherBase(const PublisherBase&) = delete;
  PublisherBase& operator=(const PublisherBase&) = delete;

  static apps::mojom::AppPtr MakeApp(apps::mojom::AppType app_type,
                                     std::string app_id,
                                     apps::mojom::Readiness readiness,
                                     const std::string& name,
                                     apps::mojom::InstallSource install_source);

  void FlushMojoCallsForTesting();

 protected:
  void Initialize(const mojo::Remote<apps::mojom::AppService>& app_service,
                  apps::mojom::AppType app_type);

  // Publish |app| to all subscribers in |subscribers|. Should be called
  // whenever the app represented by |app| undergoes some state change to inform
  // subscribers of the change.
  void Publish(apps::mojom::AppPtr app,
               const mojo::RemoteSet<apps::mojom::Subscriber>& subscribers);

  mojo::Receiver<apps::mojom::Publisher>& receiver() { return receiver_; }

 private:
  // apps::mojom::Publisher overrides.
  void LaunchAppWithFiles(const std::string& app_id,
                          apps::mojom::LaunchContainer container,
                          int32_t event_flags,
                          apps::mojom::LaunchSource launch_source,
                          apps::mojom::FilePathsPtr file_paths) override;
  void LaunchAppWithIntent(const std::string& app_id,
                           int32_t event_flags,
                           apps::mojom::IntentPtr intent,
                           apps::mojom::LaunchSource launch_source,
                           int64_t display_id) override;
  void SetPermission(const std::string& app_id,
                     apps::mojom::PermissionPtr permission) override;
  void Uninstall(const std::string& app_id,
                 bool clear_site_data,
                 bool report_abuse) override;
  void PauseApp(const std::string& app_id) override;
  void UnpauseApps(const std::string& app_id) override;
  void GetMenuModel(const std::string& app_id,
                    apps::mojom::MenuType menu_type,
                    int64_t display_id,
                    GetMenuModelCallback callback) override;
  void OpenNativeSettings(const std::string& app_id) override;
  void OnPreferredAppSet(
      const std::string& app_id,
      apps::mojom::IntentFilterPtr intent_filter,
      apps::mojom::IntentPtr intent,
      apps::mojom::ReplacedAppPreferencesPtr replaced_app_preferences) override;

  mojo::Receiver<apps::mojom::Publisher> receiver_{this};
};

}  // namespace apps

#endif  // CHROME_SERVICES_APP_SERVICE_PUBLIC_CPP_PUBLISHER_BASE_H_
