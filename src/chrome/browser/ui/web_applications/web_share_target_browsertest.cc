// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>
#include <string>
#include <vector>

#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/services/app_service/public/cpp/share_target.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "ui/display/types/display_constants.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/chromeos/fileapi/recent_file.h"
#include "chrome/browser/chromeos/fileapi/recent_model.h"
#include "chrome/browser/sharesheet/sharesheet_service.h"
#include "storage/browser/file_system/file_system_context.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/app_service_types.mojom.h"
#include "chromeos/crosapi/mojom/sharesheet.mojom.h"
#include "chromeos/crosapi/mojom/sharesheet_mojom_traits.h"
#include "chromeos/lacros/lacros_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace {

// TODO(crbug.com/1225825): Support file sharing from Lacros.
#if BUILDFLAG(IS_CHROMEOS_ASH)
base::FilePath PrepareWebShareDirectory(Profile* profile) {
  constexpr base::FilePath::CharType kWebShareDirname[] =
      FILE_PATH_LITERAL(".WebShare");
  const base::FilePath directory =
      file_manager::util::GetMyFilesFolderForProfile(profile).Append(
          kWebShareDirname);

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::File::Error result = base::File::FILE_OK;
  EXPECT_TRUE(base::CreateDirectoryAndGetError(directory, &result));
  return directory;
}

void RemoveWebShareDirectory(const base::FilePath& directory) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_TRUE(base::DeletePathRecursively(directory));
}

