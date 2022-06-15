// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/desks_storage/core/desk_template_conversion.h"

#include <string>

#include "ash/public/cpp/desk_template.h"
#include "base/guid.h"
#include "base/json/json_reader.h"
#include "base/json/values_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/account_id/account_id.h"
#include "components/app_constants/constants.h"
#include "components/app_restore/app_launch_info.h"
#include "components/app_restore/app_restore_data.h"
#include "components/app_restore/window_info.h"
#include "components/desks_storage/core/desk_test_util.h"
#include "components/desks_storage/core/saved_desk_builder.h"
#include "components/desks_storage/core/saved_desk_test_util.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_registry_cache_wrapper.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace desks_storage {

namespace {

const int32_t kTestWindowId = 1234567;
const std::string kEmptyJson = "{}";
const std::string kTestUuidBrowser = "040b6112-67f2-4d3c-8ba8-53a117272eba";
constexpr int kBrowserWindowId = 1555;
const std::string kBrowserUrl1 = "https://example.com/";
const std::string kBrowserUrl2 = "https://example.com/2";
const std::string kTestUuidChromeAndProgressive =
    "7f4b7ff0-970a-41bb-aa91-f6c3e2724207";
const std::string kBrowserTemplateName = "BrowserTest";
const std::string kChromePwaTemplateName = "ChromeAppTest";
const std::string kValidTemplateBrowser =
    "{\"version\":1,\"uuid\":\"" + kTestUuidBrowser + "\",\"name\":\"" +
    kBrowserTemplateName +
    "\",\"created_time_usec\":\"1633535632\",\"updated_time_usec\": "
    "\"1633535632\",\"desk_type\":\"TEMPLATE\",\"desk\":{\"apps\":[{\"window_"
    "bound\":{\"left\":0,\"top\":1,\"height\":121,\"width\":120},\"window_"
    "state\":\"NORMAL\",\"z_index\":1,\"app_type\":\"BROWSER\",\"tabs\":[{"
    "\"url\":\"" +
    kBrowserUrl1 +
    "\"},{\"url\":\"https://"
    "example.com/"
    "2\"}],\"tab_groups\":[{\"first_"
    "index\":1,\"last_index\":2,\"title\":\"sample_tab_"
    "group\",\"color\":\"GREY\",\"is_collapsed\":false}],\"active_tab_index\":"
    "1,\"window_id\":0,"
    "\"display_id\":\"100\",\"event_flag\":0,\"pre_minimized_window_state\":"
    "\"NORMAL\"}]}}";
const std::string kValidTemplateChromeAndProgressive =
    "{\"version\":1,\"uuid\":\"" + kTestUuidChromeAndProgressive +
    "\",\"name\":\"" + kChromePwaTemplateName +
    "\",\"created_time_usec\":\"1633535632000\",\"updated_time_usec\": "
    "\"1633535632\",\"desk_type\":\"SAVE_AND_RECALL\",\"desk\":{\"apps\":[{"
    "\"window_"
    "bound\":{"
    "\"left\":200,\"top\":200,\"height\":1000,\"width\":1000},\"window_state\":"
    "\"PRIMARY_SNAPPED\",\"z_index\":2,\"app_type\":\"CHROME_APP\",\"app_id\":"
    "\"" +
    desk_test_util::kTestChromeAppId1 +
    "\",\"window_id\":0,\"display_id\":\"100\",\"event_flag\":0,\"pre_"
    "minimized_window_state\":\"NORMAL\", \"snap_percent\":75},{\"window_"
    "bound\":{\"left\":0,\"top\":0,\"height\":120,\"width\":120},\"window_"
    "state\":\"NORMAL\",\"z_index\":1,\"app_type\":\"CHROME_APP\",\"app_id\":"
    "\"" +
    desk_test_util::kTestPwaAppId1 +
    "\",\"window_id\":1,\"display_id\":"
    "\"100\",\"event_flag\":0,\"pre_minimized_window_state\":\"NORMAL\"}]}}";
const std::string kTemplateWithoutType =
    "{\"version\":1,\"uuid\":\"" + kTestUuidBrowser + "\",\"name\":\"" +
    kBrowserTemplateName +
    "\",\"created_time_usec\":\"1633535632\",\"updated_time_usec\": "
    "\"1633535632\",\"desk\":{\"apps\":[{\"window_"
    "bound\":{\"left\":0,\"top\":1,\"height\":121,\"width\":120},\"window_"
    "state\":\"NORMAL\",\"z_index\":1,\"app_type\":\"BROWSER\",\"tabs\":[{"
    "\"url\":\"" +
    kBrowserUrl1 + "\"},{\"url\":\"" + kBrowserUrl1 +
    "\"}],\"tab_groups\":[{\"first_"
    "index\":1,\"last_index\":2,\"title\":\"sample_tab_"
    "group\",\"color\":\"GREY\",\"is_collapsed\":false}],\"active_tab_index\":"
    "1,\"window_id\":0,"
    "\"display_id\":\"100\",\"event_flag\":0,\"pre_minimized_window_state\":"
    "\"NORMAL\"}]}}";
const constexpr char16_t kSampleTabGroupTitle[] = u"sample_tab_group";

app_restore::TabGroupInfo MakeSampleTabGroup() {
  return app_restore::TabGroupInfo(
      {1, 2}, tab_groups::TabGroupVisualData(
                  kSampleTabGroupTitle, tab_groups::TabGroupColorId::kGrey));
}

}  // namespace

