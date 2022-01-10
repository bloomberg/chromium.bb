// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/store_metrics_reporter.h"

#include "base/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/os_crypt/os_crypt_mocker.h"
#include "components/password_manager/core/browser/mock_password_reuse_manager.h"
#include "components/password_manager/core/browser/mock_password_store_interface.h"
#include "components/password_manager/core/browser/password_manager_features_util.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/sync_username_test_base.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Bool;
using ::testing::Range;
using ::testing::Return;

namespace password_manager {
namespace {

PasswordForm CreateForm(const std::string& signon_realm,
                        const std::string& username,
                        const std::string& password) {
  PasswordForm form;
  form.signon_realm = signon_realm;
  form.username_value = base::ASCIIToUTF16(username);
  form.password_value = base::ASCIIToUTF16(password);
  return form;
}

void AddMetricsTestData(TestPasswordStore* store) {
  PasswordForm password_form;
  password_form.url = GURL("http://example.com");
  password_form.username_value = u"test1@gmail.com";
  password_form.password_value = u"test";
  password_form.signon_realm = "http://example.com/";
  password_form.times_used = 0;
  store->AddLogin(password_form);

  password_form.username_value = u"test2@gmail.com";
  password_form.times_used = 1;
  store->AddLogin(password_form);

  password_form.url = GURL("http://second.example.com");
  password_form.signon_realm = "http://second.example.com";
  password_form.times_used = 3;
  store->AddLogin(password_form);

  password_form.username_value = u"test3@gmail.com";
  password_form.type = PasswordForm::Type::kGenerated;
  password_form.times_used = 2;
  store->AddLogin(password_form);

  password_form.url = GURL("ftp://third.example.com/");
  password_form.signon_realm = "ftp://third.example.com/";
  password_form.times_used = 4;
  password_form.scheme = PasswordForm::Scheme::kOther;
  store->AddLogin(password_form);

  password_form.url = GURL("http://fourth.example.com/");
  password_form.signon_realm = "http://fourth.example.com/";
  password_form.type = PasswordForm::Type::kFormSubmission;
  password_form.username_value = u"";
  password_form.times_used = 10;
  password_form.scheme = PasswordForm::Scheme::kHtml;
  store->AddLogin(password_form);

  password_form.url = GURL("https://fifth.example.com/");
  password_form.signon_realm = "https://fifth.example.com/";
  password_form.password_value = u"";
  password_form.blocked_by_user = true;
  store->AddLogin(password_form);

  password_form.url = GURL("https://sixth.example.com/");
  password_form.signon_realm = "https://sixth.example.com/";
  password_form.username_value = u"my_username";
  password_form.password_value = u"my_password";
  password_form.blocked_by_user = false;
  store->AddLogin(password_form);

  password_form.url = GURL();
  password_form.signon_realm = "android://hash@com.example.android/";
  password_form.username_value = u"JohnDoe";
  password_form.password_value = u"my_password";
  password_form.blocked_by_user = false;
  store->AddLogin(password_form);

  password_form.username_value = u"JaneDoe";
  store->AddLogin(password_form);

  password_form.url = GURL("http://rsolomakhin.github.io/autofill/");
  password_form.signon_realm = "http://rsolomakhin.github.io/";
  password_form.blocked_by_user = true;
  store->AddLogin(password_form);

  password_form.url = GURL("https://rsolomakhin.github.io/autofill/");
  password_form.signon_realm = "https://rsolomakhin.github.io/";
  password_form.blocked_by_user = true;
  store->AddLogin(password_form);

  password_form.url = GURL("http://rsolomakhin.github.io/autofill/123");
  password_form.signon_realm = "http://rsolomakhin.github.io/";
  password_form.blocked_by_user = true;
  store->AddLogin(password_form);

  password_form.url = GURL("https://rsolomakhin.github.io/autofill/1234");
  password_form.signon_realm = "https://rsolomakhin.github.io/";
  password_form.blocked_by_user = true;
  store->AddLogin(password_form);
}

class StoreMetricsReporterTest : public SyncUsernameTestBase {
 public:
  StoreMetricsReporterTest() = default;

