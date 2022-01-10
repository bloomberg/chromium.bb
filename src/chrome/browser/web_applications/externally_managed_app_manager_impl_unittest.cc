// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/externally_managed_app_manager_impl.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/one_shot_event.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/timer/mock_timer.h"
#include "chrome/browser/web_applications/externally_managed_app_install_task.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/externally_managed_app_registration_task.h"
#include "chrome/browser/web_applications/test/fake_install_finalizer.h"
#include "chrome/browser/web_applications/test/fake_web_app_registry_controller.h"
#include "chrome/browser/web_applications/test/fake_web_app_ui_manager.h"
#include "chrome/browser/web_applications/test/test_web_app_url_loader.h"
#include "chrome/browser/web_applications/test/web_app_registration_waiter.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_application_info.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/url_constants.h"

namespace web_app {

namespace {

using InstallAppsResults = std::vector<std::pair<GURL, InstallResultCode>>;
using UninstallAppsResults = std::vector<std::pair<GURL, bool>>;

ExternalInstallOptions GetInstallOptions(
    GURL url,
    absl::optional<bool> override_previous_user_uninstall =
        absl::optional<bool>()) {
  ExternalInstallOptions options(std::move(url), DisplayMode::kBrowser,
                                 ExternalInstallSource::kExternalPolicy);

  if (override_previous_user_uninstall.has_value())
    options.override_previous_user_uninstall =
        *override_previous_user_uninstall;

  return options;
}

std::unique_ptr<WebApplicationInfo> GetWebApplicationInfo(const GURL& url) {
  std::unique_ptr<WebApplicationInfo> info =
      std::make_unique<WebApplicationInfo>();
  info->start_url = url;
  info->scope = url.GetWithoutFilename();
  info->title = u"Foo Web App";
  return info;
}

ExternalInstallOptions GetInstallOptionsWithWebAppInfo(
    const GURL& url,
    absl::optional<bool> override_previous_user_uninstall =
        absl::optional<bool>()) {
  ExternalInstallOptions options(url, DisplayMode::kBrowser,
                                 ExternalInstallSource::kExternalPolicy);
  options.only_use_app_info_factory = true;
  // Static to ensure re-use across multiple function calls for
  // ExternalInstallOptions equality checking.
  static WebApplicationInfoFactory app_info_factory =
      base::BindRepeating(&GetWebApplicationInfo, url);
  options.app_info_factory = app_info_factory;

  if (override_previous_user_uninstall.has_value())
    options.override_previous_user_uninstall =
        *override_previous_user_uninstall;

  return options;
}

std::string GenerateFakeAppId(const GURL& url) {
  return FakeInstallFinalizer::GetAppIdForUrl(url);
}

// Class to delay completion of TestExternallyManagedAppInstallTasks.
//
// Tests can call into this class to tell it to save install requests and to
// trigger running the saved requests. TestExternallyManagedAppInstallTasks call
// into this class with a OnceClosure. This class then decides if the closure
// should be run immediately or if it should be saved.
class TestExternallyManagedAppInstallTaskManager {
 public:
  TestExternallyManagedAppInstallTaskManager() = default;
  TestExternallyManagedAppInstallTaskManager(
      const TestExternallyManagedAppInstallTaskManager&) = delete;
  TestExternallyManagedAppInstallTaskManager& operator=(
      const TestExternallyManagedAppInstallTaskManager&) = delete;
  ~TestExternallyManagedAppInstallTaskManager() = default;

  void SaveInstallRequests() { should_save_requests_ = true; }

  void RunOrSaveRequest(base::OnceClosure install_request) {
    if (should_save_requests_) {
      pending_install_requests_.push(std::move(install_request));
      return;
    }
    // Post a task to simulate tasks completing asynchronously.
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(install_request));
  }

  void ProcessSavedRequests() {
    // Swap to avoid re-entrancy issues.
    std::queue<base::OnceClosure> pending_install_requests;
    pending_install_requests.swap(pending_install_requests_);
    while (!pending_install_requests.empty()) {
      base::OnceClosure request = std::move(pending_install_requests.front());
      pending_install_requests.pop();
      std::move(request).Run();
    }
  }

  size_t num_pending_tasks() { return pending_install_requests_.size(); }

 private:
  std::queue<base::OnceClosure> pending_install_requests_;
  bool should_save_requests_ = false;
};

class FakeExternallyManagedAppManager : public ExternallyManagedAppManagerImpl {
 public:
  struct TestTaskResult {
    InstallResultCode code;
    bool did_install_placeholder;
  };

  FakeExternallyManagedAppManager(
      Profile* profile,
      FakeWebAppRegistryController& fake_registry_controller,
      TestExternallyManagedAppInstallTaskManager& test_install_task_manager)
      : ExternallyManagedAppManagerImpl(profile),
        fake_registry_controller_(fake_registry_controller),
        test_install_task_manager_(test_install_task_manager) {}

  ~FakeExternallyManagedAppManager() override {
    DCHECK(next_installation_task_results_.empty());
    DCHECK(next_installation_launch_urls_.empty());
    DCHECK(!preempt_registration_callback_);
  }

  size_t install_run_count() { return install_run_count_; }

  const std::vector<ExternalInstallOptions>& install_options_list() {
    return install_options_list_;
  }

  size_t registration_run_count() { return registration_run_count_; }

  const GURL& last_registered_install_url() {
    return last_registered_install_url_;
  }

  void SetNextInstallationTaskResult(const GURL& app_url,
                                     InstallResultCode result_code,
                                     bool did_install_placeholder = false) {
    DCHECK(!base::Contains(next_installation_task_results_, app_url));
    next_installation_task_results_[app_url] = {result_code,
                                                did_install_placeholder};
  }

  void SetNextInstallationLaunchURL(const GURL& app_url) {
    DCHECK(!base::Contains(next_installation_launch_urls_, app_url));
    next_installation_launch_urls_[app_url] = app_url.Resolve("launch");
  }

  bool MaybePreemptRegistration() {
    if (!preempt_registration_callback_)
      return false;

    absl::optional<base::OnceClosure> callback;
    preempt_registration_callback_.swap(callback);
    std::move(*callback).Run();
    return true;
  }

  std::unique_ptr<ExternallyManagedAppInstallTask> CreateInstallationTask(
      ExternalInstallOptions install_options) override {
    return std::make_unique<TestExternallyManagedAppInstallTask>(
        this, profile(), &test_url_loader_, test_install_task_manager_,
        std::move(install_options));
  }

  std::unique_ptr<ExternallyManagedAppRegistrationTaskBase> StartRegistration(
      GURL install_url) override {
    ++registration_run_count_;
    last_registered_install_url_ = install_url;
    return std::make_unique<TestExternallyManagedAppRegistrationTask>(
        install_url, this);
  }

  void OnInstallCalled(const ExternalInstallOptions& install_options) {
    ++install_run_count_;
    install_options_list_.push_back(install_options);
  }

  TestTaskResult GetNextInstallationTaskResult(const GURL& url) {
    DCHECK(base::Contains(next_installation_task_results_, url));
    auto result = next_installation_task_results_.at(url);
    next_installation_task_results_.erase(url);
    return result;
  }

  GURL GetNextInstallationLaunchURL(const GURL& url) {
    if (!base::Contains(next_installation_launch_urls_, url))
      return GURL::EmptyGURL();

    auto result = next_installation_launch_urls_.at(url);
    next_installation_launch_urls_.erase(url);
    return result;
  }

  void WaitForRegistrationAndCancel() {
    DCHECK(!preempt_registration_callback_);

    base::RunLoop run_loop;
    preempt_registration_callback_ =
        base::BindLambdaForTesting([&run_loop]() { run_loop.Quit(); });
    run_loop.Run();
  }

  void ReleaseWebContents() override {
    ExternallyManagedAppManagerImpl::ReleaseWebContents();

    // May be called more than once, if ExternallyManagedAppManager finishes all
    // tasks before it is shutdown.
    if (!web_contents_released_event_.is_signaled()) {
      web_contents_released_event_.Signal();
    }
  }

