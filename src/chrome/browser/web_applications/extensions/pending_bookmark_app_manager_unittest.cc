// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/pending_bookmark_app_manager.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "base/test/bind_test_util.h"
#include "base/timer/mock_timer.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/install_finalizer.h"
#include "chrome/browser/web_applications/components/pending_app_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_installation_task.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_registrar.h"
#include "chrome/browser/web_applications/test/test_app_registrar.h"
#include "chrome/browser/web_applications/test/test_install_finalizer.h"
#include "chrome/browser/web_applications/test/test_web_app_ui_delegate.h"
#include "chrome/browser/web_applications/test/test_web_app_url_loader.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

using InstallAppsResults =
    std::vector<std::pair<GURL, web_app::InstallResultCode>>;
using UninstallAppsResults = std::vector<std::pair<GURL, bool>>;

const char kFooWebAppUrl[] = "https://foo.example";
const char kBarWebAppUrl[] = "https://bar.example";
const char kQuxWebAppUrl[] = "https://qux.example";

web_app::InstallOptions GetFooInstallOptions(
    base::Optional<bool> override_previous_user_uninstall =
        base::Optional<bool>()) {
  web_app::InstallOptions options(GURL(kFooWebAppUrl),
                                  web_app::LaunchContainer::kTab,
                                  web_app::InstallSource::kExternalPolicy);

  if (override_previous_user_uninstall.has_value())
    options.override_previous_user_uninstall =
        *override_previous_user_uninstall;

  return options;
}

web_app::InstallOptions GetBarInstallOptions() {
  web_app::InstallOptions options(GURL(kBarWebAppUrl),
                                  web_app::LaunchContainer::kWindow,
                                  web_app::InstallSource::kExternalPolicy);
  return options;
}

web_app::InstallOptions GetQuxInstallOptions() {
  web_app::InstallOptions options(GURL(kQuxWebAppUrl),
                                  web_app::LaunchContainer::kWindow,
                                  web_app::InstallSource::kExternalPolicy);
  return options;
}

std::string GenerateFakeAppId(const GURL& url) {
  return std::string("fake_app_id_for:") + url.spec();
}

class TestBookmarkAppInstallationTask : public BookmarkAppInstallationTask {
 public:
  TestBookmarkAppInstallationTask(Profile* profile,
                                  web_app::TestAppRegistrar* registrar,
                                  web_app::InstallFinalizer* install_finalizer,
                                  web_app::InstallOptions install_options,
                                  bool succeeds)
      : BookmarkAppInstallationTask(profile,
                                    install_finalizer,
                                    std::move(install_options)),
        profile_(profile),
        registrar_(registrar),
        succeeds_(succeeds),
        extension_ids_map_(profile_->GetPrefs()) {}
  ~TestBookmarkAppInstallationTask() override = default;

  void Install(content::WebContents* web_contents,
               BookmarkAppInstallationTask::ResultCallback callback) override {
    std::move(on_install_called_).Run();
    if (succeeds_) {
      std::move(callback).Run(SimulateInstallingApp());
    } else {
      std::move(callback).Run(BookmarkAppInstallationTask::Result(
          web_app::InstallResultCode::kFailedUnknownReason, std::string()));
    }
  }

  void InstallPlaceholder(
      BookmarkAppInstallationTask::ResultCallback callback) override {
    std::move(on_install_placeholder_called_).Run();
    if (succeeds_) {
      std::move(callback).Run(SimulateInstallingApp(true /* is_placeholder */));
    } else {
      std::move(callback).Run(BookmarkAppInstallationTask::Result(
          web_app::InstallResultCode::kFailedUnknownReason, std::string()));
    }
  }

  void SetOnInstallCalled(base::OnceClosure on_install_called) {
    on_install_called_ = std::move(on_install_called);
  }

  void SetOnInstallPlaceholderCalled(
      base::OnceClosure on_install_placeholder_called) {
    on_install_placeholder_called_ = std::move(on_install_placeholder_called);
  }

 private:
  BookmarkAppInstallationTask::Result SimulateInstallingApp(
      bool is_placeholder = false) {
    std::string app_id = GenerateFakeAppId(install_options().url);
    extension_ids_map_.Insert(install_options().url, app_id,
                              install_options().install_source);
    extension_ids_map_.SetIsPlaceholder(install_options().url, is_placeholder);
    registrar_->AddAsInstalled(app_id);
    return {web_app::InstallResultCode::kSuccess, app_id};
  }

  Profile* profile_;
  web_app::TestAppRegistrar* registrar_;
  bool succeeds_;
  web_app::ExtensionIdsMap extension_ids_map_;

  base::OnceClosure on_install_called_;
  base::OnceClosure on_install_placeholder_called_;

  DISALLOW_COPY_AND_ASSIGN(TestBookmarkAppInstallationTask);
};

class TestBookmarkAppUninstaller : public BookmarkAppUninstaller {
 public:
  TestBookmarkAppUninstaller(Profile* profile,
                             web_app::TestAppRegistrar* registrar)
      : BookmarkAppUninstaller(profile, registrar), registrar_(registrar) {}

  ~TestBookmarkAppUninstaller() override = default;

  size_t uninstall_call_count() { return uninstall_call_count_; }

  const std::vector<GURL>& uninstalled_app_urls() {
    return uninstalled_app_urls_;
  }

  const GURL& last_uninstalled_app_url() { return uninstalled_app_urls_[0]; }

  void SetNextResultForTesting(const GURL& app_url, bool result) {
    DCHECK(!base::ContainsKey(next_result_map_, app_url));
    next_result_map_[app_url] = result;
  }

  // BookmarkAppUninstaller
  bool UninstallApp(const GURL& app_url) override {
    DCHECK(base::ContainsKey(next_result_map_, app_url));

    ++uninstall_call_count_;
    uninstalled_app_urls_.push_back(app_url);

    bool result = next_result_map_[app_url];
    next_result_map_.erase(app_url);

    if (result)
      registrar_->RemoveAsInstalled(GenerateFakeAppId(app_url));

    return result;
  }

 private:
  std::map<GURL, bool> next_result_map_;
  web_app::TestAppRegistrar* registrar_;

  size_t uninstall_call_count_ = 0;
  std::vector<GURL> uninstalled_app_urls_;

  DISALLOW_COPY_AND_ASSIGN(TestBookmarkAppUninstaller);
};

}  // namespace

class PendingBookmarkAppManagerTest : public ChromeRenderViewHostTestHarness {
 public:
  PendingBookmarkAppManagerTest()
      : successful_installation_task_creator_(base::BindRepeating(
            &PendingBookmarkAppManagerTest::CreateSuccessfulInstallationTask,
            base::Unretained(this))),
        failing_installation_task_creator_(base::BindRepeating(
            &PendingBookmarkAppManagerTest::CreateFailingInstallationTask,
            base::Unretained(this))) {}

  ~PendingBookmarkAppManagerTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    registrar_ = std::make_unique<web_app::TestAppRegistrar>();
    ui_delegate_ = std::make_unique<web_app::TestWebAppUiDelegate>();
    install_finalizer_ = std::make_unique<web_app::TestInstallFinalizer>();

    web_app::WebAppProvider::Get(profile())->set_ui_delegate(
        ui_delegate_.get());
  }

  void TearDown() override {
    uninstaller_ = nullptr;
    ChromeRenderViewHostTestHarness::TearDown();
  }

  std::unique_ptr<BookmarkAppInstallationTask> CreateInstallationTask(
      Profile* profile,
      web_app::InstallFinalizer* install_finalizer,
      web_app::InstallOptions install_options,
      bool succeeds) {
    auto task = std::make_unique<TestBookmarkAppInstallationTask>(
        profile, registrar_.get(), install_finalizer,
        std::move(install_options), succeeds);
    auto* task_ptr = task.get();
    task->SetOnInstallCalled(base::BindLambdaForTesting([this, task_ptr]() {
      ++install_run_count_;
      last_app_info_ = task_ptr->install_options();
    }));

    task->SetOnInstallPlaceholderCalled(
        base::BindLambdaForTesting([this, task_ptr]() {
          ++install_placeholder_run_count_;
          last_app_info_ = task_ptr->install_options();
        }));
    return task;
  }

  std::unique_ptr<BookmarkAppInstallationTask> CreateSuccessfulInstallationTask(
      Profile* profile,
      web_app::InstallFinalizer* install_finalizer,
      web_app::InstallOptions install_options) {
    return CreateInstallationTask(profile, install_finalizer,
                                  std::move(install_options),
                                  true /* succeeds */);
  }

  std::unique_ptr<BookmarkAppInstallationTask> CreateFailingInstallationTask(
      Profile* profile,
      web_app::InstallFinalizer* install_finalizer,
      web_app::InstallOptions install_options) {
    return CreateInstallationTask(profile, install_finalizer,
                                  std::move(install_options),
                                  false /* succeeds */);
  }

 protected:
  std::pair<GURL, web_app::InstallResultCode> InstallAndWait(
      web_app::PendingAppManager* pending_app_manager,
      web_app::InstallOptions install_options) {
    base::RunLoop run_loop;

    base::Optional<GURL> url;
    base::Optional<web_app::InstallResultCode> code;

    pending_app_manager->Install(
        std::move(install_options),
        base::BindLambdaForTesting(
            [&](const GURL& u, web_app::InstallResultCode c) {
              url = u;
              code = c;
              run_loop.Quit();
            }));
    run_loop.Run();

    return {url.value(), code.value()};
  }

  std::vector<std::pair<GURL, web_app::InstallResultCode>> InstallAppsAndWait(
      web_app::PendingAppManager* pending_app_manager,
      std::vector<web_app::InstallOptions> apps_to_install) {
    std::vector<std::pair<GURL, web_app::InstallResultCode>> results;

    base::RunLoop run_loop;
    auto barrier_closure =
        base::BarrierClosure(apps_to_install.size(), run_loop.QuitClosure());
    pending_app_manager->InstallApps(
        std::move(apps_to_install),
        base::BindLambdaForTesting(
            [&](const GURL& url, web_app::InstallResultCode code) {
              results.emplace_back(url, code);
              barrier_closure.Run();
            }));
    run_loop.Run();

    return results;
  }

  std::vector<std::pair<GURL, bool>> UninstallAppsAndWait(
      web_app::PendingAppManager* pending_app_manager,
      std::vector<GURL> apps_to_uninstall) {
    std::vector<std::pair<GURL, bool>> results;

    base::RunLoop run_loop;
    auto barrier_closure =
        base::BarrierClosure(apps_to_uninstall.size(), run_loop.QuitClosure());
    pending_app_manager->UninstallApps(
        std::move(apps_to_uninstall),
        base::BindLambdaForTesting(
            [&](const GURL& url, bool successfully_uninstalled) {
              results.emplace_back(url, successfully_uninstalled);
              barrier_closure.Run();
            }));
    run_loop.Run();

    return results;
  }

  const PendingBookmarkAppManager::TaskFactory&
  successful_installation_task_creator() {
    return successful_installation_task_creator_;
  }

  const PendingBookmarkAppManager::TaskFactory&
  failing_installation_task_creator() {
    return failing_installation_task_creator_;
  }

  std::unique_ptr<PendingBookmarkAppManager>
  GetPendingBookmarkAppManagerWithTestFactories() {
    auto manager = std::make_unique<PendingBookmarkAppManager>(
        profile(), registrar_.get(), install_finalizer_.get());
    manager->SetTaskFactoryForTesting(successful_installation_task_creator());

    // The test suite doesn't support multiple uninstallers.
    DCHECK_EQ(nullptr, uninstaller_);

    auto uninstaller = std::make_unique<TestBookmarkAppUninstaller>(
        profile(), registrar_.get());
    uninstaller_ = uninstaller.get();
    manager->SetUninstallerForTesting(std::move(uninstaller));

    // The test suite doesn't support multiple loaders.
    DCHECK_EQ(nullptr, url_loader_);

    auto url_loader = std::make_unique<web_app::TestWebAppUrlLoader>();
    url_loader_ = url_loader.get();
    manager->SetUrlLoaderForTesting(std::move(url_loader));

    return manager;
  }

  // InstallOptions that was used to run the last installation task.
  const web_app::InstallOptions& last_app_info() {
    CHECK(last_app_info_.has_value());
    return *last_app_info_;
  }

  // Number of times BookmarkAppInstallationTask::Install was called. Reflects
  // how many times we've tried to create an Extension.
  size_t install_run_count() { return install_run_count_; }

  // Number of times BookmarkAppInstallatioNTask::InstallPlaceholder was called.
  size_t install_placeholder_run_count() {
    return install_placeholder_run_count_;
  }

  size_t uninstall_call_count() { return uninstaller_->uninstall_call_count(); }

  const std::vector<GURL>& uninstalled_app_urls() {
    return uninstaller_->uninstalled_app_urls();
  }

  const GURL& last_uninstalled_app_url() {
    return uninstaller_->last_uninstalled_app_url();
  }

  web_app::TestAppRegistrar* registrar() { return registrar_.get(); }

  web_app::TestWebAppUiDelegate* ui_delegate() { return ui_delegate_.get(); }

  TestBookmarkAppUninstaller* uninstaller() { return uninstaller_; }

  web_app::TestWebAppUrlLoader* url_loader() { return url_loader_; }

 private:
  base::Optional<web_app::InstallOptions> last_app_info_;
  size_t install_run_count_ = 0;
  size_t install_placeholder_run_count_ = 0;

  PendingBookmarkAppManager::TaskFactory successful_installation_task_creator_;
  PendingBookmarkAppManager::TaskFactory failing_installation_task_creator_;

  std::unique_ptr<web_app::TestAppRegistrar> registrar_;
  std::unique_ptr<web_app::TestWebAppUiDelegate> ui_delegate_;
  std::unique_ptr<web_app::TestInstallFinalizer> install_finalizer_;

  TestBookmarkAppUninstaller* uninstaller_ = nullptr;
  web_app::TestWebAppUrlLoader* url_loader_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(PendingBookmarkAppManagerTest);
};