  void SetUp() override {
    // Mock OSCrypt. There is a call to OSCrypt inside HashPasswordManager so it
    // should be mocked.
    OSCryptMocker::SetUp();

    feature_list_.InitWithFeatures({features::kPasswordReuseDetectionEnabled},
                                   {});

    prefs_.registry()->RegisterBooleanPref(prefs::kCredentialsEnableService,
                                           false);
    prefs_.registry()->RegisterBooleanPref(
        password_manager::prefs::kWasAutoSignInFirstRunExperienceShown, false);
    prefs_.registry()->RegisterBooleanPref(prefs::kWereOldGoogleLoginsRemoved,
                                           false);
  }

  void TearDown() override { OSCryptMocker::TearDown(); }

  ~StoreMetricsReporterTest() override = default;

 protected:
  base::test::ScopedFeatureList feature_list_;
  TestingPrefServiceSimple prefs_;
};

// The test fixture is used to test StoreIndependentMetrics. The parameter
// defines whether password manager is enabled.
class StoreMetricsReporterTestWithParams
    : public StoreMetricsReporterTest,
      public ::testing::WithParamInterface<bool> {};

// Test that store-independent metrics are reported correctly.
TEST_P(StoreMetricsReporterTestWithParams, StoreIndependentMetrics) {
  const bool password_manager_enabled = GetParam();

  prefs_.SetBoolean(password_manager::prefs::kCredentialsEnableService,
                    password_manager_enabled);
  base::HistogramTester histogram_tester;

  StoreMetricsReporter reporter(
      /*profile_store=*/nullptr, /*account_store=*/nullptr, sync_service(),
      identity_manager(), &prefs_, /*password_reuse_manager=*/nullptr,
      /*is_under_advanced_protection=*/false,
      /*done_callback*/ base::DoNothing());

  histogram_tester.ExpectUniqueSample("PasswordManager.Enabled2",
                                      password_manager_enabled, 1);
}

TEST_F(StoreMetricsReporterTest, ReportAccountsPerSiteHiResMetricsTest) {
  auto profile_store =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(false));
  profile_store->Init(&prefs_, /*affiliated_match_helper=*/nullptr);
  AddMetricsTestData(profile_store.get());
  // Note: We also create and populate an account store here and instruct it to
  // report metrics, even though all the checks below only test the profile DB.
  // This is to make sure that the account DB doesn't write to any of the same
  // histograms.
  auto account_store =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(true));
  account_store->Init(&prefs_, /*affiliated_match_helper=*/nullptr);
  AddMetricsTestData(account_store.get());

  base::HistogramTester histogram_tester;
  StoreMetricsReporter reporter(profile_store.get(), account_store.get(),
                                sync_service(), identity_manager(), &prefs_,
                                /*password_reuse_manager=*/nullptr,
                                /*is_under_advanced_protection=*/false,
                                /*done_callback*/ base::DoNothing());
  // Wait for the metrics to get reported, which involves queries to the
  // stores, i.e. to background task runners.
  RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountsPerSiteHiRes.AutoGenerated."
      "WithoutCustomPassphrase",
      1, 2);

  histogram_tester.ExpectBucketCount(
      "PasswordManager.AccountsPerSiteHiRes.UserCreated."
      "WithoutCustomPassphrase",
      1, 3);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.AccountsPerSiteHiRes.UserCreated."
      "WithoutCustomPassphrase",
      2, 2);

  histogram_tester.ExpectBucketCount(
      "PasswordManager.AccountsPerSiteHiRes.Overall.WithoutCustomPassphrase", 1,
      5);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.AccountsPerSiteHiRes.Overall.WithoutCustomPassphrase", 2,
      2);

  account_store->ShutdownOnUIThread();
  profile_store->ShutdownOnUIThread();
  // Make sure the PasswordStore destruction parts on the background sequence
  // finish, otherwise we get memory leak reports.
  RunUntilIdle();
}

