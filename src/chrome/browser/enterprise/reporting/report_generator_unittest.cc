// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/report_generator.h"

#include <set>

#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/reporting/report_request_definition.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/account_id/account_id.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "components/arc/arc_prefs.h"
#include "components/arc/test/fake_app_instance.h"
#endif

namespace em = enterprise_management;

namespace enterprise_reporting {
namespace {

constexpr char kProfile[] = "Profile";

const char kPluginName[] = "plugin";
const char kPluginVersion[] = "1.0";
const char kPluginDescription[] = "This is a plugin.";
const char kPluginFileName[] = "file_name";

#if defined(OS_CHROMEOS)
const char kArcAppName1[] = "app_name1";
const char kArcPackageName1[] = "package_name1";
const char kArcActivityName1[] = "activity_name1";
const char kArcAppName2[] = "app_name2";
const char kArcPackageName2[] = "package_name2";
const char kArcActivityName2[] = "activity_name2";
#endif

#if !defined(OS_CHROMEOS)
// We only upload serial number on Windows.
void VerifySerialNumber(const std::string& serial_number) {
#if defined(OS_WIN)
  EXPECT_NE(std::string(), serial_number);
#else
  EXPECT_EQ(std::string(), serial_number);
#endif
}
#endif

// Controls the way of Profile creation which affects report.
enum ProfileStatus {
  // Idle Profile does not generate full report.
  kIdle,
  // Active Profile generates full report.
  kActive,
  // Active Profile generate large full report.
  kActiveWithContent,
};

// Verify the name is in the set. Remove the name from the set afterwards.
void FindAndRemoveProfileName(std::set<std::string>* names,
                              const std::string& name) {
  auto it = names->find(name);
  EXPECT_NE(names->end(), it);
  names->erase(it);
}

void AddExtensionToProfile(TestingProfile* profile) {
  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(profile);

  std::string extension_name =
      "a super super super super super super super super super super super "
      "super super super super super super long extension name";
  extension_registry->AddEnabled(extensions::ExtensionBuilder(extension_name)
                                     .SetID("abcdefghijklmnoabcdefghijklmnoab")
                                     .Build());
}

#if defined(OS_CHROMEOS)

arc::mojom::AppInfo CreateArcApp(const std::string& app_name,
                                 const std::string& package_name,
                                 const std::string& activity_name) {
  arc::mojom::AppInfo app;
  app.name = app_name;
  app.package_name = package_name;
  app.activity = activity_name;
  app.suspended = false;
  app.sticky = true;
  app.notifications_enabled = true;
  return app;
}

arc::mojom::ArcPackageInfoPtr CreateArcPackage(
    const std::string& package_name) {
  return arc::mojom::ArcPackageInfo::New(
      package_name, 0 /* package_version */, 0 /* last_backup_android_id */,
      0 /* last_backup_time */, false /* sync */);
}

void AddArcPackageAndApp(ArcAppTest* arc_app_test,
                         const std::string& app_name,
                         const std::string& package_name,
                         const std::string& activity_name) {
  arc::mojom::ArcPackageInfoPtr package = CreateArcPackage(package_name);
  arc_app_test->app_instance()->SendPackageAdded(std::move(package));

  arc::mojom::AppInfo app = CreateArcApp(app_name, package_name, activity_name);
  arc_app_test->app_instance()->SendAppAdded(app);
}

#endif

}  // namespace

class ReportGeneratorTest : public ::testing::Test {
 public:
  using ReportRequest = definition::ReportRequest;

  ReportGeneratorTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  ~ReportGeneratorTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());

    profile_manager_.CreateGuestProfile();
    profile_manager_.CreateSystemProfile();

