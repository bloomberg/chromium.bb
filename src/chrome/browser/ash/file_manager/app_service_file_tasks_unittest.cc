// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/app_service_file_tasks.h"

#include <memory>
#include <string>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "chrome/browser/ash/file_manager/file_tasks.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/services/app_service/public/cpp/intent_test_util.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/entry_info.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

using extensions::api::file_manager_private::Verb;

namespace {
const char kAppIdText[] = "abcdefg";
const char kAppIdImage[] = "gfedcba";
const char kAppIdAny[] = "hijklmn";
const char kChromeAppId[] = "chromeappid";
const char kChromeAppWithVerbsId[] = "chromeappwithverbsid";
const char kExtensionId[] = "extensionid";
const char kAppIdTextWild[] = "zxcvbn";
const char kMimeTypeText[] = "text/plain";
const char kMimeTypeImage[] = "image/jpeg";
const char kMimeTypeHtml[] = "text/html";
const char kMimeTypeAny[] = "*/*";
const char kMimeTypeTextWild[] = "text/*";
const char kFileExtensionText[] = "txt";
const char kFileExtensionImage[] = "jpeg";
const char kFileExtensionAny[] = "fake";
const char kActivityLabelText[] = "some_text_activity";
const char kActivityLabelImage[] = "some_image_activity";
const char kActivityLabelAny[] = "some_any_file";
const char kActivityLabelTextWild[] = "some_text_wild_file";

GURL test_url(const std::string& file_name) {
  GURL url =
      GURL("filesystem:chrome-extension://extensionid/external/" + file_name);
  EXPECT_TRUE(url.is_valid());
  return url;
}

}  // namespace

namespace file_manager {
namespace file_tasks {

class AppServiceFileTasksTest : public testing::Test {
 protected:
  AppServiceFileTasksTest() {}
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    app_service_test_.SetUp(profile_.get());
    app_service_proxy_ =
        apps::AppServiceProxyFactory::GetForProfile(profile_.get());
    ASSERT_TRUE(app_service_proxy_);
  }

  Profile* profile() { return profile_.get(); }

  struct FakeFile {
    std::string file_name;
    std::string mime_type;
    bool is_directory = false;
  };

  std::vector<FullTaskDescriptor> FindAppServiceTasks(
      const std::vector<FakeFile>& files) {
    std::vector<extensions::EntryInfo> entries;
    std::vector<GURL> file_urls;
    for (const FakeFile& fake_file : files) {
      entries.emplace_back(
          util::GetMyFilesFolderForProfile(profile()).AppendASCII(
              fake_file.file_name),
          fake_file.mime_type, fake_file.is_directory);
      file_urls.push_back(test_url(fake_file.file_name));
    }

    std::vector<FullTaskDescriptor> tasks;
    file_tasks::FindAppServiceTasks(profile(), entries, file_urls, &tasks);
    // Sort by app ID so we don't rely on ordering.
    std::sort(
        tasks.begin(), tasks.end(), [](const auto& left, const auto& right) {
          return left.task_descriptor.app_id < right.task_descriptor.app_id;
        });
    return tasks;
  }

  void AddFakeAppWithIntentFilters(
      const std::string& app_id,
      std::vector<apps::mojom::IntentFilterPtr> intent_filters,
      apps::mojom::AppType app_type,
      apps::mojom::OptionalBool handles_intents) {
    std::vector<apps::mojom::AppPtr> apps;
    auto app = apps::mojom::App::New();
    app->app_id = app_id;
    app->app_type = app_type;
    app->handles_intents = handles_intents;
    app->readiness = apps::mojom::Readiness::kReady;
    app->intent_filters = std::move(intent_filters);
    apps.push_back(std::move(app));
    app_service_proxy_->AppRegistryCache().OnApps(
        std::move(apps), app_type, false /* should_notify_initialized */);
    app_service_test_.WaitForAppService();
  }

  void AddFakeWebApp(const std::string& app_id,
                     const std::string& mime_type,
                     const std::string& file_extension,
                     const std::string& activity_label,
                     apps::mojom::OptionalBool handles_intents) {
    std::vector<apps::mojom::IntentFilterPtr> filters;
    filters.push_back(apps_util::CreateFileFilterForView(
        mime_type, file_extension, activity_label));
    AddFakeAppWithIntentFilters(app_id, std::move(filters),
                                apps::mojom::AppType::kWeb, handles_intents);
  }