  // Wait for ExternallyManagedAppManager to finish all installations and
  // registrations.
  void WaitForWebContentsReleased() {
    base::RunLoop run_loop;
    web_contents_released_event_.Post(FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  FakeWebAppRegistryController& controller() {
    return fake_registry_controller_;
  }

 private:
  class TestExternallyManagedAppInstallTask
      : public ExternallyManagedAppInstallTask {
   public:
    TestExternallyManagedAppInstallTask(
        FakeExternallyManagedAppManager* externally_managed_app_manager_impl,
        Profile* profile,
        TestWebAppUrlLoader* test_url_loader,
        TestExternallyManagedAppInstallTaskManager& test_install_task_manager,
        ExternalInstallOptions install_options)
        : ExternallyManagedAppInstallTask(
              profile,
              test_url_loader,
              externally_managed_app_manager_impl->registrar(),
              externally_managed_app_manager_impl->os_integration_manager(),
              externally_managed_app_manager_impl->ui_manager(),
              externally_managed_app_manager_impl->finalizer(),
              externally_managed_app_manager_impl->install_manager(),
              install_options),
          externally_managed_app_manager_impl_(
              externally_managed_app_manager_impl),
          externally_installed_app_prefs_(profile->GetPrefs()),
          test_install_task_manager_(test_install_task_manager) {}

    TestExternallyManagedAppInstallTask(
        const TestExternallyManagedAppInstallTask&) = delete;
    TestExternallyManagedAppInstallTask& operator=(
        const TestExternallyManagedAppInstallTask&) = delete;
    ~TestExternallyManagedAppInstallTask() override = default;

    void DoInstall(const GURL& install_url, ResultCallback callback) {
      auto result =
          externally_managed_app_manager_impl_->GetNextInstallationTaskResult(
              install_url);
      absl::optional<AppId> app_id;
      if (result.code == InstallResultCode::kSuccessNewInstall) {
        app_id = GenerateFakeAppId(install_url);
        GURL launch_url =
            externally_managed_app_manager_impl_->GetNextInstallationLaunchURL(
                install_url);
        const auto install_source = install_options().install_source;
        if (!registrar().IsInstalled(*app_id)) {
          auto web_app = test::CreateWebApp(install_url, Source::kPolicy);
          controller().RegisterApp(std::move(web_app));
          externally_installed_app_prefs_.Insert(install_url, *app_id,
                                                 install_source);
          externally_installed_app_prefs_.SetIsPlaceholder(
              install_url, result.did_install_placeholder);
        }
      }
      std::move(callback).Run(
          ExternallyManagedAppManager::InstallResult(result.code, app_id));
    }

    void Install(content::WebContents* web_contents,
                 ResultCallback callback) override {
      externally_managed_app_manager_impl_->OnInstallCalled(install_options());

      const GURL install_url =
          install_options().only_use_app_info_factory
              ? install_options().app_info_factory.Run()->start_url
              : install_options().install_url;
      test_install_task_manager_.RunOrSaveRequest(base::BindLambdaForTesting(
          [&, install_url, callback = std::move(callback)]() mutable {
            DoInstall(install_url, std::move(callback));
          }));
    }

   protected:
    WebAppRegistrar& registrar() { return controller().registrar(); }

    FakeWebAppRegistryController& controller() {
      return externally_managed_app_manager_impl_->controller();
    }

   private:
    raw_ptr<FakeExternallyManagedAppManager>
        externally_managed_app_manager_impl_;
    ExternallyInstalledWebAppPrefs externally_installed_app_prefs_;
    TestExternallyManagedAppInstallTaskManager& test_install_task_manager_;
  };

  class TestExternallyManagedAppRegistrationTask
      : public ExternallyManagedAppRegistrationTaskBase {
   public:
    TestExternallyManagedAppRegistrationTask(
        const GURL& install_url,
        FakeExternallyManagedAppManager* externally_managed_app_manager_impl)
        : ExternallyManagedAppRegistrationTaskBase(install_url),
          externally_managed_app_manager_impl_(
              externally_managed_app_manager_impl) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(&TestExternallyManagedAppRegistrationTask::OnProgress,
                         weak_ptr_factory_.GetWeakPtr(), install_url));
    }
    TestExternallyManagedAppRegistrationTask(
        const TestExternallyManagedAppRegistrationTask&) = delete;
    TestExternallyManagedAppRegistrationTask& operator=(
        const TestExternallyManagedAppRegistrationTask&) = delete;
    ~TestExternallyManagedAppRegistrationTask() override = default;

   private:
    void OnProgress(GURL install_url) {
      if (externally_managed_app_manager_impl_->MaybePreemptRegistration())
        return;
      externally_managed_app_manager_impl_->OnRegistrationFinished(
          install_url, RegistrationResultCode::kSuccess);
    }

    const raw_ptr<FakeExternallyManagedAppManager>
        externally_managed_app_manager_impl_;

    base::WeakPtrFactory<TestExternallyManagedAppRegistrationTask>
        weak_ptr_factory_{this};
  };

  FakeWebAppRegistryController& fake_registry_controller_;
  TestWebAppUrlLoader test_url_loader_;
  TestExternallyManagedAppInstallTaskManager& test_install_task_manager_;

  std::vector<ExternalInstallOptions> install_options_list_;
  GURL last_registered_install_url_;
  size_t install_run_count_ = 0;
  size_t registration_run_count_ = 0;

  std::map<GURL, TestTaskResult> next_installation_task_results_;
  std::map<GURL, GURL> next_installation_launch_urls_;
  absl::optional<base::OnceClosure> preempt_registration_callback_;
  base::OneShotEvent web_contents_released_event_;
};

}  // namespace

class ExternallyManagedAppManagerImplTest : public WebAppTest {
 public:
  ExternallyManagedAppManagerImplTest() = default;
  ExternallyManagedAppManagerImplTest(
      const ExternallyManagedAppManagerImplTest&) = delete;
  ExternallyManagedAppManagerImplTest& operator=(
      const ExternallyManagedAppManagerImplTest&) = delete;

  ~ExternallyManagedAppManagerImplTest() override = default;

  void SetUp() override {
    WebAppTest::SetUp();

    fake_registry_controller_ =
        std::make_unique<FakeWebAppRegistryController>();
    fake_registry_controller_->SetUp(profile());

    externally_managed_app_manager_impl_ =
        std::make_unique<FakeExternallyManagedAppManager>(
            profile(), controller(), test_install_task_manager_);

    install_finalizer_ = std::make_unique<FakeInstallFinalizer>();

    ui_manager_ = std::make_unique<FakeWebAppUiManager>();

    externally_managed_app_manager_impl().SetSubsystems(
        &registrar(), &controller().os_integration_manager(), &ui_manager(),
        &install_finalizer(), nullptr);

    controller().Init();
  }

 protected:
  std::pair<GURL, InstallResultCode> InstallAndWait(
      ExternallyManagedAppManager* externally_managed_app_manager,
      ExternalInstallOptions install_options) {
    base::RunLoop run_loop;

    absl::optional<GURL> url;
    absl::optional<InstallResultCode> code;

    externally_managed_app_manager_impl().InstallNow(
        std::move(install_options),
        base::BindLambdaForTesting(
            [&](const GURL& u,
                ExternallyManagedAppManager::InstallResult result) {
              url = u;
              code = result.code;
              run_loop.Quit();
            }));
    run_loop.Run();

    return {url.value(), code.value()};
  }

  InstallAppsResults InstallAppsAndWait(
      ExternallyManagedAppManager* externally_managed_app_manager,
      std::vector<ExternalInstallOptions> apps_to_install) {
    InstallAppsResults results;

    base::RunLoop run_loop;
    auto barrier_closure =
        base::BarrierClosure(apps_to_install.size(), run_loop.QuitClosure());
    externally_managed_app_manager_impl().InstallApps(
        std::move(apps_to_install),
        base::BindLambdaForTesting(
            [&](const GURL& url,
                ExternallyManagedAppManager::InstallResult result) {
              results.emplace_back(url, result.code);
              barrier_closure.Run();
            }));
    run_loop.Run();

    return results;
  }

