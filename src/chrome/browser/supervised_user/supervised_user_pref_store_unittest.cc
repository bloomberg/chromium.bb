// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/values.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/browser/supervised_user/supervised_user_pref_store.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service.h"
#include "chrome/common/net/safe_search_util.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/testing_pref_store.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::base::DictionaryValue;
using ::base::Value;
using ::testing::Optional;

class SupervisedUserPrefStoreFixture : public PrefStore::Observer {
 public:
  explicit SupervisedUserPrefStoreFixture(
      SupervisedUserSettingsService* settings_service);
  ~SupervisedUserPrefStoreFixture() override;

  base::DictionaryValue* changed_prefs() {
    return &changed_prefs_;
  }

  bool initialization_completed() const {
    return initialization_completed_;
  }

  // PrefStore::Observer implementation:
  void OnPrefValueChanged(const std::string& key) override;
  void OnInitializationCompleted(bool succeeded) override;

 private:
  scoped_refptr<SupervisedUserPrefStore> pref_store_;
  base::DictionaryValue changed_prefs_;
  bool initialization_completed_;
};

SupervisedUserPrefStoreFixture::SupervisedUserPrefStoreFixture(
    SupervisedUserSettingsService* settings_service)
    : pref_store_(new SupervisedUserPrefStore(settings_service)),
      initialization_completed_(pref_store_->IsInitializationComplete()) {
  pref_store_->AddObserver(this);
}

SupervisedUserPrefStoreFixture::~SupervisedUserPrefStoreFixture() {
  pref_store_->RemoveObserver(this);
}

void SupervisedUserPrefStoreFixture::OnPrefValueChanged(
    const std::string& key) {
  const base::Value* value = nullptr;
  ASSERT_TRUE(pref_store_->GetValue(key, &value));
  changed_prefs_.SetPath(key, value->Clone());
}

void SupervisedUserPrefStoreFixture::OnInitializationCompleted(bool succeeded) {
  EXPECT_FALSE(initialization_completed_);
  EXPECT_TRUE(succeeded);
  EXPECT_TRUE(pref_store_->IsInitializationComplete());
  initialization_completed_ = true;
}

}  // namespace

class SupervisedUserPrefStoreTest : public ::testing::Test {
 public:
  SupervisedUserPrefStoreTest() {}
  void SetUp() override;
  void TearDown() override;

 protected:
  SupervisedUserSettingsService service_;
  scoped_refptr<TestingPrefStore> pref_store_;
};

void SupervisedUserPrefStoreTest::SetUp() {
  pref_store_ = new TestingPrefStore();
  service_.Init(pref_store_);
}

void SupervisedUserPrefStoreTest::TearDown() {
  service_.Shutdown();
}

