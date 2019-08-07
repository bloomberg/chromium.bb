// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_install_task.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/nullable_string16.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/installable/fake_installable_manager.h"
#include "chrome/browser/installable/installable_data.h"
#include "chrome/browser/installable/installable_manager.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_icon_generator.h"
#include "chrome/browser/web_applications/components/web_app_utils.h"
#include "chrome/browser/web_applications/test/test_data_retriever.h"
#include "chrome/browser/web_applications/test/test_file_utils.h"
#include "chrome/browser/web_applications/test/test_install_finalizer.h"
#include "chrome/browser/web_applications/test/test_web_app_database.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "url/gurl.h"

namespace web_app {

namespace {

bool ReadBitmap(FileUtilsWrapper* utils,
                const base::FilePath& file_path,
                SkBitmap* bitmap) {
  std::string icon_data;
  if (!utils->ReadFileToString(file_path, &icon_data))
    return false;

  return gfx::PNGCodec::Decode(
      reinterpret_cast<const unsigned char*>(icon_data.c_str()),
      icon_data.size(), bitmap);
}

constexpr int kIconSizes[] = {
    icon_size::k32, icon_size::k64,  icon_size::k48,
    icon_size::k96, icon_size::k128, icon_size::k256,
};

bool ContainsOneIconOfEachSize(const WebApplicationInfo& web_app_info) {
  for (int size_px : kIconSizes) {
    int num_icons_for_size =
        std::count_if(web_app_info.icons.begin(), web_app_info.icons.end(),
                      [&size_px](const WebApplicationInfo::IconInfo& icon) {
                        return icon.width == size_px && icon.height == size_px;
                      });
    if (num_icons_for_size != 1)
      return false;
  }

  return true;
}

void TestAcceptDialogCallback(
    content::WebContents* initiator_web_contents,
    std::unique_ptr<WebApplicationInfo> web_app_info,
    ForInstallableSite for_installable_site,
    InstallManager::WebAppInstallationAcceptanceCallback acceptance_callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(acceptance_callback), true /*accept*/,
                                std::move(web_app_info)));
}

void TestDeclineDialogCallback(
    content::WebContents* initiator_web_contents,
    std::unique_ptr<WebApplicationInfo> web_app_info,
    ForInstallableSite for_installable_site,
    InstallManager::WebAppInstallationAcceptanceCallback acceptance_callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(acceptance_callback),
                                false /*accept*/, std::move(web_app_info)));
}

}  // namespace

class WebAppInstallTaskTest : public WebAppTest {
 public:
  void SetUp() override {
    WebAppTest::SetUp();

    database_ = std::make_unique<TestWebAppDatabase>();
    registrar_ = std::make_unique<WebAppRegistrar>(database_.get());

    auto file_utils = std::make_unique<TestFileUtils>();
    file_utils_ = file_utils.get();

    icon_manager_ =
        std::make_unique<WebAppIconManager>(profile(), std::move(file_utils));

    install_finalizer_ = std::make_unique<WebAppInstallFinalizer>(
        registrar_.get(), icon_manager_.get());

    install_task_ = std::make_unique<WebAppInstallTask>(
        profile(), install_finalizer_.get());
  }

  void CreateTestDataRetriever() {
    auto data_retriever = std::make_unique<TestDataRetriever>();
    data_retriever_ = data_retriever.get();
    install_task_->SetDataRetrieverForTesting(std::move(data_retriever));
  }

  void CreateRendererAppInfo(const GURL& url,
                             const std::string name,
                             const std::string description,
                             const GURL& scope,
                             base::Optional<SkColor> theme_color) {
    CreateTestDataRetriever();

    auto web_app_info = std::make_unique<WebApplicationInfo>();

    web_app_info->app_url = url;
    web_app_info->title = base::UTF8ToUTF16(name);
    web_app_info->description = base::UTF8ToUTF16(description);
    web_app_info->scope = scope;
    web_app_info->theme_color = theme_color;
    web_app_info->open_as_window = true;

    data_retriever_->SetRendererWebApplicationInfo(std::move(web_app_info));
  }

  void CreateRendererAppInfo(const GURL& url,
                             const std::string name,
                             const std::string description) {
    CreateRendererAppInfo(url, name, description, GURL(), base::nullopt);
  }