  std::vector<std::pair<GURL, bool>> UninstallAppsAndWait(
      ExternallyManagedAppManager* externally_managed_app_manager,
      ExternalInstallSource install_source,
      std::vector<GURL> apps_to_uninstall) {
    std::vector<std::pair<GURL, bool>> results;

    base::RunLoop run_loop;
    auto barrier_closure =
        base::BarrierClosure(apps_to_uninstall.size(), run_loop.QuitClosure());
    externally_managed_app_manager->UninstallApps(
        std::move(apps_to_uninstall), install_source,
        base::BindLambdaForTesting(
            [&](const GURL& url, bool successfully_uninstalled) {
              results.emplace_back(url, successfully_uninstalled);
              barrier_closure.Run();
            }));
    run_loop.Run();

    return results;
  }

  // ExternalInstallOptions that was used to run the last installation task.
  const ExternalInstallOptions& last_install_options() {
    DCHECK(
        !externally_managed_app_manager_impl_->install_options_list().empty());
    return externally_managed_app_manager_impl_->install_options_list().back();
  }

  // Number of times ExternallyManagedAppInstallTask::Install was called.
  // Reflects how many times we've tried to create a web app.
  size_t install_run_count() {
    return externally_managed_app_manager_impl_->install_run_count();
  }

  // Number of times ExternallyManagedAppManagerImpl::StartRegistration was
  // called. Reflects how many times we've tried to cache service worker
  // resources for a web app.
  size_t registration_run_count() {
    return externally_managed_app_manager_impl_->registration_run_count();
  }

  size_t uninstall_call_count() {
    return install_finalizer_->uninstall_external_web_app_urls().size();
  }

  const std::vector<GURL>& uninstalled_app_urls() {
    return install_finalizer_->uninstall_external_web_app_urls();
  }

  const GURL& last_registered_install_url() {
    return externally_managed_app_manager_impl_->last_registered_install_url();
  }

  const GURL& last_uninstalled_app_url() {
    return install_finalizer_->uninstall_external_web_app_urls().back();
  }

  FakeExternallyManagedAppManager& externally_managed_app_manager_impl() {
    return *externally_managed_app_manager_impl_;
  }

  WebAppRegistrar& registrar() { return controller().registrar(); }

  FakeWebAppRegistryController& controller() {
    return *fake_registry_controller_;
  }

  FakeWebAppUiManager& ui_manager() { return *ui_manager_; }

  TestExternallyManagedAppInstallTaskManager& install_task_manager() {
    return test_install_task_manager_;
  }

  FakeInstallFinalizer& install_finalizer() { return *install_finalizer_; }

 private:
  std::unique_ptr<FakeWebAppRegistryController> fake_registry_controller_;
  std::unique_ptr<FakeExternallyManagedAppManager>
      externally_managed_app_manager_impl_;
  std::unique_ptr<FakeInstallFinalizer> install_finalizer_;
  std::unique_ptr<FakeWebAppUiManager> ui_manager_;

  TestExternallyManagedAppInstallTaskManager test_install_task_manager_;
};

TEST_F(ExternallyManagedAppManagerImplTest, Install_Succeeds) {
  const GURL kFooWebAppUrl("https://foo.example");
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kFooWebAppUrl, InstallResultCode::kSuccessNewInstall);
  externally_managed_app_manager_impl().SetNextInstallationLaunchURL(
      kFooWebAppUrl);

  absl::optional<GURL> url;
  absl::optional<InstallResultCode> code;
  std::tie(url, code) = InstallAndWait(&externally_managed_app_manager_impl(),
                                       GetInstallOptions(kFooWebAppUrl));

  EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code.value());
  EXPECT_EQ(kFooWebAppUrl, url.value());

  EXPECT_EQ(1u, install_run_count());
  EXPECT_EQ(GetInstallOptions(kFooWebAppUrl), last_install_options());

  WebAppRegistrationWaiter(&externally_managed_app_manager_impl())
      .AwaitNextRegistration(kFooWebAppUrl, RegistrationResultCode::kSuccess);
  EXPECT_EQ(1U, registration_run_count());
  EXPECT_EQ(kFooWebAppUrl, last_registered_install_url());
}

TEST_F(ExternallyManagedAppManagerImplTest, Install_SerialCallsDifferentApps) {
  // Load about:blanks twice in total, once for each install.
  const GURL kFooWebAppUrl("https://foo.example");
  const GURL kBarWebAppUrl("https://bar.example");
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kFooWebAppUrl, InstallResultCode::kSuccessNewInstall);
  externally_managed_app_manager_impl().SetNextInstallationLaunchURL(
      kFooWebAppUrl);
  {
    absl::optional<GURL> url;
    absl::optional<InstallResultCode> code;
    std::tie(url, code) = InstallAndWait(&externally_managed_app_manager_impl(),
                                         GetInstallOptions(kFooWebAppUrl));

    EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code.value());
    EXPECT_EQ(kFooWebAppUrl, url.value());

    EXPECT_EQ(1u, install_run_count());
    EXPECT_EQ(GetInstallOptions(kFooWebAppUrl), last_install_options());
  }

  externally_managed_app_manager_impl().WaitForRegistrationAndCancel();
  // Foo launch URL registration will be attempted again after
  // kBarWebAppUrl installs.

  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kBarWebAppUrl, InstallResultCode::kSuccessNewInstall);
  externally_managed_app_manager_impl().SetNextInstallationLaunchURL(
      kBarWebAppUrl);
  {
    absl::optional<GURL> url;
    absl::optional<InstallResultCode> code;

    std::tie(url, code) = InstallAndWait(&externally_managed_app_manager_impl(),
                                         GetInstallOptions(kBarWebAppUrl));

    EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code.value());
    EXPECT_EQ(kBarWebAppUrl, url.value());

    EXPECT_EQ(2u, install_run_count());
    EXPECT_EQ(GetInstallOptions(kBarWebAppUrl), last_install_options());
  }

  WebAppRegistrationWaiter(&externally_managed_app_manager_impl())
      .AwaitNextRegistration(kFooWebAppUrl, RegistrationResultCode::kSuccess);
  WebAppRegistrationWaiter(&externally_managed_app_manager_impl())
      .AwaitNextRegistration(kBarWebAppUrl, RegistrationResultCode::kSuccess);
  EXPECT_EQ(3U, registration_run_count());
  EXPECT_EQ(kBarWebAppUrl, last_registered_install_url());
}

TEST_F(ExternallyManagedAppManagerImplTest,
       Install_ConcurrentCallsDifferentApps) {
  const GURL kFooWebAppUrl("https://foo.example");
  const GURL kBarWebAppUrl("https://bar.example");
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kBarWebAppUrl, InstallResultCode::kSuccessNewInstall);
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kFooWebAppUrl, InstallResultCode::kSuccessNewInstall);

  base::RunLoop run_loop;
  externally_managed_app_manager_impl().InstallNow(
      GetInstallOptions(kFooWebAppUrl),
      base::BindLambdaForTesting(
          [&](const GURL& url,
              ExternallyManagedAppManager::InstallResult result) {
            EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
            EXPECT_EQ(kFooWebAppUrl, url);

            // Two installations tasks should have run at this point,
            // one from the last call to install (which gets higher priority),
            // and another one for this call to install.
            EXPECT_EQ(2u, install_run_count());
            EXPECT_EQ(GetInstallOptions(kFooWebAppUrl), last_install_options());

            run_loop.Quit();
          }));
  externally_managed_app_manager_impl().InstallNow(
      GetInstallOptions(kBarWebAppUrl),
      base::BindLambdaForTesting(
          [&](const GURL& url,
              ExternallyManagedAppManager::InstallResult result) {
            EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
            EXPECT_EQ(kBarWebAppUrl, url);

            // The last call gets higher priority so only one
            // installation task should have run at this point.
            EXPECT_EQ(1u, install_run_count());
            EXPECT_EQ(GetInstallOptions(kBarWebAppUrl), last_install_options());
          }));
  run_loop.Run();
}

