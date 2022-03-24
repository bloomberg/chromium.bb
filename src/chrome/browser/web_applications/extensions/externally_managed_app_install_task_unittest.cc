// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/externally_managed_app_install_task.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/web_applications/externally_installed_web_app_prefs.h"
#include "chrome/browser/web_applications/test/fake_data_retriever.h"
#include "chrome/browser/web_applications/test/fake_install_finalizer.h"
#include "chrome/browser/web_applications/test/fake_os_integration_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_app_ui_manager.h"
#include "chrome/browser/web_applications/test/test_web_app_url_loader.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_application_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "url/gurl.h"

namespace web_app {

namespace {

// Returns a factory that will return |data_retriever| the first time it gets
// called. It will DCHECK if called more than once.
WebAppInstallManager::DataRetrieverFactory GetFactoryForRetriever(
    std::unique_ptr<WebAppDataRetriever> data_retriever) {
  // Ideally we would return this lambda directly but passing a mutable lambda
  // to BindLambdaForTesting results in a OnceCallback which cannot be used as a
  // DataRetrieverFactory because DataRetrieverFactory is a RepeatingCallback.
  // For this reason, wrap the OnceCallback in a repeating callback that DCHECKs
  // if it gets called more than once.
  auto callback = base::BindLambdaForTesting(
      [data_retriever = std::move(data_retriever)]() mutable {
        return std::move(data_retriever);
      });

  return base::BindRepeating(
      [](base::OnceCallback<std::unique_ptr<WebAppDataRetriever>()> callback) {
        DCHECK(callback);
        return std::move(callback).Run();
      },
      base::Passed(std::move(callback)));
}

// TODO(ortuno): Move this to ExternallyInstalledWebAppPrefs or replace with a
// method in ExternallyInstalledWebAppPrefs once there is one.
bool IsPlaceholderApp(Profile* profile, const GURL& url) {
  const base::Value* map =
      profile->GetPrefs()->GetDictionary(prefs::kWebAppsExtensionIDs);

  const base::Value* entry = map->FindKey(url.spec());

  return entry->FindBoolKey("is_placeholder").value();
}

class TestExternallyManagedAppInstallFinalizer : public WebAppInstallFinalizer {
 public:
  explicit TestExternallyManagedAppInstallFinalizer(
      WebAppRegistrarMutable* registrar)
      : WebAppInstallFinalizer(nullptr, nullptr, nullptr),
        registrar_(registrar) {}
  TestExternallyManagedAppInstallFinalizer(
      const TestExternallyManagedAppInstallFinalizer&) = delete;
  TestExternallyManagedAppInstallFinalizer& operator=(
      const TestExternallyManagedAppInstallFinalizer&) = delete;
  ~TestExternallyManagedAppInstallFinalizer() override = default;

  // Returns what would be the AppId if an app is installed with |url|.
  AppId GetAppIdForUrl(const GURL& url) {
    return FakeInstallFinalizer::GetAppIdForUrl(url);
  }

  void RegisterApp(std::unique_ptr<web_app::WebApp> web_app) {
    web_app::AppId app_id = web_app->app_id();
    registrar_->registry().emplace(std::move(app_id), std::move(web_app));
  }

  void UnregisterApp(const AppId& app_id) {
    auto it = registrar_->registry().find(app_id);
    DCHECK(it != registrar_->registry().end());

    registrar_->registry().erase(it);
  }

  void SetNextFinalizeInstallResult(const GURL& url, InstallResultCode code) {
    DCHECK(!base::Contains(next_finalize_install_results_, url));

    AppId app_id;
    if (code == InstallResultCode::kSuccessNewInstall) {
      app_id = GetAppIdForUrl(url);
    }
    next_finalize_install_results_[url] = {app_id, code};
  }

  void SetNextUninstallExternalWebAppResult(const GURL& app_url,
                                            bool uninstalled) {
    DCHECK(!base::Contains(next_uninstall_external_web_app_results_, app_url));

    next_uninstall_external_web_app_results_[app_url] = {
        GetAppIdForUrl(app_url), uninstalled};
  }

  const std::vector<WebApplicationInfo>& web_app_info_list() {
    return web_app_info_list_;
  }

  const std::vector<FinalizeOptions>& finalize_options_list() {
    return finalize_options_list_;
  }

  const std::vector<GURL>& uninstall_external_web_app_urls() const {
    return uninstall_external_web_app_urls_;
  }

  size_t num_reparent_tab_calls() { return num_reparent_tab_calls_; }

  // WebAppInstallFinalizer
  void FinalizeInstall(const WebApplicationInfo& web_app_info,
                       const FinalizeOptions& options,
                       InstallFinalizedCallback callback) override {
    DCHECK(
        base::Contains(next_finalize_install_results_, web_app_info.start_url));

    web_app_info_list_.push_back(web_app_info);
    finalize_options_list_.push_back(options);

    AppId app_id;
    InstallResultCode code;
    std::tie(app_id, code) =
        next_finalize_install_results_[web_app_info.start_url];
    next_finalize_install_results_.erase(web_app_info.start_url);
    const GURL& url = web_app_info.start_url;

    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindLambdaForTesting(
            [&, app_id, url, code, callback = std::move(callback)]() mutable {
              auto web_app = test::CreateWebApp(url, Source::kPolicy);
              RegisterApp(std::move(web_app));
              std::move(callback).Run(app_id, code);
            }));
  }

  void UninstallWithoutRegistryUpdateFromSync(
      const std::vector<AppId>& web_apps,
      RepeatingUninstallCallback callback) override {
    NOTREACHED();
  }

  void FinalizeUpdate(const WebApplicationInfo& web_app_info,
                      InstallFinalizedCallback callback) override {
    NOTREACHED();
  }

