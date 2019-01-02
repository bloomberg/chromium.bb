// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/bookmark_app_installer.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/common/web_application_info.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "extensions/browser/install/crx_install_error.h"
#include "extensions/common/extension_id.h"

namespace extensions {

namespace {

const char kWebAppUrl[] = "https://foo.example";
const char kWebAppTitle[] = "Foo Title";

}  // namespace

class BookmarkAppInstallerTest : public ChromeRenderViewHostTestHarness {
 public:
  // Subclass that runs a closure when an extension is unpacked successfully.
  // Useful for tests that want to trigger their own succeess/failure events.
  class FakeCrxInstaller : public CrxInstaller {
   public:
    explicit FakeCrxInstaller(Profile* profile)
        : CrxInstaller(
              ExtensionSystem::Get(profile)->extension_service()->AsWeakPtr(),
              nullptr,
              nullptr) {
    }

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

  BookmarkAppInstallerTest() = default;
  ~BookmarkAppInstallerTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    // CrxInstaller in BookmarkAppInstaller needs an ExtensionService, so
    // create one for the profile.
    TestExtensionSystem* test_system =
        static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile()));
    test_system->CreateExtensionService(base::CommandLine::ForCurrentProcess(),
                                        profile()->GetPath(),
                                        false /* autoupdate_enabled */);
  }

  void InstallCallback(base::OnceClosure quit_closure,
                       const ExtensionId& extension_id) {
    app_installed_ = !extension_id.empty();
    std::move(quit_closure).Run();
  }

  bool app_installed() { return app_installed_.value(); }
  bool install_callback_called() { return app_installed_.has_value(); }

 private:
  base::Optional<bool> app_installed_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkAppInstallerTest);
};

TEST_F(BookmarkAppInstallerTest, BasicInstallSucceeds) {
  BookmarkAppInstaller installer(profile());

  WebApplicationInfo info;
  info.app_url = GURL(kWebAppUrl);
  info.title = base::ASCIIToUTF16(kWebAppTitle);

  base::RunLoop run_loop;
  installer.Install(
      info, base::BindOnce(&BookmarkAppInstallerTest::InstallCallback,
                           base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(app_installed());
}

TEST_F(BookmarkAppInstallerTest, BasicInstallFails) {
  BookmarkAppInstaller installer(profile());
  auto fake_crx_installer =
      base::MakeRefCounted<BookmarkAppInstallerTest::FakeCrxInstaller>(
          profile());
  installer.SetCrxInstallerForTesting(fake_crx_installer);

  base::RunLoop run_loop;

  WebApplicationInfo info;
  info.app_url = GURL(kWebAppUrl);
  info.title = base::ASCIIToUTF16(kWebAppTitle);
  installer.Install(
      info, base::BindOnce(&BookmarkAppInstallerTest::InstallCallback,
                           base::Unretained(this), run_loop.QuitClosure()));

  fake_crx_installer->WaitForInstallToTrigger();
  fake_crx_installer->SimulateInstallFailed();
  run_loop.Run();

  EXPECT_FALSE(app_installed());
}

}  // namespace extensions