class DeskTemplateConversionTest : public testing::Test {
 public:
  DeskTemplateConversionTest(const DeskTemplateConversionTest&) = delete;
  DeskTemplateConversionTest& operator=(const DeskTemplateConversionTest&) =
      delete;

 protected:
  DeskTemplateConversionTest()
      : account_id_(AccountId::FromUserEmail("test@gmail.com")),
        cache_(std::make_unique<apps::AppRegistryCache>()) {}

  void SetUp() override {
    desk_test_util::PopulateAppRegistryCache(account_id_, cache_.get());
  }

  AccountId account_id_;

 private:
  std::unique_ptr<apps::AppRegistryCache> cache_;
};

TEST_F(DeskTemplateConversionTest, ParseBrowserTemplate) {
  base::StringPiece raw_json = base::StringPiece(kValidTemplateBrowser);
  base::JSONReader::ValueWithError parsed_json =
      base::JSONReader::ReadAndReturnValueWithError(raw_json);

  EXPECT_TRUE(parsed_json.value.has_value());
  EXPECT_TRUE(parsed_json.value->is_dict());

  std::unique_ptr<ash::DeskTemplate> dt =
      desk_template_conversion::ParseDeskTemplateFromSource(
          parsed_json.value.value(), ash::DeskTemplateSource::kPolicy);

  EXPECT_TRUE(dt != nullptr);
  EXPECT_EQ(dt->uuid(), base::GUID::ParseCaseInsensitive(kTestUuidBrowser));
  EXPECT_EQ(dt->created_time(),
            desk_template_conversion::ProtoTimeToTime(1633535632));
  EXPECT_EQ(dt->template_name(), base::UTF8ToUTF16(kBrowserTemplateName));

  const app_restore::RestoreData* rd = dt->desk_restore_data();

  EXPECT_TRUE(rd != nullptr);
  EXPECT_EQ(rd->app_id_to_launch_list().size(), 1UL);
  EXPECT_NE(rd->app_id_to_launch_list().find(app_constants::kChromeAppId),
            rd->app_id_to_launch_list().end());

  const app_restore::AppRestoreData* ard =
      rd->GetAppRestoreData(app_constants::kChromeAppId, 0);
  EXPECT_TRUE(ard != nullptr);
  EXPECT_TRUE(ard->display_id.has_value());
  EXPECT_EQ(ard->display_id.value(), 100L);
  std::unique_ptr<app_restore::AppLaunchInfo> ali =
      ard->GetAppLaunchInfo(app_constants::kChromeAppId, 0);
  std::unique_ptr<app_restore::WindowInfo> wi = ard->GetWindowInfo();
  EXPECT_TRUE(ali != nullptr);
  EXPECT_TRUE(wi != nullptr);
  EXPECT_TRUE(ali->window_id.has_value());
  EXPECT_EQ(ali->window_id.value(), 0);
  EXPECT_TRUE(ali->display_id.has_value());
  EXPECT_EQ(ali->display_id.value(), 100L);
  // Can't test active_tab_index since GetAppLaunchInfo returns an entry without
  // it
  // https://osscs.corp.google.com/chromium/chromium/src/+/main:components/app_restore/app_restore_data.cc
  EXPECT_TRUE(ali->urls.has_value());
  EXPECT_EQ(ali->urls.value()[0].spec(), kBrowserUrl1);
  EXPECT_EQ(ali->urls.value()[1].spec(), kBrowserUrl2);
  EXPECT_TRUE(ali->tab_group_infos.has_value());
  EXPECT_EQ(ali->tab_group_infos.value()[0], MakeSampleTabGroup());
  EXPECT_TRUE(wi->window_state_type.has_value());
  EXPECT_EQ(wi->window_state_type.value(), chromeos::WindowStateType::kNormal);
  EXPECT_TRUE(wi->pre_minimized_show_state_type.has_value());
  EXPECT_EQ(wi->pre_minimized_show_state_type.value(),
            ui::WindowShowState::SHOW_STATE_NORMAL);
  EXPECT_TRUE(wi->current_bounds.has_value());
  EXPECT_EQ(wi->current_bounds.value().x(), 0);
  EXPECT_EQ(wi->current_bounds.value().y(), 1);
  EXPECT_EQ(wi->current_bounds.value().height(), 121);
  EXPECT_EQ(wi->current_bounds.value().width(), 120);
}