    content::PluginService::GetInstance()->Init();
  }

  // Creates |number| of Profiles. Returns the set of their names. The profile
  // names begin with Profile|start_index|. Profile instances are created if
  // |is_active| is true. Otherwise, information is only put into
  // ProfileAttributesStorage.
  std::set<std::string> CreateProfiles(int number,
                                       ProfileStatus status,
                                       int start_index = 0,
                                       bool with_extension = false) {
    std::set<std::string> profile_names;
    for (int i = start_index; i < number; i++) {
      std::string profile_name =
          std::string(kProfile) + base::NumberToString(i);
      switch (status) {
        case kIdle:
          profile_manager_.profile_attributes_storage()->AddProfile(
              profile_manager()->profiles_dir().AppendASCII(profile_name),
              base::ASCIIToUTF16(profile_name), std::string(), base::string16(),
              false, 0, std::string(), EmptyAccountId());
          break;
        case kActive:
          profile_manager_.CreateTestingProfile(profile_name);
          break;
        case kActiveWithContent:
          TestingProfile* profile =
              profile_manager_.CreateTestingProfile(profile_name);
          AddExtensionToProfile(profile);
          break;
      }
      profile_names.insert(profile_name);
    }
    return profile_names;
  }

  void CreatePlugin() {
    content::WebPluginInfo info;
    info.name = base::ASCIIToUTF16(kPluginName);
    info.version = base::ASCIIToUTF16(kPluginVersion);
    info.desc = base::ASCIIToUTF16(kPluginDescription);
    info.path =
        base::FilePath().AppendASCII("path").AppendASCII(kPluginFileName);
    content::PluginService* plugin_service =
        content::PluginService::GetInstance();
    plugin_service->RegisterInternalPlugin(info, true);
    plugin_service->RefreshPlugins();
  }

  std::vector<std::unique_ptr<ReportRequest>> GenerateRequests(
      bool with_profiles) {
    histogram_tester_ = std::make_unique<base::HistogramTester>();
    base::RunLoop run_loop;
    std::vector<std::unique_ptr<ReportRequest>> rets;
    generator_.Generate(
        with_profiles,
        base::BindLambdaForTesting(
            [&run_loop, &rets](ReportGenerator::ReportRequests requests) {
              while (!requests.empty()) {
                rets.push_back(std::move(requests.front()));
                requests.pop();
              }
              run_loop.Quit();
            }));
    run_loop.Run();
    if (with_profiles)
      VerifyMetrics(rets);  // Only generated for reports with profiles.
    return rets;
  }

  // Verify the profile report matches actual Profile setup.
  void VerifyProfileReport(const std::set<std::string>& active_profiles_names,
                           const std::set<std::string>& inactive_profiles_names,
                           const em::BrowserReport& actual_browser_report) {
    int expected_profile_number =
        active_profiles_names.size() + inactive_profiles_names.size();
    EXPECT_EQ(expected_profile_number,
              actual_browser_report.chrome_user_profile_infos_size());

    auto mutable_active_profiles_names(active_profiles_names);
    auto mutable_inactive_profiles_names(inactive_profiles_names);
    for (int i = 0; i < expected_profile_number; i++) {
      auto actual_profile_info =
          actual_browser_report.chrome_user_profile_infos(i);
      std::string actual_profile_name = actual_profile_info.name();

      // Verify that the profile id is set as profile path.
      EXPECT_EQ(GetProfilePath(actual_profile_name), actual_profile_info.id());

      EXPECT_TRUE(actual_profile_info.has_is_full_report());

      // Activate profiles have full report while the inactive ones don't.
      if (actual_profile_info.is_full_report())
        FindAndRemoveProfileName(&mutable_active_profiles_names,
                                 actual_profile_name);
      else
        FindAndRemoveProfileName(&mutable_inactive_profiles_names,
                                 actual_profile_name);
    }
  }

  void VerifyMetrics(std::vector<std::unique_ptr<ReportRequest>>& rets) {
    histogram_tester_->ExpectUniqueSample(
        "Enterprise.CloudReportingRequestCount", rets.size(), 1);
    histogram_tester_->ExpectUniqueSample(
        "Enterprise.CloudReportingBasicRequestSize",
        /*basic request size floor to KB*/ 0, 1);
  }

  TestingProfileManager* profile_manager() { return &profile_manager_; }
  ReportGenerator* generator() { return &generator_; }
  base::HistogramTester* histogram_tester() { return histogram_tester_.get(); }

 private:
  // Returns a Profile's path, preferring the path of the active profile for
  // user |profile_name| and falling back to a generated path based on it.
  std::string GetProfilePath(const std::string& profile_name) {
    // Find active Profile and return its path.
    for (auto* profile :
         profile_manager_.profile_manager()->GetLoadedProfiles()) {
      if (profile->GetProfileUserName() == profile_name)
        return profile->GetPath().AsUTF8Unsafe();
    }

    // No active profile, profile path must be generated by us, return path
    // with same generator.
    return profile_manager_.profiles_dir()
        .AppendASCII(profile_name)
        .AsUTF8Unsafe();
  }

  ReportGenerator generator_;

  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;

  DISALLOW_COPY_AND_ASSIGN(ReportGeneratorTest);
};