base::FilePath StoreSharedFile(const base::FilePath& directory,
                               const base::StringPiece& name,
                               const base::StringPiece& content) {
  const base::FilePath path = directory.Append(name);
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::File file(path,
                  base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  EXPECT_EQ(file.WriteAtCurrentPos(content.begin(), content.size()),
            static_cast<int>(content.size()));
  return path;
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

content::WebContents* LaunchWebAppWithIntent(Profile* profile,
                                             const web_app::AppId& app_id,
                                             apps::mojom::IntentPtr&& intent) {
  apps::AppLaunchParams params = apps::CreateAppLaunchParamsForIntent(
      app_id,
      /*event_flags=*/0, apps::mojom::LaunchSource::kFromSharesheet,
      display::kDefaultDisplayId,
      apps::mojom::LaunchContainer::kLaunchContainerWindow, std::move(intent),
      profile);

  content::WebContents* const web_contents =
      apps::AppServiceProxyFactory::GetForProfile(profile)
          ->BrowserAppLauncher()
          ->LaunchAppWithParamsForTesting(std::move(params));
  DCHECK(web_contents);
  return web_contents;
}

content::EvalJsResult ReadTextContent(content::WebContents* web_contents,
                                      const char* id) {
  const std::string script =
      base::StringPrintf("document.getElementById('%s').textContent", id);
  return content::EvalJs(web_contents, script);
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
class FakeSharesheet : public crosapi::mojom::Sharesheet {
 public:
  FakeSharesheet() = default;
  FakeSharesheet(const FakeSharesheet&) = delete;
  FakeSharesheet& operator=(const FakeSharesheet&) = delete;
  ~FakeSharesheet() override = default;

  void set_profile(Profile* profile) { profile_ = profile; }

  void set_selected_app_id(const web_app::AppId& app_id) {
    selected_app_id_ = app_id;
  }

 private:
  // crosapi::mojom::Sharesheet:
  void ShowBubble(
      const std::string& window_id,
      sharesheet::LaunchSource source,
      crosapi::mojom::IntentPtr intent,
      crosapi::mojom::Sharesheet::ShowBubbleCallback callback) override {
    LaunchWebAppWithIntent(
        profile_, selected_app_id_,
        apps_util::ConvertCrosapiToAppServiceIntent(intent, profile_));
  }
  void ShowBubbleWithOnClosed(
      const std::string& window_id,
      sharesheet::LaunchSource source,
      crosapi::mojom::IntentPtr intent,
      crosapi::mojom::Sharesheet::ShowBubbleWithOnClosedCallback callback)
      override {}
  void CloseBubble(const std::string& window_id) override {}

  Profile* profile_ = nullptr;
  web_app::AppId selected_app_id_;
};
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace

namespace web_app {

class WebShareTargetBrowserTest : public WebAppControllerBrowserTest {
 public:
  GURL share_target_url() const {
    return embedded_test_server()->GetURL("/web_share_target/share.html");
  }

  content::WebContents* LaunchAppWithIntent(const AppId& app_id,
                                            apps::mojom::IntentPtr&& intent,
                                            const GURL& expected_url) {
    ui_test_utils::UrlLoadObserver url_observer(
        expected_url, content::NotificationService::AllSources());

    content::WebContents* const web_contents =
        LaunchWebAppWithIntent(profile(), app_id, std::move(intent));
    url_observer.Wait();
    EXPECT_EQ(expected_url, web_contents->GetVisibleURL());
    return web_contents;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  unsigned NumRecentFiles(content::WebContents* contents) {
    unsigned result = std::numeric_limits<unsigned>::max();
    base::RunLoop run_loop;

    const scoped_refptr<storage::FileSystemContext> file_system_context =
        file_manager::util::GetFileSystemContextForRenderFrameHost(
            profile(), contents->GetMainFrame());
    chromeos::RecentModel::GetForProfile(profile())->GetRecentFiles(
        file_system_context.get(),
        /*origin=*/GURL(),
        /*file_type=*/chromeos::RecentModel::FileType::kAll,
        base::BindLambdaForTesting(
            [&result,
             &run_loop](const std::vector<chromeos::RecentFile>& files) {
              result = files.size();
              run_loop.Quit();
            }));

    run_loop.Run();
    return result;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Lacros tests may be run with an old version of ash-chrome where the lacros
  // service or the sharesheet interface are not available.
  bool IsServiceAvailable() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    auto* const service = chromeos::LacrosService::Get();
    return service && service->IsAvailable<crosapi::mojom::Sharesheet>();
#else
    return true;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  void SetUpOnMainThread() override {
    WebAppControllerBrowserTest::SetUpOnMainThread();

    // If the lacros service or the sharesheet interface are not
    // available on this version of ash-chrome, this test suite will no-op.
    if (!IsServiceAvailable())
      return;

    // Replace the production sharesheet with a fake for testing.
    mojo::Remote<crosapi::mojom::Sharesheet>& remote =
        chromeos::LacrosService::Get()->GetRemote<crosapi::mojom::Sharesheet>();
    remote.reset();
    service_.set_profile(profile());
    receiver_.Bind(remote.BindNewPipeAndPassReceiver());
  }

 private:
  FakeSharesheet service_;
  mojo::Receiver<crosapi::mojom::Sharesheet> receiver_{&service_};
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
};

// TODO(crbug.com/1225825): Support file sharing from Lacros.
#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(WebShareTargetBrowserTest, ShareTextFiles) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url =
      embedded_test_server()->GetURL("/web_share_target/charts.html");
  const AppId app_id = web_app::InstallWebAppFromManifest(browser(), app_url);
  const base::FilePath directory = PrepareWebShareDirectory(profile());

  apps::mojom::IntentPtr intent;
  {
    const base::FilePath first_csv =
        StoreSharedFile(directory, "first.csv", "1,2,3,4,5");
    const base::FilePath second_csv =
        StoreSharedFile(directory, "second.csv", "6,7,8,9,0");

    std::vector<base::FilePath> file_paths({first_csv, second_csv});
    std::vector<std::string> content_types(2, "text/csv");
    intent = apps_util::CreateShareIntentFromFiles(
        profile(), std::move(file_paths), std::move(content_types));
  }

  content::WebContents* const web_contents =
      LaunchAppWithIntent(app_id, std::move(intent), share_target_url());
  EXPECT_EQ("1,2,3,4,5 6,7,8,9,0", ReadTextContent(web_contents, "records"));
  EXPECT_EQ(NumRecentFiles(web_contents), 0U);

  RemoveWebShareDirectory(directory);
}

IN_PROC_BROWSER_TEST_F(WebShareTargetBrowserTest, ShareImageWithText) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url =
      embedded_test_server()->GetURL("/web_share_target/charts.html");
  const AppId app_id = web_app::InstallWebAppFromManifest(browser(), app_url);
  const base::FilePath directory = PrepareWebShareDirectory(profile());

  apps::mojom::IntentPtr intent;
  {
    const base::FilePath first_svg =
        StoreSharedFile(directory, "first.svg", "picture");

    std::vector<base::FilePath> file_paths({first_svg});
    std::vector<std::string> content_types(1, "image/svg+xml");
    intent = apps_util::CreateShareIntentFromFiles(
        profile(), std::move(file_paths), std::move(content_types),
        /*share_text=*/"Euclid https://example.org/",
        /*share_title=*/"Elements");
  }

  content::WebContents* const web_contents =
      LaunchAppWithIntent(app_id, std::move(intent), share_target_url());
  EXPECT_EQ("picture", ReadTextContent(web_contents, "graphs"));

  EXPECT_EQ("Elements", ReadTextContent(web_contents, "headline"));
  EXPECT_EQ("Euclid", ReadTextContent(web_contents, "author"));
  EXPECT_EQ("https://example.org/", ReadTextContent(web_contents, "link"));
  EXPECT_EQ(NumRecentFiles(web_contents), 0U);

  RemoveWebShareDirectory(directory);
}

IN_PROC_BROWSER_TEST_F(WebShareTargetBrowserTest, ShareAudio) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url =
      embedded_test_server()->GetURL("/web_share_target/charts.html");
  const AppId app_id = web_app::InstallWebAppFromManifest(browser(), app_url);
  const base::FilePath directory = PrepareWebShareDirectory(profile());

  apps::mojom::IntentPtr intent;
  {
    const base::FilePath first_weba =
        StoreSharedFile(directory, "first.weba", "a");
    const base::FilePath second_weba =
        StoreSharedFile(directory, "second.weba", "b");
    const base::FilePath third_weba =
        StoreSharedFile(directory, "third.weba", "c");

    std::vector<base::FilePath> file_paths(
        {first_weba, second_weba, third_weba});
    std::vector<std::string> content_types(3, "audio/webm");
    intent = apps_util::CreateShareIntentFromFiles(
        profile(), std::move(file_paths), std::move(content_types));
    intent->share_text = "";
    intent->share_title = "";
  }

  content::WebContents* const web_contents =
      LaunchAppWithIntent(app_id, std::move(intent), share_target_url());
  EXPECT_EQ("a b c", ReadTextContent(web_contents, "notes"));
  EXPECT_EQ(NumRecentFiles(web_contents), 0U);

  RemoveWebShareDirectory(directory);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

IN_PROC_BROWSER_TEST_F(WebShareTargetBrowserTest, PostBlank) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url =
      embedded_test_server()->GetURL("/web_share_target/poster.html");
  const AppId app_id = web_app::InstallWebAppFromManifest(browser(), app_url);

  apps::mojom::IntentPtr intent = apps_util::CreateShareIntentFromText(
      /*share_text=*/std::string(),
      /*share_title=*/std::string());

  content::WebContents* const web_contents =
      LaunchAppWithIntent(app_id, std::move(intent), share_target_url());
  // Poster web app's service worker detects omitted values.
  EXPECT_EQ("N/A", ReadTextContent(web_contents, "headline"));
  EXPECT_EQ("N/A", ReadTextContent(web_contents, "author"));
  EXPECT_EQ("N/A", ReadTextContent(web_contents, "link"));
}

IN_PROC_BROWSER_TEST_F(WebShareTargetBrowserTest, PostLink) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url =
      embedded_test_server()->GetURL("/web_share_target/poster.html");
  const AppId app_id = web_app::InstallWebAppFromManifest(browser(), app_url);
  const apps::ShareTarget* share_target =
      WebAppProvider::GetForTest(browser()->profile())
          ->registrar()
          .GetAppShareTarget(app_id);
  EXPECT_EQ(share_target->method, apps::ShareTarget::Method::kPost);
  EXPECT_EQ(share_target->enctype, apps::ShareTarget::Enctype::kFormUrlEncoded);

  const std::string shared_title = "Hyperlink";
  const std::string shared_link = "https://example.org/a?b=c&d=e%20#f";

  apps::mojom::IntentPtr intent = apps_util::CreateShareIntentFromText(
      /*share_text=*/shared_link,
      /*share_title=*/shared_title);

  content::WebContents* const web_contents =
      LaunchAppWithIntent(app_id, std::move(intent), share_target_url());
  EXPECT_EQ("POST", ReadTextContent(web_contents, "method"));
  EXPECT_EQ("application/x-www-form-urlencoded",
            ReadTextContent(web_contents, "type"));

  EXPECT_EQ(shared_title, ReadTextContent(web_contents, "headline"));
  // Poster web app's service worker detects omitted value.
  EXPECT_EQ("N/A", ReadTextContent(web_contents, "author"));
  EXPECT_EQ(shared_link, ReadTextContent(web_contents, "link"));
}

IN_PROC_BROWSER_TEST_F(WebShareTargetBrowserTest, GetLink) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url =
      embedded_test_server()->GetURL("/web_share_target/gatherer.html");
  const AppId app_id = web_app::InstallWebAppFromManifest(browser(), app_url);
  const apps::ShareTarget* share_target =
      WebAppProvider::GetForTest(browser()->profile())
          ->registrar()
          .GetAppShareTarget(app_id);
  EXPECT_EQ(share_target->method, apps::ShareTarget::Method::kGet);
  EXPECT_EQ(share_target->enctype, apps::ShareTarget::Enctype::kFormUrlEncoded);

  const std::string shared_title = "My News";
  const std::string shared_link = "http://example.com/news";
  const GURL expected_url(share_target_url().spec() +
                          "?headline=My+News&link=http://example.com/news");

  apps::mojom::IntentPtr intent = apps_util::CreateShareIntentFromText(
      /*share_text=*/shared_link,
      /*share_title=*/shared_title);

  content::WebContents* const web_contents =
      LaunchAppWithIntent(app_id, std::move(intent), expected_url);
  EXPECT_EQ("GET", ReadTextContent(web_contents, "method"));
  EXPECT_EQ(expected_url.spec(), ReadTextContent(web_contents, "url"));

  EXPECT_EQ(shared_title, ReadTextContent(web_contents, "headline"));
  // Gatherer web app's service worker detects omitted value.
  EXPECT_EQ("N/A", ReadTextContent(web_contents, "author"));
  EXPECT_EQ(shared_link, ReadTextContent(web_contents, "link"));
}

}  // namespace web_app
