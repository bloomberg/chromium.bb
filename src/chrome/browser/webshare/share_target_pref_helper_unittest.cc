// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webshare/share_target_pref_helper.h"
#include "base/strings/nullable_string16.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

class PrefRegistrySimple;

namespace {

class ShareTargetPrefHelperUnittest : public testing::Test {
 protected:
  ShareTargetPrefHelperUnittest() {}
  ~ShareTargetPrefHelperUnittest() override {}

  void SetUp() override {
    pref_service_.reset(new TestingPrefServiceSimple());
    pref_service_->registry()->RegisterDictionaryPref(
        prefs::kWebShareVisitedTargets);
  }

  PrefService* pref_service() { return pref_service_.get(); }

 private:
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
};

constexpr char kNameKey[] = "name";
constexpr char kActionKey[] = "action";
constexpr char kTextKey[] = "text";
constexpr char kTitleKey[] = "title";
constexpr char kUrlKey[] = "url";

TEST_F(ShareTargetPrefHelperUnittest, AddMultipleShareTargets) {
  // Add a share target to prefs that wasn't previously stored.
  GURL manifest_url("https://www.sharetarget.com/manifest.json");
  blink::Manifest::ShareTarget share_target;
  share_target.action = GURL("https://www.sharetarget.com/share");
  share_target.params.title =
      base::NullableString16(base::ASCIIToUTF16("mytitle"));
  share_target.params.text =
      base::NullableString16(base::ASCIIToUTF16("mytext"));
  share_target.params.url = base::NullableString16(base::ASCIIToUTF16("myurl"));
  blink::Manifest manifest;
  manifest.share_target =
      base::Optional<blink::Manifest::ShareTarget>(share_target);

  UpdateShareTargetInPrefs(manifest_url, manifest, pref_service());

  const base::DictionaryValue* share_target_dict =
      pref_service()->GetDictionary(prefs::kWebShareVisitedTargets);
  EXPECT_EQ(1UL, share_target_dict->size());
  const base::DictionaryValue* share_target_info_dict = nullptr;
  ASSERT_TRUE(share_target_dict->GetDictionaryWithoutPathExpansion(
      manifest_url.spec(), &share_target_info_dict));
  EXPECT_EQ(4UL, share_target_info_dict->size());
  std::string action_url_in_dict;
  EXPECT_TRUE(
      share_target_info_dict->GetString(kActionKey, &action_url_in_dict));
  EXPECT_EQ("https://www.sharetarget.com/share", action_url_in_dict);
  std::string title_in_dict;
  EXPECT_TRUE(share_target_info_dict->GetString(kTitleKey, &title_in_dict));
  EXPECT_EQ("mytitle", title_in_dict);
  std::string text_in_dict;
  EXPECT_TRUE(share_target_info_dict->GetString(kTextKey, &text_in_dict));
  EXPECT_EQ("mytext", text_in_dict);
  std::string url_in_dict;
  EXPECT_TRUE(share_target_info_dict->GetString(kUrlKey, &url_in_dict));
  EXPECT_EQ("myurl", url_in_dict);

  // Add second share target to prefs that wasn't previously stored.
  manifest_url = GURL("https://www.sharetarget2.com/manifest.json");
  std::string name = "Share Target Name";
  manifest.name = base::NullableString16(base::ASCIIToUTF16(name), false);

  UpdateShareTargetInPrefs(manifest_url, manifest, pref_service());

  share_target_dict =
      pref_service()->GetDictionary(prefs::kWebShareVisitedTargets);
  EXPECT_EQ(2UL, share_target_dict->size());
  ASSERT_TRUE(share_target_dict->GetDictionaryWithoutPathExpansion(
      manifest_url.spec(), &share_target_info_dict));
  EXPECT_EQ(5UL, share_target_info_dict->size());
  EXPECT_TRUE(
      share_target_info_dict->GetString(kActionKey, &action_url_in_dict));
  EXPECT_EQ("https://www.sharetarget.com/share", action_url_in_dict);
  std::string name_in_dict;
  EXPECT_TRUE(share_target_info_dict->GetString(kNameKey, &name_in_dict));
  EXPECT_TRUE(share_target_info_dict->GetString(kTitleKey, &title_in_dict));
  EXPECT_EQ("mytitle", title_in_dict);
  EXPECT_TRUE(share_target_info_dict->GetString(kTextKey, &text_in_dict));
  EXPECT_EQ("mytext", text_in_dict);
  EXPECT_TRUE(share_target_info_dict->GetString(kUrlKey, &url_in_dict));
  EXPECT_EQ("myurl", url_in_dict);
  EXPECT_EQ(name, name_in_dict);
}

TEST_F(ShareTargetPrefHelperUnittest, AddShareTargetTwice) {
  const char kManifestUrl[] = "https://www.sharetarget.com/manifest.json";
  const char kAction[] = "https://www.sharetarget.com/share";
  const char kTitle[] = "title";

  // Add a share target to prefs that wasn't previously stored.
  GURL manifest_url(kManifestUrl);
  blink::Manifest::ShareTarget share_target;
  share_target.action = GURL(kAction);
  share_target.params.title =
      base::NullableString16(base::ASCIIToUTF16(kTitle));
  blink::Manifest manifest;
  manifest.share_target =
      base::Optional<blink::Manifest::ShareTarget>(share_target);

  UpdateShareTargetInPrefs(manifest_url, manifest, pref_service());

  const base::DictionaryValue* share_target_dict =
      pref_service()->GetDictionary(prefs::kWebShareVisitedTargets);
  EXPECT_EQ(1UL, share_target_dict->size());
  const base::DictionaryValue* share_target_info_dict = nullptr;
  ASSERT_TRUE(share_target_dict->GetDictionaryWithoutPathExpansion(
      kManifestUrl, &share_target_info_dict));
  EXPECT_EQ(2UL, share_target_info_dict->size());
  std::string action_url_in_dict;
  EXPECT_TRUE(
      share_target_info_dict->GetString(kActionKey, &action_url_in_dict));
  EXPECT_EQ("https://www.sharetarget.com/share", action_url_in_dict);

  // Add same share target to prefs that was previously stored; shouldn't
  // duplicate it.
  UpdateShareTargetInPrefs(manifest_url, manifest, pref_service());

  share_target_dict =
      pref_service()->GetDictionary(prefs::kWebShareVisitedTargets);
  EXPECT_EQ(1UL, share_target_dict->size());
  ASSERT_TRUE(share_target_dict->GetDictionaryWithoutPathExpansion(
      kManifestUrl, &share_target_info_dict));
  EXPECT_EQ(2UL, share_target_info_dict->size());
  EXPECT_TRUE(
      share_target_info_dict->GetString(kActionKey, &action_url_in_dict));
  EXPECT_EQ("https://www.sharetarget.com/share", action_url_in_dict);
}

TEST_F(ShareTargetPrefHelperUnittest, UpdateShareTarget) {
  // Add a share target to prefs that wasn't previously stored.
  GURL manifest_url("https://www.sharetarget.com/manifest.json");
  blink::Manifest::ShareTarget share_target;
  share_target.action = GURL("https://www.sharetarget.com/share");
  share_target.params.title =
      base::NullableString16(base::ASCIIToUTF16("title"));
  blink::Manifest manifest;
  manifest.share_target =
      base::Optional<blink::Manifest::ShareTarget>(share_target);

  UpdateShareTargetInPrefs(manifest_url, manifest, pref_service());

  const base::DictionaryValue* share_target_dict =
      pref_service()->GetDictionary(prefs::kWebShareVisitedTargets);
  EXPECT_EQ(1UL, share_target_dict->size());
  const base::DictionaryValue* share_target_info_dict = nullptr;
  ASSERT_TRUE(share_target_dict->GetDictionaryWithoutPathExpansion(
      manifest_url.spec(), &share_target_info_dict));
  EXPECT_EQ(2UL, share_target_info_dict->size());
  std::string action_url_in_dict;
  EXPECT_TRUE(
      share_target_info_dict->GetString(kActionKey, &action_url_in_dict));
  EXPECT_EQ("https://www.sharetarget.com/share", action_url_in_dict);
  std::string title_in_dict;
  EXPECT_TRUE(share_target_info_dict->GetString(kTitleKey, &title_in_dict));
  EXPECT_EQ("title", title_in_dict);

  // Add same share target to prefs that was previously stored, with new
  // params; should update the value.
  manifest.share_target.value().params.text =
      base::NullableString16(base::ASCIIToUTF16("text"));

  UpdateShareTargetInPrefs(manifest_url, manifest, pref_service());

  share_target_dict =
      pref_service()->GetDictionary(prefs::kWebShareVisitedTargets);
  EXPECT_EQ(1UL, share_target_dict->size());
  ASSERT_TRUE(share_target_dict->GetDictionaryWithoutPathExpansion(
      manifest_url.spec(), &share_target_info_dict));
  EXPECT_EQ(3UL, share_target_info_dict->size());
  EXPECT_TRUE(
      share_target_info_dict->GetString(kActionKey, &action_url_in_dict));
  EXPECT_EQ("https://www.sharetarget.com/share", action_url_in_dict);
  EXPECT_TRUE(share_target_info_dict->GetString(kTitleKey, &title_in_dict));
  EXPECT_EQ("title", title_in_dict);
  std::string text_in_dict;
  EXPECT_TRUE(share_target_info_dict->GetString(kTextKey, &text_in_dict));
  EXPECT_EQ("text", text_in_dict);
}

TEST_F(ShareTargetPrefHelperUnittest, DontAddNonShareTarget) {
  const char kManifestUrl[] = "https://www.dudsharetarget.com/manifest.json";
  const base::Optional<std::string> kUrlTemplate;

  // Don't add a site that has a null template.
  UpdateShareTargetInPrefs(GURL(kManifestUrl), blink::Manifest(),
                           pref_service());

  const base::DictionaryValue* share_target_dict =
      pref_service()->GetDictionary(prefs::kWebShareVisitedTargets);
  EXPECT_EQ(0UL, share_target_dict->size());
  const base::DictionaryValue* share_target_info_dict = nullptr;
  ASSERT_FALSE(share_target_dict->GetDictionaryWithoutPathExpansion(
      kManifestUrl, &share_target_info_dict));
}

TEST_F(ShareTargetPrefHelperUnittest, RemoveShareTarget) {
  // Add a share target to prefs that wasn't previously stored.
  GURL manifest_url("https://www.sharetarget.com/manifest.json");
  blink::Manifest::ShareTarget share_target;
  share_target.action = GURL("https://www.sharetarget.com/share");
  share_target.params.title =
      base::NullableString16(base::ASCIIToUTF16("title"));
  blink::Manifest manifest;
  manifest.share_target =
      base::Optional<blink::Manifest::ShareTarget>(share_target);

  UpdateShareTargetInPrefs(manifest_url, manifest, pref_service());

  const base::DictionaryValue* share_target_dict =
      pref_service()->GetDictionary(prefs::kWebShareVisitedTargets);
  EXPECT_EQ(1UL, share_target_dict->size());
  const base::DictionaryValue* share_target_info_dict = nullptr;
  ASSERT_TRUE(share_target_dict->GetDictionaryWithoutPathExpansion(
      manifest_url.spec(), &share_target_info_dict));
  EXPECT_EQ(2UL, share_target_info_dict->size());
  std::string action_url_in_dict;
  EXPECT_TRUE(
      share_target_info_dict->GetString(kActionKey, &action_url_in_dict));
  EXPECT_EQ("https://www.sharetarget.com/share", action_url_in_dict);
  std::string title_in_dict;
  EXPECT_TRUE(share_target_info_dict->GetString(kTitleKey, &title_in_dict));
  EXPECT_EQ("title", title_in_dict);

  // Share target already added now has null template. Remove from prefs.
  manifest_url = GURL("https://www.sharetarget.com/manifest.json");

  UpdateShareTargetInPrefs(manifest_url, blink::Manifest(), pref_service());

  share_target_dict =
      pref_service()->GetDictionary(prefs::kWebShareVisitedTargets);
  EXPECT_EQ(0UL, share_target_dict->size());
}

TEST_F(ShareTargetPrefHelperUnittest, AddShareTargetsWithQuery) {
  // Add a share target to prefs that wasn't previously stored.
  GURL manifest_url("https://www.sharetarget.com/manifest.json");
  blink::Manifest::ShareTarget share_target;
  share_target.action = GURL("https://www.sharetarget.com/share?a=b&c=d");
  share_target.params.title =
      base::NullableString16(base::ASCIIToUTF16("my title"));
  share_target.params.text =
      base::NullableString16(base::ASCIIToUTF16("my text"));
  share_target.params.url =
      base::NullableString16(base::ASCIIToUTF16("my url://"));
  blink::Manifest manifest;
  manifest.share_target =
      base::Optional<blink::Manifest::ShareTarget>(share_target);

  UpdateShareTargetInPrefs(manifest_url, manifest, pref_service());

  const base::DictionaryValue* share_target_dict =
      pref_service()->GetDictionary(prefs::kWebShareVisitedTargets);
  EXPECT_EQ(1UL, share_target_dict->size());
  const base::DictionaryValue* share_target_info_dict = nullptr;
  ASSERT_TRUE(share_target_dict->GetDictionaryWithoutPathExpansion(
      manifest_url.spec(), &share_target_info_dict));
  EXPECT_EQ(4UL, share_target_info_dict->size());
  std::string action_url_in_dict;
  EXPECT_TRUE(
      share_target_info_dict->GetString(kActionKey, &action_url_in_dict));
  EXPECT_EQ("https://www.sharetarget.com/share", action_url_in_dict);
  std::string title_in_dict;
  EXPECT_TRUE(share_target_info_dict->GetString(kTitleKey, &title_in_dict));
  EXPECT_EQ("my title", title_in_dict);
  std::string text_in_dict;
  EXPECT_TRUE(share_target_info_dict->GetString(kTextKey, &text_in_dict));
  EXPECT_EQ("my text", text_in_dict);
  std::string url_in_dict;
  EXPECT_TRUE(share_target_info_dict->GetString(kUrlKey, &url_in_dict));
  EXPECT_EQ("my url://", url_in_dict);
}

}  // namespace