TEST_F(SupervisedUserPrefStoreTest, ConfigureSettings) {
  SupervisedUserPrefStoreFixture fixture(&service_);
  EXPECT_FALSE(fixture.initialization_completed());

  // Prefs should not change yet when the service is ready, but not
  // activated yet.
  pref_store_->SetInitializationCompleted();
  EXPECT_TRUE(fixture.initialization_completed());
  EXPECT_EQ(0u, fixture.changed_prefs()->DictSize());

  service_.SetActive(true);

  // kAllowDeletingBrowserHistory is hardcoded to false for supervised users.
  EXPECT_THAT(fixture.changed_prefs()->FindBoolPath(
                  prefs::kAllowDeletingBrowserHistory),
              Optional(false));

  // kSupervisedModeManualHosts does not have a hardcoded value.
  base::DictionaryValue* manual_hosts = nullptr;
  EXPECT_FALSE(fixture.changed_prefs()->GetDictionary(
      prefs::kSupervisedUserManualHosts, &manual_hosts));

  // kForceGoogleSafeSearch defaults to true and kForceYouTubeRestrict defaults
  // to Moderate for supervised users.
  EXPECT_THAT(
      fixture.changed_prefs()->FindBoolPath(prefs::kForceGoogleSafeSearch),
      Optional(true));
  int force_youtube_restrict =
      fixture.changed_prefs()
          ->FindIntPath(prefs::kForceYouTubeRestrict)
          .value_or(safe_search_util::YOUTUBE_RESTRICT_OFF);
  EXPECT_EQ(force_youtube_restrict,
            safe_search_util::YOUTUBE_RESTRICT_MODERATE);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Permissions requests default to disallowed.
  EXPECT_THAT(fixture.changed_prefs()->FindBoolPath(
                  prefs::kSupervisedUserExtensionsMayRequestPermissions),
              Optional(false));
#endif

  // Activating the service again should not change anything.
  fixture.changed_prefs()->DictClear();
  service_.SetActive(true);
  EXPECT_EQ(0u, fixture.changed_prefs()->DictSize());

  // kSupervisedModeManualHosts can be configured by the custodian.
  base::Value hosts(base::Value::Type::DICTIONARY);
  hosts.SetBoolKey("example.com", true);
  hosts.SetBoolKey("moose.org", false);
  service_.SetLocalSetting(supervised_users::kContentPackManualBehaviorHosts,
                           std::make_unique<base::Value>(hosts.Clone()));
  EXPECT_EQ(1u, fixture.changed_prefs()->DictSize());
  ASSERT_TRUE(fixture.changed_prefs()->GetDictionary(
      prefs::kSupervisedUserManualHosts, &manual_hosts));
  EXPECT_TRUE(*manual_hosts == hosts);

  // kForceGoogleSafeSearch and kForceYouTubeRestrict can be configured by the
  // custodian, overriding the hardcoded default.
  fixture.changed_prefs()->DictClear();
  service_.SetLocalSetting(supervised_users::kForceSafeSearch,
                           std::make_unique<base::Value>(false));
  EXPECT_EQ(1u, fixture.changed_prefs()->DictSize());
  EXPECT_THAT(
      fixture.changed_prefs()->FindBoolPath(prefs::kForceGoogleSafeSearch),
      Optional(false));

  force_youtube_restrict =
      fixture.changed_prefs()
          ->FindIntPath(prefs::kForceYouTubeRestrict)
          .value_or(safe_search_util::YOUTUBE_RESTRICT_MODERATE);
  EXPECT_EQ(force_youtube_restrict, safe_search_util::YOUTUBE_RESTRICT_OFF);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // The custodian can allow sites and apps to request permissions.
  // Currently tested indirectly by enabling geolocation requests.
  // TODO(crbug/1024646): Update Kids Management server to set a new bit for
  // extension permissions and update this test.

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      "SupervisedUsers.ExtensionsMayRequestPermissions", 0);

  fixture.changed_prefs()->DictClear();
  service_.SetLocalSetting(supervised_users::kGeolocationDisabled,
                           std::make_unique<base::Value>(false));
  EXPECT_EQ(1u, fixture.changed_prefs()->DictSize());
  EXPECT_THAT(fixture.changed_prefs()->FindBoolPath(
                  prefs::kSupervisedUserExtensionsMayRequestPermissions),
              Optional(true));

  histogram_tester.ExpectUniqueSample(
      "SupervisedUsers.ExtensionsMayRequestPermissions", /*enabled=*/true, 1);
  histogram_tester.ExpectTotalCount(
      "SupervisedUsers.ExtensionsMayRequestPermissions", 1);

  fixture.changed_prefs()->DictClear();
  service_.SetLocalSetting(supervised_users::kGeolocationDisabled,
                           std::make_unique<base::Value>(true));
  EXPECT_EQ(1u, fixture.changed_prefs()->DictSize());
  EXPECT_THAT(fixture.changed_prefs()->FindBoolPath(
                  prefs::kSupervisedUserExtensionsMayRequestPermissions),
              Optional(false));

  histogram_tester.ExpectBucketCount(
      "SupervisedUsers.ExtensionsMayRequestPermissions", /*enabled=*/false, 1);
  histogram_tester.ExpectTotalCount(
      "SupervisedUsers.ExtensionsMayRequestPermissions", 2);

#endif
}

TEST_F(SupervisedUserPrefStoreTest, ActivateSettingsBeforeInitialization) {
  SupervisedUserPrefStoreFixture fixture(&service_);
  EXPECT_FALSE(fixture.initialization_completed());

  service_.SetActive(true);
  EXPECT_FALSE(fixture.initialization_completed());
  EXPECT_EQ(0u, fixture.changed_prefs()->DictSize());

  pref_store_->SetInitializationCompleted();
  EXPECT_TRUE(fixture.initialization_completed());
  EXPECT_EQ(0u, fixture.changed_prefs()->DictSize());
}

TEST_F(SupervisedUserPrefStoreTest, CreatePrefStoreAfterInitialization) {
  pref_store_->SetInitializationCompleted();
  service_.SetActive(true);

  SupervisedUserPrefStoreFixture fixture(&service_);
  EXPECT_TRUE(fixture.initialization_completed());
  EXPECT_EQ(0u, fixture.changed_prefs()->DictSize());
}