  void CreateDefaultInstallableManager() {
    InstallableManager::CreateForWebContents(web_contents());
    // Required by InstallableManager.
    // Causes eligibility check to return NOT_FROM_SECURE_ORIGIN for GetData.
    SecurityStateTabHelper::CreateForWebContents(web_contents());
  }

  static base::NullableString16 ToNullableUTF16(const std::string& str) {
    return base::NullableString16(base::UTF8ToUTF16(str), false);
  }

  void SetInstallFinalizerForTesting() {
    auto test_install_finalizer = std::make_unique<TestInstallFinalizer>();
    test_install_finalizer_ = test_install_finalizer.get();
    install_finalizer_ = std::move(test_install_finalizer);
    install_task_->SetInstallFinalizerForTesting(test_install_finalizer_);
  }

  TestInstallFinalizer& test_install_finalizer() {
    DCHECK(test_install_finalizer_);
    return *test_install_finalizer_;
  }

  void SetIconsMapToRetrieve(IconsMap icons_map) {
    CHECK(data_retriever_);
    data_retriever_->SetIcons(std::move(icons_map));
  }

  struct InstallResult {
    AppId app_id;
    InstallResultCode code;
  };

  InstallResult InstallWebAppFromManifestWithFallbackAndGetResults() {
    InstallResult result;
    base::RunLoop run_loop;
    const bool force_shortcut_app = false;
    install_task_->InstallWebAppFromManifestWithFallback(
        web_contents(), force_shortcut_app,
        WebappInstallSource::MENU_BROWSER_TAB,
        base::BindOnce(TestAcceptDialogCallback),
        base::BindLambdaForTesting(
            [&](const AppId& installed_app_id, InstallResultCode code) {
              result.app_id = installed_app_id;
              result.code = code;
              run_loop.Quit();
            }));
    run_loop.Run();
    return result;
  }

  AppId InstallWebAppFromManifestWithFallback() {
    InstallResult result = InstallWebAppFromManifestWithFallbackAndGetResults();
    DCHECK_EQ(InstallResultCode::kSuccess, result.code);
    return result.app_id;
  }

  void PrepareTestAppInstall() {
    CreateRendererAppInfo(GURL("https://example.com/path"), "Name",
                          "Description");
    CreateDefaultInstallableManager();

    SetInstallFinalizerForTesting();

    IconsMap icons_map;
    SetIconsMapToRetrieve(std::move(icons_map));
  }

 protected:
  std::unique_ptr<TestWebAppDatabase> database_;
  std::unique_ptr<WebAppRegistrar> registrar_;
  std::unique_ptr<WebAppIconManager> icon_manager_;
  std::unique_ptr<WebAppInstallTask> install_task_;
  std::unique_ptr<InstallFinalizer> install_finalizer_;

  // Owned by install_task_:
  TestFileUtils* file_utils_ = nullptr;
  TestDataRetriever* data_retriever_ = nullptr;

 private:
  TestInstallFinalizer* test_install_finalizer_ = nullptr;
};