TEST_F(PendingBookmarkAppManagerTest, Install_Succeeds) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();
  url_loader()->SetNextLoadUrlResult(
      GURL(kFooWebAppUrl), web_app::WebAppUrlLoader::Result::kUrlLoaded);
  base::Optional<GURL> url;
  base::Optional<web_app::InstallResultCode> code;
  std::tie(url, code) =
      InstallAndWait(pending_app_manager.get(), GetFooInstallOptions());

  EXPECT_EQ(web_app::InstallResultCode::kSuccess, code.value());
  EXPECT_EQ(GURL(kFooWebAppUrl), url.value());

  EXPECT_EQ(1u, install_run_count());
  EXPECT_EQ(GetFooInstallOptions(), last_app_info());
}

TEST_F(PendingBookmarkAppManagerTest, Install_SerialCallsDifferentApps) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();
  url_loader()->SetNextLoadUrlResult(
      GURL(kFooWebAppUrl), web_app::WebAppUrlLoader::Result::kUrlLoaded);
  {
    base::Optional<GURL> url;
    base::Optional<web_app::InstallResultCode> code;
    std::tie(url, code) =
        InstallAndWait(pending_app_manager.get(), GetFooInstallOptions());

    EXPECT_EQ(web_app::InstallResultCode::kSuccess, code.value());
    EXPECT_EQ(GURL(kFooWebAppUrl), url.value());

    EXPECT_EQ(1u, install_run_count());
    EXPECT_EQ(GetFooInstallOptions(), last_app_info());
  }

  url_loader()->SetNextLoadUrlResult(
      GURL(kBarWebAppUrl), web_app::WebAppUrlLoader::Result::kUrlLoaded);
  {
    base::Optional<GURL> url;
    base::Optional<web_app::InstallResultCode> code;

    std::tie(url, code) =
        InstallAndWait(pending_app_manager.get(), GetBarInstallOptions());

    EXPECT_EQ(web_app::InstallResultCode::kSuccess, code.value());
    EXPECT_EQ(GURL(kBarWebAppUrl), url.value());

    EXPECT_EQ(2u, install_run_count());
    EXPECT_EQ(GetBarInstallOptions(), last_app_info());
  }
}

TEST_F(PendingBookmarkAppManagerTest, Install_ConcurrentCallsDifferentApps) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();

  url_loader()->SetNextLoadUrlResult(
      GURL(kBarWebAppUrl), web_app::WebAppUrlLoader::Result::kUrlLoaded);
  url_loader()->SetNextLoadUrlResult(
      GURL(kFooWebAppUrl), web_app::WebAppUrlLoader::Result::kUrlLoaded);

  base::RunLoop run_loop;
  pending_app_manager->Install(
      GetFooInstallOptions(),
      base::BindLambdaForTesting(
          [&](const GURL& url, web_app::InstallResultCode code) {
            EXPECT_EQ(web_app::InstallResultCode::kSuccess, code);
            EXPECT_EQ(GURL(kFooWebAppUrl), url);

            // Two installations tasks should have run at this point,
            // one from the last call to install (which gets higher priority),
            // and another one for this call to install.
            EXPECT_EQ(2u, install_run_count());
            EXPECT_EQ(GetFooInstallOptions(), last_app_info());

            run_loop.Quit();
          }));
  pending_app_manager->Install(
      GetBarInstallOptions(),
      base::BindLambdaForTesting(
          [&](const GURL& url, web_app::InstallResultCode code) {
            EXPECT_EQ(web_app::InstallResultCode::kSuccess, code);
            EXPECT_EQ(GURL(kBarWebAppUrl), url);

            // The last call gets higher priority so only one
            // installation task should have run at this point.
            EXPECT_EQ(1u, install_run_count());
            EXPECT_EQ(GetBarInstallOptions(), last_app_info());
          }));
  run_loop.Run();
}

TEST_F(PendingBookmarkAppManagerTest, Install_PendingSuccessfulTask) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();
  url_loader()->SetNextLoadUrlResult(
      GURL(kFooWebAppUrl), web_app::WebAppUrlLoader::Result::kUrlLoaded);
  url_loader()->SetNextLoadUrlResult(
      GURL(kBarWebAppUrl), web_app::WebAppUrlLoader::Result::kUrlLoaded);
  url_loader()->SaveLoadUrlRequests();

  base::RunLoop foo_run_loop;
  base::RunLoop bar_run_loop;

  pending_app_manager->Install(
      GetFooInstallOptions(),
      base::BindLambdaForTesting(
          [&](const GURL& url, web_app::InstallResultCode code) {
            EXPECT_EQ(web_app::InstallResultCode::kSuccess, code);
            EXPECT_EQ(GURL(kFooWebAppUrl), url);

            EXPECT_EQ(1u, install_run_count());
            EXPECT_EQ(GetFooInstallOptions(), last_app_info());

            foo_run_loop.Quit();
          }));
  // Make sure the installation has started.
  base::RunLoop().RunUntilIdle();

  pending_app_manager->Install(
      GetBarInstallOptions(),
      base::BindLambdaForTesting(
          [&](const GURL& url, web_app::InstallResultCode code) {
            EXPECT_EQ(web_app::InstallResultCode::kSuccess, code);
            EXPECT_EQ(GURL(kBarWebAppUrl), url);

            EXPECT_EQ(2u, install_run_count());
            EXPECT_EQ(GetBarInstallOptions(), last_app_info());

            bar_run_loop.Quit();
          }));

  url_loader()->ProcessLoadUrlRequests();
  foo_run_loop.Run();

  // Make sure the second installation has started.
  base::RunLoop().RunUntilIdle();

  url_loader()->ProcessLoadUrlRequests();
  bar_run_loop.Run();
}

TEST_F(PendingBookmarkAppManagerTest, Install_PendingFailingTask) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();
  url_loader()->SetNextLoadUrlResult(
      GURL(kFooWebAppUrl),
      web_app::WebAppUrlLoader::Result::kRedirectedUrlLoaded);
  url_loader()->SetNextLoadUrlResult(
      GURL(kBarWebAppUrl), web_app::WebAppUrlLoader::Result::kUrlLoaded);
  url_loader()->SaveLoadUrlRequests();

  base::RunLoop foo_run_loop;
  base::RunLoop bar_run_loop;

  pending_app_manager->Install(
      GetFooInstallOptions(),
      base::BindLambdaForTesting(
          [&](const GURL& url, web_app::InstallResultCode code) {
            EXPECT_EQ(web_app::InstallResultCode::kFailedUnknownReason, code);
            EXPECT_EQ(GURL(kFooWebAppUrl), url);

            // The installation didn't run because we loaded the wrong url.
            EXPECT_EQ(0u, install_run_count());
            foo_run_loop.Quit();
          }));
  // Make sure the installation has started.
  base::RunLoop().RunUntilIdle();

  pending_app_manager->Install(
      GetBarInstallOptions(),
      base::BindLambdaForTesting(
          [&](const GURL& url, web_app::InstallResultCode code) {
            EXPECT_EQ(web_app::InstallResultCode::kSuccess, code);
            EXPECT_EQ(GURL(kBarWebAppUrl), url);

            EXPECT_EQ(1u, install_run_count());
            EXPECT_EQ(GetBarInstallOptions(), last_app_info());

            bar_run_loop.Quit();
          }));

  url_loader()->ProcessLoadUrlRequests();
  foo_run_loop.Run();

  // Make sure the second installation has started.
  base::RunLoop().RunUntilIdle();

  url_loader()->ProcessLoadUrlRequests();
  bar_run_loop.Run();
}

