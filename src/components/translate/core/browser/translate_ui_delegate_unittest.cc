// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_ui_delegate.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/language/core/browser/language_prefs.h"

#include "components/infobars/core/infobar.h"
#include "components/language/core/browser/language_model.h"
#include "components/language/core/browser/language_prefs.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/language_experiments.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/translate/core/browser/mock_translate_client.h"
#include "components/translate/core/browser/mock_translate_driver.h"
#include "components/translate/core/browser/mock_translate_ranker.h"
#include "components/translate/core/browser/translate_client.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "components/variations/variations_associated_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/translate_event.pb.h"
#include "url/gurl.h"

namespace translate {
namespace {

using ::testing::_;
using testing::MockTranslateClient;
using testing::MockTranslateDriver;
using testing::MockTranslateRanker;
using ::testing::Return;
using ::testing::Test;

class MockLanguageModel : public language::LanguageModel {
  std::vector<LanguageDetails> GetLanguages() override {
    return {LanguageDetails("en", 1.0)};
  }
};

class TranslateUIDelegateTest : public ::testing::Test {
 public:
  TranslateUIDelegateTest() = default;

  TranslateUIDelegateTest(const TranslateUIDelegateTest&) = delete;
  TranslateUIDelegateTest& operator=(const TranslateUIDelegateTest&) = delete;

  void SetUp() override {
    pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    language::LanguagePrefs::RegisterProfilePrefs(pref_service_->registry());
    pref_service_->SetString(language::prefs::kAcceptLanguages, std::string());
#if BUILDFLAG(IS_CHROMEOS_ASH)
    pref_service_->SetString(language::prefs::kPreferredLanguages,
                             std::string());
#endif

    pref_service_->registry()->RegisterBooleanPref(
        prefs::kOfferTranslateEnabled, true);
    TranslatePrefs::RegisterProfilePrefs(pref_service_->registry());

    client_ =
        std::make_unique<MockTranslateClient>(&driver_, pref_service_.get());
    ranker_ = std::make_unique<MockTranslateRanker>();
    language_model_ = std::make_unique<MockLanguageModel>();
    manager_ = std::make_unique<TranslateManager>(client_.get(), ranker_.get(),
                                                  language_model_.get());
    manager_->GetLanguageState()->set_translation_declined(false);

    delegate_ = std::make_unique<TranslateUIDelegate>(manager_->GetWeakPtr(),
                                                      "ar", "fr");
  }

  void testContentLanguages(bool disableObservers) {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        language::kContentLanguagesInLanguagePicker,
        {{language::kContentLanguagesDisableObserversParam,
          disableObservers ? "true" : "false"}});
    TranslateDownloadManager::GetInstance()->set_application_locale("en");
    std::unique_ptr<TranslatePrefs> prefs(client_->GetTranslatePrefs());
    prefs->AddToLanguageList("de", /*force_blocked=*/false);
    prefs->AddToLanguageList("pl", /*force_blocked=*/false);

    std::unique_ptr<TranslateUIDelegate> delegate =
        std::make_unique<TranslateUIDelegate>(manager_->GetWeakPtr(), "en",
                                              "fr");

    std::vector<std::string> expected_codes = {"de", "pl"};
    std::vector<std::string> actual_codes;

    delegate->GetContentLanguagesCodes(&actual_codes);

    EXPECT_THAT(expected_codes, ::testing::ContainerEq(actual_codes));

    prefs->AddToLanguageList("it", /*force_blocked=*/false);

    delegate->GetContentLanguagesCodes(&actual_codes);

