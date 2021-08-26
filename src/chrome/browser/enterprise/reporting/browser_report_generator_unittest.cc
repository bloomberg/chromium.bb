// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/browser_report_generator_desktop.h"

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "base/version.h"
#include "build/branding_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/reporting/extension_request/extension_request_report_throttler_test.h"
#include "chrome/browser/enterprise/reporting/reporting_delegate_factory_desktop.h"
#include "chrome/browser/profiles/profile_attributes_init_params.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/upgrade_detector/build_state.h"
#include "chrome/common/channel_info.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/account_id/account_id.h"
#include "components/enterprise/browser/reporting/browser_report_generator.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/test/browser_task_environment.h"
#include "device_management_backend.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/common/chrome_constants.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && !defined(OS_CHROMEOS)
#include "chrome/test/base/scoped_channel_override.h"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING) && !defined(OS_CHROMEOS)

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/public/browser/plugin_service.h"
#endif

namespace em = enterprise_management;

namespace enterprise_reporting {
namespace {

const char kProfileId[] = "profile_id";
const char kProfileName[] = "profile_name";
const char16_t kProfileName16[] = u"profile_name";

#if BUILDFLAG(ENABLE_PLUGINS)
const char16_t kPluginName16[] = u"plugin_name";
const char16_t kPluginVersion16[] = u"plugin_version";
const char16_t kPluginDescription16[] = u"plugin_description";
const char kPluginFolderPath[] = "plugin_folder_path";
const char kPluginFileName[] = "plugin_file_name";
#if !BUILDFLAG(IS_CHROMEOS_ASH)
const char kPluginName[] = "plugin_name";
const char kPluginVersion[] = "plugin_version";
const char kPluginDescription[] = "plugin_description";
#endif  //  !BUILDFLAG(IS_CHROMEOS_ASH)
#endif  // BUILDFLAG(ENABLE_PLUGINS)

}  // namespace

class BrowserReportGeneratorTest : public ::testing::Test {
 public:
  BrowserReportGeneratorTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()),
        generator_(&delegate_factory_) {
  }
  ~BrowserReportGeneratorTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
#if BUILDFLAG(ENABLE_PLUGINS)
    content::PluginService::GetInstance()->Init();
#endif
  }

  void InitializeUpdate() {
    auto* build_state = g_browser_process->GetBuildState();
    build_state->SetUpdate(BuildState::UpdateType::kNormalUpdate,
                           base::Version("1.2.3.4"), absl::nullopt);
  }

  void InitializeProfile() {
    ProfileAttributesInitParams params;
    params.profile_path =
        profile_manager()->profiles_dir().AppendASCII(kProfileId);
    params.profile_name = kProfileName16;
    profile_manager_.profile_attributes_storage()->AddProfile(
        std::move(params));
  }

  void InitializeIrregularProfiles() {
    profile_manager_.CreateGuestProfile();
    profile_manager_.CreateSystemProfile();

#if BUILDFLAG(IS_CHROMEOS_ASH)
    profile_manager_.CreateTestingProfile(chrome::kInitialProfile);
    profile_manager_.CreateTestingProfile(chrome::kLockScreenAppProfile);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }

  void InitializePlugin() {
#if BUILDFLAG(ENABLE_PLUGINS)
    content::WebPluginInfo info;
    info.name = kPluginName16;
    info.version = kPluginVersion16;
    info.desc = kPluginDescription16;
    info.path = base::FilePath()
                    .AppendASCII(kPluginFolderPath)
                    .AppendASCII(kPluginFileName);

    content::PluginService* plugin_service =
        content::PluginService::GetInstance();
    plugin_service->RegisterInternalPlugin(info, true);
    plugin_service->RefreshPlugins();
#endif  // BUILDFLAG(ENABLE_PLUGINS)
  }

  void InitializeExtensionRequest() {
    throttler()->AddProfile(
        profile_manager()->profiles_dir().AppendASCII(kProfileId));
    throttler()->AddProfile(
        profile_manager()->profiles_dir().AppendASCII("deleted_profile"));
  }

  void GenerateAndVerify() {
    base::RunLoop run_loop;
    generator_.Generate(
        ReportType::kFull,
        base::BindLambdaForTesting(
            [&run_loop](std::unique_ptr<em::BrowserReport> report) {
              EXPECT_TRUE(report.get());

#if BUILDFLAG(IS_CHROMEOS_ASH)
              EXPECT_FALSE(report->has_browser_version());
              EXPECT_FALSE(report->has_channel());
              EXPECT_FALSE(report->has_installed_browser_version());
#else  // BUILDFLAG(IS_CHROMEOS_ASH)
              EXPECT_NE(std::string(), report->browser_version());
              EXPECT_TRUE(report->has_channel());
              const auto* build_state = g_browser_process->GetBuildState();
              if (build_state->update_type() == BuildState::UpdateType::kNone ||
                  !build_state->installed_version()) {
                EXPECT_FALSE(report->has_installed_browser_version());
              } else {
                EXPECT_EQ(report->installed_browser_version(),
                          build_state->installed_version()->GetString());
              }
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
              if (chrome::IsExtendedStableChannel()) {
                EXPECT_TRUE(report->has_is_extended_stable_channel());
                EXPECT_TRUE(report->is_extended_stable_channel());
                EXPECT_EQ(report->channel(), em::Channel::CHANNEL_STABLE);
              } else {
                EXPECT_FALSE(report->has_is_extended_stable_channel());
                EXPECT_NE(report->channel(), em::Channel::CHANNEL_UNKNOWN);
              }
#else   // BUILDFLAG(GOOGLE_CHROME_BRANDING)
              EXPECT_FALSE(report->has_is_extended_stable_channel());
              EXPECT_EQ(report->channel(), em::Channel::CHANNEL_UNKNOWN);
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

              EXPECT_NE(std::string(), report->executable_path());

              EXPECT_EQ(1, report->chrome_user_profile_infos_size());
              em::ChromeUserProfileInfo profile =
                  report->chrome_user_profile_infos(0);
              EXPECT_NE(std::string(), profile.id());
              EXPECT_EQ(kProfileName, profile.name());
              EXPECT_FALSE(profile.is_detail_available());

#if BUILDFLAG(IS_CHROMEOS_ASH)
              EXPECT_EQ(0, report->plugins_size());
#elif BUILDFLAG(ENABLE_PLUGINS)
              EXPECT_LE(1, report->plugins_size());
              em::Plugin plugin = report->plugins(0);
              EXPECT_EQ(kPluginName, plugin.name());
              EXPECT_EQ(kPluginVersion, plugin.version());
              EXPECT_EQ(kPluginFileName, plugin.filename());
              EXPECT_EQ(kPluginDescription, plugin.description());
#endif
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  void GenerateExtensinRequestReportAndVerify(
      const std::vector<std::string>& expected_request_profile_ids) {
    base::RunLoop run_loop;
    generator_.Generate(
        ReportType::kExtensionRequest,
        base::BindLambdaForTesting(
            [&run_loop, &expected_request_profile_ids](
                std::unique_ptr<em::BrowserReport> report) {
              EXPECT_TRUE(report.get());
              EXPECT_NE(std::string(), report->executable_path());
              EXPECT_FALSE(report->has_browser_version());
              EXPECT_FALSE(report->has_channel());
              EXPECT_FALSE(report->has_installed_browser_version());
              EXPECT_EQ(expected_request_profile_ids.size(),
                        static_cast<size_t>(
                            report->chrome_user_profile_infos_size()));
              if (expected_request_profile_ids.size() > 0 &&
                  report->chrome_user_profile_infos_size() > 0) {
                EXPECT_EQ(expected_request_profile_ids[0],
                          report->chrome_user_profile_infos(0).id());
              }
              EXPECT_EQ(0, report->plugins_size());
              run_loop.Quit();
              ExtensionRequestReportThrottler::Get()->Disable();
            }));
    run_loop.Run();
  }

  TestingProfileManager* profile_manager() { return &profile_manager_; }

  ExtensionRequestReportThrottler* throttler() { return throttler_.Get(); }

 private:
  ReportingDelegateFactoryDesktop delegate_factory_;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  BrowserReportGenerator generator_;
  ScopedExtensionRequestReportThrottler throttler_;

  DISALLOW_COPY_AND_ASSIGN(BrowserReportGeneratorTest);
};

TEST_F(BrowserReportGeneratorTest, GenerateBasicReport) {
  InitializeProfile();
  InitializeIrregularProfiles();
  InitializePlugin();
  GenerateAndVerify();
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(BrowserReportGeneratorTest, GenerateBasicReportWithUpdate) {
  InitializeUpdate();
  InitializeProfile();
  InitializeIrregularProfiles();
  InitializePlugin();
  GenerateAndVerify();
}
#endif

TEST_F(BrowserReportGeneratorTest, ExtensionRequestOnly) {
  InitializeUpdate();
  InitializeProfile();
  InitializeIrregularProfiles();
  InitializePlugin();
  InitializeExtensionRequest();
  GenerateExtensinRequestReportAndVerify({profile_manager()
                                              ->profiles_dir()
                                              .AppendASCII(kProfileId)
                                              .AsUTF8Unsafe()});
}

// It's possible that the extension request report is delayed and by the time
// report is generated, the extension request report throttler is disabled.
TEST_F(BrowserReportGeneratorTest, ExtensionRequestOnlyWithoutThrottler) {
  InitializeUpdate();
  InitializeProfile();
  InitializeIrregularProfiles();
  InitializePlugin();
  InitializeExtensionRequest();
  throttler()->Disable();
  GenerateExtensinRequestReportAndVerify({});
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && !defined(OS_CHROMEOS)
TEST_F(BrowserReportGeneratorTest, ExtendedStableChannel) {
  chrome::ScopedChannelOverride channel_override(
      chrome::ScopedChannelOverride::Channel::kExtendedStable);

  ASSERT_TRUE(chrome::IsExtendedStableChannel());
  InitializeProfile();
  InitializeIrregularProfiles();
  InitializePlugin();
  GenerateAndVerify();
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING) && !defined(OS_CHROMEOS)

}  // namespace enterprise_reporting