  void AddTextApp() {
    AddFakeWebApp(kAppIdText, kMimeTypeText, kFileExtensionText,
                  kActivityLabelText, apps::mojom::OptionalBool::kTrue);
  }

  void AddImageApp() {
    AddFakeWebApp(kAppIdImage, kMimeTypeImage, kFileExtensionImage,
                  kActivityLabelImage, apps::mojom::OptionalBool::kTrue);
  }

  void AddTextWildApp() {
    AddFakeWebApp(kAppIdTextWild, kMimeTypeTextWild, kFileExtensionAny,
                  kActivityLabelTextWild, apps::mojom::OptionalBool::kTrue);
  }

  void AddAnyApp() {
    AddFakeWebApp(kAppIdAny, kMimeTypeAny, kFileExtensionAny, kActivityLabelAny,
                  apps::mojom::OptionalBool::kTrue);
  }

  // Provides file handlers for all extensions and images.
  void AddChromeApp() {
    extensions::ExtensionBuilder baz_app;
    baz_app.SetManifest(
        extensions::DictionaryBuilder()
            .Set("name", "Baz")
            .Set("version", "1.0.0")
            .Set("manifest_version", 2)
            .Set("app",
                 extensions::DictionaryBuilder()
                     .Set("background",
                          extensions::DictionaryBuilder()
                              .Set("scripts", extensions::ListBuilder()
                                                  .Append("background.js")
                                                  .Build())
                              .Build())
                     .Build())
            .Set(
                "file_handlers",
                extensions::DictionaryBuilder()
                    .Set("any", extensions::DictionaryBuilder()
                                    .Set("extensions", extensions::ListBuilder()
                                                           .Append("*")
                                                           .Append("bar")
                                                           .Build())
                                    .Build())
                    .Set("image", extensions::DictionaryBuilder()
                                      .Set("types", extensions::ListBuilder()
                                                        .Append("image/*")
                                                        .Build())
                                      .Build())
                    .Build())
            .Build());
    baz_app.SetID(kChromeAppId);
    auto filters =
        apps_util::CreateChromeAppIntentFilters(baz_app.Build().get());
    AddFakeAppWithIntentFilters(kChromeAppId, std::move(filters),
                                apps::mojom::AppType::kChromeApp,
                                apps::mojom::OptionalBool::kTrue);
  }

  void AddChromeAppWithVerbs() {
    extensions::ExtensionBuilder foo_app;
    foo_app.SetManifest(
        extensions::DictionaryBuilder()
            .Set("name", "Foo")
            .Set("version", "1.0.0")
            .Set("manifest_version", 2)
            .Set("app",
                 extensions::DictionaryBuilder()
                     .Set("background",
                          extensions::DictionaryBuilder()
                              .Set("scripts", extensions::ListBuilder()
                                                  .Append("background.js")
                                                  .Build())
                              .Build())
                     .Build())
            .Set(
                "file_handlers",
                extensions::DictionaryBuilder()
                    .Set("any_with_directories",
                         extensions::DictionaryBuilder()
                             .Set("include_directories", true)
                             .Set("types",
                                  extensions::ListBuilder().Append("*").Build())
                             .Set("verb", "open_with")
                             .Build())
                    .Set("html_handler",
                         extensions::DictionaryBuilder()
                             .Set("title", "Html")
                             .Set("types", extensions::ListBuilder()
                                               .Append("text/html")
                                               .Build())
                             .Set("verb", "open_with")
                             .Build())
                    .Set("plain_text",
                         extensions::DictionaryBuilder()
                             .Set("title", "Plain")
                             .Set("types", extensions::ListBuilder()
                                               .Append("text/plain")
                                               .Build())
                             .Build())
                    .Set("share_plain_text",
                         extensions::DictionaryBuilder()
                             .Set("title", "Share Plain")
                             .Set("types", extensions::ListBuilder()
                                               .Append("text/plain")
                                               .Build())
                             .Set("verb", "share_with")
                             .Build())
                    .Set("any_pack", extensions::DictionaryBuilder()
                                         .Set("types", extensions::ListBuilder()
                                                           .Append("*")
                                                           .Build())
                                         .Set("verb", "pack_with")
                                         .Build())
                    .Set("plain_text_add_to",
                         extensions::DictionaryBuilder()
                             .Set("title", "Plain")
                             .Set("types", extensions::ListBuilder()
                                               .Append("text/plain")
                                               .Build())
                             .Set("verb", "add_to")
                             .Build())
                    .Build())
            .Build());
    foo_app.SetID(kChromeAppWithVerbsId);
    auto filters =
        apps_util::CreateChromeAppIntentFilters(foo_app.Build().get());
    AddFakeAppWithIntentFilters(kChromeAppWithVerbsId, std::move(filters),
                                apps::mojom::AppType::kChromeApp,
                                apps::mojom::OptionalBool::kTrue);
  }