TEST_F(ExternallyManagedAppManagerImplTest, Install_PendingSuccessfulTask) {
  const GURL kFooWebAppUrl("https://foo.example");
  const GURL kBarWebAppUrl("https://bar.example");
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kFooWebAppUrl, InstallResultCode::kSuccessNewInstall);
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kBarWebAppUrl, InstallResultCode::kSuccessNewInstall);
  install_task_manager().SaveInstallRequests();

  base::RunLoop foo_run_loop;
  base::RunLoop bar_run_loop;

  externally_managed_app_manager_impl().InstallNow(
      GetInstallOptions(kFooWebAppUrl),
      base::BindLambdaForTesting(
          [&](const GURL& url,
              ExternallyManagedAppManager::InstallResult result) {
            EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
            EXPECT_EQ(kFooWebAppUrl, url);

            EXPECT_EQ(1u, install_run_count());
            EXPECT_EQ(GetInstallOptions(kFooWebAppUrl), last_install_options());

            foo_run_loop.Quit();
          }));

  // Make sure the installation has started and that it hasn't finished yet.
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(install_task_manager().num_pending_tasks(), 1u);

  externally_managed_app_manager_impl().InstallNow(
      GetInstallOptions(kBarWebAppUrl),
      base::BindLambdaForTesting(
          [&](const GURL& url,
              ExternallyManagedAppManager::InstallResult result) {
            EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
            EXPECT_EQ(kBarWebAppUrl, url);

            EXPECT_EQ(2u, install_run_count());
            EXPECT_EQ(GetInstallOptions(kBarWebAppUrl), last_install_options());

            bar_run_loop.Quit();
          }));

  install_task_manager().ProcessSavedRequests();
  foo_run_loop.Run();

  // Make sure the second installation has started.
  base::RunLoop().RunUntilIdle();

  install_task_manager().ProcessSavedRequests();
  bar_run_loop.Run();
}

TEST_F(ExternallyManagedAppManagerImplTest, InstallWithWebAppInfo_Succeeds) {
  const GURL kFooWebAppUrl("https://foo.example");
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kFooWebAppUrl, InstallResultCode::kSuccessNewInstall);

  base::RunLoop foo_run_loop;

  externally_managed_app_manager_impl().InstallNow(
      GetInstallOptionsWithWebAppInfo(kFooWebAppUrl),
      base::BindLambdaForTesting(
          [&](const GURL& url,
              ExternallyManagedAppManager::InstallResult result) {
            EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
            EXPECT_EQ(kFooWebAppUrl, url);

            EXPECT_EQ(1u, install_run_count());
            EXPECT_EQ(GetInstallOptionsWithWebAppInfo(kFooWebAppUrl),
                      last_install_options());

            foo_run_loop.Quit();
          }));
  // Make sure the installation has started.
  foo_run_loop.Run();
}

TEST_F(ExternallyManagedAppManagerImplTest,
       InstallAppsWithWebAppInfoAndUrl_Multiple) {
  const GURL kFooWebAppUrl("https://foo.example");
  const GURL kBarWebAppUrl("https://bar.example");
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kFooWebAppUrl, InstallResultCode::kSuccessNewInstall);
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kBarWebAppUrl, InstallResultCode::kSuccessNewInstall);

  std::vector<ExternalInstallOptions> apps_to_install;
  apps_to_install.push_back(GetInstallOptionsWithWebAppInfo(kFooWebAppUrl));
  apps_to_install.push_back(GetInstallOptions(kBarWebAppUrl));

  InstallAppsResults results = InstallAppsAndWait(
      &externally_managed_app_manager_impl(), std::move(apps_to_install));

  EXPECT_EQ(results,
            InstallAppsResults(
                {{kFooWebAppUrl, InstallResultCode::kSuccessNewInstall},
                 {kBarWebAppUrl, InstallResultCode::kSuccessNewInstall}}));

  EXPECT_EQ(2u, install_run_count());
  EXPECT_EQ(GetInstallOptions(kBarWebAppUrl), last_install_options());
}

TEST_F(ExternallyManagedAppManagerImplTest,
       InstallWithWebAppInfo_Succeeds_Twice) {
  const GURL kFooWebAppUrl("https://foo.example");
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kFooWebAppUrl, InstallResultCode::kSuccessNewInstall);

  base::RunLoop foo_run_loop;
  base::RunLoop bar_run_loop;

  externally_managed_app_manager_impl().Install(
      GetInstallOptionsWithWebAppInfo(kFooWebAppUrl),
      base::BindLambdaForTesting(
          [&](const GURL& url,
              ExternallyManagedAppManager::InstallResult result) {
            EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
            EXPECT_EQ(kFooWebAppUrl, url);

            EXPECT_EQ(1u, install_run_count());
            EXPECT_EQ(GetInstallOptionsWithWebAppInfo(kFooWebAppUrl),
                      last_install_options());
            foo_run_loop.Quit();
          }));

  base::RunLoop().RunUntilIdle();

  externally_managed_app_manager_impl().Install(
      GetInstallOptionsWithWebAppInfo(kFooWebAppUrl),
      base::BindLambdaForTesting(
          [&](const GURL& url,
              ExternallyManagedAppManager::InstallResult result) {
            EXPECT_EQ(InstallResultCode::kSuccessAlreadyInstalled, result.code);
            EXPECT_EQ(kFooWebAppUrl, url);

            EXPECT_EQ(1u, install_run_count());
            EXPECT_EQ(GetInstallOptionsWithWebAppInfo(kFooWebAppUrl),
                      last_install_options());

            bar_run_loop.Quit();
          }));
  foo_run_loop.Run();
  base::RunLoop().RunUntilIdle();
  bar_run_loop.Run();
}

TEST_F(ExternallyManagedAppManagerImplTest, Install_PendingFailingTask) {
  const GURL kFooWebAppUrl("https://foo.example");
  const GURL kBarWebAppUrl("https://bar.example");
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kFooWebAppUrl, InstallResultCode::kWebAppDisabled);
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kBarWebAppUrl, InstallResultCode::kSuccessNewInstall);
  install_task_manager().SaveInstallRequests();

  base::RunLoop foo_run_loop;
  base::RunLoop bar_run_loop;

  externally_managed_app_manager_impl().InstallNow(
      GetInstallOptions(kFooWebAppUrl),
      base::BindLambdaForTesting(
          [&](const GURL& url,
              ExternallyManagedAppManager::InstallResult result) {
            EXPECT_EQ(InstallResultCode::kWebAppDisabled, result.code);
            EXPECT_EQ(kFooWebAppUrl, url);

            EXPECT_EQ(1u, install_run_count());

            foo_run_loop.Quit();
          }));
  // Make sure the installation has started and that it hasn't finished yet.
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(install_task_manager().num_pending_tasks(), 1u);

  externally_managed_app_manager_impl().InstallNow(
      GetInstallOptions(kBarWebAppUrl),
      base::BindLambdaForTesting(
          [&](const GURL& url,
              ExternallyManagedAppManager::InstallResult result) {
            EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
            EXPECT_EQ(kBarWebAppUrl, url);

            EXPECT_EQ(2u, install_run_count());
            EXPECT_EQ(GetInstallOptions(kBarWebAppUrl), last_install_options());

            bar_run_loop.Quit();
          }));

  install_task_manager().ProcessSavedRequests();
  foo_run_loop.Run();

  // Make sure the second installation has started.
  base::RunLoop().RunUntilIdle();

  install_task_manager().ProcessSavedRequests();
  bar_run_loop.Run();
}

TEST_F(ExternallyManagedAppManagerImplTest, Install_ReentrantCallback) {
  const GURL kFooWebAppUrl("https://foo.example");
  const GURL kBarWebAppUrl("https://bar.example");
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kFooWebAppUrl, InstallResultCode::kSuccessNewInstall);
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kBarWebAppUrl, InstallResultCode::kSuccessNewInstall);

  base::RunLoop run_loop;
  auto final_callback = base::BindLambdaForTesting(
      [&](const GURL& url, ExternallyManagedAppManager::InstallResult result) {
        EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
        EXPECT_EQ(kBarWebAppUrl, url);

        EXPECT_EQ(2u, install_run_count());
        EXPECT_EQ(GetInstallOptions(kBarWebAppUrl), last_install_options());
        run_loop.Quit();
      });
  auto reentrant_callback = base::BindLambdaForTesting(
      [&](const GURL& url, ExternallyManagedAppManager::InstallResult result) {
        EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
        EXPECT_EQ(kFooWebAppUrl, url);

        EXPECT_EQ(1u, install_run_count());
        EXPECT_EQ(GetInstallOptions(kFooWebAppUrl), last_install_options());

        externally_managed_app_manager_impl().InstallNow(
            GetInstallOptions(kBarWebAppUrl), final_callback);
      });

  // Call Install() with a callback that tries to install another app.
  externally_managed_app_manager_impl().InstallNow(
      GetInstallOptions(kFooWebAppUrl), reentrant_callback);
  run_loop.Run();
}

