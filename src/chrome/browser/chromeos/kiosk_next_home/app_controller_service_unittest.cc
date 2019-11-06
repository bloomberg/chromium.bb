// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/kiosk_next_home/app_controller_service.h"

#include <limits>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/optional.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/chromeos/kiosk_next_home/intent_config_helper.h"
#include "chrome/browser/chromeos/kiosk_next_home/metrics_helper.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/services/app_service/public/cpp/app_registry_cache.h"
#include "chrome/services/app_service/public/cpp/app_service_proxy.h"
#include "chrome/services/app_service/public/cpp/app_update.h"
#include "chrome/test/base/testing_profile.h"
#include "components/arc/test/fake_app_instance.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/types/display_constants.h"
#include "ui/events/event_constants.h"

namespace chromeos {
namespace kiosk_next_home {
namespace {

// Fake activity that we use when seeding data to ARC.
constexpr char kFakeActivity[] = "test.kiosk_next_home.activity";

using apps::mojom::AppType;
using apps::mojom::OptionalBool;
using apps::mojom::Readiness;

typedef std::map<std::string, mojom::AppPtr> AppMap;

}  // namespace

class FakeAppControllerClient : public mojom::AppControllerClient {
 public:
  explicit FakeAppControllerClient(mojom::AppControllerClientRequest request)
      : binding_(this, std::move(request)) {}

  const std::vector<mojom::AppPtr>& app_updates() { return app_updates_; }

  // mojom::AppControllerClient:
  void OnAppChanged(mojom::AppPtr app) override {
    app_updates_.push_back(std::move(app));
  }

 private:
  mojo::Binding<mojom::AppControllerClient> binding_;
  std::vector<mojom::AppPtr> app_updates_;
};

// Mock instance for the AppServiceProxy. It only overrides a subset of the
// methods provided by the proxy since we expect that most tests can be written
// with their real implementations (i.e. AppRegistryCache()).
class MockAppServiceProxy : public KeyedService, public apps::AppServiceProxy {
 public:
  static MockAppServiceProxy* OverrideRealProxyForProfile(Profile* profile) {
    return static_cast<MockAppServiceProxy*>(
        apps::AppServiceProxyFactory::GetInstance()->SetTestingFactoryAndUse(
            profile, base::BindRepeating([](content::BrowserContext* context) {
              return static_cast<std::unique_ptr<KeyedService>>(
                  std::make_unique<MockAppServiceProxy>());
            })));
  }

  // apps::AppServiceProxy:
  MOCK_METHOD4(Launch,
               void(const std::string& app_id,
                    int32_t event_flags,
                    apps::mojom::LaunchSource launch_source,
                    int64_t display_id));
  MOCK_METHOD1(Uninstall, void(const std::string& app_id));
};

class MockIntentConfigHelper : public IntentConfigHelper {
 public:
  // IntentConfigHelper:
  MOCK_CONST_METHOD1(IsIntentAllowed, bool(const GURL& intent_uri));
};

class AppControllerServiceTest : public testing::Test {
 protected:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();

    arc_test_.SetUp(profile());
    proxy_ = MockAppServiceProxy::OverrideRealProxyForProfile(profile());

    app_controller_service_ = AppControllerService::Get(profile());

    mojom::AppControllerClientPtr client_proxy;
    client_ = std::make_unique<FakeAppControllerClient>(
        mojo::MakeRequest(&client_proxy));
    app_controller_service_->SetClient(std::move(client_proxy));
  }

  void TearDown() override { arc_test_.TearDown(); }

  Profile* profile() { return profile_.get(); }

  MockAppServiceProxy* proxy() { return proxy_; }

  AppControllerService* service() { return app_controller_service_; }

  std::string GetAppIdFromAndroidPackage(const std::string& package) {
    return ArcAppListPrefs::GetAppId(package, kFakeActivity);
  }

  void AddAndroidPackageToArc(const std::string& package) {
    arc::mojom::AppInfo app_info;
    app_info.package_name = package;

    // We are only interested in the package name that we already set above,
    // but we need to send a full struct so ARC doesn't drop it.
    app_info.name = "test_app_name";
    app_info.activity = kFakeActivity;
    app_info.sticky = false;
    app_info.notifications_enabled = false;
    arc_test_.app_instance()->SendAppAdded(app_info);
  }