TEST_F(PendingBookmarkAppManagerTest, Install_ReentrantCallback) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();
  url_loader()->SetNextLoadUrlResult(
      GURL(kFooWebAppUrl), web_app::WebAppUrlLoader::Result::kUrlLoaded);
  url_loader()->SetNextLoadUrlResult(
      GURL(kBarWebAppUrl), web_app::WebAppUrlLoader::Result::kUrlLoaded);

  base::RunLoop run_loop;
  auto final_callback = base::BindLambdaForTesting(
      [&](const GURL& url, web_app::InstallResultCode code) {
        EXPECT_EQ(web_app::InstallResultCode::kSuccess, code);
        EXPECT_EQ(GURL(kBarWebAppUrl), url);

        EXPECT_EQ(2u, install_run_count());
        EXPECT_EQ(GetBarInstallOptions(), last_app_info());
        run_loop.Quit();
      });
  auto reentrant_callback = base::BindLambdaForTesting(
      [&](const GURL& url, web_app::InstallResultCode code) {
        EXPECT_EQ(web_app::InstallResultCode::kSuccess, code);
        EXPECT_EQ(GURL(kFooWebAppUrl), url);

        EXPECT_EQ(1u, install_run_count());
        EXPECT_EQ(GetFooInstallOptions(), last_app_info());

        pending_app_manager->Install(GetBarInstallOptions(), final_callback);
      });

  // Call Install() with a callback that tries to install another app.
  pending_app_manager->Install(GetFooInstallOptions(), reentrant_callback);
  run_loop.Run();
}

TEST_F(PendingBookmarkAppManagerTest, Install_SerialCallsSameApp) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();
  url_loader()->SetNextLoadUrlResult(
      GURL(kFooWebAppUrl), web_app::WebAppUrlLoader::Result::kUrlLoaded);

  {
    base::Optional<GURL> url;
    base::Optional<web_app::InstallResultCode> code;
    std::tie(url, code) =
        InstallAndWait(pending_app_manager.get(), GetFooInstallOptions());

    EXPECT_EQ(web_app::InstallResultCode::kSuccess, code);
    EXPECT_EQ(GURL(kFooWebAppUrl), url);

    EXPECT_EQ(1u, install_run_count());
    EXPECT_EQ(GetFooInstallOptions(), last_app_info());
  }

  {
    base::Optional<GURL> url;
    base::Optional<web_app::InstallResultCode> code;
    std::tie(url, code) =
        InstallAndWait(pending_app_manager.get(), GetFooInstallOptions());

    EXPECT_EQ(web_app::InstallResultCode::kAlreadyInstalled, code);
    EXPECT_EQ(GURL(kFooWebAppUrl), url);

    // The app is already installed so we shouldn't try to install it again.
    EXPECT_EQ(1u, install_run_count());
  }
}

TEST_F(PendingBookmarkAppManagerTest, Install_ConcurrentCallsSameApp) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();
  url_loader()->SetNextLoadUrlResult(
      GURL(kFooWebAppUrl), web_app::WebAppUrlLoader::Result::kUrlLoaded);

  base::RunLoop run_loop;
  bool first_callback_ran = false;

  pending_app_manager->Install(
      GetFooInstallOptions(),
      base::BindLambdaForTesting(
          [&](const GURL& url, web_app::InstallResultCode code) {
            // kAlreadyInstalled because the last call to Install gets higher
            // priority.
            EXPECT_EQ(web_app::InstallResultCode::kAlreadyInstalled, code);
            EXPECT_EQ(GURL(kFooWebAppUrl), url);

            // Only one installation task should run because the app was already
            // installed.
            EXPECT_EQ(1u, install_run_count());

            EXPECT_TRUE(first_callback_ran);

            run_loop.Quit();
          }));

  pending_app_manager->Install(
      GetFooInstallOptions(),
      base::BindLambdaForTesting(
          [&](const GURL& url, web_app::InstallResultCode code) {
            EXPECT_EQ(web_app::InstallResultCode::kSuccess, code);
            EXPECT_EQ(GURL(kFooWebAppUrl), url);

            EXPECT_EQ(1u, install_run_count());
            EXPECT_EQ(GetFooInstallOptions(), last_app_info());
            first_callback_ran = true;
          }));
  run_loop.Run();

  EXPECT_EQ(1u, install_run_count());
  EXPECT_EQ(GetFooInstallOptions(), last_app_info());
}

TEST_F(PendingBookmarkAppManagerTest, Install_AlwaysUpdate) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();
  url_loader()->SetNextLoadUrlResult(
      GURL(kFooWebAppUrl), web_app::WebAppUrlLoader::Result::kUrlLoaded);

  auto get_always_update_info = []() {
    web_app::InstallOptions options(GURL(kFooWebAppUrl),
                                    web_app::LaunchContainer::kWindow,
                                    web_app::InstallSource::kExternalPolicy);
    options.always_update = true;
    return options;
  };

  {
    base::Optional<GURL> url;
    base::Optional<web_app::InstallResultCode> code;
    std::tie(url, code) =
        InstallAndWait(pending_app_manager.get(), get_always_update_info());

    EXPECT_EQ(web_app::InstallResultCode::kSuccess, code);
    EXPECT_EQ(GURL(kFooWebAppUrl), url);

    EXPECT_EQ(1u, install_run_count());
    EXPECT_EQ(get_always_update_info(), last_app_info());
  }

  url_loader()->SetNextLoadUrlResult(
      GURL(kFooWebAppUrl), web_app::WebAppUrlLoader::Result::kUrlLoaded);
  {
    base::Optional<GURL> url;
    base::Optional<web_app::InstallResultCode> code;
    std::tie(url, code) =
        InstallAndWait(pending_app_manager.get(), get_always_update_info());

    EXPECT_EQ(web_app::InstallResultCode::kSuccess, code);
    EXPECT_EQ(GURL(kFooWebAppUrl), url);

    // The app should be installed again because of the |always_update| flag.
    EXPECT_EQ(2u, install_run_count());
    EXPECT_EQ(get_always_update_info(), last_app_info());
  }
}