TEST_F(StoreMetricsReporterTest, ReportTotalAccountsHiResMetricsTest) {
  auto profile_store =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(false));
  profile_store->Init(&prefs_, /*affiliated_match_helper=*/nullptr);
  AddMetricsTestData(profile_store.get());
  // Note: We also create and populate an account store here and instruct it to
  // report metrics, even though all the checks below only test the profile DB.
  // This is to make sure that the account DB doesn't write to any of the same
  // histograms.
  auto account_store =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(true));
  account_store->Init(&prefs_, /*affiliated_match_helper=*/nullptr);
  AddMetricsTestData(account_store.get());

  base::HistogramTester histogram_tester;
  StoreMetricsReporter reporter(profile_store.get(), account_store.get(),
                                sync_service(), identity_manager(), &prefs_,
                                /*password_reuse_manager=*/nullptr,
                                /*is_under_advanced_protection=*/false,
                                /*done_callback*/ base::DoNothing());

  // Wait for the metrics to get reported, which involves queries to the
  // stores, i.e. to background task runners.
  RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.TotalAccountsHiRes.ByType.AutoGenerated."
      "WithoutCustomPassphrase",
      2, 1);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.TotalAccountsHiRes.ByType.UserCreated."
      "WithoutCustomPassphrase",
      7, 1);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.TotalAccountsHiRes.ByType.Overall."
      "WithoutCustomPassphrase",
      9, 1);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.TotalAccountsHiRes.WithScheme.Android", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.TotalAccountsHiRes.WithScheme.Ftp", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.TotalAccountsHiRes.WithScheme.Http", 5, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.TotalAccountsHiRes.WithScheme.Https", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.TotalAccountsHiRes.WithScheme.Other", 0, 1);

  account_store->ShutdownOnUIThread();
  profile_store->ShutdownOnUIThread();
  // Make sure the PasswordStore destruction parts on the background sequence
  // finish, otherwise we get memory leak reports.
  RunUntilIdle();
}

TEST_F(StoreMetricsReporterTest, ReportTimesPasswordUsedMetricsTest) {
  auto profile_store =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(false));
  profile_store->Init(&prefs_, /*affiliated_match_helper=*/nullptr);
  AddMetricsTestData(profile_store.get());
  // Note: We also create and populate an account store here and instruct it to
  // report metrics, even though all the checks below only test the profile DB.
  // This is to make sure that the account DB doesn't write to any of the same
  // histograms.
  auto account_store =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(true));
  account_store->Init(&prefs_, /*affiliated_match_helper=*/nullptr);
  AddMetricsTestData(account_store.get());

  base::HistogramTester histogram_tester;
  StoreMetricsReporter reporter(profile_store.get(), account_store.get(),
                                sync_service(), identity_manager(), &prefs_,
                                /*password_reuse_manager=*/nullptr,
                                /*is_under_advanced_protection=*/false,
                                /*done_callback*/ base::DoNothing());

  // Wait for the metrics to get reported, which involves queries to the
  // stores, i.e. to background task runners.
  RunUntilIdle();

  histogram_tester.ExpectBucketCount(
      "PasswordManager.TimesPasswordUsed.AutoGenerated.WithoutCustomPassphrase",
      2, 1);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.TimesPasswordUsed.AutoGenerated.WithoutCustomPassphrase",
      4, 1);

  histogram_tester.ExpectBucketCount(
      "PasswordManager.TimesPasswordUsed.UserCreated.WithoutCustomPassphrase",
      0, 1);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.TimesPasswordUsed.UserCreated.WithoutCustomPassphrase",
      1, 1);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.TimesPasswordUsed.UserCreated.WithoutCustomPassphrase",
      3, 1);

  histogram_tester.ExpectBucketCount(
      "PasswordManager.TimesPasswordUsed.Overall.WithoutCustomPassphrase", 0,
      1);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.TimesPasswordUsed.Overall.WithoutCustomPassphrase", 1,
      1);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.TimesPasswordUsed.Overall.WithoutCustomPassphrase", 2,
      1);
  // The bucket for 3 and 4 is the same. Thus we expect two samples here.
  histogram_tester.ExpectBucketCount(
      "PasswordManager.TimesPasswordUsed.Overall.WithoutCustomPassphrase", 3,
      2);

  account_store->ShutdownOnUIThread();
  profile_store->ShutdownOnUIThread();
  // Make sure the PasswordStore destruction parts on the background sequence
  // finish, otherwise we get memory leak reports.
  RunUntilIdle();
}