TEST_F(ExternallyManagedAppManagerImplTest, Install_SerialCallsSameApp) {
  const GURL kFooWebAppUrl("https://foo.example");
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kFooWebAppUrl, InstallResultCode::kSuccessNewInstall);
  {
    absl::optional<GURL> url;
    absl::optional<InstallResultCode> code;
    std::tie(url, code) = InstallAndWait(&externally_managed_app_manager_impl(),
                                         GetInstallOptions(kFooWebAppUrl));

    EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code);
    EXPECT_EQ(kFooWebAppUrl, url);

    EXPECT_EQ(1u, install_run_count());
    EXPECT_EQ(GetInstallOptions(kFooWebAppUrl), last_install_options());
  }

  {
    absl::optional<GURL> url;
    absl::optional<InstallResultCode> code;
    std::tie(url, code) = InstallAndWait(&externally_managed_app_manager_impl(),
                                         GetInstallOptions(kFooWebAppUrl));

    EXPECT_EQ(InstallResultCode::kSuccessAlreadyInstalled, code);
    EXPECT_EQ(kFooWebAppUrl, url);

    // The app is already installed so we shouldn't try to install it again.
    EXPECT_EQ(1u, install_run_count());
  }
}

TEST_F(ExternallyManagedAppManagerImplTest, Install_ConcurrentCallsSameApp) {
  const GURL kFooWebAppUrl("https://foo.example");
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kFooWebAppUrl, InstallResultCode::kSuccessNewInstall);

  base::RunLoop run_loop;
  bool first_callback_ran = false;

  externally_managed_app_manager_impl().InstallNow(
      GetInstallOptions(kFooWebAppUrl),
      base::BindLambdaForTesting(
          [&](const GURL& url,
              ExternallyManagedAppManager::InstallResult result) {
            // kSuccessAlreadyInstalled because the last call to Install gets
            // higher priority.
            EXPECT_EQ(InstallResultCode::kSuccessAlreadyInstalled, result.code);
            EXPECT_EQ(kFooWebAppUrl, url);

            // Only one installation task should run because the app was already
            // installed.
            EXPECT_EQ(1u, install_run_count());

            EXPECT_TRUE(first_callback_ran);

            run_loop.Quit();
          }));

  externally_managed_app_manager_impl().InstallNow(
      GetInstallOptions(kFooWebAppUrl),
      base::BindLambdaForTesting(
          [&](const GURL& url,
              ExternallyManagedAppManager::InstallResult result) {
            EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
            EXPECT_EQ(kFooWebAppUrl, url);

            EXPECT_EQ(1u, install_run_count());
            EXPECT_EQ(GetInstallOptions(kFooWebAppUrl), last_install_options());
            first_callback_ran = true;
          }));
  run_loop.Run();

  EXPECT_EQ(1u, install_run_count());
  EXPECT_EQ(GetInstallOptions(kFooWebAppUrl), last_install_options());
}

TEST_F(ExternallyManagedAppManagerImplTest, Install_AlwaysUpdate) {
  const GURL kFooWebAppUrl("https://foo.example");
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kFooWebAppUrl, InstallResultCode::kSuccessNewInstall);

  auto get_force_reinstall_info = [kFooWebAppUrl]() {
    ExternalInstallOptions options(kFooWebAppUrl, DisplayMode::kStandalone,
                                   ExternalInstallSource::kExternalPolicy);
    options.force_reinstall = true;
    return options;
  };

  {
    absl::optional<GURL> url;
    absl::optional<InstallResultCode> code;
    std::tie(url, code) = InstallAndWait(&externally_managed_app_manager_impl(),
                                         get_force_reinstall_info());

    EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code);
    EXPECT_EQ(kFooWebAppUrl, url);

    EXPECT_EQ(1u, install_run_count());
    EXPECT_EQ(get_force_reinstall_info(), last_install_options());
  }

  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kFooWebAppUrl, InstallResultCode::kSuccessNewInstall);
  {
    absl::optional<GURL> url;
    absl::optional<InstallResultCode> code;
    std::tie(url, code) = InstallAndWait(&externally_managed_app_manager_impl(),
                                         get_force_reinstall_info());

    EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code);
    EXPECT_EQ(kFooWebAppUrl, url);

    // The app should be installed again because of the |force_reinstall| flag.
    EXPECT_EQ(2u, install_run_count());
    EXPECT_EQ(get_force_reinstall_info(), last_install_options());
  }
}

TEST_F(ExternallyManagedAppManagerImplTest, Install_InstallationFails) {
  const GURL kFooWebAppUrl("https://foo.example");
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kFooWebAppUrl, InstallResultCode::kWebAppDisabled);

  absl::optional<GURL> url;
  absl::optional<InstallResultCode> code;
  std::tie(url, code) = InstallAndWait(&externally_managed_app_manager_impl(),
                                       GetInstallOptions(kFooWebAppUrl));

  EXPECT_EQ(InstallResultCode::kWebAppDisabled, code);
  EXPECT_EQ(kFooWebAppUrl, url);

  EXPECT_EQ(1u, install_run_count());
}

TEST_F(ExternallyManagedAppManagerImplTest, Install_PlaceholderApp) {
  const GURL kFooWebAppUrl("https://foo.example");
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kFooWebAppUrl, InstallResultCode::kSuccessNewInstall,
      /*did_install_placeholder=*/true);

  auto install_options = GetInstallOptions(kFooWebAppUrl);
  install_options.install_placeholder = true;

  absl::optional<GURL> url;
  absl::optional<InstallResultCode> code;
  std::tie(url, code) =
      InstallAndWait(&externally_managed_app_manager_impl(), install_options);

  EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code);
  EXPECT_EQ(kFooWebAppUrl, url);

  EXPECT_EQ(1u, install_run_count());
  EXPECT_EQ(install_options, last_install_options());
}

TEST_F(ExternallyManagedAppManagerImplTest, InstallApps_Succeeds) {
  const GURL kFooWebAppUrl("https://foo.example");
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kFooWebAppUrl, InstallResultCode::kSuccessNewInstall);

  std::vector<ExternalInstallOptions> apps_to_install;
  apps_to_install.push_back(GetInstallOptions(kFooWebAppUrl));

  InstallAppsResults results = InstallAppsAndWait(
      &externally_managed_app_manager_impl(), std::move(apps_to_install));

  EXPECT_EQ(results,
            InstallAppsResults(
                {{kFooWebAppUrl, InstallResultCode::kSuccessNewInstall}}));

  EXPECT_EQ(1u, install_run_count());
  EXPECT_EQ(GetInstallOptions(kFooWebAppUrl), last_install_options());
}

TEST_F(ExternallyManagedAppManagerImplTest,
       InstallApps_FailsInstallationFails) {
  const GURL kFooWebAppUrl("https://foo.example");
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kFooWebAppUrl, InstallResultCode::kWebAppDisabled);

  std::vector<ExternalInstallOptions> apps_to_install;
  apps_to_install.push_back(GetInstallOptions(kFooWebAppUrl));

  InstallAppsResults results = InstallAppsAndWait(
      &externally_managed_app_manager_impl(), std::move(apps_to_install));

  EXPECT_EQ(results,
            InstallAppsResults(
                {{kFooWebAppUrl, InstallResultCode::kWebAppDisabled}}));

  EXPECT_EQ(1u, install_run_count());
}

