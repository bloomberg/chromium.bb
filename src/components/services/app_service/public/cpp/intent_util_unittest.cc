// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/intent_util.h"

#include "base/values.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/cpp/intent_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kFilterUrl[] = "https://www.google.com/";
}

class IntentUtilTest : public testing::Test {
 protected:
  apps::mojom::ConditionPtr CreateMultiConditionValuesCondition() {
    std::vector<apps::mojom::ConditionValuePtr> condition_values;
    condition_values.push_back(apps_util::MakeConditionValue(
        "https", apps::mojom::PatternMatchType::kNone));
    condition_values.push_back(apps_util::MakeConditionValue(
        "http", apps::mojom::PatternMatchType::kNone));
    auto condition = apps_util::MakeCondition(
        apps::mojom::ConditionType::kScheme, std::move(condition_values));
    return condition;
  }

  // TODO(crbug.com/1092784): Add other things for a completed intent.
  apps::mojom::IntentPtr CreateShareIntent(const std::string& mime_type) {
    auto intent = apps::mojom::Intent::New();
    intent->action = apps_util::kIntentActionSend;
    intent->mime_type = mime_type;
    return intent;
  }

  apps::mojom::IntentPtr CreateIntent(
      const std::string& action,
      const GURL& url,
      const std::string& mime_type,
      std::vector<apps::mojom::IntentFilePtr> files,
      const std::string& activity_name,
      const GURL& drive_share_url,
      const std::string& share_text,
      const std::string& share_title,
      const std::string& start_type,
      const std::vector<std::string>& categories,
      const std::string& data,
      apps::mojom::OptionalBool ui_bypassed,
      const base::flat_map<std::string, std::string>& extras) {
    auto intent = apps::mojom::Intent::New();
    intent->action = action;
    intent->url = url;
    intent->mime_type = mime_type;
    intent->files = std::move(files);
    intent->activity_name = activity_name;
    intent->drive_share_url = drive_share_url;
    intent->share_text = share_text;
    intent->share_title = share_title;
    intent->start_type = start_type;
    intent->categories = categories;
    intent->data = data;
    intent->ui_bypassed = ui_bypassed;
    intent->extras = extras;
    return intent;
  }
};

TEST_F(IntentUtilTest, AllConditionMatches) {
  GURL test_url = GURL("https://www.google.com/");
  auto intent = apps_util::CreateIntentFromUrl(test_url);
  auto intent_filter =
      apps_util::CreateIntentFilterForUrlScope(GURL(kFilterUrl));

  EXPECT_TRUE(apps_util::IntentMatchesFilter(intent, intent_filter));
}

TEST_F(IntentUtilTest, OneConditionDoesnotMatch) {
  GURL test_url = GURL("https://www.abc.com/");
  auto intent = apps_util::CreateIntentFromUrl(test_url);
  auto intent_filter =
      apps_util::CreateIntentFilterForUrlScope(GURL(kFilterUrl));

  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent, intent_filter));
}

TEST_F(IntentUtilTest, IntentDoesnotHaveValueToMatch) {
  GURL test_url = GURL("www.abc.com/");
  auto intent = apps_util::CreateIntentFromUrl(test_url);
  auto intent_filter =
      apps_util::CreateIntentFilterForUrlScope(GURL(kFilterUrl));

  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent, intent_filter));
}

// Test ConditionMatch with more then one condition values.

TEST_F(IntentUtilTest, OneConditionValueMatch) {
  auto condition = CreateMultiConditionValuesCondition();
  GURL test_url = GURL("https://www.google.com/");
  auto intent = apps_util::CreateIntentFromUrl(test_url);
  EXPECT_TRUE(apps_util::IntentMatchesCondition(intent, condition));
}

TEST_F(IntentUtilTest, NoneConditionValueMathc) {
  auto condition = CreateMultiConditionValuesCondition();
  GURL test_url = GURL("tel://www.google.com/");
  auto intent = apps_util::CreateIntentFromUrl(test_url);
  EXPECT_FALSE(apps_util::IntentMatchesCondition(intent, condition));
}

// Test Condition Value match with different pattern match type.
TEST_F(IntentUtilTest, NoneMatchType) {
  auto condition_value = apps_util::MakeConditionValue(
      "https", apps::mojom::PatternMatchType::kNone);
  EXPECT_TRUE(apps_util::ConditionValueMatches("https", condition_value));
  EXPECT_FALSE(apps_util::ConditionValueMatches("http", condition_value));
}
TEST_F(IntentUtilTest, LiteralMatchType) {
  auto condition_value = apps_util::MakeConditionValue(
      "https", apps::mojom::PatternMatchType::kLiteral);
  EXPECT_TRUE(apps_util::ConditionValueMatches("https", condition_value));
  EXPECT_FALSE(apps_util::ConditionValueMatches("http", condition_value));
}
TEST_F(IntentUtilTest, PrefixMatchType) {
  auto condition_value = apps_util::MakeConditionValue(
      "/ab", apps::mojom::PatternMatchType::kPrefix);
  EXPECT_TRUE(apps_util::ConditionValueMatches("/abc", condition_value));
  EXPECT_TRUE(apps_util::ConditionValueMatches("/ABC", condition_value));
  EXPECT_FALSE(apps_util::ConditionValueMatches("/d", condition_value));
}