// The following tests are mostly a copy of Report*MetricsTest, but covering
// the account store instead of the profile store. All the metrics that *are*
// covered have
// ".AccountStore" in their names.
TEST_F(StoreMetricsReporterTest,
       ReportAccountStoreAccountsPerSiteHiResMetricsTest) {
  // Note: We also populate the profile store here and instruct it to report
  // metrics, even though all the checks below only test the account DB. This is
  // to make sure that the profile DB doesn't write to any of the same
  // histograms.

  auto profile_store =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(false));
  profile_store->Init(&prefs_, /*affiliated_match_helper=*/nullptr);
  AddMetricsTestData(profile_store.get());
  // Note: We also create and populate an account store here and instruct it to
  // report metrics, even though all the checks below only test the profile DB.
  // This is to make sure that the account DB doesn't write to any of the same
  // histograms.
  auto account_store =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(true));
  account_store->Init(&prefs_, /*affiliated_match_helper=*/nullptr);
  AddMetricsTestData(account_store.get());

  base::HistogramTester histogram_tester;
  StoreMetricsReporter reporter(profile_store.get(), account_store.get(),
                                sync_service(), identity_manager(), &prefs_,
                                /*password_reuse_manager=*/nullptr,
                                /*is_under_advanced_protection=*/false,
                                /*done_callback*/ base::DoNothing());

  // Wait for the metrics to get reported, which involves queries to the
  // stores, i.e. to background task runners.
  RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountStore.AccountsPerSiteHiRes.AutoGenerated."
      "WithoutCustomPassphrase",
      1, 2);

  histogram_tester.ExpectBucketCount(
      "PasswordManager.AccountStore.AccountsPerSiteHiRes.UserCreated."
      "WithoutCustomPassphrase",
      1, 3);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.AccountStore.AccountsPerSiteHiRes.UserCreated."
      "WithoutCustomPassphrase",
      2, 2);

  histogram_tester.ExpectBucketCount(
      "PasswordManager.AccountStore.AccountsPerSiteHiRes.Overall."
      "WithoutCustomPassphrase",
      1, 5);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.AccountStore.AccountsPerSiteHiRes.Overall."
      "WithoutCustomPassphrase",
      2, 2);

  account_store->ShutdownOnUIThread();
  profile_store->ShutdownOnUIThread();
  // Make sure the PasswordStore destruction parts on the background sequence
  // finish, otherwise we get memory leak reports.
  RunUntilIdle();
}

TEST_F(StoreMetricsReporterTest,
       ReportAccountStoreTotalAccountsHiResMetricsTest) {
  // Note: We also populate the profile store here and instruct it to report
  // metrics, even though all the checks below only test the account DB. This is
  // to make sure that the profile DB doesn't write to any of the same
  // histograms.

  auto profile_store =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(false));
  profile_store->Init(&prefs_, /*affiliated_match_helper=*/nullptr);
  AddMetricsTestData(profile_store.get());
  // Note: We also create and populate an account store here and instruct it to
  // report metrics, even though all the checks below only test the profile DB.
  // This is to make sure that the account DB doesn't write to any of the same
  // histograms.
  auto account_store =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(true));
  account_store->Init(&prefs_, /*affiliated_match_helper=*/nullptr);
  AddMetricsTestData(account_store.get());

  base::HistogramTester histogram_tester;
  StoreMetricsReporter reporter(profile_store.get(), account_store.get(),
                                sync_service(), identity_manager(), &prefs_,
                                /*password_reuse_manager=*/nullptr,
                                /*is_under_advanced_protection=*/false,
                                /*done_callback*/ base::DoNothing());

  // Wait for the metrics to get reported, which involves queries to the
  // stores, i.e. to background task runners.
  RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountStore.TotalAccountsHiRes.ByType.AutoGenerated."
      "WithoutCustomPassphrase",
      2, 1);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountStore.TotalAccountsHiRes.ByType.UserCreated."
      "WithoutCustomPassphrase",
      7, 1);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountStore.TotalAccountsHiRes.ByType.Overall."
      "WithoutCustomPassphrase",
      9, 1);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountStore.TotalAccountsHiRes.WithScheme.Android", 2,
      1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountStore.TotalAccountsHiRes.WithScheme.Ftp", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountStore.TotalAccountsHiRes.WithScheme.Http", 5, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountStore.TotalAccountsHiRes.WithScheme.Https", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountStore.TotalAccountsHiRes.WithScheme.Other", 0, 1);

  account_store->ShutdownOnUIThread();
  profile_store->ShutdownOnUIThread();
  // Make sure the PasswordStore destruction parts on the background sequence
  // finish, otherwise we get memory leak reports.
  RunUntilIdle();
}