TEST_F(PendingBookmarkAppManagerTest, Install_FailsLoadIncorrectURL) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();
  url_loader()->SetNextLoadUrlResult(
      GURL(kFooWebAppUrl),
      web_app::WebAppUrlLoader::Result::kRedirectedUrlLoaded);

  base::Optional<GURL> url;
  base::Optional<web_app::InstallResultCode> code;
  std::tie(url, code) =
      InstallAndWait(pending_app_manager.get(), GetFooInstallOptions());

  EXPECT_EQ(web_app::InstallResultCode::kFailedUnknownReason, code);
  EXPECT_EQ(GURL(kFooWebAppUrl), url);

  EXPECT_EQ(0u, install_run_count());
  EXPECT_EQ(0u, install_placeholder_run_count());
}

TEST_F(PendingBookmarkAppManagerTest, Install_PlaceholderApp) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();
  url_loader()->SetNextLoadUrlResult(
      GURL(kFooWebAppUrl),
      web_app::WebAppUrlLoader::Result::kRedirectedUrlLoaded);

  auto install_options = GetFooInstallOptions();
  install_options.install_placeholder = true;

  base::Optional<GURL> url;
  base::Optional<web_app::InstallResultCode> code;
  std::tie(url, code) =
      InstallAndWait(pending_app_manager.get(), install_options);

  EXPECT_EQ(web_app::InstallResultCode::kSuccess, code);
  EXPECT_EQ(GURL(kFooWebAppUrl), url);

  EXPECT_EQ(0u, install_run_count());
  EXPECT_EQ(1u, install_placeholder_run_count());
}

TEST_F(PendingBookmarkAppManagerTest, InstallApps_Succeeds) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();
  url_loader()->SetNextLoadUrlResult(
      GURL(kFooWebAppUrl), web_app::WebAppUrlLoader::Result::kUrlLoaded);

  std::vector<web_app::InstallOptions> apps_to_install;
  apps_to_install.push_back(GetFooInstallOptions());

  InstallAppsResults results =
      InstallAppsAndWait(pending_app_manager.get(), std::move(apps_to_install));

  EXPECT_EQ(results,
            InstallAppsResults(
                {{GURL(kFooWebAppUrl), web_app::InstallResultCode::kSuccess}}));

  EXPECT_EQ(1u, install_run_count());
  EXPECT_EQ(GetFooInstallOptions(), last_app_info());
}

TEST_F(PendingBookmarkAppManagerTest, InstallApps_Fails) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();
  url_loader()->SetNextLoadUrlResult(
      GURL(kFooWebAppUrl),
      web_app::WebAppUrlLoader::Result::kRedirectedUrlLoaded);

  std::vector<web_app::InstallOptions> apps_to_install;
  apps_to_install.push_back(GetFooInstallOptions());

  InstallAppsResults results =
      InstallAppsAndWait(pending_app_manager.get(), std::move(apps_to_install));

  EXPECT_EQ(results, InstallAppsResults(
                         {{GURL(kFooWebAppUrl),
                           web_app::InstallResultCode::kFailedUnknownReason}}));

  EXPECT_EQ(0u, install_run_count());
  EXPECT_EQ(0u, install_placeholder_run_count());
}

TEST_F(PendingBookmarkAppManagerTest, InstallApps_PlaceholderApp) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();
  url_loader()->SetNextLoadUrlResult(
      GURL(kFooWebAppUrl),
      web_app::WebAppUrlLoader::Result::kRedirectedUrlLoaded);

  std::vector<web_app::InstallOptions> apps_to_install;
  apps_to_install.push_back(GetFooInstallOptions());
  apps_to_install.front().install_placeholder = true;

  InstallAppsResults results =
      InstallAppsAndWait(pending_app_manager.get(), std::move(apps_to_install));

  EXPECT_EQ(results,
            InstallAppsResults(
                {{GURL(kFooWebAppUrl), web_app::InstallResultCode::kSuccess}}));

  EXPECT_EQ(0u, install_run_count());
  EXPECT_EQ(1u, install_placeholder_run_count());
}

TEST_F(PendingBookmarkAppManagerTest, InstallApps_Multiple) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();
  url_loader()->SetNextLoadUrlResult(
      GURL(kFooWebAppUrl), web_app::WebAppUrlLoader::Result::kUrlLoaded);
  url_loader()->SetNextLoadUrlResult(
      GURL(kBarWebAppUrl), web_app::WebAppUrlLoader::Result::kUrlLoaded);

  std::vector<web_app::InstallOptions> apps_to_install;
  apps_to_install.push_back(GetFooInstallOptions());
  apps_to_install.push_back(GetBarInstallOptions());

  InstallAppsResults results =
      InstallAppsAndWait(pending_app_manager.get(), std::move(apps_to_install));

  EXPECT_EQ(results,
            InstallAppsResults(
                {{GURL(kFooWebAppUrl), web_app::InstallResultCode::kSuccess},
                 {GURL(kBarWebAppUrl), web_app::InstallResultCode::kSuccess}}));

  EXPECT_EQ(2u, install_run_count());
  EXPECT_EQ(GetBarInstallOptions(), last_app_info());
}

TEST_F(PendingBookmarkAppManagerTest, InstallApps_PendingInstallApps) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();
  url_loader()->SetNextLoadUrlResult(
      GURL(kFooWebAppUrl), web_app::WebAppUrlLoader::Result::kUrlLoaded);
  url_loader()->SetNextLoadUrlResult(
      GURL(kBarWebAppUrl), web_app::WebAppUrlLoader::Result::kUrlLoaded);

  base::RunLoop run_loop;
  {
    std::vector<web_app::InstallOptions> apps_to_install;
    apps_to_install.push_back(GetFooInstallOptions());

    pending_app_manager->InstallApps(
        std::move(apps_to_install),
        base::BindLambdaForTesting(
            [&](const GURL& url, web_app::InstallResultCode code) {
              EXPECT_EQ(web_app::InstallResultCode::kSuccess, code);
              EXPECT_EQ(GURL(kFooWebAppUrl), url);

              EXPECT_EQ(1u, install_run_count());
              EXPECT_EQ(GetFooInstallOptions(), last_app_info());
            }));
  }

  {
    std::vector<web_app::InstallOptions> apps_to_install;
    apps_to_install.push_back(GetBarInstallOptions());

    pending_app_manager->InstallApps(
        std::move(apps_to_install),
        base::BindLambdaForTesting(
            [&](const GURL& url, web_app::InstallResultCode code) {
              EXPECT_EQ(web_app::InstallResultCode::kSuccess, code);
              EXPECT_EQ(GURL(kBarWebAppUrl), url);

              EXPECT_EQ(2u, install_run_count());
              EXPECT_EQ(GetBarInstallOptions(), last_app_info());

              run_loop.Quit();
            }));
  }
  run_loop.Run();
}