TEST_F(IntentUtilTest, GlobMatchType) {
  auto condition_value_star = apps_util::MakeConditionValue(
      "/a*b", apps::mojom::PatternMatchType::kGlob);
  EXPECT_TRUE(apps_util::ConditionValueMatches("/b", condition_value_star));
  EXPECT_TRUE(apps_util::ConditionValueMatches("/ab", condition_value_star));
  EXPECT_TRUE(apps_util::ConditionValueMatches("/aab", condition_value_star));
  EXPECT_TRUE(
      apps_util::ConditionValueMatches("/aaaaaab", condition_value_star));
  EXPECT_FALSE(apps_util::ConditionValueMatches("/aabb", condition_value_star));
  EXPECT_FALSE(apps_util::ConditionValueMatches("/aabc", condition_value_star));
  EXPECT_FALSE(apps_util::ConditionValueMatches("/bb", condition_value_star));

  auto condition_value_dot = apps_util::MakeConditionValue(
      "/a.b", apps::mojom::PatternMatchType::kGlob);
  EXPECT_TRUE(apps_util::ConditionValueMatches("/aab", condition_value_dot));
  EXPECT_TRUE(apps_util::ConditionValueMatches("/acb", condition_value_dot));
  EXPECT_FALSE(apps_util::ConditionValueMatches("/ab", condition_value_dot));
  EXPECT_FALSE(apps_util::ConditionValueMatches("/abd", condition_value_dot));
  EXPECT_FALSE(apps_util::ConditionValueMatches("/abbd", condition_value_dot));

  auto condition_value_dot_and_star = apps_util::MakeConditionValue(
      "/a.*b", apps::mojom::PatternMatchType::kGlob);
  EXPECT_TRUE(
      apps_util::ConditionValueMatches("/aab", condition_value_dot_and_star));
  EXPECT_TRUE(apps_util::ConditionValueMatches("/aadsfadslkjb",
                                               condition_value_dot_and_star));
  EXPECT_TRUE(
      apps_util::ConditionValueMatches("/ab", condition_value_dot_and_star));

  // This arguably should be true, however the algorithm is transcribed from the
  // upstream Android codebase, which behaves like this.
  EXPECT_FALSE(apps_util::ConditionValueMatches("/abasdfab",
                                                condition_value_dot_and_star));
  EXPECT_FALSE(apps_util::ConditionValueMatches("/abasdfad",
                                                condition_value_dot_and_star));
  EXPECT_FALSE(apps_util::ConditionValueMatches("/bbasdfab",
                                                condition_value_dot_and_star));
  EXPECT_FALSE(
      apps_util::ConditionValueMatches("/a", condition_value_dot_and_star));
  EXPECT_FALSE(
      apps_util::ConditionValueMatches("/b", condition_value_dot_and_star));

  auto condition_value_escape_dot = apps_util::MakeConditionValue(
      "/a\\.b", apps::mojom::PatternMatchType::kGlob);
  EXPECT_TRUE(
      apps_util::ConditionValueMatches("/a.b", condition_value_escape_dot));

  // This arguably should be false, however the transcribed is carried from the
  // upstream Android codebase, which behaves like this.
  EXPECT_TRUE(
      apps_util::ConditionValueMatches("/acb", condition_value_escape_dot));

  auto condition_value_escape_star = apps_util::MakeConditionValue(
      "/a\\*b", apps::mojom::PatternMatchType::kGlob);
  EXPECT_TRUE(
      apps_util::ConditionValueMatches("/a*b", condition_value_escape_star));
  EXPECT_FALSE(
      apps_util::ConditionValueMatches("/acb", condition_value_escape_star));
}

TEST_F(IntentUtilTest, FilterMatchLevel) {
  auto filter_scheme_only = apps_util::CreateSchemeOnlyFilter("http");
  auto filter_scheme_and_host_only =
      apps_util::CreateSchemeAndHostOnlyFilter("https", "www.abc.com");
  auto filter_url = apps_util::CreateIntentFilterForUrlScope(
      GURL("https:://www.google.com/"));
  auto filter_empty = apps::mojom::IntentFilter::New();

  EXPECT_EQ(apps_util::GetFilterMatchLevel(filter_url),
            apps_util::IntentFilterMatchLevel::kScheme +
                apps_util::IntentFilterMatchLevel::kHost +
                apps_util::IntentFilterMatchLevel::kPattern);
  EXPECT_EQ(apps_util::GetFilterMatchLevel(filter_scheme_and_host_only),
            apps_util::IntentFilterMatchLevel::kScheme +
                apps_util::IntentFilterMatchLevel::kHost);
  EXPECT_EQ(apps_util::GetFilterMatchLevel(filter_scheme_only),
            apps_util::IntentFilterMatchLevel::kScheme);
  EXPECT_EQ(apps_util::GetFilterMatchLevel(filter_empty),
            apps_util::IntentFilterMatchLevel::kNone);

  EXPECT_TRUE(apps_util::GetFilterMatchLevel(filter_url) >
              apps_util::GetFilterMatchLevel(filter_scheme_and_host_only));
  EXPECT_TRUE(apps_util::GetFilterMatchLevel(filter_scheme_and_host_only) >
              apps_util::GetFilterMatchLevel(filter_scheme_only));
  EXPECT_TRUE(apps_util::GetFilterMatchLevel(filter_scheme_only) >
              apps_util::GetFilterMatchLevel(filter_empty));
}