    if (disableObservers) {
      expected_codes = {"de", "pl"};
    } else {
      expected_codes = {"de", "pl", "it"};
    }
    EXPECT_THAT(expected_codes, ::testing::ContainerEq(actual_codes));
  }
  // Do not reorder. These are ordered for dependency on creation/destruction.
  MockTranslateDriver driver_;
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> pref_service_;
  std::unique_ptr<MockTranslateClient> client_;
  std::unique_ptr<MockTranslateRanker> ranker_;
  std::unique_ptr<MockLanguageModel> language_model_;
  std::unique_ptr<TranslateManager> manager_;
  std::unique_ptr<TranslateUIDelegate> delegate_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(TranslateUIDelegateTest, CheckDeclinedFalse) {
  EXPECT_CALL(*ranker_, RecordTranslateEvent(
                            metrics::TranslateEventProto::USER_IGNORE, _, _))
      .Times(1);

  std::unique_ptr<TranslatePrefs> prefs(client_->GetTranslatePrefs());
  for (int i = 0; i < 10; i++) {
    prefs->IncrementTranslationAcceptedCount("ar");
  }
  prefs->IncrementTranslationDeniedCount("ar");
  int accepted_count = prefs->GetTranslationAcceptedCount("ar");
  int denied_count = prefs->GetTranslationDeniedCount("ar");
  int ignored_count = prefs->GetTranslationIgnoredCount("ar");

  delegate_->TranslationDeclined(false);

  EXPECT_EQ(accepted_count, prefs->GetTranslationAcceptedCount("ar"));
  EXPECT_EQ(denied_count, prefs->GetTranslationDeniedCount("ar"));
  EXPECT_EQ(ignored_count + 1, prefs->GetTranslationIgnoredCount("ar"));
  EXPECT_FALSE(manager_->GetLanguageState()->translation_declined());
}

TEST_F(TranslateUIDelegateTest, CheckDeclinedTrue) {
  EXPECT_CALL(*ranker_, RecordTranslateEvent(
                            metrics::TranslateEventProto::USER_DECLINE, _, _))
      .Times(1);

  std::unique_ptr<TranslatePrefs> prefs(client_->GetTranslatePrefs());
  for (int i = 0; i < 10; i++) {
    prefs->IncrementTranslationAcceptedCount("ar");
  }
  prefs->IncrementTranslationDeniedCount("ar");
  int denied_count = prefs->GetTranslationDeniedCount("ar");
  int ignored_count = prefs->GetTranslationIgnoredCount("ar");

  delegate_->TranslationDeclined(true);

  EXPECT_EQ(0, prefs->GetTranslationAcceptedCount("ar"));
  EXPECT_EQ(denied_count + 1, prefs->GetTranslationDeniedCount("ar"));
  EXPECT_EQ(ignored_count, prefs->GetTranslationIgnoredCount("ar"));
  EXPECT_TRUE(manager_->GetLanguageState()->translation_declined());
}

TEST_F(TranslateUIDelegateTest, SetLanguageBlocked) {
  EXPECT_CALL(
      *ranker_,
      RecordTranslateEvent(
          metrics::TranslateEventProto::USER_NEVER_TRANSLATE_LANGUAGE, _, _))
      .Times(1);

  std::unique_ptr<TranslatePrefs> prefs(client_->GetTranslatePrefs());
  manager_->GetLanguageState()->SetTranslateEnabled(true);
  prefs->UnblockLanguage("ar");
  EXPECT_FALSE(prefs->IsBlockedLanguage("ar"));

  delegate_->SetLanguageBlocked(true);
  EXPECT_TRUE(prefs->IsBlockedLanguage("ar"));

  delegate_->SetLanguageBlocked(false);
  EXPECT_FALSE(prefs->IsBlockedLanguage("ar"));
}

TEST_F(TranslateUIDelegateTest, SetShouldAlwaysTranslateUnblocksSite) {
  std::unique_ptr<TranslatePrefs> prefs(client_->GetTranslatePrefs());

  // Add example.com to the translate site blocklist.
  const GURL url("https://www.example.com/hello/world?fg=1");
  driver_.SetLastCommittedURL(url);
  delegate_->SetNeverPromptSite(true);
  EXPECT_TRUE(prefs->IsSiteOnNeverPromptList("www.example.com"));

  // Setting always translate should remove the site from the blocklist.
  delegate_->SetAlwaysTranslate(true);
  EXPECT_FALSE(prefs->IsSiteOnNeverPromptList("www.example.com"));
}

TEST_F(TranslateUIDelegateTest, ShouldAlwaysTranslateBeCheckedByDefaultNever) {
  std::unique_ptr<TranslatePrefs> prefs(client_->GetTranslatePrefs());
  prefs->ResetTranslationAcceptedCount("ar");

  for (int i = 0; i < 100; i++) {
    EXPECT_FALSE(delegate_->ShouldAlwaysTranslateBeCheckedByDefault());
    prefs->IncrementTranslationAcceptedCount("ar");
  }
}

TEST_F(TranslateUIDelegateTest, ShouldShowAlwaysTranslateShortcut) {
  std::unique_ptr<TranslatePrefs> prefs = client_->GetTranslatePrefs();
  for (int i = 0; i < kAlwaysTranslateShortcutMinimumAccepts; i++) {
    EXPECT_FALSE(delegate_->ShouldShowAlwaysTranslateShortcut())
        << " at iteration #" << i;
    prefs->IncrementTranslationAcceptedCount("ar");
  }
  EXPECT_TRUE(delegate_->ShouldShowAlwaysTranslateShortcut());
  driver_.set_incognito();
  EXPECT_FALSE(delegate_->ShouldShowAlwaysTranslateShortcut());
}

TEST_F(TranslateUIDelegateTest, ShouldShowNeverTranslateShortcut) {
  std::unique_ptr<TranslatePrefs> prefs = client_->GetTranslatePrefs();
  for (int i = 0; i < kNeverTranslateShortcutMinimumDenials; i++) {
    EXPECT_FALSE(delegate_->ShouldShowNeverTranslateShortcut())
        << " at iteration #" << i;
    prefs->IncrementTranslationDeniedCount("ar");
  }
  EXPECT_TRUE(delegate_->ShouldShowNeverTranslateShortcut());
  driver_.set_incognito();
  EXPECT_FALSE(delegate_->ShouldShowNeverTranslateShortcut());
}

TEST_F(TranslateUIDelegateTest, LanguageCodes) {
  // Test language codes.
  EXPECT_EQ("ar", delegate_->GetSourceLanguageCode());
  EXPECT_EQ("fr", delegate_->GetTargetLanguageCode());

  // Test language indices.
  const size_t ar_index = delegate_->GetSourceLanguageIndex();
  EXPECT_EQ("ar", delegate_->GetLanguageCodeAt(ar_index));
  const size_t fr_index = delegate_->GetTargetLanguageIndex();
  EXPECT_EQ("fr", delegate_->GetLanguageCodeAt(fr_index));

  // Test updating source / target codes.
  delegate_->UpdateSourceLanguage("es");
  EXPECT_EQ("es", delegate_->GetSourceLanguageCode());
  delegate_->UpdateTargetLanguage("de");
  EXPECT_EQ("de", delegate_->GetTargetLanguageCode());

  // Test updating source / target indices. Note that this also returns
  // the delegate to the starting state.
  delegate_->UpdateSourceLanguageIndex(ar_index);
  EXPECT_EQ("ar", delegate_->GetSourceLanguageCode());
  delegate_->UpdateTargetLanguageIndex(fr_index);
  EXPECT_EQ("fr", delegate_->GetTargetLanguageCode());
}

TEST_F(TranslateUIDelegateTest, ContentLanguagesWhenPrefChangeObserverEnabled) {
  testContentLanguages(/*disableObservers=*/false);
}

TEST_F(TranslateUIDelegateTest,
       ContentLanguagesWhenPrefChangeObserverDisabled) {
  testContentLanguages(/*disableObservers=*/true);
}

TEST_F(TranslateUIDelegateTest, ContentLanguagesWhenDisabled) {
  scoped_feature_list_.InitAndDisableFeature(
      language::kContentLanguagesInLanguagePicker);

  TranslateDownloadManager::GetInstance()->set_application_locale("en");
  std::unique_ptr<TranslatePrefs> prefs(client_->GetTranslatePrefs());
  prefs->AddToLanguageList("de", /*force_blocked=*/false);
  prefs->AddToLanguageList("pl", /*force_blocked=*/false);

  std::unique_ptr<TranslateUIDelegate> delegate =
      std::make_unique<TranslateUIDelegate>(manager_->GetWeakPtr(), "en", "fr");

  std::vector<std::string> actual_codes;

  delegate->GetContentLanguagesCodes(&actual_codes);
  EXPECT_TRUE(actual_codes.empty());
}

}  // namespace

TEST_F(TranslateUIDelegateTest, GetPageHost) {
  const GURL url("https://www.example.com/hello/world?fg=1");
  driver_.SetLastCommittedURL(url);
  EXPECT_EQ("www.example.com", delegate_->GetPageHost());
}

}  // namespace translate