TEST_F(ExternallyManagedAppManagerImplTest, InstallApps_PlaceholderApp) {
  const GURL kFooWebAppUrl("https://foo.example");
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kFooWebAppUrl, InstallResultCode::kSuccessNewInstall,
      /*did_install_placeholder=*/true);

  auto install_options = GetInstallOptions(kFooWebAppUrl);
  install_options.install_placeholder = true;
  std::vector<ExternalInstallOptions> apps_to_install;
  apps_to_install.push_back(install_options);

  InstallAppsResults results = InstallAppsAndWait(
      &externally_managed_app_manager_impl(), std::move(apps_to_install));

  EXPECT_EQ(results,
            InstallAppsResults(
                {{kFooWebAppUrl, InstallResultCode::kSuccessNewInstall}}));

  EXPECT_EQ(1u, install_run_count());
  EXPECT_EQ(install_options, last_install_options());
}

TEST_F(ExternallyManagedAppManagerImplTest, InstallApps_Multiple) {
  const GURL kFooWebAppUrl("https://foo.example");
  const GURL kBarWebAppUrl("https://bar.example");
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kFooWebAppUrl, InstallResultCode::kSuccessNewInstall);
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kBarWebAppUrl, InstallResultCode::kSuccessNewInstall);

  std::vector<ExternalInstallOptions> apps_to_install;
  apps_to_install.push_back(GetInstallOptions(kFooWebAppUrl));
  apps_to_install.push_back(GetInstallOptions(kBarWebAppUrl));

  InstallAppsResults results = InstallAppsAndWait(
      &externally_managed_app_manager_impl(), std::move(apps_to_install));

  EXPECT_EQ(results,
            InstallAppsResults(
                {{kFooWebAppUrl, InstallResultCode::kSuccessNewInstall},
                 {kBarWebAppUrl, InstallResultCode::kSuccessNewInstall}}));

  EXPECT_EQ(2u, install_run_count());
  EXPECT_EQ(GetInstallOptions(kBarWebAppUrl), last_install_options());
}

TEST_F(ExternallyManagedAppManagerImplTest, InstallApps_PendingInstallApps) {
  const GURL kFooWebAppUrl("https://foo.example");
  const GURL kBarWebAppUrl("https://bar.example");
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kFooWebAppUrl, InstallResultCode::kSuccessNewInstall);
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kBarWebAppUrl, InstallResultCode::kSuccessNewInstall);

  base::RunLoop run_loop;
  {
    std::vector<ExternalInstallOptions> apps_to_install;
    apps_to_install.push_back(GetInstallOptions(kFooWebAppUrl));

    externally_managed_app_manager_impl().InstallApps(
        std::move(apps_to_install),
        base::BindLambdaForTesting(
            [&](const GURL& url,
                ExternallyManagedAppManager::InstallResult result) {
              EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
              EXPECT_EQ(kFooWebAppUrl, url);

              EXPECT_EQ(1u, install_run_count());
              EXPECT_EQ(GetInstallOptions(kFooWebAppUrl),
                        last_install_options());
            }));
  }

  {
    std::vector<ExternalInstallOptions> apps_to_install;
    apps_to_install.push_back(GetInstallOptions(kBarWebAppUrl));

    externally_managed_app_manager_impl().InstallApps(
        std::move(apps_to_install),
        base::BindLambdaForTesting(
            [&](const GURL& url,
                ExternallyManagedAppManager::InstallResult result) {
              EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
              EXPECT_EQ(kBarWebAppUrl, url);

              EXPECT_EQ(2u, install_run_count());
              EXPECT_EQ(GetInstallOptions(kBarWebAppUrl),
                        last_install_options());

              run_loop.Quit();
            }));
  }
  run_loop.Run();
}

TEST_F(ExternallyManagedAppManagerImplTest,
       Install_PendingMultipleInstallApps) {
  const GURL kFooWebAppUrl("https://foo.example");
  const GURL kBarWebAppUrl("https://bar.example");
  const GURL kQuxWebAppUrl("https://qux.example");
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kFooWebAppUrl, InstallResultCode::kSuccessNewInstall);
  externally_managed_app_manager_impl().SetNextInstallationLaunchURL(
      kFooWebAppUrl);
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kBarWebAppUrl, InstallResultCode::kSuccessNewInstall);
  externally_managed_app_manager_impl().SetNextInstallationLaunchURL(
      kBarWebAppUrl);
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kQuxWebAppUrl, InstallResultCode::kSuccessNewInstall);
  externally_managed_app_manager_impl().SetNextInstallationLaunchURL(
      kQuxWebAppUrl);

  std::vector<ExternalInstallOptions> apps_to_install;
  apps_to_install.push_back(GetInstallOptions(kFooWebAppUrl));
  apps_to_install.push_back(GetInstallOptions(kBarWebAppUrl));

  // Queue through InstallApps.
  int callback_calls = 0;
  externally_managed_app_manager_impl().InstallApps(
      std::move(apps_to_install),
      base::BindLambdaForTesting(
          [&](const GURL& url,
              ExternallyManagedAppManager::InstallResult result) {
            ++callback_calls;
            if (callback_calls == 1) {
              EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
              EXPECT_EQ(kFooWebAppUrl, url);

              EXPECT_EQ(2u, install_run_count());
              EXPECT_EQ(GetInstallOptions(kFooWebAppUrl),
                        last_install_options());
            } else if (callback_calls == 2) {
              EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
              EXPECT_EQ(kBarWebAppUrl, url);

              EXPECT_EQ(3u, install_run_count());
              EXPECT_EQ(GetInstallOptions(kBarWebAppUrl),
                        last_install_options());
            } else {
              NOTREACHED();
            }
          }));

  // Queue through Install.
  externally_managed_app_manager_impl().InstallNow(
      GetInstallOptions(kQuxWebAppUrl),
      base::BindLambdaForTesting(
          [&](const GURL& url,
              ExternallyManagedAppManager::InstallResult result) {
            EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
            EXPECT_EQ(kQuxWebAppUrl, url);

            // The install request from Install should be processed first.
            EXPECT_EQ(1u, install_run_count());
            EXPECT_EQ(GetInstallOptions(kQuxWebAppUrl), last_install_options());
          }));

  WebAppRegistrationWaiter(&externally_managed_app_manager_impl())
      .AwaitNextRegistration(kQuxWebAppUrl, RegistrationResultCode::kSuccess);
  WebAppRegistrationWaiter(&externally_managed_app_manager_impl())
      .AwaitNextRegistration(kFooWebAppUrl, RegistrationResultCode::kSuccess);
  WebAppRegistrationWaiter(&externally_managed_app_manager_impl())
      .AwaitNextRegistration(kBarWebAppUrl, RegistrationResultCode::kSuccess);
  EXPECT_EQ(3U, registration_run_count());
  EXPECT_EQ(kBarWebAppUrl, last_registered_install_url());
}

TEST_F(ExternallyManagedAppManagerImplTest, InstallApps_PendingInstall) {
  const GURL kFooWebAppUrl("https://foo.example");
  const GURL kBarWebAppUrl("https://bar.example");
  const GURL kQuxWebAppUrl("https://qux.example");
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kFooWebAppUrl, InstallResultCode::kSuccessNewInstall);
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kBarWebAppUrl, InstallResultCode::kSuccessNewInstall);
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kQuxWebAppUrl, InstallResultCode::kSuccessNewInstall);

  base::RunLoop run_loop;

  // Queue through Install.
  externally_managed_app_manager_impl().InstallNow(
      GetInstallOptions(kQuxWebAppUrl),
      base::BindLambdaForTesting(
          [&](const GURL& url,
              ExternallyManagedAppManager::InstallResult result) {
            EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
            EXPECT_EQ(kQuxWebAppUrl, url);

            // The install request from Install should be processed first.
            EXPECT_EQ(1u, install_run_count());
            EXPECT_EQ(GetInstallOptions(kQuxWebAppUrl), last_install_options());
          }));

  // Queue through InstallApps.
  std::vector<ExternalInstallOptions> apps_to_install;
  apps_to_install.push_back(GetInstallOptions(kFooWebAppUrl));
  apps_to_install.push_back(GetInstallOptions(kBarWebAppUrl));

  int callback_calls = 0;
  externally_managed_app_manager_impl().InstallApps(
      std::move(apps_to_install),
      base::BindLambdaForTesting(
          [&](const GURL& url,
              ExternallyManagedAppManager::InstallResult result) {
            ++callback_calls;
            if (callback_calls == 1) {
              EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
              EXPECT_EQ(kFooWebAppUrl, url);

              // The install requests from InstallApps should be processed next.
              EXPECT_EQ(2u, install_run_count());
              EXPECT_EQ(GetInstallOptions(kFooWebAppUrl),
                        last_install_options());

              return;
            }
            if (callback_calls == 2) {
              EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
              EXPECT_EQ(kBarWebAppUrl, url);

              EXPECT_EQ(3u, install_run_count());
              EXPECT_EQ(GetInstallOptions(kBarWebAppUrl),
                        last_install_options());

              run_loop.Quit();
              return;
            }
            NOTREACHED();
          }));
  run_loop.Run();
}