TEST_F(ReportGeneratorTest, GenerateBasicReport) {
  auto profile_names = CreateProfiles(/*number*/ 2, kIdle);
  CreatePlugin();

  auto requests = GenerateRequests(/*with_profiles=*/true);
  EXPECT_EQ(1u, requests.size());

  auto* basic_request = requests[0].get();

  // In the ChromeOsUserReportRequest for Chrome OS, these fields are not
  // existing. Therefore, they are skipped according to current environment.
#if !defined(OS_CHROMEOS)
  EXPECT_NE(std::string(), basic_request->computer_name());
  EXPECT_NE(std::string(), basic_request->os_user_name());
  VerifySerialNumber(basic_request->serial_number());

  EXPECT_TRUE(basic_request->has_os_report());
  auto& os_report = basic_request->os_report();
  EXPECT_NE(std::string(), os_report.name());
  EXPECT_NE(std::string(), os_report.arch());
  EXPECT_NE(std::string(), os_report.version());
#endif

  EXPECT_TRUE(basic_request->has_browser_report());
  auto& browser_report = basic_request->browser_report();
#if defined(OS_CHROMEOS)
  EXPECT_FALSE(browser_report.has_browser_version());
  EXPECT_FALSE(browser_report.has_channel());
#else
  EXPECT_NE(std::string(), browser_report.browser_version());
  EXPECT_TRUE(browser_report.has_channel());
#endif
  EXPECT_NE(std::string(), browser_report.executable_path());

#if defined(OS_CHROMEOS)
  EXPECT_EQ(0, browser_report.plugins_size());
#else
  // There might be other plugins like PDF plugin, however, our fake plugin
  // should be the first one in the report.
  EXPECT_LE(1, browser_report.plugins_size());
  EXPECT_EQ(kPluginName, browser_report.plugins(0).name());
  EXPECT_EQ(kPluginVersion, browser_report.plugins(0).version());
  EXPECT_EQ(kPluginDescription, browser_report.plugins(0).description());
  EXPECT_EQ(kPluginFileName, browser_report.plugins(0).filename());
#endif

  VerifyProfileReport(/*active_profile_names*/ std::set<std::string>(),
                      profile_names, browser_report);
}