TEST_F(WebAppInstallTaskTest, InstallFromWebContents) {
  EXPECT_TRUE(AreWebAppsUserInstallable(profile()));

  const GURL url = GURL("https://example.com/path");
  const std::string name = "Name";
  const std::string description = "Description";
  const GURL scope = GURL("https://example.com/scope");
  const base::Optional<SkColor> theme_color = 0xAABBCCDD;

  const AppId app_id = GenerateAppIdFromURL(url);

  CreateRendererAppInfo(url, name, description, scope, theme_color);
  CreateDefaultInstallableManager();

  base::RunLoop run_loop;
  bool callback_called = false;
  const bool force_shortcut_app = false;

  install_task_->InstallWebAppFromManifestWithFallback(
      web_contents(), force_shortcut_app, WebappInstallSource::MENU_BROWSER_TAB,
      base::BindOnce(TestAcceptDialogCallback),
      base::BindLambdaForTesting(
          [&](const AppId& installed_app_id, InstallResultCode code) {
            EXPECT_EQ(InstallResultCode::kSuccess, code);
            EXPECT_EQ(app_id, installed_app_id);
            callback_called = true;
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_TRUE(callback_called);

  WebApp* web_app = registrar_->GetAppById(app_id);
  EXPECT_NE(nullptr, web_app);

  EXPECT_EQ(app_id, web_app->app_id());
  EXPECT_EQ(name, web_app->name());
  EXPECT_EQ(description, web_app->description());
  EXPECT_EQ(url, web_app->launch_url());
  EXPECT_EQ(scope, web_app->scope());
  EXPECT_EQ(theme_color, web_app->theme_color());
}

TEST_F(WebAppInstallTaskTest, AlreadyInstalled) {
  const GURL url = GURL("https://example.com/path");
  const std::string name = "Name";
  const std::string description = "Description";

  const AppId app_id = GenerateAppIdFromURL(url);

  CreateRendererAppInfo(url, name, description);
  CreateDefaultInstallableManager();

  const AppId installed_web_app = InstallWebAppFromManifestWithFallback();
  EXPECT_EQ(app_id, installed_web_app);

  // Second attempt.
  CreateRendererAppInfo(url, name, description);

  base::RunLoop run_loop;
  bool callback_called = false;
  const bool force_shortcut_app = false;

  install_task_->InstallWebAppFromManifestWithFallback(
      web_contents(), force_shortcut_app, WebappInstallSource::MENU_BROWSER_TAB,
      base::BindOnce(TestAcceptDialogCallback),
      base::BindLambdaForTesting(
          [&](const AppId& already_installed_app_id, InstallResultCode code) {
            EXPECT_EQ(InstallResultCode::kAlreadyInstalled, code);
            EXPECT_EQ(app_id, already_installed_app_id);
            callback_called = true;
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_TRUE(callback_called);
}

TEST_F(WebAppInstallTaskTest, GetWebApplicationInfoFailed) {
  auto data_retriver = std::make_unique<TestDataRetriever>();
  // data_retriver with empty info means an error.
  install_task_->SetDataRetrieverForTesting(std::move(data_retriver));

  CreateDefaultInstallableManager();

  base::RunLoop run_loop;
  bool callback_called = false;
  const bool force_shortcut_app = false;

  install_task_->InstallWebAppFromManifestWithFallback(
      web_contents(), force_shortcut_app, WebappInstallSource::MENU_BROWSER_TAB,
      base::BindOnce(TestAcceptDialogCallback),
      base::BindLambdaForTesting(
          [&](const AppId& installed_app_id, InstallResultCode code) {
            EXPECT_EQ(InstallResultCode::kGetWebApplicationInfoFailed, code);
            EXPECT_EQ(AppId(), installed_app_id);
            callback_called = true;
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_TRUE(callback_called);
}

TEST_F(WebAppInstallTaskTest, WebContentsDestroyed) {
  CreateRendererAppInfo(GURL("https://example.com/path"), "Name",
                        "Description");
  CreateDefaultInstallableManager();

  base::RunLoop run_loop;
  bool callback_called = false;
  const bool force_shortcut_app = false;

  install_task_->InstallWebAppFromManifestWithFallback(
      web_contents(), force_shortcut_app, WebappInstallSource::MENU_BROWSER_TAB,
      base::BindOnce(TestAcceptDialogCallback),
      base::BindLambdaForTesting(
          [&](const AppId& installed_app_id, InstallResultCode code) {
            EXPECT_EQ(InstallResultCode::kWebContentsDestroyed, code);
            EXPECT_EQ(AppId(), installed_app_id);
            callback_called = true;
            run_loop.Quit();
          }));

  // Destroy WebContents.
  DeleteContents();
  EXPECT_EQ(nullptr, web_contents());

  run_loop.Run();

  EXPECT_TRUE(callback_called);
}

TEST_F(WebAppInstallTaskTest, InstallableCheck) {
  const std::string renderer_description = "RendererDescription";
  CreateRendererAppInfo(GURL("https://renderer.com/path"), "RendererName",
                        renderer_description,
                        GURL("https://renderer.com/scope"), 0x00);

  const GURL manifest_start_url = GURL("https://example.com/start");
  const AppId app_id = GenerateAppIdFromURL(manifest_start_url);
  const std::string manifest_name = "Name from Manifest";
  const GURL manifest_scope = GURL("https://example.com/scope");
  const base::Optional<SkColor> manifest_theme_color = 0xAABBCCDD;

  {
    auto manifest = std::make_unique<blink::Manifest>();
    manifest->short_name = ToNullableUTF16("Short Name from Manifest");
    manifest->name = ToNullableUTF16(manifest_name);
    manifest->start_url = manifest_start_url;
    manifest->scope = manifest_scope;
    manifest->theme_color = manifest_theme_color;

    FakeInstallableManager::CreateForWebContentsWithManifest(
        web_contents(), NO_ERROR_DETECTED, GURL("https://example.com/manifest"),
        std::move(manifest));
  }

  base::RunLoop run_loop;
  bool callback_called = false;
  const bool force_shortcut_app = false;

  install_task_->InstallWebAppFromManifestWithFallback(
      web_contents(), force_shortcut_app, WebappInstallSource::MENU_BROWSER_TAB,
      base::BindOnce(TestAcceptDialogCallback),
      base::BindLambdaForTesting(
          [&](const AppId& installed_app_id, InstallResultCode code) {
            EXPECT_EQ(InstallResultCode::kSuccess, code);
            EXPECT_EQ(app_id, installed_app_id);
            callback_called = true;
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_TRUE(callback_called);

  WebApp* web_app = registrar_->GetAppById(app_id);
  EXPECT_NE(nullptr, web_app);

  // Manifest data overrides Renderer data, except |description|.
  EXPECT_EQ(app_id, web_app->app_id());
  EXPECT_EQ(manifest_name, web_app->name());
  EXPECT_EQ(manifest_start_url, web_app->launch_url());
  EXPECT_EQ(renderer_description, web_app->description());
  EXPECT_EQ(manifest_scope, web_app->scope());
  EXPECT_EQ(manifest_theme_color, web_app->theme_color());
}

TEST_F(WebAppInstallTaskTest, GetIcons) {
  CreateRendererAppInfo(GURL("https://example.com/path"), "Name",
                        "Description");
  CreateDefaultInstallableManager();

  SetInstallFinalizerForTesting();

  const GURL icon_url = GURL("https://example.com/app.ico");
  const SkColor color = SK_ColorBLUE;

  // Generate one icon as if it was downloaded.
  IconsMap icons_map =
      GenerateIconsMapWithOneIcon(icon_url, icon_size::k128, color);
  SetIconsMapToRetrieve(std::move(icons_map));

  InstallWebAppFromManifestWithFallback();

  std::unique_ptr<WebApplicationInfo> web_app_info =
      test_install_finalizer().web_app_info();

  // Make sure that icons have been generated for all sub sizes.
  EXPECT_TRUE(ContainsOneIconOfEachSize(*web_app_info));

  for (const WebApplicationInfo::IconInfo& icon : web_app_info->icons) {
    EXPECT_FALSE(icon.data.drawsNothing());
    EXPECT_EQ(color, icon.data.getColor(0, 0));

    // All icons should have an empty url except the original one:
    if (icon.url != icon_url) {
      EXPECT_EQ(GURL(), icon.url);
    }
  }
}

TEST_F(WebAppInstallTaskTest, GetIcons_NoIconsProvided) {
  CreateRendererAppInfo(GURL("https://example.com/path"), "Name",
                        "Description");
  CreateDefaultInstallableManager();

  SetInstallFinalizerForTesting();

  IconsMap icons_map;
  SetIconsMapToRetrieve(std::move(icons_map));

  InstallWebAppFromManifestWithFallback();

  std::unique_ptr<WebApplicationInfo> web_app_info =
      test_install_finalizer().web_app_info();

  // Make sure that icons have been generated for all sizes.
  EXPECT_TRUE(ContainsOneIconOfEachSize(*web_app_info));

  for (const WebApplicationInfo::IconInfo& icon : web_app_info->icons) {
    EXPECT_FALSE(icon.data.drawsNothing());
    // Since all icons are generated, they should have an empty url.
    EXPECT_TRUE(icon.url.is_empty());
  }
}

TEST_F(WebAppInstallTaskTest, WriteDataToDisk) {
  CreateRendererAppInfo(GURL("https://example.com/path"), "Name",
                        "Description");
  CreateDefaultInstallableManager();

  // TestingProfile creates temp directory if TestingProfile::path_ is empty
  // (i.e. if TestingProfile::Builder::SetPath was not called by a test fixture)
  const base::FilePath profile_dir = profile()->GetPath();
  const base::FilePath web_apps_dir = profile_dir.AppendASCII("WebApps");
  EXPECT_FALSE(file_utils_->DirectoryExists(web_apps_dir));

  const SkColor color = SK_ColorGREEN;
  const int original_icon_size_px = icon_size::k512;

  // Generate one icon as if it was fetched from renderer.
  {
    WebApplicationInfo::IconInfo icon_info = GenerateIconInfo(
        GURL("https://example.com/app.ico"), original_icon_size_px, color);
    data_retriever_->web_app_info().icons.push_back(std::move(icon_info));
  }

  const AppId app_id = InstallWebAppFromManifestWithFallback();

  EXPECT_TRUE(file_utils_->DirectoryExists(web_apps_dir));

  const base::FilePath temp_dir = web_apps_dir.AppendASCII("Temp");
  EXPECT_TRUE(file_utils_->DirectoryExists(temp_dir));
  EXPECT_TRUE(file_utils_->IsDirectoryEmpty(temp_dir));

  const base::FilePath app_dir = web_apps_dir.AppendASCII(app_id);
  EXPECT_TRUE(file_utils_->DirectoryExists(app_dir));

  const base::FilePath icons_dir = app_dir.AppendASCII("Icons");
  EXPECT_TRUE(file_utils_->DirectoryExists(icons_dir));

  std::set<int> written_sizes_px;

  base::FileEnumerator enumerator(icons_dir, true, base::FileEnumerator::FILES);
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    EXPECT_TRUE(path.MatchesExtension(FILE_PATH_LITERAL(".png")));

    SkBitmap bitmap;
    EXPECT_TRUE(ReadBitmap(file_utils_, path, &bitmap));

    EXPECT_EQ(bitmap.width(), bitmap.height());

    const int size_px = bitmap.width();
    EXPECT_EQ(0UL, written_sizes_px.count(size_px));

    base::FilePath size_file_name;
    size_file_name =
        size_file_name.AppendASCII(base::StringPrintf("%i.png", size_px));
    EXPECT_EQ(size_file_name, path.BaseName());

    written_sizes_px.insert(size_px);

    EXPECT_EQ(color, bitmap.getColor(0, 0));
  }

  EXPECT_EQ(base::size(kIconSizes) + 1UL, written_sizes_px.size());

  for (int size_px : kIconSizes)
    written_sizes_px.erase(size_px);
  written_sizes_px.erase(original_icon_size_px);

  EXPECT_TRUE(written_sizes_px.empty());
}

TEST_F(WebAppInstallTaskTest, WriteDataToDiskFailed) {
  const GURL app_url = GURL("https://example.com/path");
  CreateRendererAppInfo(app_url, "Name", "Description");
  CreateDefaultInstallableManager();

  IconsMap icons_map = GenerateIconsMapWithOneIcon(
      GURL("https://example.com/app.ico"), icon_size::k512, SK_ColorBLUE);
  SetIconsMapToRetrieve(std::move(icons_map));

  const base::FilePath profile_dir = profile()->GetPath();
  const base::FilePath web_apps_dir = profile_dir.AppendASCII("WebApps");

  EXPECT_TRUE(file_utils_->CreateDirectory(web_apps_dir));

  // Induce an error: Simulate "Disk Full" for writing icon files.
  file_utils_->SetRemainingDiskSpaceSize(1024);

  base::RunLoop run_loop;
  bool callback_called = false;
  const bool force_shortcut_app = false;

  install_task_->InstallWebAppFromManifestWithFallback(
      web_contents(), force_shortcut_app, WebappInstallSource::MENU_BROWSER_TAB,
      base::BindOnce(TestAcceptDialogCallback),
      base::BindLambdaForTesting(
          [&](const AppId& installed_app_id, InstallResultCode code) {
            EXPECT_EQ(InstallResultCode::kWriteDataFailed, code);
            EXPECT_EQ(AppId(), installed_app_id);
            callback_called = true;
            run_loop.Quit();
          }));
  run_loop.Run();
  EXPECT_TRUE(callback_called);

  const base::FilePath temp_dir = web_apps_dir.AppendASCII("Temp");
  EXPECT_TRUE(file_utils_->DirectoryExists(temp_dir));
  EXPECT_TRUE(file_utils_->IsDirectoryEmpty(temp_dir));

  const AppId app_id = GenerateAppIdFromURL(app_url);
  const base::FilePath app_dir = web_apps_dir.AppendASCII(app_id);
  EXPECT_FALSE(file_utils_->DirectoryExists(app_dir));
}

TEST_F(WebAppInstallTaskTest, UserInstallDeclined) {
  const GURL url = GURL("https://example.com/path");
  const AppId app_id = GenerateAppIdFromURL(url);

  CreateRendererAppInfo(url, "Name", "Description");
  CreateDefaultInstallableManager();

  base::RunLoop run_loop;
  bool callback_called = false;
  const bool force_shortcut_app = false;

  install_task_->InstallWebAppFromManifestWithFallback(
      web_contents(), force_shortcut_app, WebappInstallSource::MENU_BROWSER_TAB,
      base::BindOnce(TestDeclineDialogCallback),
      base::BindLambdaForTesting(
          [&](const AppId& installed_app_id, InstallResultCode code) {
            EXPECT_EQ(InstallResultCode::kUserInstallDeclined, code);
            EXPECT_EQ(installed_app_id, AppId());
            callback_called = true;
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_TRUE(callback_called);

  WebApp* web_app = registrar_->GetAppById(app_id);
  EXPECT_EQ(nullptr, web_app);
}

TEST_F(WebAppInstallTaskTest, FinalizerMethodsCalled) {
  PrepareTestAppInstall();

  InstallWebAppFromManifestWithFallback();

  EXPECT_EQ(1, test_install_finalizer().num_create_os_shortcuts_calls());
  EXPECT_EQ(1, test_install_finalizer().num_reparent_tab_calls());
  EXPECT_EQ(1, test_install_finalizer().num_reveal_appshim_calls());
  EXPECT_EQ(1, test_install_finalizer().num_pin_app_to_shelf_calls());
}

TEST_F(WebAppInstallTaskTest, FinalizerMethodsNotCalled) {
  PrepareTestAppInstall();
  test_install_finalizer().SetNextFinalizeInstallResult(
      AppId(), InstallResultCode::kFailedUnknownReason);

  InstallResult result = InstallWebAppFromManifestWithFallbackAndGetResults();

  EXPECT_TRUE(result.app_id.empty());
  EXPECT_EQ(InstallResultCode::kFailedUnknownReason, result.code);

  EXPECT_EQ(0, test_install_finalizer().num_create_os_shortcuts_calls());
  EXPECT_EQ(0, test_install_finalizer().num_reparent_tab_calls());
  EXPECT_EQ(0, test_install_finalizer().num_reveal_appshim_calls());
  EXPECT_EQ(0, test_install_finalizer().num_pin_app_to_shelf_calls());
}

TEST_F(WebAppInstallTaskTest, InstallWebAppFromManifest_Success) {
  CreateTestDataRetriever();

  const GURL url = GURL("https://example.com/path");
  const AppId app_id = GenerateAppIdFromURL(url);

  auto manifest = std::make_unique<blink::Manifest>();
  manifest->start_url = url;

  data_retriever_->SetManifest(std::move(manifest), /*is_installable=*/true);

  base::RunLoop run_loop;

  install_task_->InstallWebAppFromManifest(
      web_contents(), WebappInstallSource::MENU_BROWSER_TAB,
      base::BindOnce(TestAcceptDialogCallback),
      base::BindLambdaForTesting(
          [&](const AppId& installed_app_id, InstallResultCode code) {
            EXPECT_EQ(InstallResultCode::kSuccess, code);
            EXPECT_EQ(app_id, installed_app_id);
            run_loop.Quit();
          }));

  run_loop.Run();
}

// TODO(loyso): Convert more tests from bookmark_app_helper_unittest.cc

}  // namespace web_app