TEST_F(StoreMetricsReporterTest,
       ReportAccountStoreTimesPasswordUsedMetricsTest) {
  // Note: We also populate the profile store here and instruct it to report
  // metrics, even though all the checks below only test the account DB. This is
  // to make sure that the profile DB doesn't write to any of the same
  // histograms.

  auto profile_store =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(false));
  profile_store->Init(&prefs_, /*affiliated_match_helper=*/nullptr);
  AddMetricsTestData(profile_store.get());
  // Note: We also create and populate an account store here and instruct it to
  // report metrics, even though all the checks below only test the profile DB.
  // This is to make sure that the account DB doesn't write to any of the same
  // histograms.
  auto account_store =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(true));
  account_store->Init(&prefs_, /*affiliated_match_helper=*/nullptr);
  AddMetricsTestData(account_store.get());

  base::HistogramTester histogram_tester;
  StoreMetricsReporter reporter(profile_store.get(), account_store.get(),
                                sync_service(), identity_manager(), &prefs_,
                                /*password_reuse_manager=*/nullptr,
                                /*is_under_advanced_protection=*/false,
                                /*done_callback*/ base::DoNothing());

  // Wait for the metrics to get reported, which involves queries to the
  // stores, i.e. to background task runners.
  RunUntilIdle();

  histogram_tester.ExpectBucketCount(
      "PasswordManager.AccountStore.TimesPasswordUsed.AutoGenerated."
      "WithoutCustomPassphrase",
      2, 1);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.AccountStore.TimesPasswordUsed.AutoGenerated."
      "WithoutCustomPassphrase",
      4, 1);

  histogram_tester.ExpectBucketCount(
      "PasswordManager.AccountStore.TimesPasswordUsed.UserCreated."
      "WithoutCustomPassphrase",
      0, 1);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.AccountStore.TimesPasswordUsed.UserCreated."
      "WithoutCustomPassphrase",
      1, 1);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.AccountStore.TimesPasswordUsed.UserCreated."
      "WithoutCustomPassphrase",
      3, 1);

  histogram_tester.ExpectBucketCount(
      "PasswordManager.AccountStore.TimesPasswordUsed.Overall."
      "WithoutCustomPassphrase",
      0, 1);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.AccountStore.TimesPasswordUsed.Overall."
      "WithoutCustomPassphrase",
      1, 1);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.AccountStore.TimesPasswordUsed.Overall."
      "WithoutCustomPassphrase",
      2, 1);
  // The bucket for 3 and 4 is the same. Thus we expect two samples here.
  histogram_tester.ExpectBucketCount(
      "PasswordManager.AccountStore.TimesPasswordUsed.Overall."
      "WithoutCustomPassphrase",
      3, 2);

  account_store->ShutdownOnUIThread();
  profile_store->ShutdownOnUIThread();
  // Make sure the PasswordStore destruction parts on the background sequence
  // finish, otherwise we get memory leak reports.
  RunUntilIdle();
}