  void SetAndroidId(int64_t android_id) {
    arc_test_.app_instance()->set_android_id(android_id);
  }

  void StopArc() { arc_test_.StopArcInstance(); }

  void AddAppDeltaToAppService(apps::mojom::AppPtr delta) {
    std::vector<apps::mojom::AppPtr> deltas;
    deltas.push_back(std::move(delta));
    proxy_->AppRegistryCache().OnApps(std::move(deltas));

    // We just triggered some mojo calls by adding the above delta, we need to
    // wait for them to finish before we continue with the test.
    base::RunLoop().RunUntilIdle();
  }

  // Gets all apps from the AppControllerService instance being tested and
  // returns them in a map keyed by their |app_id|.
  AppMap GetAppsFromController() {
    AppMap apps;
    service()->GetApps(base::BindLambdaForTesting(
        [&apps](std::vector<mojom::AppPtr> app_list) {
          for (const auto& app : app_list) {
            apps[app->app_id] = app.Clone();
          }
        }));
    return apps;
  }

  // Expects the given apps to be returned by a call to
  // AppControllerService::GetApps(). This function doesn't take into account
  // the order of the returned apps.
  void ExpectApps(const std::vector<mojom::App>& expected_apps) {
    AppMap returned_apps_map = GetAppsFromController();

    EXPECT_EQ(expected_apps.size(), returned_apps_map.size())
        << "AppServiceController::GetApps() returned wrong number of apps.";

    for (const auto& expected_app : expected_apps) {
      auto returned_app_it = returned_apps_map.find(expected_app.app_id);
      ASSERT_NE(returned_app_it, returned_apps_map.end())
          << "App with app_id " << expected_app.app_id
          << " was not returned by the AppControllerService::GetApps() "
             "call.";

      mojom::AppPtr& returned_app = returned_app_it->second;

      ExpectEqualApps(expected_app, *returned_app);
    }
  }

  void ExpectArcAndroidIdResponse(bool success, const std::string& android_id) {
    bool returned_success;
    std::string returned_android_id;

    service()->GetArcAndroidId(base::BindLambdaForTesting(
        [&returned_success, &returned_android_id](
            bool success, const std::string& android_id) {
          returned_success = success;
          returned_android_id = android_id;
        }));

    EXPECT_EQ(returned_success, success);
    EXPECT_EQ(returned_android_id, android_id);
  }

  void SetLaunchIntentAllowed(bool allowed) {
    auto intent_config_helper = std::make_unique<MockIntentConfigHelper>();
    EXPECT_CALL(*intent_config_helper, IsIntentAllowed(testing::_))
        .WillOnce(testing::Return(allowed));
    service()->SetIntentConfigHelperForTesting(std::move(intent_config_helper));
  }

  void ExpectLaunchIntentResponse(
      const std::string& intent_uri,
      bool success,
      const base::Optional<std::string>& error_message) {
    bool returned_success;
    base::Optional<std::string> returned_error_message;

    service()->LaunchIntent(
        intent_uri, base::BindLambdaForTesting(
                        [&returned_success, &returned_error_message](
                            bool success,
                            const base::Optional<std::string>& error_message) {
                          returned_success = success;
                          returned_error_message = error_message;
                        }));

    EXPECT_EQ(returned_success, success);

    ASSERT_EQ(returned_error_message.has_value(), error_message.has_value());
    if (returned_error_message.has_value())
      EXPECT_EQ(returned_error_message.value(), error_message.value());
  }

  void ExpectNoLaunchedIntents() {
    EXPECT_EQ(0U, arc_test_.app_instance()->launch_intents().size())
        << "At least one ARC intent was lauched, we expected none.";
  }

  void ExpectIntentLaunched(const std::string& intent_uri) {
    ASSERT_EQ(arc_test_.app_instance()->launch_intents().size(), 1U)
        << "We expect exactly one ARC intent to be launched.";
    EXPECT_EQ(arc_test_.app_instance()->launch_intents()[0], intent_uri);
  }

