// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tracing/chrome_background_tracing_metrics_provider.h"

#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/tracing/common/trace_startup_config.h"
#include "content/public/browser/background_tracing_config.h"
#include "content/public/browser/background_tracing_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#include "third_party/metrics_proto/trace_log.pb.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
// "nogncheck" because of crbug.com/1125897.
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/tpm_manager/tpm_manager_client.h"  // nogncheck
#include "chromeos/login/login_state/login_state.h"
#include "chromeos/system/fake_statistics_provider.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace tracing {
namespace {
const char kDummyTrace[] = "Trace bytes as serialized proto";
}  // namespace

class ChromeBackgroundTracingMetricsProviderTest : public testing::Test {
 public:
  ChromeBackgroundTracingMetricsProviderTest()
      : local_state_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    base::Value dict(base::Value::Type::DICTIONARY);

    dict.SetStringKey("mode", "REACTIVE_TRACING_MODE");
    dict.SetStringKey("custom_categories",
                      tracing::TraceStartupConfig::kDefaultStartupCategories);

    base::Value rules_list(base::Value::Type::LIST);
    {
      base::Value rules_dict(base::Value::Type::DICTIONARY);
      rules_dict.SetStringKey("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED");
      rules_dict.SetStringKey("trigger_name", "test");
      rules_list.Append(std::move(rules_dict));
    }
    dict.SetKey("configs", std::move(rules_list));

    std::unique_ptr<content::BackgroundTracingConfig> config(
        content::BackgroundTracingConfig::FromDict(std::move(dict)));
    ASSERT_TRUE(config);

    ASSERT_TRUE(
        content::BackgroundTracingManager::GetInstance().SetActiveScenario(
            std::move(config),
            content::BackgroundTracingManager::ANONYMIZE_DATA));
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  ScopedTestingLocalState local_state_;
};

TEST_F(ChromeBackgroundTracingMetricsProviderTest, NoTraceData) {
  ChromeBackgroundTracingMetricsProvider provider;
  ASSERT_FALSE(provider.HasIndependentMetrics());
}

TEST_F(ChromeBackgroundTracingMetricsProviderTest, UploadsTraceLog) {
  ChromeBackgroundTracingMetricsProvider provider;
  EXPECT_FALSE(provider.HasIndependentMetrics());

  content::BackgroundTracingManager::GetInstance().SetTraceToUploadForTesting(
      std::make_unique<std::string>(kDummyTrace));

  EXPECT_TRUE(provider.HasIndependentMetrics());
  metrics::ChromeUserMetricsExtension uma_proto;
  uma_proto.set_client_id(100);
  uma_proto.set_session_id(15);
  provider.ProvideIndependentMetrics(
      base::BindOnce([](bool success) { EXPECT_TRUE(success); }), &uma_proto,
      /* snapshot_manager=*/nullptr);

  EXPECT_EQ(100u, uma_proto.client_id());
  EXPECT_EQ(15, uma_proto.session_id());
  ASSERT_EQ(1, uma_proto.trace_log_size());
  EXPECT_EQ(kDummyTrace, uma_proto.trace_log(0).raw_data());

  EXPECT_FALSE(provider.HasIndependentMetrics());
}

TEST_F(ChromeBackgroundTracingMetricsProviderTest, HandleMissingTrace) {
  ChromeBackgroundTracingMetricsProvider provider;
  EXPECT_FALSE(provider.HasIndependentMetrics());

  content::BackgroundTracingManager::GetInstance().SetTraceToUploadForTesting(
      std::make_unique<std::string>(kDummyTrace));
  EXPECT_TRUE(provider.HasIndependentMetrics());

  content::BackgroundTracingManager::GetInstance().SetTraceToUploadForTesting(
      nullptr);
  metrics::ChromeUserMetricsExtension uma_proto;
  uma_proto.set_client_id(100);
  uma_proto.set_session_id(15);
  provider.ProvideIndependentMetrics(
      base::BindOnce([](bool success) { EXPECT_FALSE(success); }), &uma_proto,
      /* snapshot_manager=*/nullptr);

  EXPECT_EQ(100u, uma_proto.client_id());
  EXPECT_EQ(15, uma_proto.session_id());
  EXPECT_EQ(0, uma_proto.trace_log_size());
  EXPECT_FALSE(provider.HasIndependentMetrics());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
class ChromeBackgroundTracingMetricsProviderChromeOSTest
    : public ChromeBackgroundTracingMetricsProviderTest {
 public:
  // ChromeBackgroundTracingMetricsProviderTest:
  void SetUp() override {
    ChromeBackgroundTracingMetricsProviderTest::SetUp();

    // ChromeOSMetricsProvider needs the following to provide system profile
    // meta.
    chromeos::PowerManagerClient::InitializeFake();
    chromeos::TpmManagerClient::InitializeFake();
    ash::DemoSession::SetDemoConfigForTesting(
        ash::DemoSession::DemoModeConfig::kNone);
    chromeos::LoginState::Initialize();
  }

  void TearDown() override {
    ChromeBackgroundTracingMetricsProviderTest::TearDown();

    chromeos::LoginState::Shutdown();
    ash::DemoSession::ResetDemoConfigForTesting();
    chromeos::TpmManagerClient::Shutdown();
    chromeos::PowerManagerClient::Shutdown();
  }
};

TEST_F(ChromeBackgroundTracingMetricsProviderChromeOSTest, HardwareClass) {
  // Set a fake hardware class.
  constexpr char kFakeHardwareClass[] = "Fake hardware class";
  chromeos::system::ScopedFakeStatisticsProvider fake_statistics_provider;
  fake_statistics_provider.SetMachineStatistic("hardware_class",
                                               kFakeHardwareClass);

  ChromeBackgroundTracingMetricsProvider provider;
  provider.Init();

  // AsyncInit needs to happen to collect `hardware_class` etc.
  {
    base::RunLoop run_loop;
    provider.AsyncInit(base::BindLambdaForTesting([&] { run_loop.Quit(); }));
    run_loop.Run();
  }

  // Fake a UMA collection for background tracing.
  content::BackgroundTracingManager::GetInstance().SetTraceToUploadForTesting(
      std::make_unique<std::string>(kDummyTrace));
  ASSERT_TRUE(provider.HasIndependentMetrics());

  metrics::ChromeUserMetricsExtension uma_proto;
  {
    base::RunLoop run_loop;
    provider.ProvideIndependentMetrics(
        base::BindLambdaForTesting([&](bool success) {
          EXPECT_TRUE(success);
          run_loop.Quit();
        }),
        &uma_proto,
        /* snapshot_manager=*/nullptr);
    run_loop.Run();
  }

  // Verify `hardware_class` is collected correctly.
  ASSERT_EQ(1, uma_proto.trace_log_size());
  const metrics::SystemProfileProto& system_profile =
      uma_proto.system_profile();
  const metrics::SystemProfileProto::Hardware& hardware =
      system_profile.hardware();
  EXPECT_EQ(kFakeHardwareClass, hardware.full_hardware_class());

  EXPECT_FALSE(provider.HasIndependentMetrics());
}
#endif

}  // namespace tracing