TEST_F(IntentUtilTest, ActionMatch) {
  GURL test_url = GURL("https://www.google.com/");
  auto intent = apps_util::CreateIntentFromUrl(test_url);
  auto intent_filter =
      apps_util::CreateIntentFilterForUrlScope(GURL(kFilterUrl));
  EXPECT_TRUE(apps_util::IntentMatchesFilter(intent, intent_filter));

  auto send_intent = apps_util::CreateIntentFromUrl(test_url);
  send_intent->action = apps_util::kIntentActionSend;
  EXPECT_FALSE(apps_util::IntentMatchesFilter(send_intent, intent_filter));

  auto send_intent_filter =
      apps_util::CreateIntentFilterForUrlScope(GURL(kFilterUrl));
  send_intent_filter->conditions[0]->condition_values[0]->value =
      apps_util::kIntentActionSend;
  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent, send_intent_filter));
}

TEST_F(IntentUtilTest, MimeTypeMatch) {
  std::string mime_type1 = "text/plain";
  std::string mime_type2 = "image/jpeg";
  std::string mime_type_sub_wildcard = "text/*";
  std::string mime_type_all_wildcard = "*/*";
  std::string mime_type_only_main_type = "text";
  std::string mime_type_only_star = "*";

  auto intent1 = CreateShareIntent(mime_type1);
  auto intent2 = CreateShareIntent(mime_type2);
  auto intent_sub_wildcard = CreateShareIntent(mime_type_sub_wildcard);
  auto intent_all_wildcard = CreateShareIntent(mime_type_all_wildcard);
  auto intent_only_main_type = CreateShareIntent(mime_type_only_main_type);
  auto intent_only_star = CreateShareIntent(mime_type_only_star);

  auto filter1 = apps_util::CreateIntentFilterForMimeType(mime_type1);

  EXPECT_TRUE(apps_util::IntentMatchesFilter(intent1, filter1));
  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent2, filter1));
  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent_sub_wildcard, filter1));
  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent_all_wildcard, filter1));
  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent_only_main_type, filter1));
  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent_only_star, filter1));

  auto filter2 = apps_util::CreateIntentFilterForMimeType(mime_type2);

  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent1, filter2));
  EXPECT_TRUE(apps_util::IntentMatchesFilter(intent2, filter2));
  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent_sub_wildcard, filter2));
  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent_all_wildcard, filter2));
  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent_only_main_type, filter2));
  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent_only_star, filter2));

  auto filter_sub_wildcard =
      apps_util::CreateIntentFilterForMimeType(mime_type_sub_wildcard);

  EXPECT_TRUE(apps_util::IntentMatchesFilter(intent1, filter_sub_wildcard));
  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent2, filter_sub_wildcard));
  EXPECT_TRUE(
      apps_util::IntentMatchesFilter(intent_sub_wildcard, filter_sub_wildcard));
  EXPECT_FALSE(
      apps_util::IntentMatchesFilter(intent_all_wildcard, filter_sub_wildcard));
  EXPECT_TRUE(apps_util::IntentMatchesFilter(intent_only_main_type,
                                             filter_sub_wildcard));
  EXPECT_FALSE(
      apps_util::IntentMatchesFilter(intent_only_star, filter_sub_wildcard));

  auto filter_all_wildcard =
      apps_util::CreateIntentFilterForMimeType(mime_type_all_wildcard);

  EXPECT_TRUE(apps_util::IntentMatchesFilter(intent1, filter_all_wildcard));
  EXPECT_TRUE(apps_util::IntentMatchesFilter(intent2, filter_all_wildcard));
  EXPECT_TRUE(
      apps_util::IntentMatchesFilter(intent_sub_wildcard, filter_all_wildcard));
  EXPECT_TRUE(
      apps_util::IntentMatchesFilter(intent_all_wildcard, filter_all_wildcard));
  EXPECT_TRUE(apps_util::IntentMatchesFilter(intent_only_main_type,
                                             filter_all_wildcard));
  EXPECT_TRUE(
      apps_util::IntentMatchesFilter(intent_only_star, filter_all_wildcard));

  auto filter_only_main_type =
      apps_util::CreateIntentFilterForMimeType(mime_type_only_main_type);

  EXPECT_TRUE(apps_util::IntentMatchesFilter(intent1, filter_only_main_type));
  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent2, filter_only_main_type));
  EXPECT_TRUE(apps_util::IntentMatchesFilter(intent_sub_wildcard,
                                             filter_only_main_type));
  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent_all_wildcard,
                                              filter_only_main_type));
  EXPECT_TRUE(apps_util::IntentMatchesFilter(intent_only_main_type,
                                             filter_only_main_type));
  EXPECT_FALSE(
      apps_util::IntentMatchesFilter(intent_only_star, filter_only_main_type));

  auto filter_only_star =
      apps_util::CreateIntentFilterForMimeType(mime_type_only_star);

  EXPECT_TRUE(apps_util::IntentMatchesFilter(intent1, filter_only_star));
  EXPECT_TRUE(apps_util::IntentMatchesFilter(intent2, filter_only_star));
  EXPECT_TRUE(
      apps_util::IntentMatchesFilter(intent_sub_wildcard, filter_only_star));
  EXPECT_TRUE(
      apps_util::IntentMatchesFilter(intent_all_wildcard, filter_only_star));
  EXPECT_TRUE(
      apps_util::IntentMatchesFilter(intent_only_main_type, filter_only_star));
  EXPECT_TRUE(
      apps_util::IntentMatchesFilter(intent_only_star, filter_only_star));
}

