// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/bookmark_app_install_finalizer.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/common/web_application_info.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/install/crx_install_error.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest.h"

namespace extensions {

namespace {

const char kWebAppUrl[] = "https://foo.example";
const char kWebAppTitle[] = "Foo Title";

}  // namespace

class BookmarkAppInstallFinalizerTest : public ChromeRenderViewHostTestHarness {
 public:
  // Subclass that runs a closure when an extension is unpacked successfully.
  // Useful for tests that want to trigger their own succeess/failure events.
  class FakeCrxInstaller : public CrxInstaller {
   public:
    explicit FakeCrxInstaller(Profile* profile)
        : CrxInstaller(
              ExtensionSystem::Get(profile)->extension_service()->AsWeakPtr(),
              nullptr,
              nullptr) {}

    void OnUnpackSuccess(
        const base::FilePath& temp_dir,
        const base::FilePath& extension_dir,
        std::unique_ptr<base::DictionaryValue> original_manifest,
        const Extension* extension,
        const SkBitmap& install_icon,
        const base::Optional<int>& dnr_ruleset_checksum) override {
      run_loop_.QuitClosure().Run();
    }

    void WaitForInstallToTrigger() { run_loop_.Run(); }

    void SimulateInstallFailed() {
      CrxInstallError error(CrxInstallErrorType::DECLINED,
                            CrxInstallErrorDetail::INSTALL_NOT_ENABLED,
                            base::ASCIIToUTF16(""));
      NotifyCrxInstallComplete(error);
    }

   private:
    ~FakeCrxInstaller() override = default;

    base::RunLoop run_loop_;

    DISALLOW_COPY_AND_ASSIGN(FakeCrxInstaller);
  };

  BookmarkAppInstallFinalizerTest() = default;
  ~BookmarkAppInstallFinalizerTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    // CrxInstaller in BookmarkAppInstallFinalizer needs an ExtensionService, so
    // create one for the profile.
    TestExtensionSystem* test_system =
        static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile()));
    test_system->CreateExtensionService(base::CommandLine::ForCurrentProcess(),
                                        profile()->GetPath(),
                                        false /* autoupdate_enabled */);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BookmarkAppInstallFinalizerTest);
};

TEST_F(BookmarkAppInstallFinalizerTest, BasicInstallSucceeds) {
  BookmarkAppInstallFinalizer installer(profile());

  auto info = std::make_unique<WebApplicationInfo>();
  info->app_url = GURL(kWebAppUrl);
  info->title = base::ASCIIToUTF16(kWebAppTitle);

  base::RunLoop run_loop;
  web_app::InstallFinalizer::FinalizeOptions options;
  web_app::AppId app_id;
  bool callback_called = false;

  installer.FinalizeInstall(
      *info, options,
      base::BindLambdaForTesting([&](const web_app::AppId& installed_app_id,
                                     web_app::InstallResultCode code) {
        EXPECT_EQ(web_app::InstallResultCode::kSuccess, code);
        app_id = installed_app_id;
        callback_called = true;
        run_loop.Quit();
      }));
  run_loop.Run();

  EXPECT_TRUE(callback_called);
}

TEST_F(BookmarkAppInstallFinalizerTest, BasicInstallFails) {
  BookmarkAppInstallFinalizer installer(profile());

  auto fake_crx_installer =
      base::MakeRefCounted<BookmarkAppInstallFinalizerTest::FakeCrxInstaller>(
          profile());

  installer.SetCrxInstallerFactoryForTesting(
      base::BindLambdaForTesting([&](Profile* profile) {
        scoped_refptr<CrxInstaller> crx_installer = fake_crx_installer;
        return crx_installer;
      }));

  auto info = std::make_unique<WebApplicationInfo>();
  info->app_url = GURL(kWebAppUrl);
  info->title = base::ASCIIToUTF16(kWebAppTitle);

  base::RunLoop run_loop;
  web_app::InstallFinalizer::FinalizeOptions options;
  bool callback_called = false;

  installer.FinalizeInstall(
      *info, options,
      base::BindLambdaForTesting([&](const web_app::AppId& installed_app_id,
                                     web_app::InstallResultCode code) {
        EXPECT_EQ(web_app::InstallResultCode::kFailedUnknownReason, code);
        EXPECT_TRUE(installed_app_id.empty());
        callback_called = true;
        run_loop.Quit();
      }));

  fake_crx_installer->WaitForInstallToTrigger();
  fake_crx_installer->SimulateInstallFailed();
  run_loop.Run();

  EXPECT_TRUE(callback_called);
}