TEST_F(DeskTemplateConversionTest, ParseChromePwaTemplate) {
  base::StringPiece raw_json =
      base::StringPiece(kValidTemplateChromeAndProgressive);
  base::JSONReader::ValueWithError parsed_json =
      base::JSONReader::ReadAndReturnValueWithError(raw_json);

  EXPECT_TRUE(parsed_json.value.has_value());
  EXPECT_TRUE(parsed_json.value->is_dict());

  std::unique_ptr<ash::DeskTemplate> dt =
      desk_template_conversion::ParseDeskTemplateFromSource(
          parsed_json.value.value(), ash::DeskTemplateSource::kPolicy);

  EXPECT_TRUE(dt != nullptr);
  EXPECT_EQ(dt->uuid(),
            base::GUID::ParseCaseInsensitive(kTestUuidChromeAndProgressive));
  EXPECT_EQ(dt->created_time(),
            desk_template_conversion::ProtoTimeToTime(1633535632000LL));
  EXPECT_EQ(dt->template_name(), base::UTF8ToUTF16(kChromePwaTemplateName));

  const app_restore::RestoreData* rd = dt->desk_restore_data();

  EXPECT_TRUE(rd != nullptr);
  EXPECT_EQ(rd->app_id_to_launch_list().size(), 2UL);
  EXPECT_NE(rd->app_id_to_launch_list().find(desk_test_util::kTestChromeAppId1),
            rd->app_id_to_launch_list().end());
  EXPECT_NE(rd->app_id_to_launch_list().find(desk_test_util::kTestPwaAppId1),
            rd->app_id_to_launch_list().end());

  const app_restore::AppRestoreData* ard_chrome =
      rd->GetAppRestoreData(desk_test_util::kTestChromeAppId1, 0);
  const app_restore::AppRestoreData* ard_pwa =
      rd->GetAppRestoreData(desk_test_util::kTestPwaAppId1, 1);
  EXPECT_TRUE(ard_chrome != nullptr);
  EXPECT_TRUE(ard_pwa != nullptr);
  std::unique_ptr<app_restore::AppLaunchInfo> ali_chrome =
      ard_chrome->GetAppLaunchInfo(desk_test_util::kTestChromeAppId1, 0);
  std::unique_ptr<app_restore::AppLaunchInfo> ali_pwa =
      ard_pwa->GetAppLaunchInfo(desk_test_util::kTestPwaAppId1, 1);
  std::unique_ptr<app_restore::WindowInfo> wi_chrome =
      ard_chrome->GetWindowInfo();
  std::unique_ptr<app_restore::WindowInfo> wi_pwa = ard_pwa->GetWindowInfo();

  EXPECT_TRUE(ali_chrome != nullptr);
  EXPECT_TRUE(ali_chrome->window_id.has_value());
  EXPECT_EQ(ali_chrome->window_id.value(), 0);
  EXPECT_TRUE(ali_chrome->display_id.has_value());
  EXPECT_EQ(ali_chrome->display_id.value(), 100L);
  EXPECT_FALSE(ali_chrome->active_tab_index.has_value());
  EXPECT_FALSE(ali_chrome->urls.has_value());

  EXPECT_TRUE(ali_pwa != nullptr);

  EXPECT_TRUE(ali_pwa != nullptr);
  EXPECT_TRUE(ali_pwa->window_id.has_value());
  EXPECT_EQ(ali_pwa->window_id.value(), 1);
  EXPECT_TRUE(ali_pwa->display_id.has_value());
  EXPECT_EQ(ali_pwa->display_id.value(), 100L);
  EXPECT_FALSE(ali_pwa->active_tab_index.has_value());
  EXPECT_FALSE(ali_pwa->urls.has_value());

  EXPECT_TRUE(wi_chrome != nullptr);
  EXPECT_TRUE(wi_chrome->window_state_type.has_value());
  EXPECT_EQ(wi_chrome->window_state_type.value(),
            chromeos::WindowStateType::kPrimarySnapped);
  EXPECT_TRUE(wi_chrome->pre_minimized_show_state_type.has_value());
  EXPECT_EQ(wi_chrome->pre_minimized_show_state_type.value(),
            ui::WindowShowState::SHOW_STATE_NORMAL);
  EXPECT_TRUE(wi_chrome->current_bounds.has_value());
  EXPECT_EQ(wi_chrome->current_bounds.value().x(), 200);
  EXPECT_EQ(wi_chrome->current_bounds.value().y(), 200);
  EXPECT_EQ(wi_chrome->current_bounds.value().height(), 1000);
  EXPECT_EQ(wi_chrome->current_bounds.value().width(), 1000);

  EXPECT_TRUE(wi_pwa != nullptr);
  EXPECT_TRUE(wi_pwa->window_state_type.has_value());
  EXPECT_EQ(wi_pwa->window_state_type.value(),
            chromeos::WindowStateType::kNormal);
  EXPECT_TRUE(wi_pwa->pre_minimized_show_state_type.has_value());
  EXPECT_EQ(wi_pwa->pre_minimized_show_state_type.value(),
            ui::WindowShowState::SHOW_STATE_NORMAL);
  EXPECT_TRUE(wi_pwa->current_bounds.has_value());
  EXPECT_EQ(wi_pwa->current_bounds.value().x(), 0);
  EXPECT_EQ(wi_pwa->current_bounds.value().y(), 0);
  EXPECT_EQ(wi_pwa->current_bounds.value().height(), 120);
  EXPECT_EQ(wi_pwa->current_bounds.value().width(), 120);
}