  // Expects the given apps were passed to the
  // AppControllerClient::OnAppChanged method in order.
  void ExpectAppChangedUpdates(
      const std::vector<mojom::App>& expected_updates) {
    ASSERT_EQ(expected_updates.size(), client_->app_updates().size());

    for (std::size_t i = 0; i < expected_updates.size(); ++i) {
      ExpectEqualApps(expected_updates[i], *(client_->app_updates()[i]));
    }
  }

  void ExpectBridgeActionRecorded(BridgeAction action, int count) {
    // Since seeding apps may fire bridge actions, check only the bucket of
    // interest.
    histogram_tester_.ExpectBucketCount("KioskNextHome.Bridge.Action", action,
                                        count);
  }

  void ExpectLaunchIntentResultRecorded(LaunchIntentResult result) {
    histogram_tester_.ExpectUniqueSample(
        "KioskNextHome.Bridge.LaunchIntentResult", result, 1);
  }

  void ExpectGetAndroidIdSuccessRecorded(bool success, int count) {
    histogram_tester_.ExpectUniqueSample(
        "KioskNextHome.Bridge.GetAndroidIdSuccess", success, count);
  }

 private:
  content::TestBrowserThreadBundle test_browser_thread_bundle_;
  std::unique_ptr<TestingProfile> profile_;
  ArcAppTest arc_test_;
  MockAppServiceProxy* proxy_ = nullptr;
  AppControllerService* app_controller_service_ = nullptr;
  std::unique_ptr<FakeAppControllerClient> client_;
  base::HistogramTester histogram_tester_;