TEST_F(PendingBookmarkAppManagerTest, Install_PendingMulitpleInstallApps) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();
  url_loader()->SetNextLoadUrlResult(
      GURL(kFooWebAppUrl), web_app::WebAppUrlLoader::Result::kUrlLoaded);
  url_loader()->SetNextLoadUrlResult(
      GURL(kBarWebAppUrl), web_app::WebAppUrlLoader::Result::kUrlLoaded);
  url_loader()->SetNextLoadUrlResult(
      GURL(kQuxWebAppUrl), web_app::WebAppUrlLoader::Result::kUrlLoaded);

  base::RunLoop run_loop;

  std::vector<web_app::InstallOptions> apps_to_install;
  apps_to_install.push_back(GetFooInstallOptions());
  apps_to_install.push_back(GetBarInstallOptions());

  // Queue through InstallApps.
  int callback_calls = 0;
  pending_app_manager->InstallApps(
      std::move(apps_to_install),
      base::BindLambdaForTesting(
          [&](const GURL& url, web_app::InstallResultCode code) {
            ++callback_calls;
            if (callback_calls == 1) {
              EXPECT_EQ(web_app::InstallResultCode::kSuccess, code);
              EXPECT_EQ(GURL(kFooWebAppUrl), url);

              EXPECT_EQ(2u, install_run_count());
              EXPECT_EQ(GetFooInstallOptions(), last_app_info());
            } else if (callback_calls == 2) {
              EXPECT_EQ(web_app::InstallResultCode::kSuccess, code);
              EXPECT_EQ(GURL(kBarWebAppUrl), url);

              EXPECT_EQ(3u, install_run_count());
              EXPECT_EQ(GetBarInstallOptions(), last_app_info());

              run_loop.Quit();
            } else {
              NOTREACHED();
            }
          }));

  // Queue through Install.
  pending_app_manager->Install(
      GetQuxInstallOptions(),
      base::BindLambdaForTesting(
          [&](const GURL& url, web_app::InstallResultCode code) {
            EXPECT_EQ(web_app::InstallResultCode::kSuccess, code);
            EXPECT_EQ(GURL(kQuxWebAppUrl), url);

            // The install request from Install should be processed first.
            EXPECT_EQ(1u, install_run_count());
            EXPECT_EQ(GetQuxInstallOptions(), last_app_info());
          }));

  run_loop.Run();
}

TEST_F(PendingBookmarkAppManagerTest, InstallApps_PendingInstall) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();
  url_loader()->SetNextLoadUrlResult(
      GURL(kFooWebAppUrl), web_app::WebAppUrlLoader::Result::kUrlLoaded);
  url_loader()->SetNextLoadUrlResult(
      GURL(kBarWebAppUrl), web_app::WebAppUrlLoader::Result::kUrlLoaded);
  url_loader()->SetNextLoadUrlResult(
      GURL(kQuxWebAppUrl), web_app::WebAppUrlLoader::Result::kUrlLoaded);

  base::RunLoop run_loop;

  // Queue through Install.
  pending_app_manager->Install(
      GetQuxInstallOptions(),
      base::BindLambdaForTesting(
          [&](const GURL& url, web_app::InstallResultCode code) {
            EXPECT_EQ(web_app::InstallResultCode::kSuccess, code);
            EXPECT_EQ(GURL(kQuxWebAppUrl), url);

            // The install request from Install should be processed first.
            EXPECT_EQ(1u, install_run_count());
            EXPECT_EQ(GetQuxInstallOptions(), last_app_info());
          }));

  // Queue through InstallApps.
  std::vector<web_app::InstallOptions> apps_to_install;
  apps_to_install.push_back(GetFooInstallOptions());
  apps_to_install.push_back(GetBarInstallOptions());

  int callback_calls = 0;
  pending_app_manager->InstallApps(
      std::move(apps_to_install),
      base::BindLambdaForTesting(
          [&](const GURL& url, web_app::InstallResultCode code) {
            ++callback_calls;
            if (callback_calls == 1) {
              EXPECT_EQ(web_app::InstallResultCode::kSuccess, code);
              EXPECT_EQ(GURL(kFooWebAppUrl), url);

              // The install requests from InstallApps should be processed next.
              EXPECT_EQ(2u, install_run_count());
              EXPECT_EQ(GetFooInstallOptions(), last_app_info());

              return;
            }
            if (callback_calls == 2) {
              EXPECT_EQ(web_app::InstallResultCode::kSuccess, code);
              EXPECT_EQ(GURL(kBarWebAppUrl), url);

              EXPECT_EQ(3u, install_run_count());
              EXPECT_EQ(GetBarInstallOptions(), last_app_info());

              run_loop.Quit();
              return;
            }
            NOTREACHED();
          }));
  run_loop.Run();
}

TEST_F(PendingBookmarkAppManagerTest, ExtensionUninstalled) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();
  url_loader()->SetNextLoadUrlResult(
      GURL(kFooWebAppUrl), web_app::WebAppUrlLoader::Result::kUrlLoaded);

  {
    base::Optional<GURL> url;
    base::Optional<web_app::InstallResultCode> code;
    std::tie(url, code) =
        InstallAndWait(pending_app_manager.get(), GetFooInstallOptions());

    EXPECT_EQ(1u, install_run_count());
    EXPECT_EQ(web_app::InstallResultCode::kSuccess, code.value());
  }

  // Simulate the extension for the app getting uninstalled.
  const std::string app_id = GenerateFakeAppId(GURL(kFooWebAppUrl));
  registrar()->RemoveAsInstalled(app_id);

  // Try to install the app again.
  {
    url_loader()->SetNextLoadUrlResult(
        GURL(kFooWebAppUrl), web_app::WebAppUrlLoader::Result::kUrlLoaded);

    base::Optional<GURL> url;
    base::Optional<web_app::InstallResultCode> code;
    std::tie(url, code) =
        InstallAndWait(pending_app_manager.get(), GetFooInstallOptions());

    // The extension was uninstalled so a new installation task should run.
    EXPECT_EQ(2u, install_run_count());
    EXPECT_EQ(web_app::InstallResultCode::kSuccess, code.value());
  }
}

TEST_F(PendingBookmarkAppManagerTest, ExternalExtensionUninstalled) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();
  url_loader()->SetNextLoadUrlResult(
      GURL(kFooWebAppUrl), web_app::WebAppUrlLoader::Result::kUrlLoaded);

  {
    base::Optional<GURL> url;
    base::Optional<web_app::InstallResultCode> code;
    std::tie(url, code) =
        InstallAndWait(pending_app_manager.get(), GetFooInstallOptions());

    EXPECT_EQ(1u, install_run_count());
    EXPECT_EQ(web_app::InstallResultCode::kSuccess, code.value());
  }

  // Simulate external extension for the app getting uninstalled by the user.
  const std::string app_id = GenerateFakeAppId(GURL(kFooWebAppUrl));
  registrar()->AddAsExternalAppUninstalledByUser(app_id);
  registrar()->RemoveAsInstalled(app_id);

  // The extension was uninstalled by the user. Installing again should succeed
  // or fail depending on whether we set override_previous_user_uninstall. We
  // try with override_previous_user_uninstall false first, true second.
  {
    base::Optional<GURL> url;
    base::Optional<web_app::InstallResultCode> code;
    std::tie(url, code) = InstallAndWait(
        pending_app_manager.get(),
        GetFooInstallOptions(false /* override_previous_user_uninstall */));

    // The app shouldn't be installed because the user previously uninstalled
    // it, so there shouldn't be any new installation task runs.
    EXPECT_EQ(1u, install_run_count());
    EXPECT_EQ(web_app::InstallResultCode::kPreviouslyUninstalled, code.value());
  }

  {
    url_loader()->SetNextLoadUrlResult(
        GURL(kFooWebAppUrl), web_app::WebAppUrlLoader::Result::kUrlLoaded);

    base::Optional<GURL> url;
    base::Optional<web_app::InstallResultCode> code;
    std::tie(url, code) = InstallAndWait(
        pending_app_manager.get(),
        GetFooInstallOptions(true /* override_previous_user_uninstall */));

    EXPECT_EQ(2u, install_run_count());
    EXPECT_EQ(web_app::InstallResultCode::kSuccess, code.value());
  }
}

