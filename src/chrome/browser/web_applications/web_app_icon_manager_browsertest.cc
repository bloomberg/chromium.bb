// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/web_applications/web_app_browser_controller.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_application_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/image_model_utils.h"
#include "url/gurl.h"

namespace web_app {

class WebAppIconManagerBrowserTest : public InProcessBrowserTest {
 public:
  WebAppIconManagerBrowserTest() = default;
  WebAppIconManagerBrowserTest(const WebAppIconManagerBrowserTest&) = delete;
  WebAppIconManagerBrowserTest& operator=(const WebAppIconManagerBrowserTest&) =
      delete;

  ~WebAppIconManagerBrowserTest() override = default;

 protected:
  net::EmbeddedTestServer* https_server() { return &https_server_; }

  void SetUpOnMainThread() override {
    Profile* profile = browser()->profile();
    app_service_test_.SetUp(profile);
    web_app::test::WaitUntilReady(web_app::WebAppProvider::GetForTest(profile));
  }

  // InProcessBrowserTest:
  void SetUp() override {
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    InProcessBrowserTest::SetUp();
  }

  apps::AppServiceTest& app_service_test() { return app_service_test_; }

 private:
  net::EmbeddedTestServer https_server_;
  apps::AppServiceTest app_service_test_;

};

IN_PROC_BROWSER_TEST_F(WebAppIconManagerBrowserTest, SingleIcon) {
  ASSERT_TRUE(https_server()->Start());
  const GURL start_url =
      https_server()->GetURL("/banners/manifest_test_page.html");

  AppId app_id;
  {
    std::unique_ptr<WebApplicationInfo> web_application_info =
        std::make_unique<WebApplicationInfo>();
    web_application_info->start_url = start_url;
    web_application_info->scope = start_url.GetWithoutFilename();
    web_application_info->title = u"App Name";
    web_application_info->user_display_mode = DisplayMode::kStandalone;

    {
      SkBitmap bitmap;
      bitmap.allocN32Pixels(icon_size::k32, icon_size::k32, true);
      bitmap.eraseColor(SK_ColorBLUE);
      web_application_info->icon_bitmaps.any[icon_size::k32] =
          std::move(bitmap);
    }

    WebAppInstallManager& install_manager =
        WebAppProvider::GetForTest(browser()->profile())->install_manager();

    base::RunLoop run_loop;
    install_manager.InstallWebAppFromInfo(
        std::move(web_application_info),
        /*overwrite_existing_manifest_fields=*/false, ForInstallableSite::kYes,
        webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
        base::BindLambdaForTesting(
            [&app_id, &run_loop](const AppId& installed_app_id,
                                 InstallResultCode code) {
              EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code);
              app_id = installed_app_id;
              run_loop.Quit();
            }));

    run_loop.Run();
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  gfx::ImageSkia image_skia;
  app_service_test().FlushMojoCalls();
  image_skia = app_service_test().LoadAppIconBlocking(
      apps::mojom::AppType::kWeb, app_id, kWebAppIconSmall);
#endif

  WebAppBrowserController* controller;
  {
    apps::AppLaunchParams params(
        app_id, apps::mojom::LaunchContainer::kLaunchContainerWindow,
        WindowOpenDisposition::NEW_WINDOW,
        apps::mojom::LaunchSource::kFromTest);
    content::WebContents* contents =
        apps::AppServiceProxyFactory::GetForProfile(browser()->profile())
            ->BrowserAppLauncher()
            ->LaunchAppWithParams(std::move(params));
    controller = chrome::FindBrowserWithWebContents(contents)
                     ->app_controller()
                     ->AsWebAppBrowserController();
  }

  base::RunLoop run_loop;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  controller->SetReadIconCallbackForTesting(
      base::BindLambdaForTesting([controller, &image_skia, &run_loop, this]() {
        EXPECT_TRUE(app_service_test().AreIconImageEqual(
            image_skia, views::GetImageSkiaFromImageModel(
                            controller->GetWindowAppIcon(), nullptr)));
        run_loop.Quit();
      }));
  run_loop.Run();
#else
  controller->SetReadIconCallbackForTesting(
      base::BindLambdaForTesting([controller, &run_loop]() {
        const SkBitmap* bitmap = views::GetImageSkiaFromImageModel(
                                     controller->GetWindowAppIcon(), nullptr)
                                     .bitmap();
        EXPECT_EQ(SK_ColorBLUE, bitmap->getColor(0, 0));
        EXPECT_EQ(32, bitmap->width());
        EXPECT_EQ(32, bitmap->height());
        run_loop.Quit();
      }));

  run_loop.Run();
#endif
}

}  // namespace web_app