  void UninstallExternalWebApp(
      const AppId& app_id,
      webapps::WebappUninstallSource external_install_source,
      UninstallWebAppCallback callback) override {
    UnregisterApp(app_id);

    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), /*uninstalled=*/true));
  }

  void UninstallExternalWebAppByUrl(
      const GURL& app_url,
      webapps::WebappUninstallSource external_install_source,
      UninstallWebAppCallback callback) override {
    DCHECK(base::Contains(next_uninstall_external_web_app_results_, app_url));
    uninstall_external_web_app_urls_.push_back(app_url);

    AppId app_id;
    bool uninstalled;
    std::tie(app_id, uninstalled) =
        next_uninstall_external_web_app_results_[app_url];
    next_uninstall_external_web_app_results_.erase(app_url);

    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindLambdaForTesting(
            [&, app_id, uninstalled, callback = std::move(callback)]() mutable {
              if (uninstalled)
                UnregisterApp(app_id);
              std::move(callback).Run(uninstalled);
            }));
  }

  void RetryIncompleteUninstalls(
      const std::vector<AppId>& apps_to_uninstall) override {
    NOTREACHED();
  }

  bool CanUserUninstallWebApp(const AppId& app_id) const override {
    NOTIMPLEMENTED();
    return false;
  }

  void UninstallWebApp(const AppId& app_id,
                       webapps::WebappUninstallSource uninstall_source,
                       UninstallWebAppCallback callback) override {
    NOTIMPLEMENTED();
  }

  bool WasPreinstalledWebAppUninstalled(const AppId& app_id) const override {
    NOTIMPLEMENTED();
    return false;
  }

  bool CanReparentTab(const AppId& app_id,
                      bool shortcut_created) const override {
    return true;
  }

  void ReparentTab(const AppId& app_id,
                   bool shortcut_created,
                   content::WebContents* web_contents) override {
    ++num_reparent_tab_calls_;
  }

  void SetRemoveSourceCallbackForTesting(
      base::RepeatingCallback<void(const AppId&)>) override {
    NOTIMPLEMENTED();
  }

 private:
  raw_ptr<WebAppRegistrarMutable> registrar_ = nullptr;

  std::vector<WebApplicationInfo> web_app_info_list_;
  std::vector<FinalizeOptions> finalize_options_list_;
  std::vector<GURL> uninstall_external_web_app_urls_;

  size_t num_reparent_tab_calls_ = 0;

  std::map<GURL, std::pair<AppId, InstallResultCode>>
      next_finalize_install_results_;

  // Maps app URLs to the id of the app that would have been installed for that
  // url and the result of trying to uninstall it.
  std::map<GURL, std::pair<AppId, bool>>
      next_uninstall_external_web_app_results_;
};

}  // namespace

class ExternallyManagedAppInstallTaskTest
    : public ChromeRenderViewHostTestHarness {
 public:
  ExternallyManagedAppInstallTaskTest() = default;
  ExternallyManagedAppInstallTaskTest(
      const ExternallyManagedAppInstallTaskTest&) = delete;
  ExternallyManagedAppInstallTaskTest& operator=(
      const ExternallyManagedAppInstallTaskTest&) = delete;
  ~ExternallyManagedAppInstallTaskTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    url_loader_ = std::make_unique<TestWebAppUrlLoader>();

    auto* provider = FakeWebAppProvider::Get(profile());

    auto registrar = std::make_unique<WebAppRegistrarMutable>(profile());
    registrar_ = registrar.get();

    auto install_finalizer =
        std::make_unique<TestExternallyManagedAppInstallFinalizer>(
            registrar.get());
    install_finalizer_ = install_finalizer.get();

    auto install_manager = std::make_unique<WebAppInstallManager>(profile());
    install_manager_ = install_manager.get();

    auto os_integration_manager = std::make_unique<FakeOsIntegrationManager>(
        profile(), /*app_shortcut_manager=*/nullptr,
        /*file_handler_manager=*/nullptr,
        /*protocol_handler_manager=*/nullptr,
        /*url_handler_manager*/ nullptr);
    os_integration_manager_ = os_integration_manager.get();

    auto ui_manager = std::make_unique<FakeWebAppUiManager>();
    ui_manager_ = ui_manager.get();

    provider->SetRegistrar(std::move(registrar));
    provider->SetInstallManager(std::move(install_manager));
    provider->SetInstallFinalizer(std::move(install_finalizer));
    provider->SetWebAppUiManager(std::move(ui_manager));
    provider->SetOsIntegrationManager(std::move(os_integration_manager));

    provider->Start();
    // Start only WebAppInstallManager for real.
    install_manager_->Start();
  }

 protected:
  TestWebAppUrlLoader& url_loader() { return *url_loader_; }

  FakeWebAppUiManager* ui_manager() { return ui_manager_; }
  WebAppRegistrar* registrar() { return registrar_; }
  TestExternallyManagedAppInstallFinalizer* finalizer() {
    return install_finalizer_;
  }
  WebAppInstallManager* install_manager() { return install_manager_; }
  FakeOsIntegrationManager* os_integration_manager() {
    return os_integration_manager_;
  }

  FakeDataRetriever* data_retriever() { return data_retriever_; }

  const WebApplicationInfo& web_app_info() {
    DCHECK_EQ(1u, install_finalizer_->web_app_info_list().size());
    return install_finalizer_->web_app_info_list().at(0);
  }

  const WebAppInstallFinalizer::FinalizeOptions& finalize_options() {
    DCHECK_EQ(1u, install_finalizer_->finalize_options_list().size());
    return install_finalizer_->finalize_options_list().at(0);
  }

  std::unique_ptr<ExternallyManagedAppInstallTask>
  GetInstallationTaskWithTestMocks(ExternalInstallOptions options) {
    auto data_retriever = std::make_unique<FakeDataRetriever>();
    data_retriever_ = data_retriever.get();

    install_manager_->SetDataRetrieverFactoryForTesting(
        GetFactoryForRetriever(std::move(data_retriever)));
    auto manifest = blink::mojom::Manifest::New();
    manifest->start_url = options.install_url;
    manifest->name = u"Manifest Name";

    data_retriever_->SetRendererWebApplicationInfo(
        std::make_unique<WebApplicationInfo>());

    data_retriever_->SetManifest(std::move(manifest), /*is_installable=*/true);

    data_retriever_->SetIcons(IconsMap{});

    install_finalizer_->SetNextFinalizeInstallResult(
        options.install_url, InstallResultCode::kSuccessNewInstall);

    os_integration_manager_->SetNextCreateShortcutsResult(
        install_finalizer_->GetAppIdForUrl(options.install_url), true);

    auto task = std::make_unique<ExternallyManagedAppInstallTask>(
        profile(), url_loader_.get(), registrar_, os_integration_manager_,
        ui_manager_, install_finalizer_, install_manager_, std::move(options));
    return task;
  }

 private:
  std::unique_ptr<TestWebAppUrlLoader> url_loader_;
  raw_ptr<WebAppInstallManager> install_manager_ = nullptr;
  raw_ptr<WebAppRegistrar> registrar_ = nullptr;
  raw_ptr<FakeDataRetriever> data_retriever_ = nullptr;
  raw_ptr<TestExternallyManagedAppInstallFinalizer> install_finalizer_ =
      nullptr;
  raw_ptr<FakeWebAppUiManager> ui_manager_ = nullptr;
  raw_ptr<FakeOsIntegrationManager> os_integration_manager_ = nullptr;
};