TEST_F(IntentUtilTest, CommonMimeTypeMatch) {
  std::string mime_type1 = "text/plain";
  std::string mime_type2 = "image/jpeg";
  std::string mime_type3 = "text/html";
  std::string mime_type_sub_wildcard = "text/*";
  std::string mime_type_all_wildcard = "*/*";

  auto filter1 = apps_util::CreateIntentFilterForSend(mime_type1);
  auto filter2 = apps_util::CreateIntentFilterForSend(mime_type2);
  auto filter3 = apps_util::CreateIntentFilterForSend(mime_type3);
  auto filter_sub_wildcard =
      apps_util::CreateIntentFilterForSend(mime_type_sub_wildcard);
  auto filter_all_wildcard =
      apps_util::CreateIntentFilterForSend(mime_type_all_wildcard);
  auto filter1_and_2 = apps_util::CreateIntentFilterForSend(mime_type1);
  filter1_and_2->conditions[1]->condition_values.push_back(
      apps_util::MakeConditionValue(mime_type2,
                                    apps::mojom::PatternMatchType::kMimeType));

  std::vector<GURL> urls;
  std::vector<std::string> mime_types;

  urls.emplace_back("abc");

  // Test match with text/plain type.
  mime_types.push_back(mime_type1);
  auto intent = apps_util::CreateShareIntentFromFiles(urls, mime_types);
  EXPECT_TRUE(apps_util::IntentMatchesFilter(intent, filter1));
  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent, filter2));
  EXPECT_TRUE(apps_util::IntentMatchesFilter(intent, filter1_and_2));
  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent, filter3));
  EXPECT_TRUE(apps_util::IntentMatchesFilter(intent, filter_sub_wildcard));
  EXPECT_TRUE(apps_util::IntentMatchesFilter(intent, filter_all_wildcard));

  // Test match with image/jpeg type.
  mime_types.clear();
  mime_types.push_back(mime_type2);
  intent = apps_util::CreateShareIntentFromFiles(urls, mime_types);
  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent, filter1));
  EXPECT_TRUE(apps_util::IntentMatchesFilter(intent, filter2));
  EXPECT_TRUE(apps_util::IntentMatchesFilter(intent, filter1_and_2));
  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent, filter3));
  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent, filter_sub_wildcard));
  EXPECT_TRUE(apps_util::IntentMatchesFilter(intent, filter_all_wildcard));

  // Test match with text/html type.
  mime_types.clear();
  mime_types.push_back(mime_type3);
  intent = apps_util::CreateShareIntentFromFiles(urls, mime_types);
  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent, filter1));
  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent, filter2));
  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent, filter1_and_2));
  EXPECT_TRUE(apps_util::IntentMatchesFilter(intent, filter3));
  EXPECT_TRUE(apps_util::IntentMatchesFilter(intent, filter_sub_wildcard));
  EXPECT_TRUE(apps_util::IntentMatchesFilter(intent, filter_all_wildcard));

  // Test match with text/* types.
  mime_types.clear();
  mime_types.push_back(mime_type_sub_wildcard);
  intent = apps_util::CreateShareIntentFromFiles(urls, mime_types);
  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent, filter1));
  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent, filter2));
  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent, filter1_and_2));
  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent, filter3));
  EXPECT_TRUE(apps_util::IntentMatchesFilter(intent, filter_sub_wildcard));
  EXPECT_TRUE(apps_util::IntentMatchesFilter(intent, filter_all_wildcard));

  // Test match with */* type.
  mime_types.clear();
  mime_types.push_back(mime_type_all_wildcard);
  intent = apps_util::CreateShareIntentFromFiles(urls, mime_types);
  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent, filter1));
  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent, filter2));
  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent, filter1_and_2));
  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent, filter3));
  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent, filter_sub_wildcard));
  EXPECT_TRUE(apps_util::IntentMatchesFilter(intent, filter_all_wildcard));
}

