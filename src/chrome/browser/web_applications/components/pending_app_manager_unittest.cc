// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/pending_app_manager.h"

#include <algorithm>
#include <sstream>
#include <vector>

#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/browser/web_applications/components/test_pending_app_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

class PendingAppManagerTest : public testing::Test {
 protected:
  void Sync(std::vector<GURL> urls) {
    pending_app_manager_.ResetCounts();

    std::vector<InstallOptions> install_options_list;
    for (const auto& url : urls) {
      install_options_list.emplace_back(url, LaunchContainer::kWindow,
                                        InstallSource::kInternal);
    }

    base::RunLoop run_loop;
    pending_app_manager_.SynchronizeInstalledApps(
        std::move(install_options_list), InstallSource::kInternal,
        base::BindLambdaForTesting(
            [&run_loop](PendingAppManager::SynchronizeResult result) {
              ASSERT_EQ(PendingAppManager::SynchronizeResult::kSuccess, result);
              run_loop.Quit();
            }));
    // Wait for SynchronizeInstalledApps to finish.
    run_loop.Run();
  }

  void Expect(int deduped_install_count,
              int deduped_uninstall_count,
              std::vector<GURL> installed_app_urls) {
    EXPECT_EQ(deduped_install_count,
              pending_app_manager_.deduped_install_count());
    EXPECT_EQ(deduped_uninstall_count,
              pending_app_manager_.deduped_uninstall_count());

    std::vector<GURL> urls =
        pending_app_manager_.GetInstalledAppUrls(InstallSource::kInternal);
    std::sort(urls.begin(), urls.end());
    EXPECT_EQ(installed_app_urls, urls);
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  TestPendingAppManager pending_app_manager_;
};

// Test that destroying PendingAppManager during a synchronize call that
// installs an app doesn't crash.
// Regression test for https://crbug.com/962808
TEST_F(PendingAppManagerTest, DestroyDuringInstallInSynchronize) {
  auto pending_app_manager = std::make_unique<TestPendingAppManager>();

  std::vector<InstallOptions> install_options_list;
  install_options_list.emplace_back(GURL("https://foo.example"),
                                    LaunchContainer::kWindow,
                                    InstallSource::kInternal);
  install_options_list.emplace_back(GURL("https://bar.example"),
                                    LaunchContainer::kWindow,
                                    InstallSource::kInternal);

  pending_app_manager->SynchronizeInstalledApps(
      std::move(install_options_list), InstallSource::kInternal,
      // PendingAppManager gives no guarantees about whether its pending
      // callbacks will be run or not when it gets destroyed.
      base::DoNothing());
  pending_app_manager.reset();
  base::RunLoop().RunUntilIdle();
}

// Test that destroying PendingAppManager during a synchronize call that
// uninstalls an app doesn't crash.
// Regression test for https://crbug.com/962808
TEST_F(PendingAppManagerTest, DestroyDuringUninstallInSynchronize) {
  auto pending_app_manager = std::make_unique<TestPendingAppManager>();

  // Install an app that will be uninstalled next.
  {
    std::vector<InstallOptions> install_options_list;
    install_options_list.emplace_back(GURL("https://foo.example"),
                                      LaunchContainer::kWindow,
                                      InstallSource::kInternal);
    base::RunLoop run_loop;
    pending_app_manager->SynchronizeInstalledApps(
        std::move(install_options_list), InstallSource::kInternal,
        base::BindLambdaForTesting(
            [&](PendingAppManager::SynchronizeResult result) {
              ASSERT_EQ(PendingAppManager::SynchronizeResult::kSuccess, result);
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  pending_app_manager->SynchronizeInstalledApps(
      std::vector<InstallOptions>(), InstallSource::kInternal,
      // PendingAppManager gives no guarantees about whether its pending
      // callbacks will be run or not when it gets destroyed.
      base::DoNothing());
  pending_app_manager.reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(PendingAppManagerTest, SynchronizeInstalledApps) {
  GURL a("https://a.example.com/");
  GURL b("https://b.example.com/");
  GURL c("https://c.example.com/");
  GURL d("https://d.example.com/");
  GURL e("https://e.example.com/");

  Sync(std::vector<GURL>{a, b, d});
  Expect(3, 0, std::vector<GURL>{a, b, d});

  Sync(std::vector<GURL>{b, e});
  Expect(1, 2, std::vector<GURL>{b, e});

  Sync(std::vector<GURL>{e});
  Expect(0, 1, std::vector<GURL>{e});

  Sync(std::vector<GURL>{c});
  Expect(1, 1, std::vector<GURL>{c});

  Sync(std::vector<GURL>{e, a, d});
  Expect(3, 1, std::vector<GURL>{a, d, e});

  Sync(std::vector<GURL>{c, a, b, d, e});
  Expect(2, 0, std::vector<GURL>{a, b, c, d, e});

  Sync(std::vector<GURL>{});
  Expect(0, 5, std::vector<GURL>{});

  // The remaining code tests duplicate inputs.

  Sync(std::vector<GURL>{b, a, b, c});
  Expect(3, 0, std::vector<GURL>{a, b, c});

  Sync(std::vector<GURL>{e, a, e, e, e, a});
  Expect(1, 2, std::vector<GURL>{a, e});

  Sync(std::vector<GURL>{b, c, d});
  Expect(3, 2, std::vector<GURL>{b, c, d});

  Sync(std::vector<GURL>{a, a, a, a, a, a});
  Expect(1, 3, std::vector<GURL>{a});

  Sync(std::vector<GURL>{});
  Expect(0, 1, std::vector<GURL>{});
}

}  // namespace web_app
