// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ECHE_APP_ECHE_APP_MANAGER_FACTORY_H_
#define CHROME_BROWSER_ASH_ECHE_APP_ECHE_APP_MANAGER_FACTORY_H_

#include "ash/webui/eche_app_ui/launch_app_helper.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/image/image.h"

class Profile;

namespace ash {
namespace eche_app {

class EcheAppManager;
class EcheAppNotificationController;
class SystemInfo;

class LaunchedAppInfo {
 public:
  class Builder {
   public:
    Builder();
    ~Builder();

    std::unique_ptr<LaunchedAppInfo> Build() {
      return base::WrapUnique(
          new LaunchedAppInfo(package_name_, visible_name_, user_id_, icon_));
    }
    Builder& SetPackageName(const std::string& package_name) {
      package_name_ = package_name;
      return *this;
    }

    Builder& SetVisibleName(const std::u16string& visible_name) {
      visible_name_ = visible_name;
      return *this;
    }

    Builder& SetUserId(const absl::optional<int64_t>& user_id) {
      user_id_ = user_id;
      return *this;
    }

    Builder& SetIcon(const gfx::Image& icon) {
      icon_ = icon;
      return *this;
    }

   private:
    std::string package_name_;
    std::u16string visible_name_;
    absl::optional<int64_t> user_id_;
    gfx::Image icon_;
  };

  LaunchedAppInfo();
  LaunchedAppInfo(const LaunchedAppInfo&) = delete;
  LaunchedAppInfo& operator=(const LaunchedAppInfo&) = delete;
  ~LaunchedAppInfo();

  std::string package_name() const { return package_name_; }
  std::u16string visible_name() const { return visible_name_; }
  absl::optional<int64_t> user_id() const { return user_id_; }
  gfx::Image icon() const { return icon_; }

 protected:
  LaunchedAppInfo(const std::string& package_name,
                  const std::u16string& visible_name,
                  const absl::optional<int64_t>& user_id,
                  const gfx::Image& icon);

 private:
  std::string package_name_;
  std::u16string visible_name_;
  absl::optional<int64_t> user_id_;
  gfx::Image icon_;
};

// Factory to create a single EcheAppManager.
class EcheAppManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  static EcheAppManager* GetForProfile(Profile* profile);
  static EcheAppManagerFactory* GetInstance();
  static void ShowNotification(
      base::WeakPtr<EcheAppManagerFactory> weak_ptr,
      Profile* profile,
      const absl::optional<std::u16string>& title,
      const absl::optional<std::u16string>& message,
      std::unique_ptr<LaunchAppHelper::NotificationInfo> info);
  static void CloseEche(Profile* profile);
  static void LaunchEcheApp(Profile* profile,
                            const absl::optional<int64_t>& notification_id,
                            const std::string& package_name,
                            const std::u16string& visible_name,
                            const absl::optional<int64_t>& user_id,
                            const gfx::Image& icon);

  void SetLastLaunchedAppInfo(
      std::unique_ptr<LaunchedAppInfo> last_launched_app_info);
  std::unique_ptr<LaunchedAppInfo> GetLastLaunchedAppInfo();
  void CloseConnectionOrLaunchErrorNotifications();

  EcheAppManagerFactory(const EcheAppManagerFactory&) = delete;
  EcheAppManagerFactory& operator=(const EcheAppManagerFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<EcheAppManagerFactory>;

  EcheAppManagerFactory();
  ~EcheAppManagerFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;

  std::unique_ptr<SystemInfo> GetSystemInfo(Profile* profile) const;

  std::unique_ptr<LaunchedAppInfo> last_launched_app_info_;

  std::unique_ptr<EcheAppNotificationController> notification_controller_;
  base::WeakPtrFactory<EcheAppManagerFactory> weak_ptr_factory_{this};
};

}  // namespace eche_app
}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
namespace eche_app {
using ::ash::eche_app::EcheAppManagerFactory;
}
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_ECHE_APP_ECHE_APP_MANAGER_FACTORY_H_