TEST_F(IntentUtilTest, CommonMimeTypeMatchMultiple) {
  std::string mime_type1 = "text/plain";
  std::string mime_type2 = "image/jpeg";
  std::string mime_type3 = "text/html";
  std::string mime_type_sub_wildcard = "text/*";
  std::string mime_type_all_wildcard = "*/*";

  auto filter1 = apps_util::CreateIntentFilterForSendMultiple(mime_type1);
  auto filter2 = apps_util::CreateIntentFilterForSendMultiple(mime_type2);
  auto filter3 = apps_util::CreateIntentFilterForSendMultiple(mime_type3);
  auto filter_sub_wildcard =
      apps_util::CreateIntentFilterForSendMultiple(mime_type_sub_wildcard);
  auto filter_all_wildcard =
      apps_util::CreateIntentFilterForSendMultiple(mime_type_all_wildcard);
  auto filter1_and_2 = apps_util::CreateIntentFilterForSendMultiple(mime_type1);
  filter1_and_2->conditions[1]->condition_values.push_back(
      apps_util::MakeConditionValue(mime_type2,
                                    apps::mojom::PatternMatchType::kMimeType));

  std::vector<GURL> urls;
  std::vector<std::string> mime_types;

  urls.emplace_back("abc");
  urls.emplace_back("def");

  // Test match with same mime types.
  mime_types.push_back(mime_type1);
  mime_types.push_back(mime_type1);
  auto intent = apps_util::CreateShareIntentFromFiles(urls, mime_types);
  EXPECT_TRUE(apps_util::IntentMatchesFilter(intent, filter1));
  EXPECT_TRUE(apps_util::IntentMatchesFilter(intent, filter1_and_2));
  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent, filter3));

  // Test match with same main type and different sub type.
  mime_types.clear();
  mime_types.push_back(mime_type1);
  mime_types.push_back(mime_type3);
  intent = apps_util::CreateShareIntentFromFiles(urls, mime_types);
  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent, filter1));
  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent, filter1_and_2));
  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent, filter3));
  EXPECT_TRUE(apps_util::IntentMatchesFilter(intent, filter_sub_wildcard));

  // Test match with explicit type and wildcard sub type.
  mime_types.clear();
  mime_types.push_back(mime_type1);
  mime_types.push_back(mime_type_sub_wildcard);
  intent = apps_util::CreateShareIntentFromFiles(urls, mime_types);
  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent, filter1));
  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent, filter1_and_2));
  EXPECT_TRUE(apps_util::IntentMatchesFilter(intent, filter_sub_wildcard));

  // Test match with different mime types.
  mime_types.clear();
  mime_types.push_back(mime_type1);
  mime_types.push_back(mime_type2);
  intent = apps_util::CreateShareIntentFromFiles(urls, mime_types);
  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent, filter1));
  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent, filter2));
  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent, filter_sub_wildcard));
  EXPECT_TRUE(apps_util::IntentMatchesFilter(intent, filter1_and_2));
  EXPECT_TRUE(apps_util::IntentMatchesFilter(intent, filter_all_wildcard));

  // Test match with explicit type and general wildcard type.
  mime_types.clear();
  mime_types.push_back(mime_type1);
  mime_types.push_back(mime_type_all_wildcard);
  intent = apps_util::CreateShareIntentFromFiles(urls, mime_types);
  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent, filter1));
  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent, filter1_and_2));
  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent, filter_sub_wildcard));
  EXPECT_TRUE(apps_util::IntentMatchesFilter(intent, filter_all_wildcard));
}

GURL test_url(const std::string& file_name) {
  GURL url = GURL("filesystem:https://site.com/isolated/" + file_name);
  EXPECT_TRUE(url.is_valid());
  return url;
}

GURL ext_test_url(const std::string& file_name) {
  GURL url =
      GURL("filesystem:chrome-extension://extensionid/external/" + file_name);
  EXPECT_TRUE(url.is_valid());
  return url;
}

std::vector<apps::mojom::IntentFilePtr> vectorise(
    const apps::mojom::IntentFilePtr& file) {
  std::vector<apps::mojom::IntentFilePtr> vector;
  vector.push_back(file.Clone());
  return vector;
}

TEST_F(IntentUtilTest, FileExtensionMatch) {
  std::string mime_type_mp3 = "audio/mp3";
  std::string file_ext_mp3 = "mp3";
  std::string mime_type_mpeg = "audio/mpeg";

  auto file_filter =
      apps_util::CreateFileFilterForView(mime_type_mp3, file_ext_mp3, "label");

  auto file = apps::mojom::IntentFile::New();
  file->url = test_url("abc.mp3");
  file->is_directory = apps::mojom::OptionalBool::kFalse;

  // Test match with the same mime type and the same file extension.
  file->mime_type = mime_type_mp3;
  auto intent = apps_util::CreateViewIntentFromFiles(vectorise(file));
  EXPECT_TRUE(apps_util::IntentMatchesFilter(intent, file_filter));

  // Test match with different mime types and the same file extension.
  file->mime_type = mime_type_mpeg;
  intent = apps_util::CreateViewIntentFromFiles(vectorise(file));
  EXPECT_TRUE(apps_util::IntentMatchesFilter(intent, file_filter));

  // Test match with the same mime type and a different file extension.
  file->url = test_url("abc.png");
  file->mime_type = mime_type_mp3;
  intent = apps_util::CreateViewIntentFromFiles(vectorise(file));
  EXPECT_TRUE(apps_util::IntentMatchesFilter(intent, file_filter));

  // Test match with different mime types and a different file extension.
  file->url = test_url("abc.png");
  file->mime_type = mime_type_mpeg;
  intent = apps_util::CreateViewIntentFromFiles(vectorise(file));
  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent, file_filter));

  std::string file_ext_dot_mp3 = ".mp3";
  auto file_filter_dot = apps_util::CreateFileFilterForView(
      mime_type_mp3, file_ext_dot_mp3, "label");

  // The whole extension must match, not just the end.
  file->url = test_url("abc.extramp3");
  file->mime_type = mime_type_mpeg;
  intent = apps_util::CreateViewIntentFromFiles(vectorise(file));
  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent, file_filter));
  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent, file_filter_dot));

  // Check that the filter behaves the same with and without a leading ".".
  file->url = test_url("abc.mp3");
  file->mime_type = mime_type_mpeg;
  intent = apps_util::CreateViewIntentFromFiles(vectorise(file));
  EXPECT_TRUE(apps_util::IntentMatchesFilter(intent, file_filter_dot));
}

