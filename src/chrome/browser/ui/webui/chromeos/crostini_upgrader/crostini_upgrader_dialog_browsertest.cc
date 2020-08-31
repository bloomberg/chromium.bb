// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/crostini_upgrader/crostini_upgrader_dialog.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_base.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_test_helper.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/guest_os/guest_os_registry_service.h"
#include "chrome/browser/chromeos/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/crostini/crostini_browser_test_util.h"
#include "chrome/browser/ui/webui/chromeos/crostini_upgrader/crostini_upgrader.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/dbus/cicerone/cicerone_service.pb.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)

constexpr char kDesktopFileId[] = "test_app";
constexpr int kDisplayId = 0;

class CrostiniUpgraderDialogBrowserTest : public CrostiniDialogBrowserTest {
 public:
  CrostiniUpgraderDialogBrowserTest()
      : CrostiniDialogBrowserTest(true /*register_termina*/),
        app_id_(crostini::CrostiniTestHelper::GenerateAppId(kDesktopFileId)) {}

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    chromeos::CrostiniUpgraderDialog::Show(base::DoNothing(), false);
  }

  chromeos::CrostiniUpgraderDialog* GetCrostiniUpgraderDialog() {
    auto url = GURL{chrome::kChromeUICrostiniUpgraderUrl};
    return reinterpret_cast<chromeos::CrostiniUpgraderDialog*>(
        chromeos::SystemWebDialogDelegate::FindInstance(url.spec()));
  }

  void SafelyCloseDialog() {
    auto* upgrader_dialog = GetCrostiniUpgraderDialog();
    // Make sure the WebUI has launches sufficiently. Closing immediately would
    // miss breakages in the underlying plumbing.
    auto* web_contents = upgrader_dialog->GetWebUIForTest()->GetWebContents();
    WaitForLoadFinished(web_contents);

    // Now there should be enough WebUI hooked up to close properly.
    base::RunLoop run_loop;
    upgrader_dialog->SetDeletionClosureForTesting(run_loop.QuitClosure());
    upgrader_dialog->Close();
    run_loop.Run();
  }

  void ExpectDialog() {
    // A new Widget was created in ShowUi() or since the last VerifyUi().
    EXPECT_TRUE(crostini_manager()->GetCrostiniDialogStatus(
        crostini::DialogType::UPGRADER));

    EXPECT_NE(nullptr, GetCrostiniUpgraderDialog());
  }

  void ExpectNoDialog() {
    // No new Widget was created in ShowUi() or since the last VerifyUi().
    EXPECT_FALSE(crostini_manager()->GetCrostiniDialogStatus(
        crostini::DialogType::UPGRADER));
    // Our dialog has really been deleted.
    EXPECT_EQ(nullptr, GetCrostiniUpgraderDialog());
  }

  void RegisterApp() {
    vm_tools::apps::ApplicationList app_list =
        crostini::CrostiniTestHelper::BasicAppList(
            kDesktopFileId, crostini::kCrostiniDefaultVmName,
            crostini::kCrostiniDefaultContainerName);
    guest_os::GuestOsRegistryServiceFactory::GetForProfile(browser()->profile())
        ->UpdateApplicationList(app_list);
  }

  void DowngradeOSRelease() {
    vm_tools::cicerone::OsRelease os_release;
    os_release.set_id("debian");
    os_release.set_version_id("9");
    auto container_id = crostini::DefaultContainerId();
    crostini_manager()->SetContainerOsRelease(
        container_id.vm_name, container_id.container_name, os_release);
  }

  const std::string& app_id() const { return app_id_; }

  crostini::CrostiniManager* crostini_manager() {
    return crostini::CrostiniManager::GetForProfile(browser()->profile());
  }

 private:
  std::string app_id_;

  DISALLOW_COPY_AND_ASSIGN(CrostiniUpgraderDialogBrowserTest);
};

IN_PROC_BROWSER_TEST_F(CrostiniUpgraderDialogBrowserTest,
                       NoDialogOnNormalStartup) {
  base::HistogramTester histogram_tester;
  RegisterApp();

  crostini::LaunchCrostiniApp(browser()->profile(), app_id(), kDisplayId);
  ExpectNoDialog();
}

IN_PROC_BROWSER_TEST_F(CrostiniUpgraderDialogBrowserTest, ShowsOnAppLaunch) {
  base::HistogramTester histogram_tester;

  DowngradeOSRelease();
  RegisterApp();

  crostini::LaunchCrostiniApp(browser()->profile(), app_id(), kDisplayId);
  ExpectDialog();

  SafelyCloseDialog();
  ExpectNoDialog();

  // Once only - second time we launch an app, the dialog should not appear.
  crostini::LaunchCrostiniApp(browser()->profile(), app_id(), kDisplayId);
  ExpectNoDialog();

  histogram_tester.ExpectUniqueSample(
      crostini::kUpgradeDialogEventHistogram,
      static_cast<base::HistogramBase::Sample>(
          crostini::UpgradeDialogEvent::kDialogShown),
      1);
}

#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)