TEST_F(PendingBookmarkAppManagerTest, UninstallApps_Succeeds) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();
  registrar()->AddAsInstalled(GenerateFakeAppId(GURL(kFooWebAppUrl)));

  uninstaller()->SetNextResultForTesting(GURL(kFooWebAppUrl), true);
  UninstallAppsResults results = UninstallAppsAndWait(
      pending_app_manager.get(), std::vector<GURL>{GURL(kFooWebAppUrl)});

  EXPECT_EQ(results, UninstallAppsResults({{GURL(kFooWebAppUrl), true}}));

  EXPECT_EQ(1u, uninstall_call_count());
  EXPECT_EQ(GURL(kFooWebAppUrl), last_uninstalled_app_url());
}

TEST_F(PendingBookmarkAppManagerTest, UninstallApps_Fails) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();

  uninstaller()->SetNextResultForTesting(GURL(kFooWebAppUrl), false);
  UninstallAppsResults results = UninstallAppsAndWait(
      pending_app_manager.get(), std::vector<GURL>{GURL(kFooWebAppUrl)});
  EXPECT_EQ(results, UninstallAppsResults({{GURL(kFooWebAppUrl), false}}));

  EXPECT_EQ(1u, uninstall_call_count());
  EXPECT_EQ(GURL(kFooWebAppUrl), last_uninstalled_app_url());
}

TEST_F(PendingBookmarkAppManagerTest, UninstallApps_Multiple) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();
  registrar()->AddAsInstalled(GenerateFakeAppId(GURL(kFooWebAppUrl)));
  registrar()->AddAsInstalled(GenerateFakeAppId(GURL(kBarWebAppUrl)));

  uninstaller()->SetNextResultForTesting(GURL(kFooWebAppUrl), true);
  uninstaller()->SetNextResultForTesting(GURL(kBarWebAppUrl), true);
  UninstallAppsResults results = UninstallAppsAndWait(
      pending_app_manager.get(),
      std::vector<GURL>{GURL(kFooWebAppUrl), GURL(kBarWebAppUrl)});
  EXPECT_EQ(results, UninstallAppsResults({{GURL(kFooWebAppUrl), true},
                                           {GURL(kBarWebAppUrl), true}}));

  EXPECT_EQ(2u, uninstall_call_count());
  EXPECT_EQ(std::vector<GURL>({GURL(kFooWebAppUrl), GURL(kBarWebAppUrl)}),
            uninstalled_app_urls());
}

TEST_F(PendingBookmarkAppManagerTest, UninstallApps_PendingInstall) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();
  url_loader()->SetNextLoadUrlResult(
      GURL(kFooWebAppUrl), web_app::WebAppUrlLoader::Result::kUrlLoaded);

  base::RunLoop run_loop;
  pending_app_manager->Install(
      GetFooInstallOptions(),
      base::BindLambdaForTesting(
          [&](const GURL& url, web_app::InstallResultCode code) {
            EXPECT_EQ(web_app::InstallResultCode::kSuccess, code);
            EXPECT_EQ(GURL(kFooWebAppUrl), url);
            run_loop.Quit();
          }));

  uninstaller()->SetNextResultForTesting(GURL(kFooWebAppUrl), false);
  UninstallAppsResults uninstall_results = UninstallAppsAndWait(
      pending_app_manager.get(), std::vector<GURL>{GURL(kFooWebAppUrl)});
  EXPECT_EQ(uninstall_results,
            UninstallAppsResults({{GURL(kFooWebAppUrl), false}}));
  EXPECT_EQ(1u, uninstall_call_count());

  run_loop.Run();
}

TEST_F(PendingBookmarkAppManagerTest, ReinstallPlaceholderApp_Success) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();
  // Install a placeholder app
  auto install_options = GetFooInstallOptions();
  install_options.install_placeholder = true;

  {
    url_loader()->SetNextLoadUrlResult(
        GURL(kFooWebAppUrl),
        web_app::WebAppUrlLoader::Result::kRedirectedUrlLoaded);
    base::Optional<GURL> url;
    base::Optional<web_app::InstallResultCode> code;
    std::tie(url, code) =
        InstallAndWait(pending_app_manager.get(), install_options);
    ASSERT_EQ(web_app::InstallResultCode::kSuccess, code.value());
    EXPECT_EQ(0u, install_run_count());
    EXPECT_EQ(1u, install_placeholder_run_count());
  }

  // Reinstall placeholder
  {
    install_options.reinstall_placeholder = true;
    url_loader()->SetNextLoadUrlResult(
        GURL(kFooWebAppUrl), web_app::WebAppUrlLoader::Result::kUrlLoaded);
    uninstaller()->SetNextResultForTesting(GURL(kFooWebAppUrl), true);

    base::Optional<GURL> url;
    base::Optional<web_app::InstallResultCode> code;
    std::tie(url, code) =
        InstallAndWait(pending_app_manager.get(), install_options);

    EXPECT_EQ(web_app::InstallResultCode::kSuccess, code.value());
    EXPECT_EQ(GURL(kFooWebAppUrl), url.value());

    EXPECT_EQ(1u, uninstall_call_count());
    EXPECT_EQ(GURL(kFooWebAppUrl), last_uninstalled_app_url());

    EXPECT_EQ(1u, install_run_count());
    EXPECT_EQ(1u, install_placeholder_run_count());
  }
}