TEST_F(IntentUtilTest, FileURLMatch) {
  std::string mp3_url_pattern = R"(filesystem:chrome-extension://.*/.*\.mp3)";

  auto url_filter = apps_util::CreateURLFilterForView(mp3_url_pattern, "label");

  auto file = apps::mojom::IntentFile::New();
  file->url = ext_test_url("abc.mp3");
  file->is_directory = apps::mojom::OptionalBool::kFalse;

  // Test match with mp3 file extension.
  file->mime_type = "";
  auto intent = apps_util::CreateViewIntentFromFiles(vectorise(file));
  EXPECT_TRUE(apps_util::IntentMatchesFilter(intent, url_filter));

  // Test non-match with mp4 file extension.
  file->url = ext_test_url("abc.mp4");
  intent = apps_util::CreateViewIntentFromFiles(vectorise(file));
  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent, url_filter));

  // Test non-match with just the end of a file extension.
  file->url = ext_test_url("abc.testmp3");
  intent = apps_util::CreateViewIntentFromFiles(vectorise(file));
  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent, url_filter));

  std::string single_wild_url_pattern = "filesystem:chrome-extension://.*/.*";
  auto wild_filter =
      apps_util::CreateURLFilterForView(single_wild_url_pattern, "label");

  // Test that mp3 matches with *
  file->url = ext_test_url("abc.mp3");
  intent = apps_util::CreateViewIntentFromFiles(vectorise(file));
  EXPECT_TRUE(apps_util::IntentMatchesFilter(intent, wild_filter));

  // Test that no file extension matches with *
  file->url = ext_test_url("abc");
  intent = apps_util::CreateViewIntentFromFiles(vectorise(file));
  EXPECT_TRUE(apps_util::IntentMatchesFilter(intent, wild_filter));

  std::string ext_wild_url_pattern =
      R"(filesystem:chrome-extension://.*/.*\..*)";
  auto ext_wild_filter =
      apps_util::CreateURLFilterForView(ext_wild_url_pattern, "label");

  // Test that mp3 matches with *.*
  file->url = ext_test_url("abc.mp3");
  intent = apps_util::CreateViewIntentFromFiles(vectorise(file));
  EXPECT_TRUE(apps_util::IntentMatchesFilter(intent, ext_wild_filter));

  // Test that no file extension does not match with *.*
  file->url = ext_test_url("abc");
  intent = apps_util::CreateViewIntentFromFiles(vectorise(file));
  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent, ext_wild_filter));
}

TEST_F(IntentUtilTest, FileWithTitleText) {
  const std::string mime_type = "image/jpeg";
  auto filter = apps_util::CreateIntentFilterForSend(mime_type);

  const std::vector<GURL> urls{GURL("abc")};
  const std::vector<std::string> mime_types{mime_type};

  auto intent =
      apps_util::CreateShareIntentFromFiles(urls, mime_types, "text", "title");
  EXPECT_TRUE(intent->share_text.has_value());
  EXPECT_EQ(intent->share_text.value(), "text");
  EXPECT_TRUE(intent->share_title.has_value());
  EXPECT_EQ(intent->share_title.value(), "title");
  EXPECT_TRUE(apps_util::IntentMatchesFilter(intent, filter));

  intent = apps_util::CreateShareIntentFromFiles(urls, mime_types, "text", "");
  EXPECT_TRUE(intent->share_text.has_value());
  EXPECT_EQ(intent->share_text.value(), "text");
  EXPECT_FALSE(intent->share_title.has_value());
  EXPECT_TRUE(apps_util::IntentMatchesFilter(intent, filter));

  intent = apps_util::CreateShareIntentFromFiles(urls, mime_types, "", "title");
  EXPECT_FALSE(intent->share_text.has_value());
  EXPECT_TRUE(intent->share_title.has_value());
  EXPECT_EQ(intent->share_title.value(), "title");
  EXPECT_TRUE(apps_util::IntentMatchesFilter(intent, filter));

  intent = apps_util::CreateShareIntentFromFiles(urls, mime_types, "", "");
  EXPECT_FALSE(intent->share_text.has_value());
  EXPECT_FALSE(intent->share_title.has_value());
  EXPECT_TRUE(apps_util::IntentMatchesFilter(intent, filter));
}

TEST_F(IntentUtilTest, TextMatch) {
  std::string mime_type1 = "text/plain";
  std::string mime_type2 = "image/jpeg";
  auto filter1 = apps_util::CreateIntentFilterForMimeType(mime_type1);
  auto filter2 = apps_util::CreateIntentFilterForMimeType(mime_type2);

  auto intent = apps_util::CreateShareIntentFromText("text", "");
  EXPECT_TRUE(apps_util::IntentMatchesFilter(intent, filter1));
  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent, filter2));

  intent = apps_util::CreateShareIntentFromText("", "title");
  EXPECT_TRUE(apps_util::IntentMatchesFilter(intent, filter1));
  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent, filter2));

  intent = apps_util::CreateShareIntentFromText("text", "title");
  EXPECT_TRUE(apps_util::IntentMatchesFilter(intent, filter1));
  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent, filter2));
}

