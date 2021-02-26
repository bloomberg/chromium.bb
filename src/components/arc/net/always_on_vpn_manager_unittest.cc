// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/net/always_on_vpn_manager.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill/shill_manager_client.h"
#include "chromeos/network/network_handler.h"
#include "components/arc/arc_prefs.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace {

constexpr const char kVpnPackage[] = "com.android.vpn";
const base::Value kVpnPackageValue(kVpnPackage);

void OnGetProperties(bool* success_out,
                     std::string* package_name_out,
                     base::OnceClosure callback,
                     base::Optional<base::Value> result) {
  *success_out = result.has_value();
  if (result) {
    const base::Value* value = result->FindKeyOfType(
        shill::kAlwaysOnVpnPackageProperty, base::Value::Type::STRING);
    if (value != nullptr)
      *package_name_out = value->GetString();
  }
  std::move(callback).Run();
}

std::string GetAlwaysOnPackageName() {
  bool success = false;
  std::string package_name;
  chromeos::ShillManagerClient* shill_manager =
      chromeos::DBusThreadManager::Get()->GetShillManagerClient();
  base::RunLoop run_loop;
  shill_manager->GetProperties(
      base::BindOnce(&OnGetProperties, base::Unretained(&success),
                     base::Unretained(&package_name), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(success);
  return package_name;
}

}  // namespace

namespace arc {
namespace {

class AlwaysOnVpnManagerTest : public testing::Test {
 public:
  AlwaysOnVpnManagerTest() = default;

  void SetUp() override {
    chromeos::DBusThreadManager::Initialize();
    EXPECT_TRUE(chromeos::DBusThreadManager::Get()->IsUsingFakes());
    chromeos::NetworkHandler::Initialize();
    EXPECT_TRUE(chromeos::NetworkHandler::IsInitialized());
    arc::prefs::RegisterProfilePrefs(pref_service()->registry());
  }

  void TearDown() override {
    chromeos::NetworkHandler::Shutdown();
    chromeos::DBusThreadManager::Shutdown();
  }

  TestingPrefServiceSimple* pref_service() { return &pref_service_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;

  DISALLOW_COPY_AND_ASSIGN(AlwaysOnVpnManagerTest);
};

TEST_F(AlwaysOnVpnManagerTest, SetPackageWhileLockdownUnset) {
  auto always_on_manager = std::make_unique<AlwaysOnVpnManager>(pref_service());

  EXPECT_EQ(std::string(), GetAlwaysOnPackageName());

  pref_service()->Set(arc::prefs::kAlwaysOnVpnPackage, kVpnPackageValue);

  EXPECT_EQ(std::string(), GetAlwaysOnPackageName());
}

TEST_F(AlwaysOnVpnManagerTest, SetPackageWhileLockdownTrue) {
  pref_service()->Set(arc::prefs::kAlwaysOnVpnLockdown, base::Value(true));

  auto always_on_manager = std::make_unique<AlwaysOnVpnManager>(pref_service());

  EXPECT_EQ(std::string(), GetAlwaysOnPackageName());

  pref_service()->Set(arc::prefs::kAlwaysOnVpnPackage, kVpnPackageValue);

  EXPECT_EQ(kVpnPackage, GetAlwaysOnPackageName());

  pref_service()->Set(arc::prefs::kAlwaysOnVpnPackage,
                      base::Value(base::StringPiece()));

  EXPECT_EQ(std::string(), GetAlwaysOnPackageName());
}

TEST_F(AlwaysOnVpnManagerTest, SetPackageThatsAlreadySetAtBoot) {
  pref_service()->Set(arc::prefs::kAlwaysOnVpnLockdown, base::Value(true));
  pref_service()->Set(arc::prefs::kAlwaysOnVpnPackage, kVpnPackageValue);

  auto always_on_manager = std::make_unique<AlwaysOnVpnManager>(pref_service());

  EXPECT_EQ(kVpnPackage, GetAlwaysOnPackageName());
}

TEST_F(AlwaysOnVpnManagerTest, SetLockdown) {
  pref_service()->Set(arc::prefs::kAlwaysOnVpnPackage, kVpnPackageValue);

  auto always_on_manager = std::make_unique<AlwaysOnVpnManager>(pref_service());

  pref_service()->Set(arc::prefs::kAlwaysOnVpnLockdown, base::Value(true));

  EXPECT_EQ(kVpnPackage, GetAlwaysOnPackageName());

  pref_service()->Set(arc::prefs::kAlwaysOnVpnLockdown, base::Value(false));

  EXPECT_EQ(std::string(), GetAlwaysOnPackageName());
}

}  // namespace
}  // namespace arc