TEST_F(ExternallyManagedAppManagerImplTest, AppUninstalled) {
  const GURL kFooWebAppUrl("https://foo.example");
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kFooWebAppUrl, InstallResultCode::kSuccessNewInstall);
  {
    absl::optional<GURL> url;
    absl::optional<InstallResultCode> code;
    std::tie(url, code) = InstallAndWait(&externally_managed_app_manager_impl(),
                                         GetInstallOptions(kFooWebAppUrl));

    EXPECT_EQ(1u, install_run_count());
    EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code.value());
  }

  absl::optional<AppId> app_id = registrar().LookupExternalAppId(kFooWebAppUrl);
  controller().UnregisterApp(*app_id);

  // Try to install the app again.
  {
    externally_managed_app_manager_impl().SetNextInstallationTaskResult(
        kFooWebAppUrl, InstallResultCode::kSuccessNewInstall);

    absl::optional<GURL> url;
    absl::optional<InstallResultCode> code;
    std::tie(url, code) = InstallAndWait(&externally_managed_app_manager_impl(),
                                         GetInstallOptions(kFooWebAppUrl));

    // The app was uninstalled so a new installation task should run.
    EXPECT_EQ(2u, install_run_count());
    EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code.value());
  }
}

TEST_F(ExternallyManagedAppManagerImplTest, ExternalAppUninstalled) {
  const GURL kFooWebAppUrl("https://foo.example");
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kFooWebAppUrl, InstallResultCode::kSuccessNewInstall);
  {
    absl::optional<GURL> url;
    absl::optional<InstallResultCode> code;
    std::tie(url, code) = InstallAndWait(&externally_managed_app_manager_impl(),
                                         GetInstallOptions(kFooWebAppUrl));

    EXPECT_EQ(1u, install_run_count());
    EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code.value());
  }

  // Simulate external app for the app getting uninstalled by the user.
  const std::string app_id = GenerateFakeAppId(kFooWebAppUrl);
  install_finalizer().SimulateExternalAppUninstalledByUser(app_id);
  if (registrar().IsInstalled(app_id))
    controller().UnregisterApp(app_id);

  // The app was uninstalled by the user. Installing again should succeed
  // or fail depending on whether we set override_previous_user_uninstall. We
  // try with override_previous_user_uninstall false first, true second.
  {
    absl::optional<GURL> url;
    absl::optional<InstallResultCode> code;
    std::tie(url, code) = InstallAndWait(
        &externally_managed_app_manager_impl(),
        GetInstallOptions(kFooWebAppUrl,
                          false /* override_previous_user_uninstall */));

    // The app shouldn't be installed because the user previously uninstalled
    // it, so there shouldn't be any new installation task runs.
    EXPECT_EQ(1u, install_run_count());
    EXPECT_EQ(InstallResultCode::kPreviouslyUninstalled, code.value());
  }

  {
    externally_managed_app_manager_impl().SetNextInstallationTaskResult(
        kFooWebAppUrl, InstallResultCode::kSuccessNewInstall);

    absl::optional<GURL> url;
    absl::optional<InstallResultCode> code;
    std::tie(url, code) = InstallAndWait(
        &externally_managed_app_manager_impl(),
        GetInstallOptions(kFooWebAppUrl,
                          true /* override_previous_user_uninstall */));

    EXPECT_EQ(2u, install_run_count());
    EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code.value());
  }
}

TEST_F(ExternallyManagedAppManagerImplTest, UninstallApps_Succeeds) {
  const GURL kFooWebAppUrl("https://foo.example");
  auto web_app = test::CreateWebApp(kFooWebAppUrl, Source::kPolicy);
  controller().RegisterApp(std::move(web_app));

  install_finalizer().SetNextUninstallExternalWebAppResult(kFooWebAppUrl, true);
  UninstallAppsResults results = UninstallAppsAndWait(
      &externally_managed_app_manager_impl(),
      ExternalInstallSource::kExternalPolicy, std::vector<GURL>{kFooWebAppUrl});

  EXPECT_EQ(results, UninstallAppsResults({{kFooWebAppUrl, true}}));

  EXPECT_EQ(1u, uninstall_call_count());
  EXPECT_EQ(kFooWebAppUrl, last_uninstalled_app_url());
}

TEST_F(ExternallyManagedAppManagerImplTest, UninstallApps_Fails) {
  const GURL kFooWebAppUrl("https://foo.example");
  install_finalizer().SetNextUninstallExternalWebAppResult(kFooWebAppUrl,
                                                           false);
  UninstallAppsResults results = UninstallAppsAndWait(
      &externally_managed_app_manager_impl(),
      ExternalInstallSource::kExternalPolicy, std::vector<GURL>{kFooWebAppUrl});
  EXPECT_EQ(results, UninstallAppsResults({{kFooWebAppUrl, false}}));

  EXPECT_EQ(1u, uninstall_call_count());
  EXPECT_EQ(kFooWebAppUrl, last_uninstalled_app_url());
}

TEST_F(ExternallyManagedAppManagerImplTest, UninstallApps_Multiple) {
  const GURL kFooWebAppUrl("https://foo.example");
  const GURL kBarWebAppUrl("https://bar.example");
  auto web_app = test::CreateWebApp(kFooWebAppUrl, Source::kPolicy);
  controller().RegisterApp(std::move(web_app));
  web_app = test::CreateWebApp(kBarWebAppUrl, Source::kPolicy);
  controller().RegisterApp(std::move(web_app));

  install_finalizer().SetNextUninstallExternalWebAppResult(kFooWebAppUrl, true);
  install_finalizer().SetNextUninstallExternalWebAppResult(kBarWebAppUrl, true);
  UninstallAppsResults results =
      UninstallAppsAndWait(&externally_managed_app_manager_impl(),
                           ExternalInstallSource::kExternalPolicy,
                           std::vector<GURL>{kFooWebAppUrl, kBarWebAppUrl});
  EXPECT_EQ(results, UninstallAppsResults(
                         {{kFooWebAppUrl, true}, {kBarWebAppUrl, true}}));

  EXPECT_EQ(2u, uninstall_call_count());
  EXPECT_EQ(std::vector<GURL>({kFooWebAppUrl, kBarWebAppUrl}),
            uninstalled_app_urls());
}

TEST_F(ExternallyManagedAppManagerImplTest, UninstallApps_PendingInstall) {
  const GURL kFooWebAppUrl("https://foo.example");
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kFooWebAppUrl, InstallResultCode::kSuccessNewInstall);

  base::RunLoop run_loop;
  externally_managed_app_manager_impl().InstallNow(
      GetInstallOptions(kFooWebAppUrl),
      base::BindLambdaForTesting(
          [&](const GURL& url,
              ExternallyManagedAppManager::InstallResult result) {
            EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
            EXPECT_EQ(kFooWebAppUrl, url);
            run_loop.Quit();
          }));

  install_finalizer().SetNextUninstallExternalWebAppResult(kFooWebAppUrl,
                                                           false);
  UninstallAppsResults uninstall_results = UninstallAppsAndWait(
      &externally_managed_app_manager_impl(),
      ExternalInstallSource::kExternalPolicy, std::vector<GURL>{kFooWebAppUrl});
  EXPECT_EQ(uninstall_results, UninstallAppsResults({{kFooWebAppUrl, false}}));
  EXPECT_EQ(1u, uninstall_call_count());

  run_loop.Run();
}