TEST_F(StoreMetricsReporterTest, DuplicatesMetrics_NoDuplicates) {
  auto profile_store =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(false));
  profile_store->Init(&prefs_, /*affiliated_match_helper=*/nullptr);

  // No duplicate.
  PasswordForm password_form;
  password_form.signon_realm = "http://example1.com/";
  password_form.url = GURL("http://example1.com/");
  password_form.username_element = u"userelem_1";
  password_form.username_value = u"username_1";
  password_form.password_value = u"password_1";
  profile_store->AddLogin(password_form);

  // Different username -> no duplicate.
  password_form.signon_realm = "http://example2.com/";
  password_form.url = GURL("http://example2.com/");
  password_form.username_value = u"username_1";
  profile_store->AddLogin(password_form);
  password_form.username_value = u"username_2";
  profile_store->AddLogin(password_form);

  // Blocklisted forms don't count as duplicates (neither against other
  // blocklisted forms nor against actual saved credentials).
  password_form.signon_realm = "http://example3.com/";
  password_form.url = GURL("http://example3.com/");
  password_form.username_value = u"username_1";
  profile_store->AddLogin(password_form);
  password_form.blocked_by_user = true;
  password_form.username_value = u"username_2";
  profile_store->AddLogin(password_form);
  password_form.username_value = u"username_3";
  profile_store->AddLogin(password_form);

  base::HistogramTester histogram_tester;
  StoreMetricsReporter reporter(profile_store.get(), /*account_store=*/nullptr,
                                sync_service(), identity_manager(), &prefs_,
                                /*password_reuse_manager=*/nullptr,
                                /*is_under_advanced_protection=*/false,
                                /*done_callback*/ base::DoNothing());

  // Wait for the metrics to get reported, which involves queries to the
  // stores, i.e. to background task runners.
  RunUntilIdle();

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "PasswordManager.CredentialsWithDuplicates"),
              testing::ElementsAre(base::Bucket(0, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "PasswordManager.CredentialsWithMismatchedDuplicates"),
              testing::ElementsAre(base::Bucket(0, 1)));

  profile_store->ShutdownOnUIThread();
  // Make sure the PasswordStore destruction parts on the background sequence
  // finish, otherwise we get memory leak reports.
  RunUntilIdle();
}

TEST_F(StoreMetricsReporterTest, DuplicatesMetrics_ExactDuplicates) {
  auto profile_store =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(false));
  profile_store->Init(&prefs_, /*affiliated_match_helper=*/nullptr);

  // Add some PasswordForms that are "exact" duplicates (only the
  // username_element is different, which doesn't matter).
  PasswordForm password_form;
  password_form.signon_realm = "http://example1.com/";
  password_form.url = GURL("http://example1.com/");
  password_form.username_element = u"userelem_1";
  password_form.username_value = u"username_1";
  profile_store->AddLogin(password_form);
  password_form.username_element = u"userelem_2";
  profile_store->AddLogin(password_form);
  // The number of "identical" credentials doesn't matter; we count the *sets*
  // of duplicates.
  password_form.username_element = u"userelem_3";
  profile_store->AddLogin(password_form);

  // Similarly, origin doesn't make forms "different" either.
  password_form.signon_realm = "http://example2.com/";
  password_form.url = GURL("http://example2.com/path1");
  profile_store->AddLogin(password_form);
  password_form.url = GURL("http://example2.com/path2");
  profile_store->AddLogin(password_form);

  base::HistogramTester histogram_tester;
  StoreMetricsReporter reporter(profile_store.get(), /*account_store=*/nullptr,
                                sync_service(), identity_manager(), &prefs_,
                                /*password_reuse_manager=*/nullptr,
                                /*is_under_advanced_protection=*/false,
                                /*done_callback*/ base::DoNothing());

  // Wait for the metrics to get reported, which involves queries to the
  // stores, i.e. to background task runners.
  RunUntilIdle();

  // There should be 2 groups of "exact" duplicates.
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "PasswordManager.CredentialsWithDuplicates"),
              testing::ElementsAre(base::Bucket(2, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "PasswordManager.CredentialsWithMismatchedDuplicates"),
              testing::ElementsAre(base::Bucket(0, 1)));

  profile_store->ShutdownOnUIThread();
  // Make sure the PasswordStore destruction parts on the background sequence
  // finish, otherwise we get memory leak reports.
  RunUntilIdle();
}