class ExternallyManagedAppInstallTaskWithRunOnOsLoginTest
    : public ExternallyManagedAppInstallTaskTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures({features::kDesktopPWAsRunOnOsLogin},
                                          {});
    ExternallyManagedAppInstallTaskTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ExternallyManagedAppInstallTaskTest, InstallSucceeds) {
  const GURL kWebAppUrl("https://foo.example");
  auto task = GetInstallationTaskWithTestMocks(
      {kWebAppUrl, DisplayMode::kUndefined,
       ExternalInstallSource::kInternalDefault});
  url_loader().SetPrepareForLoadResultLoaded();
  url_loader().SetNextLoadUrlResult(kWebAppUrl,
                                    WebAppUrlLoader::Result::kUrlLoaded);

  base::RunLoop run_loop;

  task->Install(
      web_contents(),
      base::BindLambdaForTesting(
          [&](ExternallyManagedAppManager::InstallResult result) {
            absl::optional<AppId> id =
                ExternallyInstalledWebAppPrefs(profile()->GetPrefs())
                    .LookupAppId(kWebAppUrl);

            EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
            EXPECT_TRUE(result.app_id.has_value());

            EXPECT_FALSE(IsPlaceholderApp(profile(), kWebAppUrl));

            EXPECT_EQ(result.app_id.value(), id.value());

            EXPECT_EQ(1u,
                      os_integration_manager()->num_create_shortcuts_calls());
            EXPECT_TRUE(os_integration_manager()->did_add_to_desktop().value());

            EXPECT_EQ(1u, os_integration_manager()
                              ->num_add_app_to_quick_launch_bar_calls());
            EXPECT_EQ(0u, finalizer()->num_reparent_tab_calls());

            EXPECT_EQ(web_app_info().user_display_mode, DisplayMode::kBrowser);
            EXPECT_EQ(webapps::WebappInstallSource::INTERNAL_DEFAULT,
                      finalize_options().install_source);

            run_loop.Quit();
          }));

  run_loop.Run();
}

TEST_F(ExternallyManagedAppInstallTaskTest, InstallFails) {
  const GURL kWebAppUrl("https://foo.example");
  auto task = GetInstallationTaskWithTestMocks(
      {kWebAppUrl, DisplayMode::kStandalone,
       ExternalInstallSource::kInternalDefault});
  data_retriever()->SetRendererWebApplicationInfo(nullptr);
  url_loader().SetPrepareForLoadResultLoaded();
  url_loader().SetNextLoadUrlResult(kWebAppUrl,
                                    WebAppUrlLoader::Result::kUrlLoaded);

  base::RunLoop run_loop;

  task->Install(web_contents(),
                base::BindLambdaForTesting(
                    [&](ExternallyManagedAppManager::InstallResult result) {
                      absl::optional<AppId> id =
                          ExternallyInstalledWebAppPrefs(profile()->GetPrefs())
                              .LookupAppId(kWebAppUrl);

                      EXPECT_EQ(InstallResultCode::kGetWebApplicationInfoFailed,
                                result.code);
                      EXPECT_FALSE(result.app_id.has_value());

                      EXPECT_FALSE(id.has_value());

                      run_loop.Quit();
                    }));

  run_loop.Run();
}

TEST_F(ExternallyManagedAppInstallTaskTest, InstallNoDesktopShortcut) {
  const GURL kWebAppUrl("https://foo.example");
  ExternalInstallOptions install_options(
      kWebAppUrl, DisplayMode::kStandalone,
      ExternalInstallSource::kInternalDefault);
  install_options.add_to_desktop = false;
  auto task = GetInstallationTaskWithTestMocks(std::move(install_options));
  url_loader().SetPrepareForLoadResultLoaded();
  url_loader().SetNextLoadUrlResult(kWebAppUrl,
                                    WebAppUrlLoader::Result::kUrlLoaded);

  base::RunLoop run_loop;

  task->Install(
      web_contents(),
      base::BindLambdaForTesting([&](ExternallyManagedAppManager::InstallResult
                                         result) {
        EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
        EXPECT_TRUE(result.app_id.has_value());

        EXPECT_EQ(1u, os_integration_manager()->num_create_shortcuts_calls());
        EXPECT_FALSE(os_integration_manager()->did_add_to_desktop().value());

        EXPECT_EQ(
            1u,
            os_integration_manager()->num_add_app_to_quick_launch_bar_calls());
        EXPECT_EQ(0u, finalizer()->num_reparent_tab_calls());

        run_loop.Quit();
      }));

  run_loop.Run();
}