TEST_F(BookmarkAppInstallFinalizerTest, ConcurrentInstallSucceeds) {
  BookmarkAppInstallFinalizer finalizer(profile());

  base::RunLoop run_loop;

  const GURL url1("https://foo1.example");
  const GURL url2("https://foo2.example");

  bool callback1_called = false;
  bool callback2_called = false;
  web_app::InstallFinalizer::FinalizeOptions options;

  // Start install finalization for the 1st app
  {
    WebApplicationInfo web_application_info;
    web_application_info.app_url = url1;

    finalizer.FinalizeInstall(
        web_application_info, options,
        base::BindLambdaForTesting([&](const web_app::AppId& installed_app_id,
                                       web_app::InstallResultCode code) {
          EXPECT_EQ(web_app::InstallResultCode::kSuccess, code);
          EXPECT_EQ(installed_app_id, web_app::GenerateAppIdFromURL(url1));
          callback1_called = true;
          if (callback2_called)
            run_loop.Quit();
        }));
  }

  // Start install finalization for the 2nd app
  {
    WebApplicationInfo web_application_info;
    web_application_info.app_url = url2;

    finalizer.FinalizeInstall(
        web_application_info, options,
        base::BindLambdaForTesting([&](const web_app::AppId& installed_app_id,
                                       web_app::InstallResultCode code) {
          EXPECT_EQ(web_app::InstallResultCode::kSuccess, code);
          EXPECT_EQ(installed_app_id, web_app::GenerateAppIdFromURL(url2));
          callback2_called = true;
          if (callback1_called)
            run_loop.Quit();
        }));
  }

  run_loop.Run();

  EXPECT_TRUE(callback1_called);
  EXPECT_TRUE(callback2_called);
}

TEST_F(BookmarkAppInstallFinalizerTest, PolicyInstallSucceeds) {
  BookmarkAppInstallFinalizer installer(profile());

  auto info = std::make_unique<WebApplicationInfo>();
  info->app_url = GURL(kWebAppUrl);
  info->title = base::ASCIIToUTF16(kWebAppTitle);

  web_app::InstallFinalizer::FinalizeOptions options;
  options.policy_installed = true;

  base::RunLoop run_loop;
  installer.FinalizeInstall(
      *info, options,
      base::BindLambdaForTesting([&](const web_app::AppId& installed_app_id,
                                     web_app::InstallResultCode code) {
        EXPECT_EQ(web_app::InstallResultCode::kSuccess, code);

        auto* extension =
            ExtensionRegistry::Get(profile())->GetInstalledExtension(
                installed_app_id);
        EXPECT_TRUE(Manifest::IsPolicyLocation(extension->location()));

        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(BookmarkAppInstallFinalizerTest, NoNetworkInstallSucceeds) {
  BookmarkAppInstallFinalizer installer(profile());

  auto info = std::make_unique<WebApplicationInfo>();
  info->app_url = GURL(kWebAppUrl);

  web_app::InstallFinalizer::FinalizeOptions options;
  options.no_network_install = true;

  base::RunLoop run_loop;
  installer.FinalizeInstall(
      *info, options,
      base::BindLambdaForTesting([&](const web_app::AppId& installed_app_id,
                                     web_app::InstallResultCode code) {
        EXPECT_EQ(web_app::InstallResultCode::kSuccess, code);

        auto* extension =
            ExtensionRegistry::Get(profile())->GetInstalledExtension(
                installed_app_id);
        EXPECT_TRUE(Manifest::IsExternalLocation(extension->location()));
        EXPECT_EQ(Manifest::EXTERNAL_PREF_DOWNLOAD, extension->location());

        run_loop.Quit();
      }));
  run_loop.Run();
}

}  // namespace extensions