TEST_F(DeskTemplateConversionTest, EmptyJsonTest) {
  base::StringPiece raw_json = base::StringPiece(kEmptyJson);
  base::JSONReader::ValueWithError parsed_json =
      base::JSONReader::ReadAndReturnValueWithError(raw_json);

  EXPECT_TRUE(parsed_json.value.has_value());
  EXPECT_TRUE(parsed_json.value->is_dict());

  std::unique_ptr<ash::DeskTemplate> dt =
      desk_template_conversion::ParseDeskTemplateFromSource(
          parsed_json.value.value(), ash::DeskTemplateSource::kPolicy);
  EXPECT_TRUE(dt == nullptr);
}

TEST_F(DeskTemplateConversionTest, ParsesWithDefaultValueSetToTemplates) {
  base::StringPiece raw_json = base::StringPiece(kTemplateWithoutType);
  base::JSONReader::ValueWithError parsed_json =
      base::JSONReader::ReadAndReturnValueWithError(raw_json);

  EXPECT_TRUE(parsed_json.value.has_value());
  EXPECT_TRUE(parsed_json.value->is_dict());

  std::unique_ptr<ash::DeskTemplate> dt =
      desk_template_conversion::ParseDeskTemplateFromSource(
          parsed_json.value.value(), ash::DeskTemplateSource::kPolicy);
  EXPECT_TRUE(dt);
  EXPECT_EQ(ash::DeskTemplateType::kTemplate, dt->type());
}

TEST_F(DeskTemplateConversionTest, DeskTemplateFromJsonBrowserTest) {
  base::StringPiece raw_json = base::StringPiece(kValidTemplateBrowser);
  base::JSONReader::ValueWithError parsed_json =
      base::JSONReader::ReadAndReturnValueWithError(raw_json);

  EXPECT_TRUE(parsed_json.value.has_value());
  EXPECT_TRUE(parsed_json.value->is_dict());

  std::unique_ptr<ash::DeskTemplate> desk_template =
      desk_template_conversion::ParseDeskTemplateFromSource(
          parsed_json.value.value(), ash::DeskTemplateSource::kPolicy);

  apps::AppRegistryCache* app_cache =
      apps::AppRegistryCacheWrapper::Get().GetAppRegistryCache(account_id_);

  base::Value desk_template_value =
      desk_template_conversion::SerializeDeskTemplateAsPolicy(
          desk_template.get(), app_cache);

  EXPECT_EQ(parsed_json.value, desk_template_value);
}