TEST_F(IntentUtilTest, Convert) {
  const std::string action = apps_util::kIntentActionSend;
  GURL test_url1 = GURL("https://www.google.com/");
  GURL test_url2 = GURL("https://www.abc.com/");
  GURL test_url3 = GURL("https://www.foo.com/");
  const std::string mime_type = "image/jpeg";
  const std::string activity_name = "test";
  const std::string share_text = "share text";
  const std::string share_title = "share title";
  const std::string start_type = "start type";
  const std::string category1 = "category1";
  const std::string data = "data";
  const apps::mojom::OptionalBool ui_bypassed =
      apps::mojom::OptionalBool::kTrue;
  base::flat_map<std::string, std::string> extras = {
      {"key1", "value1"}, {"key2", "value2"}, {"key3", "value3"}};

  auto file1 = apps::mojom::IntentFile::New();
  file1->url = test_url1;
  auto file2 = apps::mojom::IntentFile::New();
  file2->url = test_url2;
  auto files = std::vector<apps::mojom::IntentFilePtr>();
  files.push_back(std::move(file1));
  files.push_back(std::move(file2));

  auto src_intent =
      CreateIntent(action, test_url1, mime_type, std::move(files),
                   activity_name, test_url3, share_text, share_title,
                   start_type, {category1}, data, ui_bypassed, extras);
  base::Value value = apps_util::ConvertIntentToValue(src_intent);
  auto dst_intent = apps_util::ConvertValueToIntent(std::move(value));

  EXPECT_EQ(action, dst_intent->action);
  EXPECT_EQ(test_url1, dst_intent->url.value());
  EXPECT_EQ(mime_type, dst_intent->mime_type.value());
  EXPECT_EQ(2u, dst_intent->files->size());
  EXPECT_EQ(test_url1, dst_intent->files.value()[0]->url);
  EXPECT_EQ(test_url2, dst_intent->files.value()[1]->url);
  EXPECT_EQ(activity_name, dst_intent->activity_name.value());
  EXPECT_EQ(test_url3, dst_intent->drive_share_url.value());
  EXPECT_EQ(share_text, dst_intent->share_text.value());
  EXPECT_EQ(share_title, dst_intent->share_title.value());
  EXPECT_EQ(start_type, dst_intent->start_type.value());
  EXPECT_EQ(1u, dst_intent->categories->size());
  EXPECT_EQ(category1, dst_intent->categories.value()[0]);
  EXPECT_EQ(data, dst_intent->data.value());
  EXPECT_EQ(ui_bypassed, dst_intent->ui_bypassed);
  EXPECT_TRUE(dst_intent->extras.has_value());
  EXPECT_EQ(3u, dst_intent->extras->size());
  EXPECT_EQ(extras, dst_intent->extras.value());
}

TEST_F(IntentUtilTest, ConvertEmptyIntent) {
  auto intent = apps::mojom::Intent::New();
  base::Value value = apps_util::ConvertIntentToValue(intent);
  auto dst_intent = apps_util::ConvertValueToIntent(std::move(value));

  EXPECT_FALSE(dst_intent->url.has_value());
  EXPECT_FALSE(dst_intent->mime_type.has_value());
  EXPECT_FALSE(dst_intent->files.has_value());
  EXPECT_FALSE(dst_intent->activity_name.has_value());
  EXPECT_FALSE(dst_intent->drive_share_url.has_value());
  EXPECT_FALSE(dst_intent->share_text.has_value());
  EXPECT_FALSE(dst_intent->share_title.has_value());
  EXPECT_FALSE(dst_intent->start_type.has_value());
  EXPECT_FALSE(dst_intent->categories.has_value());
  EXPECT_FALSE(dst_intent->data.has_value());
  EXPECT_EQ(apps::mojom::OptionalBool::kUnknown, dst_intent->ui_bypassed);
  EXPECT_FALSE(dst_intent->extras.has_value());
}

TEST_F(IntentUtilTest, CalculateCommonMimeType) {
  EXPECT_EQ("*/*", apps_util::CalculateCommonMimeType({}));

  EXPECT_EQ("*/*", apps_util::CalculateCommonMimeType({""}));
  EXPECT_EQ("*/*", apps_util::CalculateCommonMimeType({"not_a_valid_type"}));
  EXPECT_EQ("*/*", apps_util::CalculateCommonMimeType({"not_a_valid_type/"}));
  EXPECT_EQ("*/*",
            apps_util::CalculateCommonMimeType({"not_a_valid_type/foo/bar"}));

  EXPECT_EQ("image/png", apps_util::CalculateCommonMimeType({"image/png"}));
  EXPECT_EQ("image/png",
            apps_util::CalculateCommonMimeType({"image/png", "image/png"}));
  EXPECT_EQ("image/*",
            apps_util::CalculateCommonMimeType({"image/png", "image/jpeg"}));
  EXPECT_EQ("*/*",
            apps_util::CalculateCommonMimeType({"image/png", "text/plain"}));
  EXPECT_EQ("*/*", apps_util::CalculateCommonMimeType(
                       {"image/png", "image/jpeg", "text/plain"}));
}