  void ExpectEqualApps(const mojom::App& expected_app,
                       const mojom::App& actual_app) {
    // Test equality of every single field to make tests failures more
    // readable.
    EXPECT_EQ(actual_app.app_id, expected_app.app_id);
    EXPECT_EQ(actual_app.type, expected_app.type);
    EXPECT_EQ(actual_app.display_name, expected_app.display_name);
    EXPECT_EQ(actual_app.readiness, expected_app.readiness);
    EXPECT_EQ(actual_app.android_package_name,
              expected_app.android_package_name);

    // Catch all clause of equality. This will only be necessary if we add
    // more fields that are not expected above.
    ASSERT_TRUE(expected_app.Equals(actual_app));
  }
};

TEST_F(AppControllerServiceTest, AppIsFetchedCorrectly) {
  std::string app_id = "fake_app_id";
  std::string display_name = "Fake app name";
  AppType app_type = AppType::kExtension;
  Readiness readiness = Readiness::kReady;

  // Seeding data.
  apps::mojom::App delta;
  delta.app_id = app_id;
  delta.name = display_name;
  delta.app_type = app_type;
  delta.readiness = readiness;
  delta.show_in_launcher = OptionalBool::kTrue;
  AddAppDeltaToAppService(delta.Clone());

  mojom::App expected_app;
  expected_app.android_package_name = "";
  expected_app.app_id = app_id;
  expected_app.display_name = display_name;
  expected_app.type = app_type;
  expected_app.readiness = readiness;

  ExpectApps({expected_app});
  ExpectBridgeActionRecorded(BridgeAction::kListApps, 1);
}

TEST_F(AppControllerServiceTest, AndroidAppIsFetchedCorrectly) {
  std::string android_package_name = "fake.app.package";
  std::string app_id = GetAppIdFromAndroidPackage(android_package_name);
  std::string display_name = "Fake app name";
  AppType app_type = AppType::kArc;
  Readiness readiness = Readiness::kReady;

  // Seeding data.
  AddAndroidPackageToArc(android_package_name);
  apps::mojom::App delta;
  delta.app_id = app_id;
  delta.name = display_name;
  delta.app_type = app_type;
  delta.readiness = readiness;
  delta.show_in_launcher = OptionalBool::kTrue;
  AddAppDeltaToAppService(delta.Clone());

  mojom::App expected_app;
  expected_app.android_package_name = android_package_name;
  expected_app.app_id = app_id;
  expected_app.display_name = display_name;
  expected_app.type = app_type;
  expected_app.readiness = readiness;

  ExpectApps({expected_app});
  ExpectBridgeActionRecorded(BridgeAction::kListApps, 1);
}

TEST_F(AppControllerServiceTest, AndroidAppWithMissingPackageFetchedCorrectly) {
  std::string android_package_name = "fake.app.package";
  std::string app_id = GetAppIdFromAndroidPackage(android_package_name);
  std::string display_name = "Fake app name";
  AppType app_type = AppType::kArc;
  Readiness readiness = Readiness::kReady;

  // Seeding data. This time we intentionally don't seed the package to ARC.
  apps::mojom::App delta;
  delta.app_id = app_id;
  delta.name = display_name;
  delta.app_type = app_type;
  delta.readiness = readiness;
  delta.show_in_launcher = OptionalBool::kTrue;
  AddAppDeltaToAppService(delta.Clone());

  mojom::App expected_app;
  // Since we don't seed information to ARC, we don't expect to receive a
  // package here. In this case, expect an empty string (and not a crash :)
  expected_app.android_package_name = "";
  expected_app.app_id = app_id;
  expected_app.display_name = display_name;
  expected_app.type = app_type;
  expected_app.readiness = readiness;

  ExpectApps({expected_app});
}

TEST_F(AppControllerServiceTest, AppReadinessIsUpdated) {
  std::string app_id = "fake_app_id";
  AppType app_type = AppType::kExtension;
  Readiness readiness = Readiness::kUnknown;

  apps::mojom::App delta;
  delta.app_id = app_id;
  delta.app_type = app_type;
  delta.readiness = readiness;
  delta.show_in_launcher = OptionalBool::kTrue;
  AddAppDeltaToAppService(delta.Clone());
  ExpectBridgeActionRecorded(BridgeAction::kNotifiedAppChange, 1);

  mojom::App expected_app;
  expected_app.app_id = app_id;
  expected_app.type = app_type;
  expected_app.readiness = readiness;
  ExpectApps({expected_app});

  // Now we change the readiness.
  delta.readiness = Readiness::kReady;
  AddAppDeltaToAppService(delta.Clone());
  ExpectBridgeActionRecorded(BridgeAction::kNotifiedAppChange, 2);

  expected_app.readiness = Readiness::kReady;
  ExpectApps({expected_app});
  ExpectBridgeActionRecorded(BridgeAction::kListApps, 2);
}

TEST_F(AppControllerServiceTest, AppDisplayNameIsUpdated) {
  std::string app_id = "fake_app_id";
  AppType app_type = AppType::kExtension;
  std::string display_name = "Initial App Name";

  apps::mojom::App delta;
  delta.app_id = app_id;
  delta.app_type = app_type;
  delta.name = display_name;
  delta.show_in_launcher = OptionalBool::kTrue;
  AddAppDeltaToAppService(delta.Clone());
  ExpectBridgeActionRecorded(BridgeAction::kNotifiedAppChange, 1);

  mojom::App expected_app;
  expected_app.app_id = app_id;
  expected_app.type = app_type;
  expected_app.display_name = display_name;
  ExpectApps({expected_app});

  // Now we change the name.
  std::string new_display_name = "New App Name";
  delta.name = new_display_name;
  AddAppDeltaToAppService(delta.Clone());
  ExpectBridgeActionRecorded(BridgeAction::kNotifiedAppChange, 2);

  expected_app.display_name = new_display_name;
  ExpectApps({expected_app});
  ExpectBridgeActionRecorded(BridgeAction::kListApps, 2);
}

TEST_F(AppControllerServiceTest, MultipleAppsAreFetchedCorrectly) {
  // Seed the first app.
  apps::mojom::App first_delta;
  first_delta.app_id = "first_app";
  first_delta.app_type = AppType::kBuiltIn;
  first_delta.show_in_launcher = OptionalBool::kTrue;
  AddAppDeltaToAppService(first_delta.Clone());

  mojom::App first_expected_app;
  first_expected_app.app_id = "first_app";
  first_expected_app.type = AppType::kBuiltIn;
  // Expect only the first app.
  ExpectApps({first_expected_app});

  // Seed second app.
  apps::mojom::App second_delta;
  second_delta.app_id = "second_app";
  second_delta.app_type = AppType::kBuiltIn;
  second_delta.show_in_launcher = OptionalBool::kTrue;
  AddAppDeltaToAppService(second_delta.Clone());

  mojom::App second_expected_app;
  second_expected_app.app_id = "second_app";
  second_expected_app.type = AppType::kBuiltIn;
  // Expect both apps.
  ExpectApps({first_expected_app, second_expected_app});
}

TEST_F(AppControllerServiceTest, UninstalledAppsAreFiltered) {
  // First seed an installed app and expect it to be returned.
  apps::mojom::App app_delta;
  app_delta.app_id = "app_id";
  app_delta.app_type = AppType::kBuiltIn;
  app_delta.show_in_launcher = OptionalBool::kTrue;
  app_delta.readiness = Readiness::kReady;
  AddAppDeltaToAppService(app_delta.Clone());

  mojom::App app;
  app.app_id = "app_id";
  app.type = AppType::kBuiltIn;
  app.readiness = Readiness::kReady;
  ExpectApps({app});

  // Then change the app's readiness and expect it to be filtered.
  app_delta.readiness = Readiness::kUninstalledByUser;
  AddAppDeltaToAppService(app_delta.Clone());
  ExpectApps({});
}

TEST_F(AppControllerServiceTest, AppsThatAreNotRelevantAreFiltered) {
  // Seed an app that's allowed to be returned by the AppControllerService.
  apps::mojom::App allowed_app_delta;
  allowed_app_delta.app_id = "allowed_app";
  allowed_app_delta.app_type = AppType::kBuiltIn;
  allowed_app_delta.show_in_launcher = OptionalBool::kTrue;
  AddAppDeltaToAppService(allowed_app_delta.Clone());

  apps::mojom::App first_blocked_app_delta;
  first_blocked_app_delta.app_id = "first_blocked_app";
  first_blocked_app_delta.app_type = AppType::kBuiltIn;
  first_blocked_app_delta.show_in_launcher = OptionalBool::kUnknown;
  AddAppDeltaToAppService(first_blocked_app_delta.Clone());

  apps::mojom::App second_blocked_app_delta;
  second_blocked_app_delta.app_id = "second_blocked_app";
  second_blocked_app_delta.app_type = AppType::kBuiltIn;
  second_blocked_app_delta.show_in_launcher = OptionalBool::kFalse;
  AddAppDeltaToAppService(second_blocked_app_delta.Clone());

  apps::mojom::App kiosk_next_app_delta;
  kiosk_next_app_delta.app_id = extension_misc::kKioskNextHomeAppId;
  kiosk_next_app_delta.app_type = AppType::kBuiltIn;
  // Even though the Kiosk Next might be allowed in the launcher, we cannot
  // return it.
  kiosk_next_app_delta.show_in_launcher = OptionalBool::kTrue;
  AddAppDeltaToAppService(kiosk_next_app_delta.Clone());

  mojom::App allowed_app;
  allowed_app.app_id = "allowed_app";
  allowed_app.type = AppType::kBuiltIn;

  // Expect only the allowed app, all the other ones were filtered.
  ExpectApps({allowed_app});
}

TEST_F(AppControllerServiceTest, GetArcAndroidIdReturnsItWhenItHasIt) {
  SetAndroidId(123456789L);
  ExpectArcAndroidIdResponse(true, "75bcd15");  // 75bcd15 is 123456789 in hex.

  // Make sure the returned Android ID doesn't get clipped when it's too large.
  SetAndroidId(std::numeric_limits<int64_t>::max());
  ExpectArcAndroidIdResponse(true, "7fffffffffffffff");

  ExpectBridgeActionRecorded(BridgeAction::kGetAndroidId, 2);
  ExpectGetAndroidIdSuccessRecorded(/*success=*/true, 2);
}

TEST_F(AppControllerServiceTest, GetArcAndroidIdFailureIsPropagated) {
  // Stop the ARC instance to simulate a failure.
  StopArc();

  ExpectArcAndroidIdResponse(false, "0");
  ExpectBridgeActionRecorded(BridgeAction::kGetAndroidId, 1);
  ExpectGetAndroidIdSuccessRecorded(/*success=*/false, 1);
}

TEST_F(AppControllerServiceTest, LaunchIntentLaunchesWhenAllowed) {
  SetLaunchIntentAllowed(/*allowed=*/true);
  std::string intent = "https://example.com/path?q=query";
  ExpectLaunchIntentResponse(intent, true, base::nullopt);

  ExpectIntentLaunched(intent);
  ExpectBridgeActionRecorded(BridgeAction::kLaunchIntent, 1);
  ExpectLaunchIntentResultRecorded(LaunchIntentResult::kSuccess);
}

TEST_F(AppControllerServiceTest, LaunchIntentFailsWhenNotAllowed) {
  SetLaunchIntentAllowed(/*allowed=*/false);
  std::string intent = "https://example.com/path?q=query";
  ExpectLaunchIntentResponse(
      intent, false, base::Optional<std::string>("Intent not allowed."));

  ExpectNoLaunchedIntents();
  ExpectBridgeActionRecorded(BridgeAction::kLaunchIntent, 1);
  ExpectLaunchIntentResultRecorded(LaunchIntentResult::kNotAllowed);
}

TEST_F(AppControllerServiceTest, LaunchIntentFailsWhenArcIsDisabled) {
  SetLaunchIntentAllowed(/*allowed=*/true);
  StopArc();
  std::string intent = "https://example.com/path?q=query";
  ExpectLaunchIntentResponse(
      intent, false, base::Optional<std::string>("ARC bridge not available."));

  ExpectNoLaunchedIntents();
  ExpectBridgeActionRecorded(BridgeAction::kLaunchIntent, 1);
  ExpectLaunchIntentResultRecorded(LaunchIntentResult::kArcUnavailable);
}

TEST_F(AppControllerServiceTest, ClientIsNotifiedOfChangesToAndroidApp) {
  std::string android_package_name = "fake.app.package.2.0";
  std::string app_id = GetAppIdFromAndroidPackage(android_package_name);
  AppType app_type = AppType::kArc;

  // Install the Android app.
  AddAndroidPackageToArc(android_package_name);
  apps::mojom::App delta;
  delta.app_id = app_id;
  delta.app_type = app_type;
  delta.show_in_launcher = OptionalBool::kTrue;
  delta.name = "Fake App Name - First Version";
  AddAppDeltaToAppService(delta.Clone());

  mojom::App first_app_state;
  first_app_state.android_package_name = android_package_name;
  first_app_state.app_id = app_id;
  first_app_state.display_name = "Fake App Name - First Version";
  first_app_state.type = app_type;
  ExpectAppChangedUpdates({first_app_state});

  delta.name = "Fake App Name - Second Version";
  AddAppDeltaToAppService(delta.Clone());
  mojom::App second_app_state = first_app_state;
  second_app_state.display_name = "Fake App Name - Second Version";
  ExpectAppChangedUpdates({first_app_state, second_app_state});

  // Now we stop ARC and check if we still have all the information we had about
  // the app. We should have cached all that we needed from the previous
  // updates.
  StopArc();
  delta.name = "Fake App Name - Third Version";
  AddAppDeltaToAppService(delta.Clone());
  mojom::App third_app_state = second_app_state;
  third_app_state.display_name = "Fake App Name - Third Version";
  ExpectAppChangedUpdates({first_app_state, second_app_state, third_app_state});
}

TEST_F(AppControllerServiceTest, ClientIsNotifiedOfReadinessChanges) {
  apps::mojom::App app_delta;
  app_delta.app_id = "app";
  app_delta.app_type = AppType::kBuiltIn;
  app_delta.show_in_launcher = OptionalBool::kTrue;
  app_delta.readiness = Readiness::kReady;
  AddAppDeltaToAppService(app_delta.Clone());

  mojom::App first_app_state;
  first_app_state.app_id = "app";
  first_app_state.type = AppType::kBuiltIn;
  first_app_state.readiness = Readiness::kReady;
  ExpectAppChangedUpdates({first_app_state});

  app_delta.readiness = Readiness::kUninstalledByUser;
  AddAppDeltaToAppService(app_delta.Clone());

  mojom::App second_app_state;
  second_app_state.app_id = "app";
  second_app_state.type = AppType::kBuiltIn;
  second_app_state.readiness = Readiness::kUninstalledByUser;
  ExpectAppChangedUpdates({first_app_state, second_app_state});
}

TEST_F(AppControllerServiceTest, ClientIsNotifiedOfNameChanges) {
  apps::mojom::App app_delta;
  app_delta.app_id = "app";
  app_delta.app_type = AppType::kBuiltIn;
  app_delta.show_in_launcher = OptionalBool::kTrue;
  app_delta.name = "First App Name";
  AddAppDeltaToAppService(app_delta.Clone());

  mojom::App first_app_state;
  first_app_state.app_id = "app";
  first_app_state.type = AppType::kBuiltIn;
  first_app_state.display_name = "First App Name";
  ExpectAppChangedUpdates({first_app_state});

  app_delta.name = "Second App Name";
  AddAppDeltaToAppService(app_delta.Clone());

  mojom::App second_app_state;
  second_app_state.app_id = "app";
  second_app_state.type = AppType::kBuiltIn;
  second_app_state.display_name = "Second App Name";
  ExpectAppChangedUpdates({first_app_state, second_app_state});
}

TEST_F(AppControllerServiceTest, ClientIsNotNotifiedOfChangesToHiddenApp) {
  // Install an app that should not be shown in the launcher.
  apps::mojom::App app_delta;
  app_delta.app_id = "app";
  app_delta.app_type = AppType::kBuiltIn;
  app_delta.show_in_launcher = OptionalBool::kFalse;
  AddAppDeltaToAppService(app_delta.Clone());
  ExpectAppChangedUpdates({});

  // Even if we keep changing the app, we will not notify the client.
  app_delta.name = "Fake App Name";
  AddAppDeltaToAppService(app_delta.Clone());
  ExpectAppChangedUpdates({});
  app_delta.readiness = Readiness::kReady;
  AddAppDeltaToAppService(app_delta.Clone());
  ExpectAppChangedUpdates({});
  app_delta.show_in_launcher = OptionalBool::kUnknown;
  AddAppDeltaToAppService(app_delta.Clone());
  ExpectAppChangedUpdates({});

  // Now if we change |show_in_launcher| to true, we notify the client with
  // the latest app state.
  app_delta.show_in_launcher = OptionalBool::kTrue;
  AddAppDeltaToAppService(app_delta.Clone());

  mojom::App app_state;
  app_state.app_id = "app";
  app_state.type = AppType::kBuiltIn;
  app_state.display_name = "Fake App Name";
  app_state.readiness = Readiness::kReady;
  ExpectAppChangedUpdates({app_state});
}

TEST_F(AppControllerServiceTest, ClientIsNotNotifiedOfSuperfluousChanges) {
  apps::mojom::App app_delta;
  app_delta.app_id = "app";
  app_delta.app_type = AppType::kBuiltIn;
  app_delta.show_in_launcher = OptionalBool::kTrue;
  AddAppDeltaToAppService(app_delta.Clone());

  mojom::App first_app_state;
  first_app_state.app_id = "app";
  first_app_state.type = AppType::kBuiltIn;
  ExpectAppChangedUpdates({first_app_state});
  ExpectBridgeActionRecorded(BridgeAction::kNotifiedAppChange, 1);

  // We don't care about this field, so changes to it shouldn't be
  // propagated to the client.
  app_delta.additional_search_terms = {"random_term"};
  AddAppDeltaToAppService(app_delta.Clone());
  ExpectAppChangedUpdates({first_app_state});
  ExpectBridgeActionRecorded(BridgeAction::kNotifiedAppChange, 1);
}

TEST_F(AppControllerServiceTest, LaunchAppCallsAppServiceCorrectly) {
  EXPECT_CALL(*proxy(), Launch("fake_app_id", ui::EventFlags::EF_NONE,
                               apps::mojom::LaunchSource::kFromKioskNextHome,
                               display::kDefaultDisplayId));

  service()->LaunchApp("fake_app_id");
  ExpectBridgeActionRecorded(BridgeAction::kLaunchApp, 1);
}

TEST_F(AppControllerServiceTest, UninstallAppCallsAppServiceCorrectly) {
  EXPECT_CALL(*proxy(), Uninstall("fake_app_id"));

  service()->UninstallApp("fake_app_id");
  ExpectBridgeActionRecorded(BridgeAction::kUninstallApp, 1);
}

}  // namespace kiosk_next_home
}  // namespace chromeos