TEST_F(DeskTemplateConversionTest, ToJsonIgnoreUnsupportedApp) {
  base::StringPiece raw_json = base::StringPiece(kValidTemplateBrowser);
  base::JSONReader::ValueWithError parsed_json =
      base::JSONReader::ReadAndReturnValueWithError(raw_json);

  EXPECT_TRUE(parsed_json.value.has_value());
  EXPECT_TRUE(parsed_json.value->is_dict());

  std::unique_ptr<ash::DeskTemplate> desk_template =
      desk_template_conversion::ParseDeskTemplateFromSource(
          parsed_json.value.value(), ash::DeskTemplateSource::kUser);

  // Adding this unsupported app should not change the serialized JSON content.
  saved_desk_test_util::AddGenericAppWindow(
      kTestWindowId, desk_test_util::kTestUnsupportedAppId,
      desk_template->mutable_desk_restore_data());

  apps::AppRegistryCache* app_cache =
      apps::AppRegistryCacheWrapper::Get().GetAppRegistryCache(account_id_);

  base::Value desk_template_value =
      desk_template_conversion::SerializeDeskTemplateAsPolicy(
          desk_template.get(), app_cache);

  EXPECT_EQ(parsed_json.value, desk_template_value);
}

TEST_F(DeskTemplateConversionTest, DeskTemplateFromJsonAppTest) {
  base::StringPiece raw_json =
      base::StringPiece(kValidTemplateChromeAndProgressive);
  base::JSONReader::ValueWithError parsed_json =
      base::JSONReader::ReadAndReturnValueWithError(raw_json);

  EXPECT_TRUE(parsed_json.value.has_value());
  EXPECT_TRUE(parsed_json.value->is_dict());

  std::unique_ptr<ash::DeskTemplate> desk_template =
      desk_template_conversion::ParseDeskTemplateFromSource(
          parsed_json.value.value(), ash::DeskTemplateSource::kPolicy);

  apps::AppRegistryCache* app_cache =
      apps::AppRegistryCacheWrapper::Get().GetAppRegistryCache(account_id_);

  base::Value desk_template_value =
      desk_template_conversion::SerializeDeskTemplateAsPolicy(
          desk_template.get(), app_cache);

  EXPECT_EQ(parsed_json.value, desk_template_value);
}

TEST_F(DeskTemplateConversionTest, EnsureLacrosBrowserWindowsSavedProperly) {
  base::Time created_time = base::Time::Now();
  std::unique_ptr<ash::DeskTemplate> desk_template =
      SavedDeskBuilder()
          .SetUuid(kTestUuidBrowser)
          .SetName(kBrowserTemplateName)
          .SetType(ash::DeskTemplateType::kSaveAndRecall)
          .SetCreatedTime(created_time)
          .AddLacrosBrowserAppWindow(kBrowserWindowId,
                                     {GURL(kBrowserUrl1), GURL(kBrowserUrl2)})
          .Build();

  apps::AppRegistryCache* app_cache =
      apps::AppRegistryCacheWrapper::Get().GetAppRegistryCache(account_id_);

  base::Value desk_template_value =
      desk_template_conversion::SerializeDeskTemplateAsPolicy(
          desk_template.get(), app_cache);

  base::Value::Dict expected_browser_tab1;
  expected_browser_tab1.Set("url", base::Value(kBrowserUrl1));
  base::Value::Dict expected_browser_tab2;
  expected_browser_tab2.Set("url", base::Value(kBrowserUrl2));
  base::Value::List expected_tab_list;
  expected_tab_list.Append(std::move(expected_browser_tab1));
  expected_tab_list.Append(std::move(expected_browser_tab2));

  base::Value::Dict expected_browser_app_value;
  expected_browser_app_value.Set("app_type", base::Value("BROWSER"));
  expected_browser_app_value.Set("event_flag", base::Value(0));
  expected_browser_app_value.Set("window_id", base::Value(kBrowserWindowId));
  expected_browser_app_value.Set("tabs", std::move(expected_tab_list));

  base::Value::List expected_app_list;
  expected_app_list.Append(std::move(expected_browser_app_value));

  base::Value::Dict expected_desk_value;
  expected_desk_value.Set("apps", std::move(expected_app_list));

  base::Value::Dict expected_value;
  expected_value.Set("version", base::Value(1));
  expected_value.Set("uuid", base::Value(kTestUuidBrowser));
  expected_value.Set("name", base::Value(kBrowserTemplateName));
  expected_value.Set("created_time_usec", base::TimeToValue(created_time));
  expected_value.Set("updated_time_usec",
                     base::TimeToValue(desk_template->GetLastUpdatedTime()));
  expected_value.Set("desk_type", base::Value("SAVE_AND_RECALL"));
  expected_value.Set("desk", std::move(expected_desk_value));

  EXPECT_EQ(expected_value, desk_template_value);
}

}  // namespace desks_storage