TEST_F(StoreMetricsReporterTest, DuplicatesMetrics_MismatchedDuplicates) {
  auto profile_store =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(false));
  profile_store->Init(&prefs_, /*affiliated_match_helper=*/nullptr);

  // Mismatched duplicates: Identical except for the password.
  PasswordForm password_form;
  password_form.signon_realm = "http://example1.com/";
  password_form.url = GURL("http://example1.com/");
  password_form.username_element = u"userelem_1";
  password_form.username_value = u"username_1";
  password_form.password_element = u"passelem_1";
  password_form.password_value = u"password_1";
  profile_store->AddLogin(password_form);
  // Note: password_value is not part of the unique key, so we need to change
  // some other value to be able to insert the duplicate into the DB.
  password_form.password_element = u"passelem_2";
  password_form.password_value = u"password_2";
  profile_store->AddLogin(password_form);
  // The number of "identical" credentials doesn't matter; we count the *sets*
  // of duplicates.
  password_form.password_element = u"passelem_3";
  password_form.password_value = u"password_3";
  profile_store->AddLogin(password_form);

  base::HistogramTester histogram_tester;
  StoreMetricsReporter reporter(profile_store.get(), /*account_store=*/nullptr,
                                sync_service(), identity_manager(), &prefs_,
                                /*password_reuse_manager=*/nullptr,
                                /*is_under_advanced_protection=*/false,
                                /*done_callback*/ base::DoNothing());

  // Wait for the metrics to get reported, which involves queries to the
  // stores, i.e. to background task runners.
  RunUntilIdle();

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "PasswordManager.CredentialsWithDuplicates"),
              testing::ElementsAre(base::Bucket(0, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "PasswordManager.CredentialsWithMismatchedDuplicates"),
              testing::ElementsAre(base::Bucket(1, 1)));

  profile_store->ShutdownOnUIThread();
  // Make sure the PasswordStore destruction parts on the background sequence
  // finish, otherwise we get memory leak reports.
  RunUntilIdle();
}

