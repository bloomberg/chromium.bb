// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_pref.h"
#include "components/content_settings/core/common/content_settings_pattern.h"

#include "base/bind_helpers.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content_settings {
namespace {

// Test content setting pattern pairs in string format. The percent-encoded
// sequence "%61" should get canonicalized to the letter 'a'.
constexpr char kTestPatternCanonicalAlpha[] = "https://alpha.com,*";
constexpr char kTestPatternNonCanonicalAlpha1[] = "https://%61lpha.com,*";
constexpr char kTestPatternNonCanonicalAlpha2[] = "https://alph%61.com,*";
constexpr char kTestPatternCanonicalBeta[] = "https://beta.com,*";
constexpr char kTestPatternNonCanonicalBeta[] = "https://bet%61.com,*";

constexpr char kTestContentSettingPrefName[] = "content_settings.test";

constexpr char kLastModifiedKey[] = "last_modified";
constexpr char kSettingKey[] = "setting";
constexpr char kTagKey[] = "tag";

// Creates a JSON dictionary representing a dummy content setting exception
// value in preferences. The setting will be marked with the |tag| like so:
//
//   value = {
//     "last_modified": "...",
//     "setting": {
//       "tag": "...",
//     }
//   }
base::Value CreateDummyContentSettingValue(base::StringPiece tag) {
  base::Value setting(base::Value::Type::DICTIONARY);
  setting.SetKey(kTagKey, base::Value(tag));

  base::Value pref_value(base::Value::Type::DICTIONARY);
  pref_value.SetKey(kLastModifiedKey, base::Value("13189876543210000"));
  pref_value.SetKey(kSettingKey, std::move(setting));
  return pref_value;
}

// Given the JSON dictionary representing the "setting" stored under a content
// setting exception value, returns the tag.
std::string GetTagFromDummyContentSetting(const base::Value& setting) {
  const auto* tag = setting.FindKey(kTagKey);
  return tag ? tag->GetString() : std::string();
}

// Given the JSON dictionary representing a content setting exception value,
// returns the tag.
std::string GetTagFromDummyContentSettingValue(const base::Value& pref_value) {
  const auto* setting = pref_value.FindKey(kSettingKey);
  return setting ? GetTagFromDummyContentSetting(*setting) : std::string();
}

}  // namespace

TEST(ContentSettingsPref, CanonicalizationWhileReadingFromPrefs) {
  // Canonical/non-canonical patterns originally in preferences.
  constexpr const char* kTestOriginalPatterns[] = {
      kTestPatternNonCanonicalAlpha1,
      kTestPatternNonCanonicalAlpha2,
      kTestPatternCanonicalBeta,
      kTestPatternNonCanonicalBeta,
  };

  // Upon construction, ContentSettingPref reads all content setting exception
  // data stored in Preferences for a given content setting. This process also
  // has the side effect that it migrates all data keyed under non-canonical
  // content setting pattern pairs to be keyed under the corresponding canoncial
  // pattern pair, both in Preferences, as well as in ContentSettingPref's
  // in-memory |value_map| representation. There are two edge cases here:
  //
  //   1) If multiple non-canonical patterns map to the same canonical pattern,
  //      the data for the last read pattern is retained, the rest thrown away.
  //   2) We ignore and delete non-canonical pattern pairs if a canonical one
  //      already exists.
  //
  // With regard to the test data, NonCanonicalAlpha1 and NonCanonicalAlpha2
  // would both map to CanonicalAlpha, so the value for the latter should be
  // retained.
  //
  // NonCanonicalBeta would be canonicalized to CanonicalBeta, but because there
  // is already a value under that key, the latter should be retained and the
  // non-canonical value thrown away.
  using CanonicalPatternToTag = std::pair<std::string, std::string>;
  const std::vector<CanonicalPatternToTag> kExpectedPatternsToTags = {
      {kTestPatternCanonicalAlpha, kTestPatternNonCanonicalAlpha2},
      {kTestPatternCanonicalBeta, kTestPatternCanonicalBeta},
  };

  auto original_pref_value = std::make_unique<base::DictionaryValue>();
  for (const auto* pattern : kTestOriginalPatterns) {
    original_pref_value->SetKey(pattern,
                                CreateDummyContentSettingValue(pattern));
  }

  TestingPrefServiceSimple prefs;
  prefs.registry()->RegisterDictionaryPref(kTestContentSettingPrefName);
  prefs.SetUserPref(kTestContentSettingPrefName,
                    std::move(original_pref_value));

  PrefChangeRegistrar registrar;
  registrar.Init(&prefs);
  ContentSettingsPref content_settings_pref(
      CONTENT_SETTINGS_TYPE_MEDIA_ENGAGEMENT, &prefs, &registrar,
      kTestContentSettingPrefName, false, base::DoNothing());

  // Verify that the |value_map| contains the expected content setting patterns
  // and setting.

  std::vector<CanonicalPatternToTag> patterns_to_tags_in_memory;
  auto rule_iterator = content_settings_pref.GetRuleIterator(
      std::string() /* resource_identifier */, false /* is_incognito */);
  while (rule_iterator->HasNext()) {
    auto rule = rule_iterator->Next();
    patterns_to_tags_in_memory.emplace_back(
        CreatePatternString(rule.primary_pattern, rule.secondary_pattern),
        GetTagFromDummyContentSetting(rule.value));
  }

  EXPECT_THAT(patterns_to_tags_in_memory,
              testing::UnorderedElementsAreArray(kExpectedPatternsToTags));

  // Verify that Preferences do, as well.

  std::vector<CanonicalPatternToTag> patterns_to_tags_in_prefs;
  const auto* canonical_pref_value =
      prefs.GetUserPref(kTestContentSettingPrefName);
  ASSERT_TRUE(canonical_pref_value->is_dict());
  for (const auto& key_value : canonical_pref_value->DictItems()) {
    patterns_to_tags_in_prefs.emplace_back(
        key_value.first, GetTagFromDummyContentSettingValue(key_value.second));
  }

  EXPECT_THAT(patterns_to_tags_in_prefs,
              testing::UnorderedElementsAreArray(kExpectedPatternsToTags));
}

}  // namespace content_settings