TEST_F(ExternallyManagedAppManagerImplTest, ReinstallPlaceholderApp_Success) {
  // Install a placeholder app
  const GURL kFooWebAppUrl("https://foo.example");
  auto install_options = GetInstallOptions(kFooWebAppUrl);
  install_options.install_placeholder = true;

  {
    externally_managed_app_manager_impl().SetNextInstallationTaskResult(
        kFooWebAppUrl, InstallResultCode::kSuccessNewInstall,
        /*did_install_placeholder=*/true);
    absl::optional<GURL> url;
    absl::optional<InstallResultCode> code;
    std::tie(url, code) =
        InstallAndWait(&externally_managed_app_manager_impl(), install_options);
    ASSERT_EQ(InstallResultCode::kSuccessNewInstall, code.value());
    EXPECT_EQ(1u, install_run_count());
  }

  // Reinstall placeholder
  {
    install_options.reinstall_placeholder = true;
    externally_managed_app_manager_impl().SetNextInstallationTaskResult(
        kFooWebAppUrl, InstallResultCode::kSuccessNewInstall,
        /*did_install_placeholder=*/false);
    install_finalizer().SetNextUninstallExternalWebAppResult(kFooWebAppUrl,
                                                             true);

    absl::optional<GURL> url;
    absl::optional<InstallResultCode> code;
    std::tie(url, code) =
        InstallAndWait(&externally_managed_app_manager_impl(), install_options);

    EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code.value());
    EXPECT_EQ(kFooWebAppUrl, url.value());

    EXPECT_EQ(2u, install_run_count());
  }
}

TEST_F(ExternallyManagedAppManagerImplTest,
       ReinstallPlaceholderApp_ReinstallNotPossible) {
  // Install a placeholder app
  const GURL kFooWebAppUrl("https://foo.example");
  auto install_options = GetInstallOptions(kFooWebAppUrl);
  install_options.install_placeholder = true;

  {
    externally_managed_app_manager_impl().SetNextInstallationTaskResult(
        kFooWebAppUrl, InstallResultCode::kSuccessNewInstall,
        /*did_install_placeholder=*/true);
    absl::optional<GURL> url;
    absl::optional<InstallResultCode> code;
    std::tie(url, code) =
        InstallAndWait(&externally_managed_app_manager_impl(), install_options);
    ASSERT_EQ(InstallResultCode::kSuccessNewInstall, code.value());
    EXPECT_EQ(1u, install_run_count());
  }

  // Try to reinstall placeholder
  {
    install_options.reinstall_placeholder = true;
    externally_managed_app_manager_impl().SetNextInstallationTaskResult(
        kFooWebAppUrl, InstallResultCode::kSuccessNewInstall,
        /*did_install_placeholder=*/true);

    absl::optional<GURL> url;
    absl::optional<InstallResultCode> code;
    std::tie(url, code) =
        InstallAndWait(&externally_managed_app_manager_impl(), install_options);

    EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code.value());
    EXPECT_EQ(kFooWebAppUrl, url.value());

    // Even though the placeholder app is already install, we make a call to
    // WebAppInstallFinalizer. WebAppInstallFinalizer ensures we don't
    // unnecessarily install the placeholder app again.
    EXPECT_EQ(2u, install_run_count());
  }
}

TEST_F(ExternallyManagedAppManagerImplTest,
       ReinstallPlaceholderAppWhenUnused_NoOpenedWindows) {
  // Install a placeholder app
  const GURL kFooWebAppUrl("https://foo.example");
  auto install_options = GetInstallOptions(kFooWebAppUrl);
  install_options.install_placeholder = true;

  {
    externally_managed_app_manager_impl().SetNextInstallationTaskResult(
        kFooWebAppUrl, InstallResultCode::kSuccessNewInstall,
        /*did_install_placeholder=*/true);
    absl::optional<GURL> url;
    absl::optional<InstallResultCode> code;
    std::tie(url, code) =
        InstallAndWait(&externally_managed_app_manager_impl(), install_options);
    ASSERT_EQ(InstallResultCode::kSuccessNewInstall, code.value());
    EXPECT_EQ(1u, install_run_count());
  }

  // Reinstall placeholder
  {
    install_options.reinstall_placeholder = true;
    install_options.wait_for_windows_closed = true;
    externally_managed_app_manager_impl().SetNextInstallationTaskResult(
        kFooWebAppUrl, InstallResultCode::kSuccessNewInstall,
        /*did_install_placeholder=*/false);
    ui_manager().SetNumWindowsForApp(GenerateFakeAppId(kFooWebAppUrl), 0);

    absl::optional<GURL> url;
    absl::optional<InstallResultCode> code;
    std::tie(url, code) =
        InstallAndWait(&externally_managed_app_manager_impl(), install_options);

    EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code.value());
    EXPECT_EQ(kFooWebAppUrl, url.value());

    EXPECT_EQ(2u, install_run_count());
  }
}

TEST_F(ExternallyManagedAppManagerImplTest,
       ReinstallPlaceholderAppWhenUnused_OneWindowOpened) {
  // Install a placeholder app
  const GURL kFooWebAppUrl("https://foo.example");
  auto install_options = GetInstallOptions(kFooWebAppUrl);
  install_options.install_placeholder = true;

  {
    externally_managed_app_manager_impl().SetNextInstallationTaskResult(
        kFooWebAppUrl, InstallResultCode::kSuccessNewInstall,
        /*did_install_placeholder=*/true);
    absl::optional<GURL> url;
    absl::optional<InstallResultCode> code;
    std::tie(url, code) =
        InstallAndWait(&externally_managed_app_manager_impl(), install_options);
    ASSERT_EQ(InstallResultCode::kSuccessNewInstall, code.value());
    EXPECT_EQ(1u, install_run_count());
  }

  // Reinstall placeholder
  {
    install_options.reinstall_placeholder = true;
    install_options.wait_for_windows_closed = true;
    externally_managed_app_manager_impl().SetNextInstallationTaskResult(
        kFooWebAppUrl, InstallResultCode::kSuccessNewInstall,
        /*did_install_placeholder=*/false);
    ui_manager().SetNumWindowsForApp(GenerateFakeAppId(kFooWebAppUrl), 1);
    install_finalizer().SetNextUninstallExternalWebAppResult(kFooWebAppUrl,
                                                             true);

    absl::optional<GURL> url;
    absl::optional<InstallResultCode> code;
    std::tie(url, code) =
        InstallAndWait(&externally_managed_app_manager_impl(), install_options);

    EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code.value());
    EXPECT_EQ(kFooWebAppUrl, url.value());

    EXPECT_EQ(2u, install_run_count());
  }
}

TEST_F(ExternallyManagedAppManagerImplTest,
       DoNotRegisterServiceWorkerForLocalApps) {
  GURL local_urls[] = {GURL("chrome://sample"),
                       GURL("chrome-untrusted://sample")};

  for (const auto& install_url : local_urls) {
    size_t prev_install_run_count = install_run_count();

    externally_managed_app_manager_impl().SetNextInstallationTaskResult(
        install_url, InstallResultCode::kSuccessNewInstall);
    externally_managed_app_manager_impl().SetNextInstallationLaunchURL(
        install_url);
    ExternalInstallOptions install_option(
        install_url, DisplayMode::kStandalone,
        ExternalInstallSource::kSystemInstalled);
    const auto& url_and_result =
        InstallAndWait(&externally_managed_app_manager_impl(), install_option);
    EXPECT_EQ(install_url, url_and_result.first);
    EXPECT_EQ(InstallResultCode::kSuccessNewInstall, url_and_result.second);

    externally_managed_app_manager_impl().WaitForWebContentsReleased();
    EXPECT_EQ(prev_install_run_count + 1, install_run_count());
    EXPECT_EQ(0u, registration_run_count());
  }
}

}  // namespace web_app