// A test that covers multi-store metrics, which are recorded by the
// StoreMetricsReporter directly.
TEST_F(StoreMetricsReporterTest, MultiStoreMetrics) {
  // This test is only relevant when the passwords accounts store is enabled.
  if (!base::FeatureList::IsEnabled(features::kEnablePasswordsAccountStorage))
    return;
  prefs_.registry()->RegisterDictionaryPref(
      prefs::kAccountStoragePerAccountSettings);
  auto profile_store =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(false));
  auto account_store =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(true));

  profile_store->Init(&prefs_, /*affiliated_match_helper=*/nullptr);
  account_store->Init(&prefs_, /*affiliated_match_helper=*/nullptr);

  // Simulate account store active.
  AccountInfo account_info;
  account_info.email = "account@gmail.com";
  account_info.gaia = "account";
  test_sync_service()->SetAccountInfo(account_info);
  test_sync_service()->SetHasSyncConsent(false);

  const std::string kRealm1 = "https://example.com";
  const std::string kRealm2 = "https://example2.com";

  // Add test data to the profile store:
  // - 3 credentials that don't exist in the account store
  // - 1 credential that conflicts with the account store (exists there with the
  //   same username but different password)
  // - 2 credentials with identical copies in the account store
  // Note: In the implementation, the credentials are processed in alphabetical
  // order of usernames. Choose usernames here so that some profile-store-only
  // credentials end up at both the start and the end of the list, to make sure
  // these cases are handled correctly.
  profile_store->AddLogin(
      CreateForm(kRealm1, "aprofileuser1", "aprofilepass1"));
  profile_store->AddLogin(
      CreateForm(kRealm1, "aprofileuser2", "aprofilepass2"));
  profile_store->AddLogin(
      CreateForm(kRealm1, "zprofileuser3", "zprofilepass3"));
  profile_store->AddLogin(CreateForm(kRealm1, "conflictinguser", "localpass"));
  profile_store->AddLogin(
      CreateForm(kRealm1, "identicaluser1", "identicalpass1"));
  profile_store->AddLogin(
      CreateForm(kRealm1, "identicaluser2", "identicalpass2"));

  // Add test data to the account store:
  // - 2 credentials that don't exist in the account store
  // - 1 credential that conflicts with the profile store (exists there with the
  //   same username but different password)
  // - 2 credentials with identical copies in the profile store
  account_store->AddLogin(CreateForm(kRealm1, "accountuser1", "accountpass1"));
  account_store->AddLogin(
      CreateForm(kRealm1, "zaccountuser2", "zaccountpass2"));
  account_store->AddLogin(
      CreateForm(kRealm1, "conflictinguser", "accountpass"));
  account_store->AddLogin(
      CreateForm(kRealm1, "identicaluser1", "identicalpass1"));
  account_store->AddLogin(
      CreateForm(kRealm1, "identicaluser2", "identicalpass2"));

  // Finally, add one more identical credential to the profile store. However
  // this one is on a different signon realm, so should be counted as just
  // another (4th) credential that's missing in the account store.
  profile_store->AddLogin(
      CreateForm(kRealm2, "identicaluser1", "identicalpass1"));

  for (bool opted_in : {false, true}) {
    if (opted_in) {
      features_util::OptInToAccountStorage(&prefs_, sync_service());
    } else {
      features_util::OptOutOfAccountStorageAndClearSettings(&prefs_,
                                                            sync_service());
    }

    base::HistogramTester histogram_tester;

    StoreMetricsReporter reporter(profile_store.get(), account_store.get(),
                                  sync_service(), identity_manager(), &prefs_,
                                  /*password_reuse_manager=*/nullptr,
                                  /*is_under_advanced_protection=*/false,
                                  /*done_callback*/ base::DoNothing());

    // Wait for the metrics to get reported, which involves queries to the
    // stores, i.e. to background task runners.
    RunUntilIdle();

    if (opted_in) {
      histogram_tester.ExpectUniqueSample(
          "PasswordManager.AccountStoreVsProfileStore2.Additional", 2, 1);
      histogram_tester.ExpectUniqueSample(
          "PasswordManager.AccountStoreVsProfileStore2.Missing", 4, 1);
      histogram_tester.ExpectUniqueSample(
          "PasswordManager.AccountStoreVsProfileStore2.Identical", 2, 1);
      histogram_tester.ExpectUniqueSample(
          "PasswordManager.AccountStoreVsProfileStore2.Conflicting", 1, 1);
    } else {
      histogram_tester.ExpectTotalCount(
          "PasswordManager.AccountStoreVsProfileStore2.Additional", 0);
      histogram_tester.ExpectTotalCount(
          "PasswordManager.AccountStoreVsProfileStore2.Missing", 0);
      histogram_tester.ExpectTotalCount(
          "PasswordManager.AccountStoreVsProfileStore2.Identical", 0);
      histogram_tester.ExpectTotalCount(
          "PasswordManager.AccountStoreVsProfileStore2.Conflicting", 0);
    }
  }

  account_store->ShutdownOnUIThread();
  profile_store->ShutdownOnUIThread();
  // Make sure the PasswordStore destruction parts on the background sequence
  // finish, otherwise we get memory leak reports.
  RunUntilIdle();
}

TEST_F(StoreMetricsReporterTest, ReportMetricsForAdvancedProtection) {
  prefs_.registry()->RegisterListPref(prefs::kPasswordHashDataList,
                                      PrefRegistry::NO_REGISTRATION_FLAGS);
  ASSERT_FALSE(prefs_.HasPrefPath(prefs::kSyncPasswordHash));

  auto store = base::MakeRefCounted<MockPasswordStoreInterface>();

  MockPasswordReuseManager reuse_manager;

  const std::string username = "test@google.com";
  SetSyncingPasswords(true);
  FakeSigninAs(username);

  base::HistogramTester histogram_tester;

  EXPECT_CALL(reuse_manager, ReportMetrics(username, true));
  StoreMetricsReporter reporter(/*profile_store=*/store.get(),
                                /*account_store=*/nullptr, sync_service(),
                                identity_manager(), &prefs_, &reuse_manager,
                                /*is_under_advanced_protection=*/true,
                                /*done_callback*/ base::DoNothing());

  // Wait for the metrics to get reported, which involves queries to the stores,
  // i.e. to background task runners.
  RunUntilIdle();
}

INSTANTIATE_TEST_SUITE_P(All, StoreMetricsReporterTestWithParams, Bool());
}  // namespace
}  // namespace password_manager