TEST_F(ExternallyManagedAppInstallTaskTest, InstallNoQuickLaunchBarShortcut) {
  const GURL kWebAppUrl("https://foo.example");
  ExternalInstallOptions install_options(
      kWebAppUrl, DisplayMode::kStandalone,
      ExternalInstallSource::kInternalDefault);
  install_options.add_to_quick_launch_bar = false;
  auto task = GetInstallationTaskWithTestMocks(std::move(install_options));
  url_loader().SetPrepareForLoadResultLoaded();
  url_loader().SetNextLoadUrlResult(kWebAppUrl,
                                    WebAppUrlLoader::Result::kUrlLoaded);

  base::RunLoop run_loop;
  task->Install(
      web_contents(),
      base::BindLambdaForTesting(
          [&](ExternallyManagedAppManager::InstallResult result) {
            EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
            EXPECT_TRUE(result.app_id.has_value());

            EXPECT_EQ(1u,
                      os_integration_manager()->num_create_shortcuts_calls());
            EXPECT_TRUE(os_integration_manager()->did_add_to_desktop().value());

            EXPECT_EQ(0u, os_integration_manager()
                              ->num_add_app_to_quick_launch_bar_calls());
            EXPECT_EQ(0u, finalizer()->num_reparent_tab_calls());

            run_loop.Quit();
          }));

  run_loop.Run();
}

TEST_F(ExternallyManagedAppInstallTaskTest,
       InstallNoDesktopShortcutAndNoQuickLaunchBarShortcut) {
  const GURL kWebAppUrl("https://foo.example");
  ExternalInstallOptions install_options(
      kWebAppUrl, DisplayMode::kStandalone,
      ExternalInstallSource::kInternalDefault);
  install_options.add_to_desktop = false;
  install_options.add_to_quick_launch_bar = false;
  auto task = GetInstallationTaskWithTestMocks(std::move(install_options));
  url_loader().SetPrepareForLoadResultLoaded();
  url_loader().SetNextLoadUrlResult(kWebAppUrl,
                                    WebAppUrlLoader::Result::kUrlLoaded);

  base::RunLoop run_loop;
  task->Install(
      web_contents(),
      base::BindLambdaForTesting([&](ExternallyManagedAppManager::InstallResult
                                         result) {
        EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
        EXPECT_TRUE(result.app_id.has_value());

        EXPECT_EQ(1u, os_integration_manager()->num_create_shortcuts_calls());
        EXPECT_FALSE(os_integration_manager()->did_add_to_desktop().value());

        EXPECT_EQ(
            0u,
            os_integration_manager()->num_add_app_to_quick_launch_bar_calls());
        EXPECT_EQ(0u, finalizer()->num_reparent_tab_calls());

        run_loop.Quit();
      }));

  run_loop.Run();
}

TEST_F(ExternallyManagedAppInstallTaskTest, InstallForcedContainerWindow) {
  const GURL kWebAppUrl("https://foo.example");
  auto install_options =
      ExternalInstallOptions(kWebAppUrl, DisplayMode::kStandalone,
                             ExternalInstallSource::kInternalDefault);
  auto task = GetInstallationTaskWithTestMocks(std::move(install_options));
  url_loader().SetPrepareForLoadResultLoaded();
  url_loader().SetNextLoadUrlResult(kWebAppUrl,
                                    WebAppUrlLoader::Result::kUrlLoaded);

  base::RunLoop run_loop;
  task->Install(web_contents(),
                base::BindLambdaForTesting(
                    [&](ExternallyManagedAppManager::InstallResult result) {
                      EXPECT_EQ(InstallResultCode::kSuccessNewInstall,
                                result.code);
                      EXPECT_TRUE(result.app_id.has_value());
                      EXPECT_EQ(web_app_info().user_display_mode,
                                DisplayMode::kStandalone);
                      run_loop.Quit();
                    }));

  run_loop.Run();
}

TEST_F(ExternallyManagedAppInstallTaskTest, InstallForcedContainerTab) {
  const GURL kWebAppUrl("https://foo.example");
  auto install_options =
      ExternalInstallOptions(kWebAppUrl, DisplayMode::kBrowser,
                             ExternalInstallSource::kInternalDefault);
  auto task = GetInstallationTaskWithTestMocks(std::move(install_options));
  url_loader().SetPrepareForLoadResultLoaded();
  url_loader().SetNextLoadUrlResult(kWebAppUrl,
                                    WebAppUrlLoader::Result::kUrlLoaded);

  base::RunLoop run_loop;
  task->Install(web_contents(),
                base::BindLambdaForTesting(
                    [&](ExternallyManagedAppManager::InstallResult result) {
                      EXPECT_EQ(InstallResultCode::kSuccessNewInstall,
                                result.code);
                      EXPECT_TRUE(result.app_id.has_value());
                      EXPECT_EQ(web_app_info().user_display_mode,
                                DisplayMode::kBrowser);
                      run_loop.Quit();
                    }));

  run_loop.Run();
}

TEST_F(ExternallyManagedAppInstallTaskTest, InstallPreinstalledApp) {
  const GURL kWebAppUrl("https://foo.example");
  auto install_options =
      ExternalInstallOptions(kWebAppUrl, DisplayMode::kUndefined,
                             ExternalInstallSource::kInternalDefault);
  auto task = GetInstallationTaskWithTestMocks(std::move(install_options));
  url_loader().SetPrepareForLoadResultLoaded();
  url_loader().SetNextLoadUrlResult(kWebAppUrl,
                                    WebAppUrlLoader::Result::kUrlLoaded);

  base::RunLoop run_loop;
  task->Install(web_contents(),
                base::BindLambdaForTesting(
                    [&](ExternallyManagedAppManager::InstallResult result) {
                      EXPECT_EQ(InstallResultCode::kSuccessNewInstall,
                                result.code);
                      EXPECT_TRUE(result.app_id.has_value());

                      EXPECT_EQ(webapps::WebappInstallSource::INTERNAL_DEFAULT,
                                finalize_options().install_source);
                      run_loop.Quit();
                    }));

  run_loop.Run();
}