TEST_F(ReportGeneratorTest, GenerateWithoutProfiles) {
  auto profile_names = CreateProfiles(/*number*/ 2, kActive);
  CreatePlugin();

  auto requests = GenerateRequests(/*with_profiles=*/false);
  EXPECT_EQ(1u, requests.size());

  auto* basic_request = requests[0].get();

  // In the ChromeOsUserReportRequest for Chrome OS, these fields are not
  // existing. Therefore, they are skipped according to current environment.
#if !defined(OS_CHROMEOS)
  EXPECT_NE(std::string(), basic_request->computer_name());
  EXPECT_NE(std::string(), basic_request->os_user_name());
  VerifySerialNumber(basic_request->serial_number());

  EXPECT_TRUE(basic_request->has_os_report());
  auto& os_report = basic_request->os_report();
  EXPECT_NE(std::string(), os_report.name());
  EXPECT_NE(std::string(), os_report.arch());
  EXPECT_NE(std::string(), os_report.version());
#endif

  EXPECT_TRUE(basic_request->has_browser_report());
  auto& browser_report = basic_request->browser_report();
#if defined(OS_CHROMEOS)
  EXPECT_FALSE(browser_report.has_browser_version());
  EXPECT_FALSE(browser_report.has_channel());
#else
  EXPECT_NE(std::string(), browser_report.browser_version());
  EXPECT_TRUE(browser_report.has_channel());
#endif
  EXPECT_NE(std::string(), browser_report.executable_path());

#if defined(OS_CHROMEOS)
  EXPECT_EQ(0, browser_report.plugins_size());
#else
  // There might be other plugins like PDF plugin, however, our fake plugin
  // should be the first one in the report.
  EXPECT_LE(1, browser_report.plugins_size());
  EXPECT_EQ(kPluginName, browser_report.plugins(0).name());
  EXPECT_EQ(kPluginVersion, browser_report.plugins(0).version());
  EXPECT_EQ(kPluginDescription, browser_report.plugins(0).description());
  EXPECT_EQ(kPluginFileName, browser_report.plugins(0).filename());
#endif

  VerifyProfileReport(/*active_profile_names*/ std::set<std::string>(),
                      profile_names, browser_report);
}

#if defined(OS_CHROMEOS)

TEST_F(ReportGeneratorTest, ReportArcAppInChromeOS) {
  ArcAppTest arc_app_test;
  TestingProfile primary_profile;
  arc_app_test.SetUp(&primary_profile);

  // Create two Arc applications in primary profile.
  AddArcPackageAndApp(&arc_app_test, kArcAppName1, kArcPackageName1,
                      kArcActivityName1);
  AddArcPackageAndApp(&arc_app_test, kArcAppName2, kArcPackageName2,
                      kArcActivityName2);

  EXPECT_EQ(2u, arc_app_test.arc_app_list_prefs()->GetAppIds().size());

  // Verify the Arc application information in the report is same as the test
  // data.
  auto requests = GenerateRequests(/*with_profiles=*/true);
  EXPECT_EQ(1u, requests.size());

  ReportRequest* request = requests.front().get();
  EXPECT_EQ(2, request->android_app_infos_size());
  em::AndroidAppInfo app_info1 = request->android_app_infos(1);
  EXPECT_EQ(kArcAppName1, app_info1.app_name());
  em::AndroidAppInfo app_info2 = request->android_app_infos(0);
  EXPECT_EQ(kArcAppName2, app_info2.app_name());

  // Generate the Arc application information again and make sure the report
  // remains the same.
  requests = GenerateRequests(/*with_profiles=*/true);
  EXPECT_EQ(1u, requests.size());

  request = requests.front().get();
  EXPECT_EQ(2, request->android_app_infos_size());
  app_info1 = request->android_app_infos(1);
  EXPECT_EQ(kArcAppName1, app_info1.app_name());
  app_info2 = request->android_app_infos(0);
  EXPECT_EQ(kArcAppName2, app_info2.app_name());

  arc_app_test.TearDown();
}

TEST_F(ReportGeneratorTest, ArcPlayStoreDisabled) {
  ArcAppTest arc_app_test;
  TestingProfile primary_profile;
  arc_app_test.SetUp(&primary_profile);

  // Create two Arc applications in primary profile.
  AddArcPackageAndApp(&arc_app_test, kArcAppName1, kArcPackageName1,
                      kArcActivityName1);
  AddArcPackageAndApp(&arc_app_test, kArcAppName2, kArcPackageName2,
                      kArcActivityName2);

  EXPECT_EQ(2u, arc_app_test.arc_app_list_prefs()->GetAppIds().size());

  // No Arc application information is reported after the Arc Play Store
  // support for given profile is disabled.
  primary_profile.GetPrefs()->SetBoolean(arc::prefs::kArcEnabled, false);
  auto requests = GenerateRequests(/*with_profiles=*/true);
  EXPECT_EQ(1u, requests.size());

  ReportRequest* request = requests.front().get();
  EXPECT_EQ(0, request->android_app_infos_size());

  arc_app_test.TearDown();
}

#endif

}  // namespace enterprise_reporting
