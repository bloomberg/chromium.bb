// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/arc/arc_vpn_provider_manager.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_test_util.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "components/arc/test/fake_app_instance.h"

constexpr char kVpnAppName[] = "vpn.app.name";
constexpr char kVpnAppNameUpdate[] = "vpn.app.name.update";
constexpr char kVpnAppActivity[] = "vpn.app.activity";
constexpr char kVpnPackageName[] = "vpn.app.package.name";
constexpr char kNonVpnAppName[] = "non.vpn.app.name";
constexpr char kNonVpnAppActivity[] = "non.vpn.app.activity";
constexpr char kNonVpnPackageName[] = "non.vpn.app.package.name";

class ArcVpnObserver : public app_list::ArcVpnProviderManager::Observer {
 public:
  ArcVpnObserver() = default;
  ~ArcVpnObserver() override = default;

  int GetArcVpnProviderUpdateCount(const std::string& package_name) {
    return arc_vpn_provider_counter_[package_name];
  }

  // ArcVpnProviderManager::Observer::
  void OnArcVpnProvidersRefreshed(
      const std::vector<
          std::unique_ptr<app_list::ArcVpnProviderManager::ArcVpnProvider>>&
          arc_vpn_providers) override {
    for (const auto& arc_vpn_provider : arc_vpn_providers)
      arc_vpn_provider_counter_[arc_vpn_provider->package_name] = 1;
  }

  void OnArcVpnProviderUpdated(app_list::ArcVpnProviderManager::ArcVpnProvider*
                                   arc_vpn_provider) override {
    arc_vpn_provider_counter_[arc_vpn_provider->package_name] += 1;
  }

  void OnArcVpnProviderRemoved(const std::string& pacakge_name) override {
    arc_vpn_provider_counter_.erase(pacakge_name);
  }

  const std::map<std::string, int>& arc_vpn_provider_counter() const {
    return arc_vpn_provider_counter_;
  }

 private:
  std::map<std::string, int> arc_vpn_provider_counter_;

  DISALLOW_COPY_AND_ASSIGN(ArcVpnObserver);
};

class ArcVpnProviderTest : public AppListTestBase {
 public:
  ArcVpnProviderTest() = default;
  ~ArcVpnProviderTest() override = default;

  // AppListTestBase:
  void SetUp() override {
    AppListTestBase::SetUp();
    arc_test_.SetUp(profile());
    app_list::ArcVpnProviderManager* arc_vpn_provider_manager =
        app_list::ArcVpnProviderManager::Get(profile());
    DCHECK(arc_vpn_provider_manager);
    arc_vpn_provider_manager->AddObserver(&arc_vpn_observer_);
  }

  void TearDown() override {
    app_list::ArcVpnProviderManager* arc_vpn_provider_manager =
        app_list::ArcVpnProviderManager::Get(profile());
    DCHECK(arc_vpn_provider_manager);
    arc_vpn_provider_manager->RemoveObserver(&arc_vpn_observer_);
    arc_test_.TearDown();
    AppListTestBase::TearDown();
  }

  void AddArcApp(const std::string& app_name,
                 const std::string& package_name,
                 const std::string& activity) {
    arc::mojom::AppInfo app_info;
    app_info.name = app_name;
    app_info.package_name = package_name;
    app_info.activity = activity;

    app_instance()->SendPackageAppListRefreshed(package_name, {app_info});
  }

  void AddArcPackage(const std::string& package_name, bool vpn_provider) {
    app_instance()->SendPackageAdded(arc::mojom::ArcPackageInfo::New(
        package_name, 0 /* package_version */, 0 /* last_backup_android_id */,
        0 /* last_backup_time */, false /* sync */, false /* system */,
        vpn_provider));
  }

  void RemovePackage(const std::string& package_name) {
    app_instance()->UninstallPackage(package_name);
  }

  ArcAppTest& arc_test() { return arc_test_; }

  arc::FakeAppInstance* app_instance() { return arc_test_.app_instance(); }

  ArcVpnObserver& arc_vpn_observer() { return arc_vpn_observer_; }

 private:
  ArcAppTest arc_test_;
  ArcVpnObserver arc_vpn_observer_;
};

TEST_F(ArcVpnProviderTest, ArcVpnProviderUpdateCount) {
  // Starts with no arc vpn provider.
  app_instance()->SendRefreshAppList(std::vector<arc::mojom::AppInfo>());
  app_instance()->SendRefreshPackageList({});

  // Arc Vpn Observer should observe Arc Vpn app installation.
  AddArcApp(kVpnAppName, kVpnPackageName, kVpnAppActivity);
  AddArcPackage(kVpnPackageName, true);

  EXPECT_EQ(1u, arc_vpn_observer().arc_vpn_provider_counter().size());
  EXPECT_EQ(1,
            arc_vpn_observer().GetArcVpnProviderUpdateCount(kVpnPackageName));

  // Arc Vpn Observer ignores non Arc Vpn app installation.
  AddArcApp(kNonVpnAppName, kNonVpnPackageName, kNonVpnAppActivity);
  AddArcPackage(kNonVpnPackageName, false);
  EXPECT_EQ(1u, arc_vpn_observer().arc_vpn_provider_counter().size());

  // Arc Vpn Observer observes Arc Vpn app launch time and app name updates.
  std::string vpnAppId =
      ArcAppListPrefs::GetAppId(kVpnPackageName, kVpnAppActivity);
  arc_test().arc_app_list_prefs()->SetLastLaunchTime(vpnAppId);
  EXPECT_EQ(2,
            arc_vpn_observer().GetArcVpnProviderUpdateCount(kVpnPackageName));
  AddArcApp(kVpnAppNameUpdate, kVpnPackageName, kVpnAppActivity);
  // Update Arc Vpn app name. No new app installed.
  EXPECT_EQ(1u, arc_vpn_observer().arc_vpn_provider_counter().size());
  EXPECT_EQ(3,
            arc_vpn_observer().GetArcVpnProviderUpdateCount(kVpnPackageName));

  // Arc Vpn Observer should observe Arc Vpn app uninstallation.
  RemovePackage(kVpnPackageName);
  EXPECT_EQ(0u, arc_vpn_observer().arc_vpn_provider_counter().size());
}