TEST_F(ExternallyManagedAppInstallTaskTest, InstallAppFromPolicy) {
  const GURL kWebAppUrl("https://foo.example");
  auto install_options =
      ExternalInstallOptions(kWebAppUrl, DisplayMode::kUndefined,
                             ExternalInstallSource::kExternalPolicy);
  auto task = GetInstallationTaskWithTestMocks(std::move(install_options));
  url_loader().SetPrepareForLoadResultLoaded();
  url_loader().SetNextLoadUrlResult(kWebAppUrl,
                                    WebAppUrlLoader::Result::kUrlLoaded);

  base::RunLoop run_loop;
  task->Install(web_contents(),
                base::BindLambdaForTesting(
                    [&](ExternallyManagedAppManager::InstallResult result) {
                      EXPECT_EQ(InstallResultCode::kSuccessNewInstall,
                                result.code);
                      EXPECT_TRUE(result.app_id.has_value());

                      EXPECT_EQ(webapps::WebappInstallSource::EXTERNAL_POLICY,
                                finalize_options().install_source);
                      run_loop.Quit();
                    }));

  run_loop.Run();
}

TEST_F(ExternallyManagedAppInstallTaskTest, InstallPlaceholder) {
  const GURL kWebAppUrl("https://foo.example");
  ExternalInstallOptions options(kWebAppUrl, DisplayMode::kStandalone,
                                 ExternalInstallSource::kExternalPolicy);
  options.install_placeholder = true;
  auto task = GetInstallationTaskWithTestMocks(std::move(options));
  url_loader().SetPrepareForLoadResultLoaded();
  url_loader().SetNextLoadUrlResult(
      kWebAppUrl, WebAppUrlLoader::Result::kRedirectedUrlLoaded);

  base::RunLoop run_loop;
  task->Install(
      web_contents(),
      base::BindLambdaForTesting(
          [&](ExternallyManagedAppManager::InstallResult result) {
            EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
            EXPECT_TRUE(result.app_id.has_value());

            EXPECT_TRUE(IsPlaceholderApp(profile(), kWebAppUrl));

            EXPECT_EQ(1u,
                      os_integration_manager()->num_create_shortcuts_calls());
            EXPECT_EQ(1u, finalizer()->finalize_options_list().size());
            EXPECT_EQ(webapps::WebappInstallSource::EXTERNAL_POLICY,
                      finalize_options().install_source);
            const WebApplicationInfo& web_app_info =
                finalizer()->web_app_info_list().at(0);

            EXPECT_EQ(base::UTF8ToUTF16(kWebAppUrl.spec()), web_app_info.title);
            EXPECT_EQ(kWebAppUrl, web_app_info.start_url);
            EXPECT_EQ(web_app_info.user_display_mode, DisplayMode::kStandalone);
            EXPECT_TRUE(web_app_info.manifest_icons.empty());
            EXPECT_TRUE(web_app_info.icon_bitmaps.any.empty());

            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(ExternallyManagedAppInstallTaskTest, InstallPlaceholderDefaultSource) {
  const GURL kWebAppUrl("https://foo.example");
  ExternalInstallOptions options(kWebAppUrl, DisplayMode::kStandalone,
                                 ExternalInstallSource::kExternalDefault);
  options.install_placeholder = true;
  auto task = GetInstallationTaskWithTestMocks(std::move(options));
  url_loader().SetPrepareForLoadResultLoaded();
  url_loader().SetNextLoadUrlResult(
      kWebAppUrl, WebAppUrlLoader::Result::kRedirectedUrlLoaded);

  base::RunLoop run_loop;
  task->Install(
      web_contents(),
      base::BindLambdaForTesting(
          [&](ExternallyManagedAppManager::InstallResult result) {
            EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
            EXPECT_TRUE(result.app_id.has_value());

            EXPECT_TRUE(IsPlaceholderApp(profile(), kWebAppUrl));

            EXPECT_EQ(1u,
                      os_integration_manager()->num_create_shortcuts_calls());
            EXPECT_EQ(1u, finalizer()->finalize_options_list().size());
            EXPECT_EQ(webapps::WebappInstallSource::EXTERNAL_DEFAULT,
                      finalize_options().install_source);
            const WebApplicationInfo& web_app_info =
                finalizer()->web_app_info_list().at(0);

            EXPECT_EQ(base::UTF8ToUTF16(kWebAppUrl.spec()), web_app_info.title);
            EXPECT_EQ(kWebAppUrl, web_app_info.start_url);
            EXPECT_EQ(web_app_info.user_display_mode, DisplayMode::kStandalone);
            EXPECT_TRUE(web_app_info.manifest_icons.empty());
            EXPECT_TRUE(web_app_info.icon_bitmaps.any.empty());

            run_loop.Quit();
          }));
  run_loop.Run();
}

// Tests that palceholders are correctly installed when the platform doesn't
// support os shortcuts.
TEST_F(ExternallyManagedAppInstallTaskTest,
       InstallPlaceholderNoCreateOsShorcuts) {
  const GURL kWebAppUrl("https://foo.example");
  ExternalInstallOptions options(kWebAppUrl, DisplayMode::kStandalone,
                                 ExternalInstallSource::kExternalPolicy);
  options.install_placeholder = true;
  auto task = GetInstallationTaskWithTestMocks(std::move(options));
  os_integration_manager()->set_can_create_shortcuts(false);
  url_loader().SetPrepareForLoadResultLoaded();
  url_loader().SetNextLoadUrlResult(
      kWebAppUrl, WebAppUrlLoader::Result::kRedirectedUrlLoaded);

  base::RunLoop run_loop;
  task->Install(
      web_contents(),
      base::BindLambdaForTesting(
          [&](ExternallyManagedAppManager::InstallResult result) {
            EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
            EXPECT_TRUE(result.app_id.has_value());

            EXPECT_TRUE(IsPlaceholderApp(profile(), kWebAppUrl));

            EXPECT_EQ(0u,
                      os_integration_manager()->num_create_shortcuts_calls());
            EXPECT_EQ(1u, finalizer()->finalize_options_list().size());
            EXPECT_EQ(webapps::WebappInstallSource::EXTERNAL_POLICY,
                      finalize_options().install_source);
            const WebApplicationInfo& web_app_info =
                finalizer()->web_app_info_list().at(0);

            EXPECT_EQ(base::UTF8ToUTF16(kWebAppUrl.spec()), web_app_info.title);
            EXPECT_EQ(kWebAppUrl, web_app_info.start_url);
            EXPECT_EQ(web_app_info.user_display_mode, DisplayMode::kStandalone);
            EXPECT_TRUE(web_app_info.manifest_icons.empty());
            EXPECT_TRUE(web_app_info.icon_bitmaps.any.empty());

            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(ExternallyManagedAppInstallTaskTest, InstallPlaceholderTwice) {
  const GURL kWebAppUrl("https://foo.example");
  ExternalInstallOptions options(kWebAppUrl, DisplayMode::kStandalone,
                                 ExternalInstallSource::kExternalPolicy);
  options.install_placeholder = true;
  AppId placeholder_app_id;

  // Install a placeholder app.
  {
    auto task = GetInstallationTaskWithTestMocks(options);
    url_loader().SetPrepareForLoadResultLoaded();
    url_loader().SetNextLoadUrlResult(
        kWebAppUrl, WebAppUrlLoader::Result::kRedirectedUrlLoaded);

    base::RunLoop run_loop;
    task->Install(
        web_contents(),
        base::BindLambdaForTesting(
            [&](ExternallyManagedAppManager::InstallResult result) {
              EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
              placeholder_app_id = result.app_id.value();

              EXPECT_EQ(1u, finalizer()->finalize_options_list().size());
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  // Try to install it again.
  auto task = GetInstallationTaskWithTestMocks(options);
  url_loader().SetPrepareForLoadResultLoaded();
  url_loader().SetNextLoadUrlResult(
      kWebAppUrl, WebAppUrlLoader::Result::kRedirectedUrlLoaded);

  base::RunLoop run_loop;
  task->Install(
      web_contents(),
      base::BindLambdaForTesting(
          [&](ExternallyManagedAppManager::InstallResult result) {
            EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
            EXPECT_EQ(placeholder_app_id, result.app_id.value());

            // There shouldn't be a second call to the finalizer.
            EXPECT_EQ(1u, finalizer()->finalize_options_list().size());

            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(ExternallyManagedAppInstallTaskTest, ReinstallPlaceholderSucceeds) {
  const GURL kWebAppUrl("https://foo.example");
  ExternalInstallOptions options(kWebAppUrl, DisplayMode::kStandalone,
                                 ExternalInstallSource::kExternalPolicy);
  options.install_placeholder = true;
  AppId placeholder_app_id;

  // Install a placeholder app.
  {
    auto task = GetInstallationTaskWithTestMocks(options);
    url_loader().SetPrepareForLoadResultLoaded();
    url_loader().SetNextLoadUrlResult(
        kWebAppUrl, WebAppUrlLoader::Result::kRedirectedUrlLoaded);

    base::RunLoop run_loop;
    task->Install(
        web_contents(),
        base::BindLambdaForTesting(
            [&](ExternallyManagedAppManager::InstallResult result) {
              EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
              placeholder_app_id = result.app_id.value();

              EXPECT_EQ(1u, finalizer()->finalize_options_list().size());
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  // Replace the placeholder with a real app.
  options.reinstall_placeholder = true;
  auto task = GetInstallationTaskWithTestMocks(options);
  finalizer()->SetNextUninstallExternalWebAppResult(kWebAppUrl, true);
  url_loader().SetPrepareForLoadResultLoaded();
  url_loader().SetNextLoadUrlResult(kWebAppUrl,
                                    WebAppUrlLoader::Result::kUrlLoaded);

  base::RunLoop run_loop;
  task->Install(
      web_contents(),
      base::BindLambdaForTesting(
          [&](ExternallyManagedAppManager::InstallResult result) {
            EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
            EXPECT_TRUE(result.app_id.has_value());
            EXPECT_FALSE(IsPlaceholderApp(profile(), kWebAppUrl));

            EXPECT_EQ(1u,
                      finalizer()->uninstall_external_web_app_urls().size());
            EXPECT_EQ(kWebAppUrl,
                      finalizer()->uninstall_external_web_app_urls().at(0));

            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(ExternallyManagedAppInstallTaskTest, ReinstallPlaceholderFails) {
  const GURL kWebAppUrl("https://foo.example");
  ExternalInstallOptions options(kWebAppUrl, DisplayMode::kStandalone,
                                 ExternalInstallSource::kExternalPolicy);
  options.install_placeholder = true;
  AppId placeholder_app_id;

  // Install a placeholder app.
  {
    auto task = GetInstallationTaskWithTestMocks(options);
    url_loader().SetPrepareForLoadResultLoaded();
    url_loader().SetNextLoadUrlResult(
        kWebAppUrl, WebAppUrlLoader::Result::kRedirectedUrlLoaded);

    base::RunLoop run_loop;
    task->Install(
        web_contents(),
        base::BindLambdaForTesting(
            [&](ExternallyManagedAppManager::InstallResult result) {
              EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
              placeholder_app_id = result.app_id.value();

              EXPECT_EQ(1u, finalizer()->finalize_options_list().size());

              run_loop.Quit();
            }));
    run_loop.Run();
  }

  // Replace the placeholder with a real app.
  options.reinstall_placeholder = true;
  auto task = GetInstallationTaskWithTestMocks(options);

  finalizer()->SetNextUninstallExternalWebAppResult(kWebAppUrl, false);
  url_loader().SetPrepareForLoadResultLoaded();
  url_loader().SetNextLoadUrlResult(kWebAppUrl,
                                    WebAppUrlLoader::Result::kUrlLoaded);

  base::RunLoop run_loop;
  task->Install(
      web_contents(),
      base::BindLambdaForTesting(
          [&](ExternallyManagedAppManager::InstallResult result) {
            EXPECT_EQ(InstallResultCode::kFailedPlaceholderUninstall,
                      result.code);
            EXPECT_FALSE(result.app_id.has_value());
            EXPECT_TRUE(IsPlaceholderApp(profile(), kWebAppUrl));

            EXPECT_EQ(1u,
                      finalizer()->uninstall_external_web_app_urls().size());
            EXPECT_EQ(kWebAppUrl,
                      finalizer()->uninstall_external_web_app_urls().at(0));

            // There should have been no new calls to install a placeholder.
            EXPECT_EQ(1u, finalizer()->finalize_options_list().size());

            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(ExternallyManagedAppInstallTaskTest, UninstallAndReplace) {
  const GURL kWebAppUrl("https://foo.example");
  ExternalInstallOptions options = {kWebAppUrl, DisplayMode::kUndefined,
                                    ExternalInstallSource::kInternalDefault};
  AppId app_id;
  {
    // Migrate app1 and app2.
    options.uninstall_and_replace = {"app1", "app2"};

    base::RunLoop run_loop;
    auto task = GetInstallationTaskWithTestMocks(options);
    url_loader().SetPrepareForLoadResultLoaded();
    url_loader().SetNextLoadUrlResult(kWebAppUrl,
                                      WebAppUrlLoader::Result::kUrlLoaded);

    task->Install(
        web_contents(),
        base::BindLambdaForTesting(
            [&](ExternallyManagedAppManager::InstallResult result) {
              app_id = result.app_id.value();

              EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
              EXPECT_EQ(result.app_id,
                        *ExternallyInstalledWebAppPrefs(profile()->GetPrefs())
                             .LookupAppId(kWebAppUrl));

              EXPECT_TRUE(ui_manager()->DidUninstallAndReplace("app1", app_id));
              EXPECT_TRUE(ui_manager()->DidUninstallAndReplace("app2", app_id));

              run_loop.Quit();
            }));
    run_loop.Run();
  }
  {
    // Migration should run on every install of the app.
    options.uninstall_and_replace = {"app3"};

    base::RunLoop run_loop;
    auto task = GetInstallationTaskWithTestMocks(options);
    url_loader().SetPrepareForLoadResultLoaded();
    url_loader().SetNextLoadUrlResult(kWebAppUrl,
                                      WebAppUrlLoader::Result::kUrlLoaded);

    task->Install(
        web_contents(),
        base::BindLambdaForTesting(
            [&](ExternallyManagedAppManager::InstallResult result) {
              EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
              EXPECT_EQ(app_id, result.app_id.value());

              EXPECT_TRUE(ui_manager()->DidUninstallAndReplace("app3", app_id));

              run_loop.Quit();
            }));
    run_loop.Run();
  }
}

TEST_F(ExternallyManagedAppInstallTaskTest, InstallURLLoadFailed) {
  struct ResultPair {
    WebAppUrlLoader::Result loader_result;
    InstallResultCode install_result;
  } result_pairs[] = {{WebAppUrlLoader::Result::kRedirectedUrlLoaded,
                       InstallResultCode::kInstallURLRedirected},
                      {WebAppUrlLoader::Result::kFailedUnknownReason,
                       InstallResultCode::kInstallURLLoadFailed},
                      {WebAppUrlLoader::Result::kFailedPageTookTooLong,
                       InstallResultCode::kInstallURLLoadTimeOut}};

  for (const auto& result_pair : result_pairs) {
    base::RunLoop run_loop;

    ExternalInstallOptions install_options(
        GURL(), DisplayMode::kStandalone,
        ExternalInstallSource::kInternalDefault);
    ExternallyManagedAppInstallTask install_task(
        profile(), &url_loader(), registrar(), os_integration_manager(),
        ui_manager(), finalizer(), install_manager(), install_options);
    url_loader().SetPrepareForLoadResultLoaded();
    url_loader().SetNextLoadUrlResult(GURL(), result_pair.loader_result);

    install_task.Install(
        web_contents(),
        base::BindLambdaForTesting(
            [&](ExternallyManagedAppManager::InstallResult result) {
              EXPECT_EQ(result.code, result_pair.install_result);
              run_loop.Quit();
            }));

    run_loop.Run();
  }
}

TEST_F(ExternallyManagedAppInstallTaskTest, InstallFailedWebContentsDestroyed) {
  ExternalInstallOptions install_options(
      GURL(), DisplayMode::kStandalone,
      ExternalInstallSource::kInternalDefault);
  ExternallyManagedAppInstallTask install_task(
      profile(), &url_loader(), registrar(), os_integration_manager(),
      ui_manager(), finalizer(), install_manager(), install_options);
  url_loader().SetPrepareForLoadResultLoaded();
  url_loader().SetNextLoadUrlResult(
      GURL(), WebAppUrlLoader::Result::kFailedWebContentsDestroyed);

  install_task.Install(
      web_contents(),
      base::BindLambdaForTesting(
          [&](ExternallyManagedAppManager::InstallResult) { NOTREACHED(); }));

  base::RunLoop().RunUntilIdle();
}

TEST_F(ExternallyManagedAppInstallTaskTest, InstallWithWebAppInfoSucceeds) {
  const GURL kWebAppUrl("https://foo.example");
  ExternalInstallOptions options(kWebAppUrl, DisplayMode::kStandalone,
                                 ExternalInstallSource::kSystemInstalled);
  options.only_use_app_info_factory = true;
  options.app_info_factory = base::BindLambdaForTesting([&kWebAppUrl]() {
    auto info = std::make_unique<WebApplicationInfo>();
    info->start_url = kWebAppUrl;
    info->scope = kWebAppUrl.GetWithoutFilename();
    info->title = u"Foo Web App";
    return info;
  });

  ExternallyManagedAppInstallTask task(
      profile(), /*url_loader=*/nullptr, registrar(), os_integration_manager(),
      ui_manager(), finalizer(), install_manager(), std::move(options));

  finalizer()->SetNextFinalizeInstallResult(
      kWebAppUrl, InstallResultCode::kSuccessNewInstall);

  base::RunLoop run_loop;
  task.Install(
      /*web_contents=*/nullptr,
      base::BindLambdaForTesting([&](ExternallyManagedAppManager::InstallResult
                                         result) {
        absl::optional<AppId> id =
            ExternallyInstalledWebAppPrefs(profile()->GetPrefs())
                .LookupAppId(kWebAppUrl);
        EXPECT_EQ(InstallResultCode::kSuccessOfflineOnlyInstall, result.code);
        EXPECT_TRUE(result.app_id.has_value());

        EXPECT_FALSE(IsPlaceholderApp(profile(), kWebAppUrl));

        EXPECT_EQ(result.app_id.value(), id.value());

        // Installing with an App Info doesn't call into OS Integration Manager.
        // This might be an issue for default apps.
        EXPECT_FALSE(
            os_integration_manager()->get_last_install_options().has_value());

        EXPECT_EQ(0u, finalizer()->num_reparent_tab_calls());

        EXPECT_EQ(web_app_info().user_display_mode, DisplayMode::kStandalone);
        EXPECT_EQ(webapps::WebappInstallSource::SYSTEM_DEFAULT,
                  finalize_options().install_source);

        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ExternallyManagedAppInstallTaskTest, InstallWithWebAppInfoFails) {
  const GURL kWebAppUrl("https://foo.example");
  ExternalInstallOptions options(kWebAppUrl, DisplayMode::kStandalone,
                                 ExternalInstallSource::kSystemInstalled);
  options.only_use_app_info_factory = true;
  options.app_info_factory = base::BindLambdaForTesting([&kWebAppUrl]() {
    auto info = std::make_unique<WebApplicationInfo>();
    info->start_url = kWebAppUrl;
    info->scope = kWebAppUrl.GetWithoutFilename();
    info->title = u"Foo Web App";
    return info;
  });

  ExternallyManagedAppInstallTask task(
      profile(), /*url_loader=*/nullptr, registrar(), os_integration_manager(),
      ui_manager(), finalizer(), install_manager(), std::move(options));

  finalizer()->SetNextFinalizeInstallResult(
      kWebAppUrl, InstallResultCode::kWriteDataFailed);

  base::RunLoop run_loop;

  task.Install(web_contents(),
               base::BindLambdaForTesting(
                   [&](ExternallyManagedAppManager::InstallResult result) {
                     absl::optional<AppId> id =
                         ExternallyInstalledWebAppPrefs(profile()->GetPrefs())
                             .LookupAppId(kWebAppUrl);

                     EXPECT_EQ(InstallResultCode::kWriteDataFailed,
                               result.code);
                     EXPECT_FALSE(result.app_id.has_value());

                     EXPECT_FALSE(id.has_value());

                     run_loop.Quit();
                   }));

  run_loop.Run();
}

TEST_F(ExternallyManagedAppInstallTaskWithRunOnOsLoginTest,
       InstallRunOnOsLogin) {
  const GURL kWebAppUrl("https://foo.example");
  ExternalInstallOptions install_options(
      kWebAppUrl, DisplayMode::kStandalone,
      ExternalInstallSource::kInternalDefault);
  install_options.run_on_os_login = true;

  auto task = GetInstallationTaskWithTestMocks(std::move(install_options));
  url_loader().SetPrepareForLoadResultLoaded();
  url_loader().SetNextLoadUrlResult(kWebAppUrl,
                                    WebAppUrlLoader::Result::kUrlLoaded);

  base::RunLoop run_loop;

  task->Install(
      web_contents(),
      base::BindLambdaForTesting([&](ExternallyManagedAppManager::InstallResult
                                         result) {
        EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
        EXPECT_TRUE(result.app_id.has_value());

        EXPECT_EQ(1u, os_integration_manager()->num_create_shortcuts_calls());
        EXPECT_TRUE(os_integration_manager()->did_add_to_desktop().value());

        EXPECT_EQ(
            1u, os_integration_manager()->num_register_run_on_os_login_calls());

        EXPECT_EQ(
            1u,
            os_integration_manager()->num_add_app_to_quick_launch_bar_calls());
        EXPECT_EQ(0u, finalizer()->num_reparent_tab_calls());

        run_loop.Quit();
      }));

  run_loop.Run();
}

TEST_F(ExternallyManagedAppInstallTaskWithRunOnOsLoginTest,
       InstallNoRunOnOsLogin) {
  const GURL kWebAppUrl("https://foo.example");
  ExternalInstallOptions install_options(
      kWebAppUrl, DisplayMode::kStandalone,
      ExternalInstallSource::kInternalDefault);
  install_options.run_on_os_login = false;
  auto task = GetInstallationTaskWithTestMocks(std::move(install_options));
  url_loader().SetPrepareForLoadResultLoaded();
  url_loader().SetNextLoadUrlResult(kWebAppUrl,
                                    WebAppUrlLoader::Result::kUrlLoaded);

  base::RunLoop run_loop;

  task->Install(
      web_contents(),
      base::BindLambdaForTesting([&](ExternallyManagedAppManager::InstallResult
                                         result) {
        EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
        EXPECT_TRUE(result.app_id.has_value());

        EXPECT_EQ(1u, os_integration_manager()->num_create_shortcuts_calls());
        EXPECT_TRUE(os_integration_manager()->did_add_to_desktop().value());

        EXPECT_EQ(
            0u, os_integration_manager()->num_register_run_on_os_login_calls());

        EXPECT_EQ(
            1u,
            os_integration_manager()->num_add_app_to_quick_launch_bar_calls());
        EXPECT_EQ(0u, finalizer()->num_reparent_tab_calls());

        run_loop.Quit();
      }));

  run_loop.Run();
}

}  // namespace web_app