TEST_F(IntentUtilTest, IsGenericFileHandler) {
  using apps::mojom::IntentFile;
  using apps::mojom::IntentFilePtr;
  using apps::mojom::IntentFilterPtr;
  using apps::mojom::IntentPtr;
  using apps::mojom::OptionalBool;

  std::vector<IntentFilePtr> intent_files;
  IntentFilePtr foo = IntentFile::New();
  foo->url = test_url("foo.jpg");
  foo->mime_type = "image/jpeg";
  foo->is_directory = OptionalBool::kFalse;
  intent_files.push_back(std::move(foo));

  IntentFilePtr bar = IntentFile::New();
  bar->url = test_url("bar.txt");
  bar->mime_type = "text/plain";
  bar->is_directory = OptionalBool::kFalse;
  intent_files.push_back(std::move(bar));

  std::vector<IntentFilePtr> intent_files2;
  IntentFilePtr foo2 = IntentFile::New();
  foo2->url = test_url("foo.ics");
  foo2->mime_type = "text/calendar";
  foo2->is_directory = OptionalBool::kFalse;
  intent_files2.push_back(std::move(foo2));

  std::vector<IntentFilePtr> intent_files3;
  IntentFilePtr foo_dir = IntentFile::New();
  foo_dir->url = test_url("foo/");
  foo_dir->mime_type = "";
  foo_dir->is_directory = OptionalBool::kTrue;
  intent_files3.push_back(std::move(foo_dir));

  IntentPtr intent =
      apps_util::CreateViewIntentFromFiles(std::move(intent_files));
  IntentPtr intent2 =
      apps_util::CreateViewIntentFromFiles(std::move(intent_files2));
  IntentPtr intent3 =
      apps_util::CreateViewIntentFromFiles(std::move(intent_files3));

  const std::string kLabel = "";

  // extensions: ["*"]
  IntentFilterPtr filter1 = apps_util::CreateFileFilterForView("", "*", kLabel);
  EXPECT_TRUE(apps_util::IsGenericFileHandler(intent, filter1));

  // extensions: ["*", "jpg"]
  IntentFilterPtr filter2 = apps_util::CreateFileFilterForView("", "*", kLabel);
  apps_util::AddConditionValue(apps::mojom::ConditionType::kFile, "jpg",
                               apps::mojom::PatternMatchType::kFileExtension,
                               filter2);
  EXPECT_TRUE(apps_util::IsGenericFileHandler(intent, filter2));

  // extensions: ["jpg"]
  IntentFilterPtr filter3 =
      apps_util::CreateFileFilterForView("", "jpg", kLabel);
  EXPECT_FALSE(apps_util::IsGenericFileHandler(intent, filter3));

  // types: ["*"]
  IntentFilterPtr filter4 = apps_util::CreateFileFilterForView("*", "", kLabel);
  EXPECT_TRUE(apps_util::IsGenericFileHandler(intent, filter4));

  // types: ["*/*"]
  IntentFilterPtr filter5 =
      apps_util::CreateFileFilterForView("*/*", "", kLabel);
  EXPECT_TRUE(apps_util::IsGenericFileHandler(intent, filter5));

  // types: ["image/*"]
  IntentFilterPtr filter6 =
      apps_util::CreateFileFilterForView("image/*", "", kLabel);
  // Partial wild card is not generic.
  EXPECT_FALSE(apps_util::IsGenericFileHandler(intent, filter6));

  // types: ["*", "image/*"]
  IntentFilterPtr filter7 = apps_util::CreateFileFilterForView("*", "", kLabel);
  apps_util::AddConditionValue(apps::mojom::ConditionType::kFile, "image/*",
                               apps::mojom::PatternMatchType::kMimeType,
                               filter7);
  EXPECT_TRUE(apps_util::IsGenericFileHandler(intent, filter7));

  // extensions: ["*"], types: ["image/*"]
  IntentFilterPtr filter8 =
      apps_util::CreateFileFilterForView("image/*", "*", kLabel);
  EXPECT_TRUE(apps_util::IsGenericFileHandler(intent, filter8));

  // types: ["text/*"] and target files contain unsupported text mime type, e.g.
  // text/calendar.
  IntentFilterPtr filter9 =
      apps_util::CreateFileFilterForView("text/*", "", kLabel);
  EXPECT_TRUE(apps_util::IsGenericFileHandler(intent2, filter9));

  // types: ["text/*"] and target files don't contain unsupported text mime
  // type.
  IntentFilterPtr filter10 =
      apps_util::CreateFileFilterForView("text/*", "", kLabel);
  EXPECT_FALSE(apps_util::IsGenericFileHandler(intent, filter10));

  // File is a directory.
  IntentFilterPtr filter11 =
      apps_util::CreateFileFilterForView("text/*", "", kLabel);
  EXPECT_TRUE(apps_util::IsGenericFileHandler(intent3, filter11));

  // File is a directory, but filter is inode/directory.
  IntentFilterPtr filter12 =
      apps_util::CreateFileFilterForView("inode/directory", "", kLabel);
  EXPECT_FALSE(apps_util::IsGenericFileHandler(intent3, filter12));
}