  // Adds file_browser_handler to handle .txt files.
  void AddExtension() {
    extensions::ExtensionBuilder fbh_app;
    fbh_app.SetManifest(
        extensions::DictionaryBuilder()
            .Set("name", "Fbh")
            .Set("version", "1.0.0")
            .Set("manifest_version", 2)
            .Set("permissions",
                 extensions::ListBuilder().Append("fileBrowserHandler").Build())
            .Set("file_browser_handlers",
                 extensions::ListBuilder()
                     .Append(extensions::DictionaryBuilder()
                                 .Set("id", "open")
                                 .Set("default_title", "open title")
                                 .Set("file_filters",
                                      extensions::ListBuilder()
                                          .Append("filesystem:*.txt")
                                          .Build())
                                 .Build())
                     .Build())
            .Build());
    fbh_app.SetID(kExtensionId);
    auto filters =
        apps_util::CreateExtensionIntentFilters(fbh_app.Build().get());
    AddFakeAppWithIntentFilters(kExtensionId, std::move(filters),
                                apps::mojom::AppType::kChromeApp,
                                apps::mojom::OptionalBool::kTrue);
  }

  base::test::ScopedFeatureList feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  apps::AppServiceProxy* app_service_proxy_ = nullptr;
  apps::AppServiceTest app_service_test_;
};

class AppServiceFileTasksTestEnabled : public AppServiceFileTasksTest {
 public:
  AppServiceFileTasksTestEnabled() {
    feature_list_.InitWithFeatures({blink::features::kFileHandlingAPI}, {});
  }
};

class AppServiceFileTasksTestDisabled : public AppServiceFileTasksTest {
 public:
  AppServiceFileTasksTestDisabled() {
    feature_list_.InitWithFeatures({}, {blink::features::kFileHandlingAPI});
  }
};

// Web Apps should not be able to handle files when kFileHandlingAPI is
// disabled.
TEST_F(AppServiceFileTasksTestDisabled, FindAppServiceFileTasksText) {
  AddTextApp();
  // Find apps for a "text/plain" file.
  std::vector<FullTaskDescriptor> tasks =
      FindAppServiceTasks({{"foo.txt", kMimeTypeText}});
  ASSERT_EQ(0U, tasks.size());
}

// An app which does not handle intents should not be found even if the filters
// match.
TEST_F(AppServiceFileTasksTestEnabled, FindAppServiceFileTasksHandlesIntent) {
  AddFakeWebApp(kAppIdImage, kMimeTypeImage, kFileExtensionImage,
                kActivityLabelImage, apps::mojom::OptionalBool::kFalse);
  std::vector<FullTaskDescriptor> tasks =
      FindAppServiceTasks({{"foo.jpeg", kMimeTypeImage}});
  ASSERT_EQ(0U, tasks.size());
}

// Test that between an image app and text app, the text app can be
// found for an text file entry.
TEST_F(AppServiceFileTasksTestEnabled, FindAppServiceFileTasksText) {
  AddTextApp();
  AddImageApp();
  // Find apps for a "text/plain" file.
  std::vector<FullTaskDescriptor> tasks =
      FindAppServiceTasks({{"foo.txt", kMimeTypeText}});
  ASSERT_EQ(1U, tasks.size());
  EXPECT_EQ(kAppIdText, tasks[0].task_descriptor.app_id);
  EXPECT_EQ(kActivityLabelText, tasks[0].task_title);
  EXPECT_FALSE(tasks[0].is_generic_file_handler);
}

// Test that between an image app and text app, the image app can be
// found for an image file entry.
TEST_F(AppServiceFileTasksTestEnabled, FindAppServiceFileTasksImage) {
  AddTextApp();
  AddImageApp();
  // Find apps for a "image/jpeg" file.
  std::vector<FullTaskDescriptor> tasks =
      FindAppServiceTasks({{"bar.jpeg", kMimeTypeImage}});
  ASSERT_EQ(1U, tasks.size());
  EXPECT_EQ(kAppIdImage, tasks[0].task_descriptor.app_id);
  EXPECT_EQ(kActivityLabelImage, tasks[0].task_title);
  EXPECT_FALSE(tasks[0].is_generic_file_handler);
}

// Test that between an image app, text app and an app that can handle every
// file, the app that can handle every file can be found for an image file entry
// and text file entry.
TEST_F(AppServiceFileTasksTestEnabled, FindAppServiceFileTasksMultiple) {
  AddTextApp();
  AddImageApp();
  AddAnyApp();
  std::vector<FullTaskDescriptor> tasks = FindAppServiceTasks(
      {{"foo.txt", kMimeTypeText}, {"bar.jpeg", kMimeTypeImage}});
  ASSERT_EQ(1U, tasks.size());
  EXPECT_EQ(kAppIdAny, tasks[0].task_descriptor.app_id);
  EXPECT_EQ(kActivityLabelAny, tasks[0].task_title);
  EXPECT_TRUE(tasks[0].is_generic_file_handler);
}

// Don't register any apps and check that we get no matches.
TEST_F(AppServiceFileTasksTestEnabled, FindAppServiceWebFileTasksNoTasks) {
  // Find web apps for a "text/plain" file.
  std::vector<FullTaskDescriptor> tasks =
      FindAppServiceTasks({{"foo.txt", kMimeTypeText}});
  ASSERT_EQ(0U, tasks.size());
}

// Register a text handler and check we get no matches with an image.
TEST_F(AppServiceFileTasksTestEnabled,
       FindAppServiceWebFileTasksNoMatchingTask) {
  AddTextApp();
  // Find apps for a "image/jpeg" file.
  std::vector<FullTaskDescriptor> tasks =
      FindAppServiceTasks({{"bar.jpeg", kMimeTypeImage}});
  ASSERT_EQ(0U, tasks.size());
}

// Check we get a match for a text file + web app.
TEST_F(AppServiceFileTasksTestEnabled, FindAppServiceWebFileTasksText) {
  AddTextApp();
  // Find web apps for a "text/plain" file.
  std::vector<FullTaskDescriptor> tasks =
      FindAppServiceTasks({{"foo.txt", kMimeTypeText}});
  ASSERT_EQ(1U, tasks.size());
  EXPECT_EQ(kAppIdText, tasks[0].task_descriptor.app_id);
  EXPECT_EQ(kActivityLabelText, tasks[0].task_title);
  EXPECT_FALSE(tasks[0].is_generic_file_handler);
}

// Check that a web app that only handles text does not match when we have both
// a text file and an image.
TEST_F(AppServiceFileTasksTestEnabled,
       FindAppServiceWebFileTasksTwoFilesNoMatch) {
  AddTextApp();
  std::vector<FullTaskDescriptor> tasks = FindAppServiceTasks(
      {{"foo.txt", kMimeTypeText}, {"bar.jpeg", kMimeTypeImage}});
  ASSERT_EQ(0U, tasks.size());
}

// Check we get a match for a text file + text wildcard filter.
TEST_F(AppServiceFileTasksTestEnabled, FindAppServiceWebFileTasksTextWild) {
  AddTextWildApp();
  AddTextApp();
  // Find web apps for a "text/plain" file.
  std::vector<FullTaskDescriptor> tasks =
      FindAppServiceTasks({{"foo.txt", kMimeTypeText}});
  ASSERT_EQ(2U, tasks.size());
  EXPECT_EQ(kAppIdText, tasks[0].task_descriptor.app_id);
  EXPECT_EQ(kActivityLabelText, tasks[0].task_title);
  EXPECT_EQ(kAppIdTextWild, tasks[1].task_descriptor.app_id);
  EXPECT_EQ(kActivityLabelTextWild, tasks[1].task_title);
}

// Check we get a match for a text file and HTML file + text wildcard filter.
TEST_F(AppServiceFileTasksTestEnabled,
       FindAppServiceWebFileTasksTextWildMultiple) {
  AddTextWildApp();
  AddTextApp();   // Should not be matched.
  AddImageApp();  // Should not be matched.
  std::vector<FullTaskDescriptor> tasks = FindAppServiceTasks(
      {{"foo.txt", kMimeTypeText}, {"bar.html", kMimeTypeHtml}});
  ASSERT_EQ(1U, tasks.size());
  EXPECT_EQ(kAppIdTextWild, tasks[0].task_descriptor.app_id);
  EXPECT_EQ(kActivityLabelTextWild, tasks[0].task_title);
}

// An edge case where we have one file that matches the mime type but not the
// file extension, and another file that matches the file extension but not the
// mime type. This should still match the handler.
TEST_F(AppServiceFileTasksTestEnabled,
       FindAppServiceWebFileTasksAllFilesMatchEither) {
  AddTextApp();

  // First check that each file alone matches the text app.
  std::vector<FullTaskDescriptor> tasksFoo =
      FindAppServiceTasks({{"foo.txt", "text/plane"}});
  ASSERT_EQ(1U, tasksFoo.size());
  std::vector<FullTaskDescriptor> tasksBar =
      FindAppServiceTasks({{"bar.text", kMimeTypeText}});
  ASSERT_EQ(1U, tasksFoo.size());

  // Now check that both together match.
  std::vector<FullTaskDescriptor> tasksBoth = FindAppServiceTasks(
      {{"foo.txt", "text/plane"}, {"bar.text", kMimeTypeText}});
  ASSERT_EQ(1U, tasksBoth.size());
  EXPECT_EQ(kAppIdText, tasksBoth[0].task_descriptor.app_id);
  EXPECT_EQ(kActivityLabelText, tasksBoth[0].task_title);
}

// Check that Baz's ".*" handler, which is generic, is matched.
TEST_F(AppServiceFileTasksTestEnabled, FindAppServiceChromeAppText) {
  AddChromeApp();
  std::vector<FullTaskDescriptor> tasks =
      FindAppServiceTasks({{"foo.txt", kMimeTypeText}});
  ASSERT_EQ(1U, tasks.size());
  EXPECT_EQ(kChromeAppId, tasks[0].task_descriptor.app_id);
  EXPECT_EQ("any", tasks[0].task_descriptor.action_id);
  EXPECT_EQ(TASK_TYPE_FILE_HANDLER, tasks[0].task_descriptor.task_type);
  EXPECT_EQ("Baz", tasks[0].task_title);
  EXPECT_TRUE(tasks[0].is_generic_file_handler);
  EXPECT_TRUE(tasks[0].is_file_extension_match);
  EXPECT_EQ(Verb::VERB_OPEN_WITH, tasks[0].task_verb);
}

// File extension matches with bar, but there is a generic * type as well,
// so the overall match should still be generic.
TEST_F(AppServiceFileTasksTestEnabled, FindAppServiceChromeAppBar) {
  AddChromeApp();
  std::vector<FullTaskDescriptor> tasks =
      FindAppServiceTasks({{"foo.bar", kMimeTypeText}});
  ASSERT_EQ(1U, tasks.size());
  EXPECT_EQ(kChromeAppId, tasks[0].task_descriptor.app_id);
  EXPECT_EQ("any", tasks[0].task_descriptor.action_id);
  EXPECT_EQ(TASK_TYPE_FILE_HANDLER, tasks[0].task_descriptor.task_type);
  EXPECT_EQ("Baz", tasks[0].task_title);
  EXPECT_TRUE(tasks[0].is_generic_file_handler);
  EXPECT_TRUE(tasks[0].is_file_extension_match);
  EXPECT_EQ(Verb::VERB_OPEN_WITH, tasks[0].task_verb);
}

// Check that we can get web apps and Chrome apps in the same call.
TEST_F(AppServiceFileTasksTestEnabled, FindAppServiceMultiAppType) {
  AddTextApp();
  AddChromeApp();
  std::vector<FullTaskDescriptor> tasks =
      FindAppServiceTasks({{"foo.txt", kMimeTypeText}});
  ASSERT_EQ(2U, tasks.size());
  EXPECT_EQ(kAppIdText, tasks[0].task_descriptor.app_id);
  EXPECT_EQ(kActivityLabelText, tasks[0].task_title);
  EXPECT_EQ(TASK_TYPE_WEB_APP, tasks[0].task_descriptor.task_type);
  EXPECT_EQ(kChromeAppId, tasks[1].task_descriptor.app_id);
  EXPECT_EQ("Baz", tasks[1].task_title);
  EXPECT_EQ(TASK_TYPE_FILE_HANDLER, tasks[1].task_descriptor.task_type);
}

// Check that Baz's "image/*" handler is picked because it is not generic,
// because it matches the mime type directly, even though there is an earlier
// generic handler.
TEST_F(AppServiceFileTasksTestEnabled, FindAppServiceChromeAppImage) {
  AddChromeApp();
  std::vector<FullTaskDescriptor> tasks =
      FindAppServiceTasks({{"bar.jpeg", kMimeTypeImage}});
  ASSERT_EQ(1U, tasks.size());
  EXPECT_EQ(kChromeAppId, tasks[0].task_descriptor.app_id);
  EXPECT_EQ("image", tasks[0].task_descriptor.action_id);
  EXPECT_EQ(TASK_TYPE_FILE_HANDLER, tasks[0].task_descriptor.task_type);
  EXPECT_EQ("Baz", tasks[0].task_title);
  EXPECT_FALSE(tasks[0].is_generic_file_handler);
  EXPECT_FALSE(tasks[0].is_file_extension_match);
  EXPECT_EQ(Verb::VERB_OPEN_WITH, tasks[0].task_verb);
}

TEST_F(AppServiceFileTasksTestEnabled, FindAppServiceChromeAppWithVerbs) {
  AddChromeAppWithVerbs();
  std::vector<FullTaskDescriptor> tasks =
      FindAppServiceTasks({{"foo.txt", kMimeTypeText}});

  // We expect that all non-"open_with" handlers are ignored, and that we
  // only get one open_with handler.
  ASSERT_EQ(1U, tasks.size());
  EXPECT_EQ(kChromeAppWithVerbsId, tasks[0].task_descriptor.app_id);
  EXPECT_EQ("Foo", tasks[0].task_title);
  EXPECT_EQ("plain_text", tasks[0].task_descriptor.action_id);
  EXPECT_FALSE(tasks[0].is_generic_file_handler);
  EXPECT_FALSE(tasks[0].is_file_extension_match);
  EXPECT_EQ(Verb::VERB_OPEN_WITH, tasks[0].task_verb);
}

TEST_F(AppServiceFileTasksTestEnabled, FindAppServiceChromeAppWithVerbs_Html) {
  AddChromeAppWithVerbs();
  std::vector<FullTaskDescriptor> tasks =
      FindAppServiceTasks({{"foo.html", kMimeTypeHtml}});

  // Check that we get the non-generic handler which appears later in the
  // manifest.
  EXPECT_EQ(kChromeAppWithVerbsId, tasks[0].task_descriptor.app_id);
  EXPECT_EQ("Foo", tasks[0].task_title);
  EXPECT_EQ("html_handler", tasks[0].task_descriptor.action_id);
  EXPECT_FALSE(tasks[0].is_generic_file_handler);
  EXPECT_FALSE(tasks[0].is_file_extension_match);
  EXPECT_EQ(Verb::VERB_OPEN_WITH, tasks[0].task_verb);
}

TEST_F(AppServiceFileTasksTestEnabled,
       FindAppServiceChromeAppWithVerbs_Directory) {
  AddChromeAppWithVerbs();
  std::vector<FullTaskDescriptor> tasks =
      FindAppServiceTasks({{"dir", "", true}});

  // Only one handler handles directories.
  ASSERT_EQ(1U, tasks.size());
  EXPECT_EQ(kChromeAppWithVerbsId, tasks[0].task_descriptor.app_id);
  EXPECT_EQ("Foo", tasks[0].task_title);
  EXPECT_EQ("any_with_directories", tasks[0].task_descriptor.action_id);
  EXPECT_TRUE(tasks[0].is_generic_file_handler);
  EXPECT_FALSE(tasks[0].is_file_extension_match);
  EXPECT_EQ(Verb::VERB_OPEN_WITH, tasks[0].task_verb);
}

TEST_F(AppServiceFileTasksTestEnabled, FindAppServiceExtension) {
  AddExtension();
  std::vector<FullTaskDescriptor> tasks =
      FindAppServiceTasks({{"foo.txt", kMimeTypeText}});

  ASSERT_EQ(1U, tasks.size());
  EXPECT_EQ(kExtensionId, tasks[0].task_descriptor.app_id);
  EXPECT_EQ("open title", tasks[0].task_title);
  EXPECT_EQ("open", tasks[0].task_descriptor.action_id);
  EXPECT_FALSE(tasks[0].is_generic_file_handler);
  EXPECT_FALSE(tasks[0].is_file_extension_match);
}

}  // namespace file_tasks
}  // namespace file_manager.