TEST_F(PendingBookmarkAppManagerTest,
       ReinstallPlaceholderApp_FailsToUninstall) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();

  // Install a placeholder app
  auto install_options = GetFooInstallOptions();
  install_options.install_placeholder = true;

  {
    url_loader()->SetNextLoadUrlResult(
        GURL(kFooWebAppUrl),
        web_app::WebAppUrlLoader::Result::kRedirectedUrlLoaded);
    base::Optional<GURL> url;
    base::Optional<web_app::InstallResultCode> code;
    std::tie(url, code) =
        InstallAndWait(pending_app_manager.get(), install_options);
    ASSERT_EQ(web_app::InstallResultCode::kSuccess, code.value());
    EXPECT_EQ(0u, install_run_count());
    EXPECT_EQ(1u, install_placeholder_run_count());
  }

  // Reinstall placeholder
  {
    install_options.reinstall_placeholder = true;
    url_loader()->SetNextLoadUrlResult(
        GURL(kFooWebAppUrl), web_app::WebAppUrlLoader::Result::kUrlLoaded);
    uninstaller()->SetNextResultForTesting(GURL(kFooWebAppUrl), false);

    base::Optional<GURL> url;
    base::Optional<web_app::InstallResultCode> code;
    std::tie(url, code) =
        InstallAndWait(pending_app_manager.get(), install_options);

    EXPECT_EQ(web_app::InstallResultCode::kFailedUnknownReason, code.value());
    EXPECT_EQ(GURL(kFooWebAppUrl), url.value());

    EXPECT_EQ(1u, uninstall_call_count());
    EXPECT_EQ(GURL(kFooWebAppUrl), last_uninstalled_app_url());

    EXPECT_EQ(0u, install_run_count());
    EXPECT_EQ(1u, install_placeholder_run_count());
  }
}

TEST_F(PendingBookmarkAppManagerTest,
       ReinstallPlaceholderApp_ReinstallNotPossible) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();

  // Install a placeholder app
  auto install_options = GetFooInstallOptions();
  install_options.install_placeholder = true;

  {
    url_loader()->SetNextLoadUrlResult(
        GURL(kFooWebAppUrl),
        web_app::WebAppUrlLoader::Result::kRedirectedUrlLoaded);
    base::Optional<GURL> url;
    base::Optional<web_app::InstallResultCode> code;
    std::tie(url, code) =
        InstallAndWait(pending_app_manager.get(), install_options);
    ASSERT_EQ(web_app::InstallResultCode::kSuccess, code.value());
    EXPECT_EQ(0u, install_run_count());
    EXPECT_EQ(1u, install_placeholder_run_count());
  }

  // Reinstall placeholder
  {
    install_options.reinstall_placeholder = true;
    url_loader()->SetNextLoadUrlResult(
        GURL(kFooWebAppUrl),
        web_app::WebAppUrlLoader::Result::kRedirectedUrlLoaded);
    uninstaller()->SetNextResultForTesting(GURL(kFooWebAppUrl), true);

    base::Optional<GURL> url;
    base::Optional<web_app::InstallResultCode> code;
    std::tie(url, code) =
        InstallAndWait(pending_app_manager.get(), install_options);

    EXPECT_EQ(web_app::InstallResultCode::kSuccess, code.value());
    EXPECT_EQ(GURL(kFooWebAppUrl), url.value());

    // We don't uninstall the placeholder app if we are going to fail
    // installing the new app.
    EXPECT_EQ(0u, uninstall_call_count());

    EXPECT_EQ(0u, install_run_count());
    EXPECT_EQ(1u, install_placeholder_run_count());
  }
}

TEST_F(PendingBookmarkAppManagerTest,
       ReinstallPlaceholderAppWhenUnused_NoOpenedWindows) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();
  // Install a placeholder app
  auto install_options = GetFooInstallOptions();
  install_options.install_placeholder = true;

  {
    url_loader()->SetNextLoadUrlResult(
        GURL(kFooWebAppUrl),
        web_app::WebAppUrlLoader::Result::kRedirectedUrlLoaded);
    base::Optional<GURL> url;
    base::Optional<web_app::InstallResultCode> code;
    std::tie(url, code) =
        InstallAndWait(pending_app_manager.get(), install_options);
    ASSERT_EQ(web_app::InstallResultCode::kSuccess, code.value());
    EXPECT_EQ(0u, install_run_count());
    EXPECT_EQ(1u, install_placeholder_run_count());
  }

  // Reinstall placeholder
  {
    install_options.reinstall_placeholder = true;
    install_options.wait_for_windows_closed = true;
    ui_delegate()->SetNumWindowsForApp(GenerateFakeAppId(GURL(kFooWebAppUrl)),
                                       0);
    url_loader()->SetNextLoadUrlResult(
        GURL(kFooWebAppUrl), web_app::WebAppUrlLoader::Result::kUrlLoaded);
    uninstaller()->SetNextResultForTesting(GURL(kFooWebAppUrl), true);

    base::Optional<GURL> url;
    base::Optional<web_app::InstallResultCode> code;
    std::tie(url, code) =
        InstallAndWait(pending_app_manager.get(), install_options);

    EXPECT_EQ(web_app::InstallResultCode::kSuccess, code.value());
    EXPECT_EQ(GURL(kFooWebAppUrl), url.value());

    EXPECT_EQ(1u, uninstall_call_count());
    EXPECT_EQ(GURL(kFooWebAppUrl), last_uninstalled_app_url());

    EXPECT_EQ(1u, install_run_count());
    EXPECT_EQ(1u, install_placeholder_run_count());
  }
}

TEST_F(PendingBookmarkAppManagerTest,
       ReinstallPlaceholderAppWhenUnused_OneWindowOpened) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();

  // Install a placeholder app
  auto install_options = GetFooInstallOptions();
  install_options.install_placeholder = true;

  {
    url_loader()->SetNextLoadUrlResult(
        GURL(kFooWebAppUrl),
        web_app::WebAppUrlLoader::Result::kRedirectedUrlLoaded);
    base::Optional<GURL> url;
    base::Optional<web_app::InstallResultCode> code;
    std::tie(url, code) =
        InstallAndWait(pending_app_manager.get(), install_options);
    ASSERT_EQ(web_app::InstallResultCode::kSuccess, code.value());
    EXPECT_EQ(0u, install_run_count());
    EXPECT_EQ(1u, install_placeholder_run_count());
  }

  // Reinstall placeholder
  {
    install_options.reinstall_placeholder = true;
    install_options.wait_for_windows_closed = true;
    ui_delegate()->SetNumWindowsForApp(GenerateFakeAppId(GURL(kFooWebAppUrl)),
                                       1);
    url_loader()->SetNextLoadUrlResult(
        GURL(kFooWebAppUrl), web_app::WebAppUrlLoader::Result::kUrlLoaded);
    uninstaller()->SetNextResultForTesting(GURL(kFooWebAppUrl), true);

    base::Optional<GURL> url;
    base::Optional<web_app::InstallResultCode> code;
    std::tie(url, code) =
        InstallAndWait(pending_app_manager.get(), install_options);

    EXPECT_EQ(web_app::InstallResultCode::kSuccess, code.value());
    EXPECT_EQ(GURL(kFooWebAppUrl), url.value());

    EXPECT_EQ(1u, uninstall_call_count());
    EXPECT_EQ(GURL(kFooWebAppUrl), last_uninstalled_app_url());

    EXPECT_EQ(1u, install_run_count());
    EXPECT_EQ(1u, install_placeholder_run_count());
  }
}

}  // namespace extensions
