// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/common/signatures.h"
#include "components/os_crypt/os_crypt_mocker.h"
#include "components/password_manager/core/browser/android_affiliation/affiliated_match_helper.h"
#include "components/password_manager/core/browser/android_affiliation/android_affiliation_service.h"
#include "components/password_manager/core/browser/android_affiliation/mock_affiliated_match_helper.h"
#include "components/password_manager/core/browser/form_parsing/form_parser.h"
#include "components/password_manager/core/browser/insecure_credentials_consumer.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_reuse_detector.h"
#include "components/password_manager/core/browser/password_reuse_manager.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/browser/password_store_impl.h"
#include "components/password_manager/core/browser/password_store_signin_notifier.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::WaitableEvent;
using testing::_;
using testing::DoAll;
using testing::ElementsAre;
using testing::ElementsAreArray;
using testing::IsEmpty;
using testing::UnorderedElementsAre;
using testing::WithArg;

namespace password_manager {

namespace {

constexpr const char kTestWebRealm1[] = "https://one.example.com/";
constexpr const char kTestWebOrigin1[] = "https://one.example.com/origin";
constexpr const char kTestWebRealm2[] = "https://two.example.com/";
constexpr const char kTestWebOrigin2[] = "https://two.example.com/origin";
constexpr const char kTestWebRealm3[] = "https://three.example.com/";
constexpr const char kTestWebOrigin3[] = "https://three.example.com/origin";
constexpr const char kTestWebRealm5[] = "https://five.example.com/";
constexpr const char kTestWebOrigin5[] = "https://five.example.com/origin";
constexpr const char kTestPSLMatchingWebRealm[] = "https://psl.example.com/";
constexpr const char kTestPSLMatchingWebOrigin[] =
    "https://psl.example.com/origin";
constexpr const char kTestUnrelatedWebRealm[] = "https://notexample.com/";
constexpr const char kTestUnrelatedWebOrigin[] = "https:/notexample.com/origin";
constexpr const char kTestUnrelatedWebRealm2[] = "https://notexample2.com/";
constexpr const char kTestUnrelatedWebOrigin2[] =
    "https:/notexample2.com/origin";
constexpr const char kTestInsecureWebRealm[] = "http://one.example.com/";
constexpr const char kTestInsecureWebOrigin[] = "http://one.example.com/origin";
constexpr const char kTestAndroidRealm1[] =
    "android://hash@com.example.android/";
constexpr const char kTestAndroidRealm2[] =
    "android://hash@com.example.two.android/";
constexpr const char kTestAndroidRealm3[] =
    "android://hash@com.example.three.android/";
constexpr const char kTestUnrelatedAndroidRealm[] =
    "android://hash@com.notexample.android/";
constexpr const char kTestAndroidName1[] = "Example Android App 1";
constexpr const char kTestAndroidIconURL1[] = "https://example.com/icon_1.png";
constexpr const char kTestAndroidName2[] = "Example Android App 2";
constexpr const char kTestAndroidIconURL2[] = "https://example.com/icon_2.png";
constexpr const time_t kTestLastUsageTime = 1546300800;  // 00:00 Jan 1 2019 UTC

class MockInsecureCredentialsConsumer : public InsecureCredentialsConsumer {
 public:
  MockInsecureCredentialsConsumer() = default;

  MOCK_METHOD1(OnGetInsecureCredentials, void(std::vector<InsecureCredential>));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockInsecureCredentialsConsumer);
};

class MockPasswordStoreConsumer : public PasswordStoreConsumer {
 public:
  MockPasswordStoreConsumer() = default;

  MOCK_METHOD1(OnGetPasswordStoreResultsConstRef,
               void(const std::vector<std::unique_ptr<PasswordForm>>&));
  MOCK_METHOD1(OnGetAllFieldInfo, void(const std::vector<FieldInfo>));

  // GMock cannot mock methods with move-only args.
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<PasswordForm>> results) override {
    OnGetPasswordStoreResultsConstRef(results);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPasswordStoreConsumer);
};

class MockPasswordStoreSigninNotifier : public PasswordStoreSigninNotifier {
 public:
  MOCK_METHOD(void,
              SubscribeToSigninEvents,
              (PasswordReuseManager * manager),
              (override));
  MOCK_METHOD(void, UnsubscribeFromSigninEvents, (), (override));
};

class MockMetadataStore : public PasswordStoreSync::MetadataStore {
 public:
  MOCK_METHOD(void, DeleteAllSyncMetadata, (), (override));
  MOCK_METHOD(void,
              SetDeletionsHaveSyncedCallback,
              (base::RepeatingCallback<void(bool)> callback),
              (override));
  MOCK_METHOD(bool, HasUnsyncedDeletions, (), (override));

  std::unique_ptr<syncer::MetadataBatch> GetAllSyncMetadata() override {
    return std::make_unique<syncer::MetadataBatch>();
  }

  bool UpdateSyncMetadata(syncer::ModelType model_type,
                          const std::string& storage_key,
                          const sync_pb::EntityMetadata& metadata) override {
    return true;
  }

  bool ClearSyncMetadata(syncer::ModelType model_type,
                         const std::string& storage_key) override {
    return true;
  }

  bool UpdateModelTypeState(
      syncer::ModelType model_type,
      const sync_pb::ModelTypeState& model_type_state) override {
    return true;
  }

  bool ClearModelTypeState(syncer::ModelType model_type) override {
    return true;
  }
};

class PasswordStoreWithMockedMetadataStore : public PasswordStoreImpl {
 public:
  using PasswordStoreImpl::PasswordStoreImpl;

  PasswordStoreSync::MetadataStore* GetMetadataStore() override {
    return &metadata_store_;
  }

  MockMetadataStore& GetMockedMetadataStore() { return metadata_store_; }

 private:
  ~PasswordStoreWithMockedMetadataStore() override = default;
  MockMetadataStore metadata_store_;
};

PasswordForm MakePasswordForm(const std::string& signon_realm) {
  PasswordForm form;
  form.url = GURL("http://www.origin.com");
  form.username_element = u"username_element";
  form.username_value = u"username_value";
  form.password_element = u"password_element";
  form.signon_realm = signon_realm;
  return form;
}

bool MatchesOrigin(const GURL& origin, const GURL& url) {
  return origin.GetOrigin() == url.GetOrigin();
}

PasswordFormData CreateTestPasswordFormDataByOrigin(const char* origin_url) {
  PasswordFormData data = {PasswordForm::Scheme::kHtml,
                           origin_url,
                           origin_url,
                           "login_element",
                           u"submit_element",
                           u"username_element",
                           u"password_element",
                           u"username_value",
                           u"password_value",
                           true,
                           1};
  return data;
}

}  // namespace

class PasswordStoreTest : public testing::Test {
 protected:
  PasswordStoreTest() = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    // Mock OSCrypt. There is a call to OSCrypt on initializling
    // PasswordReuseDetector, so it should be mocked.
    OSCryptMocker::SetUp();

    feature_list_.InitWithFeatures({features::kPasswordReuseDetectionEnabled},
                                   {});
    pref_service_.registry()->RegisterBooleanPref(
        password_manager::prefs::kWereOldGoogleLoginsRemoved, false);
  }

  void TearDown() override { OSCryptMocker::TearDown(); }

  void WaitForPasswordStore() { task_environment_.RunUntilIdle(); }

  base::FilePath test_login_db_file_path() const {
    return temp_dir_.GetPath().Append(FILE_PATH_LITERAL("login_test"));
  }

  scoped_refptr<PasswordStoreImpl> CreatePasswordStore() {
    return new PasswordStoreImpl(std::make_unique<LoginDatabase>(
        test_login_db_file_path(), password_manager::IsAccountStore(false)));
  }

  scoped_refptr<PasswordStoreWithMockedMetadataStore>
  CreatePasswordStoreWithMockedMetaData() {
    return base::MakeRefCounted<PasswordStoreWithMockedMetadataStore>(
        std::make_unique<LoginDatabase>(
            test_login_db_file_path(),
            password_manager::IsAccountStore(false)));
  }

  TestingPrefServiceSimple* pref_service() { return &pref_service_; }

 private:
  base::ScopedTempDir temp_dir_;
  TestingPrefServiceSimple pref_service_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(PasswordStoreTest);
};

absl::optional<PasswordHashData> GetPasswordFromPref(
    const std::string& username,
    bool is_gaia_password,
    PrefService* prefs) {
  HashPasswordManager hash_password_manager;
  hash_password_manager.set_prefs(prefs);

  return hash_password_manager.RetrievePasswordHash(username, is_gaia_password);
}

TEST_F(PasswordStoreTest, UpdateLoginPrimaryKeyFields) {
  /* clang-format off */
  static const PasswordFormData kTestCredentials[] = {
      // The old credential.
      {PasswordForm::Scheme::kHtml,
       kTestWebRealm1,
       kTestWebOrigin1,
       "", u"", u"username_element_1",  u"password_element_1",
       u"username_value_1",
       u"", kTestLastUsageTime, 1},
      // The new credential with different values for all primary key fields.
      {PasswordForm::Scheme::kHtml,
       kTestWebRealm2,
       kTestWebOrigin2,
       "", u"", u"username_element_2",  u"password_element_2",
       u"username_value_2",
       u"", kTestLastUsageTime, 1}};
  /* clang-format on */

  scoped_refptr<PasswordStoreImpl> store = CreatePasswordStore();
  store->Init(nullptr);

  std::unique_ptr<PasswordForm> old_form(
      FillPasswordFormWithData(kTestCredentials[0]));
  old_form->password_issues = {
      {InsecureType::kLeaked,
       InsecurityMetadata(base::Time(), IsMuted(false))}};
  store->AddLogin(*old_form);
  WaitForPasswordStore();

  MockPasswordStoreObserver mock_observer;
  store->AddObserver(&mock_observer);

  std::unique_ptr<PasswordForm> new_form(
      FillPasswordFormWithData(kTestCredentials[1]));
  new_form->password_issues = old_form->password_issues;
  EXPECT_CALL(mock_observer, OnLoginsChanged(_, testing::SizeIs(2u)));
  PasswordForm old_primary_key;
  old_primary_key.signon_realm = old_form->signon_realm;
  old_primary_key.url = old_form->url;
  old_primary_key.username_element = old_form->username_element;
  old_primary_key.username_value = old_form->username_value;
  old_primary_key.password_element = old_form->password_element;
  store->UpdateLoginWithPrimaryKey(*new_form, old_primary_key);
  WaitForPasswordStore();
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  MockPasswordStoreConsumer mock_consumer;
  std::vector<std::unique_ptr<PasswordForm>> expected_forms;
  expected_forms.push_back(std::move(new_form));
  // The expected form should have no password_issues.
  expected_forms[0]->password_issues =
      base::flat_map<InsecureType, InsecurityMetadata>();
  EXPECT_CALL(mock_consumer,
              OnGetPasswordStoreResultsConstRef(
                  UnorderedPasswordFormElementsAre(&expected_forms)));
  store->GetAutofillableLogins(&mock_consumer);
  WaitForPasswordStore();

  store->RemoveObserver(&mock_observer);
  store->ShutdownOnUIThread();
}

// Verify that RemoveLoginsCreatedBetween() fires the completion callback after
// deletions have been performed and notifications have been sent out. Whether
// the correct logins are removed or not is verified in detail in other tests.
TEST_F(PasswordStoreTest, RemoveLoginsCreatedBetweenCallbackIsCalled) {
  /* clang-format off */
  static const PasswordFormData kTestCredential =
      {PasswordForm::Scheme::kHtml,
       kTestWebRealm1,
       kTestWebOrigin1,
       "", u"", u"username_element_1",  u"password_element_1",
       u"username_value_1",
       u"", kTestLastUsageTime, 1};
  /* clang-format on */

  scoped_refptr<PasswordStoreImpl> store = CreatePasswordStore();
  store->Init(nullptr);

  std::unique_ptr<PasswordForm> test_form(
      FillPasswordFormWithData(kTestCredential));
  store->AddLogin(*test_form);
  WaitForPasswordStore();

  MockPasswordStoreObserver mock_observer;
  store->AddObserver(&mock_observer);

  EXPECT_CALL(mock_observer, OnLoginsChanged(_, testing::SizeIs(1u)));
  base::RunLoop run_loop;
  store->RemoveLoginsCreatedBetween(
      base::Time::FromDoubleT(0), base::Time::FromDoubleT(2),
      base::BindLambdaForTesting([&run_loop](bool) { run_loop.Quit(); }));
  run_loop.Run();
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  store->RemoveObserver(&mock_observer);
  store->ShutdownOnUIThread();
}

// Verify that when a login is removed that the corresponding row is also
// removed from the insecure credentials table.
TEST_F(PasswordStoreTest, InsecureCredentialsObserverOnRemoveLogin) {
  InsecureCredential insecure_credential(kTestWebRealm1, u"username_value_1",
                                         base::Time::FromTimeT(1),
                                         InsecureType::kLeaked, IsMuted(false));

  scoped_refptr<PasswordStoreImpl> store = CreatePasswordStore();
  store->Init(nullptr);

  /* clang-format off */
  static const PasswordFormData kTestCredential =
      {PasswordForm::Scheme::kHtml,
       kTestWebRealm1,
       kTestWebOrigin1,
       "", u"", u"username_element_1",  u"password_element_1",
       u"username_value_1",
       u"", kTestLastUsageTime, 1};
  /* clang-format on */

  std::unique_ptr<PasswordForm> test_form(
      FillPasswordFormWithData(kTestCredential));
  store->AddLogin(*test_form);
  store->AddInsecureCredential(insecure_credential);
  WaitForPasswordStore();

  MockInsecureCredentialsConsumer consumer;
  base::RunLoop run_loop;
  store->RemoveLoginsCreatedBetween(
      base::Time::FromDoubleT(0), base::Time::FromDoubleT(2),
      base::BindLambdaForTesting([&run_loop](bool) { run_loop.Quit(); }));
  run_loop.Run();

  EXPECT_CALL(consumer, OnGetInsecureCredentials(testing::IsEmpty()));
  store->GetAllInsecureCredentials(&consumer);
  WaitForPasswordStore();

  store->ShutdownOnUIThread();
}

// Verify that when a login password is updated that the corresponding row is
// removed from the insecure credentials table.
TEST_F(PasswordStoreTest, InsecureCredentialsObserverOnLoginUpdated) {
  InsecureCredential insecure_credential(kTestWebRealm1, u"username_value_1",
                                         base::Time::FromTimeT(1),
                                         InsecureType::kLeaked, IsMuted(false));
  scoped_refptr<PasswordStoreImpl> store = CreatePasswordStore();
  store->Init(nullptr);

  /* clang-format off */
  PasswordFormData kTestCredential =
      {PasswordForm::Scheme::kHtml,
       kTestWebRealm1,
       kTestWebOrigin1,
       "", u"", u"username_element_1",  u"password_element_1",
       u"username_value_1",
       u"password_value_1", kTestLastUsageTime, 1};
  /* clang-format on */

  std::unique_ptr<PasswordForm> test_form(
      FillPasswordFormWithData(kTestCredential));
  store->AddLogin(*test_form);
  store->AddInsecureCredential(insecure_credential);
  WaitForPasswordStore();

  MockInsecureCredentialsConsumer consumer;
  kTestCredential.password_value = u"password_value_2";
  std::unique_ptr<PasswordForm> test_form_2(
      FillPasswordFormWithData(kTestCredential));
  store->UpdateLogin(*test_form_2);
  WaitForPasswordStore();

  EXPECT_CALL(consumer, OnGetInsecureCredentials(testing::IsEmpty()));
  store->GetAllInsecureCredentials(&consumer);
  WaitForPasswordStore();

  store->ShutdownOnUIThread();
}

// Verify that when a login password is added with the password changed that the
// corresponding row is removed from the insecure credentials table.
TEST_F(PasswordStoreTest, InsecureCredentialsObserverOnLoginAdded) {
  InsecureCredential insecure_credential(kTestWebRealm1, u"username_value_1",
                                         base::Time::FromTimeT(1),
                                         InsecureType::kLeaked, IsMuted(false));
  scoped_refptr<PasswordStoreImpl> store = CreatePasswordStore();
  store->Init(nullptr);

  /* clang-format off */
  PasswordFormData kTestCredential =
      {PasswordForm::Scheme::kHtml,
       kTestWebRealm1,
       kTestWebOrigin1,
       "", u"", u"username_element_1",  u"password_element_1",
       u"username_value_1",
       u"password_value_1", kTestLastUsageTime, 1};
  /* clang-format on */

  std::unique_ptr<PasswordForm> test_form(
      FillPasswordFormWithData(kTestCredential));
  store->AddLogin(*test_form);
  store->AddInsecureCredential(insecure_credential);
  WaitForPasswordStore();

  MockInsecureCredentialsConsumer consumer;
  kTestCredential.password_value = u"password_value_2";
  std::unique_ptr<PasswordForm> test_form_2(
      FillPasswordFormWithData(kTestCredential));
  store->AddLogin(*test_form_2);
  WaitForPasswordStore();

  EXPECT_CALL(consumer, OnGetInsecureCredentials(testing::IsEmpty()));
  store->GetAllInsecureCredentials(&consumer);
  WaitForPasswordStore();

  store->ShutdownOnUIThread();
}

TEST_F(PasswordStoreTest, InsecurePasswordObserverOnInsecureCredentialAdded) {
  constexpr PasswordFormData kTestCredentials = {PasswordForm::Scheme::kHtml,
                                                 kTestWebRealm1,
                                                 kTestWebRealm1,
                                                 "",
                                                 u"",
                                                 u"",
                                                 u"",
                                                 u"username_value_1",
                                                 u"password",
                                                 kTestLastUsageTime,
                                                 1};
  InsecureCredential insecure_credential(kTestWebRealm1, u"username_value_1",
                                         base::Time::FromTimeT(1),
                                         InsecureType::kLeaked, IsMuted(false));

  scoped_refptr<PasswordStoreImpl> store = CreatePasswordStore();
  store->Init(nullptr);
  store->AddLogin(*FillPasswordFormWithData(kTestCredentials));
  WaitForPasswordStore();

  MockPasswordStoreObserver mock_observer;
  store->AddObserver(&mock_observer);

  // Expect a notification after adding a credential.
  EXPECT_CALL(mock_observer, OnLoginsChanged);
  store->AddInsecureCredential(insecure_credential);
  WaitForPasswordStore();

  // Adding the same credential should not result in another notification.
  EXPECT_CALL(mock_observer, OnLoginsChanged).Times(0);
  store->AddInsecureCredential(insecure_credential);
  WaitForPasswordStore();

  store->RemoveObserver(&mock_observer);
  store->ShutdownOnUIThread();
}

TEST_F(PasswordStoreTest, InsecurePasswordObserverOnInsecureCredentialRemoved) {
  constexpr PasswordFormData kTestCredentials = {PasswordForm::Scheme::kHtml,
                                                 kTestWebRealm1,
                                                 kTestWebRealm1,
                                                 "",
                                                 u"",
                                                 u"",
                                                 u"",
                                                 u"username_value_1",
                                                 u"password",
                                                 kTestLastUsageTime,
                                                 1};

  InsecureCredential insecure_credential(kTestWebRealm1, u"username_value_1",
                                         base::Time::FromTimeT(1),
                                         InsecureType::kLeaked, IsMuted(false));

  scoped_refptr<PasswordStoreImpl> store = CreatePasswordStore();
  store->Init(nullptr);
  store->AddLogin(*FillPasswordFormWithData(kTestCredentials));
  store->AddInsecureCredential(insecure_credential);
  WaitForPasswordStore();

  MockPasswordStoreObserver mock_observer;
  store->AddObserver(&mock_observer);

  // Expect a notification after removing a credential.
  EXPECT_CALL(mock_observer, OnLoginsChanged);
  store->RemoveInsecureCredentials(insecure_credential.signon_realm,
                                   insecure_credential.username,
                                   RemoveInsecureCredentialsReason::kRemove);
  WaitForPasswordStore();

  // Removing the same credential should not result in another notification.
  EXPECT_CALL(mock_observer, OnLoginsChanged).Times(0);
  store->RemoveInsecureCredentials(insecure_credential.signon_realm,
                                   insecure_credential.username,
                                   RemoveInsecureCredentialsReason::kRemove);
  WaitForPasswordStore();

  store->RemoveObserver(&mock_observer);
  store->ShutdownOnUIThread();
}

// When no Android applications are actually affiliated with the realm of the
// observed form, GetLoginsWithAffiliations() should still return the exact and
// PSL matching results, but not any stored Android credentials.
TEST_F(PasswordStoreTest, GetLoginsWithoutAffiliations) {
  /* clang-format off */
  static const PasswordFormData kTestCredentials[] = {
      // Credential that is an exact match of the observed form.
      {PasswordForm::Scheme::kHtml,
       kTestWebRealm1,
       kTestWebOrigin1,
       "", u"", u"",  u"",
       u"username_value_1",
       u"", kTestLastUsageTime, 1},
      // Credential that is a PSL match of the observed form.
      {PasswordForm::Scheme::kHtml,
       kTestPSLMatchingWebRealm,
       kTestPSLMatchingWebOrigin,
       "", u"", u"",  u"",
       u"username_value_2",
       u"", kTestLastUsageTime, 1},
      // Credential for an unrelated Android application.
      {PasswordForm::Scheme::kHtml,
       kTestUnrelatedAndroidRealm,
       "", "", u"", u"", u"",
       u"username_value_3",
       u"", kTestLastUsageTime, 1}};
  /* clang-format on */

  scoped_refptr<PasswordStoreImpl> store = CreatePasswordStore();
  store->Init(nullptr);

  std::vector<std::unique_ptr<PasswordForm>> all_credentials;
  for (const auto& credential : kTestCredentials) {
    all_credentials.push_back(FillPasswordFormWithData(credential));
    store->AddLogin(*all_credentials.back());
  }

  PasswordFormDigest observed_form = {PasswordForm::Scheme::kHtml,
                                      kTestWebRealm1, GURL(kTestWebOrigin1)};

  std::vector<std::unique_ptr<PasswordForm>> expected_results;
  expected_results.push_back(
      std::make_unique<PasswordForm>(*all_credentials[0]));
  expected_results.push_back(
      std::make_unique<PasswordForm>(*all_credentials[1]));
  for (const auto& result : expected_results) {
    if (result->signon_realm != observed_form.signon_realm)
      result->is_public_suffix_match = true;
  }

  std::vector<std::string> no_affiliated_android_realms;
  auto mock_helper = std::make_unique<MockAffiliatedMatchHelper>();
  mock_helper->ExpectCallToGetAffiliatedAndroidRealms(
      observed_form, no_affiliated_android_realms);
  store->SetAffiliatedMatchHelper(std::move(mock_helper));

  MockPasswordStoreConsumer mock_consumer;
  EXPECT_CALL(mock_consumer,
              OnGetPasswordStoreResultsConstRef(
                  UnorderedPasswordFormElementsAre(&expected_results)));
  store->GetLogins(observed_form, &mock_consumer);
  WaitForPasswordStore();
  store->ShutdownOnUIThread();
}

// There are 3 Android applications affiliated with the realm of the observed
// form, with the PasswordStore having credentials for two of these (even two
// credentials for one). GetLoginsWithAffiliations() should return the exact,
// and PSL matching credentials, and the credentials for these two Android
// applications, but not for the unaffiliated Android application.
TEST_F(PasswordStoreTest, GetLoginsWithAffiliations) {
  static constexpr const struct {
    PasswordFormData form_data;
    bool use_federated_login;
  } kTestCredentials[] = {
      // Credential that is an exact match of the observed form.
      {
          {PasswordForm::Scheme::kHtml, kTestWebRealm1, kTestWebOrigin1, "",
           u"", u"", u"", u"username_value_1", u"", kTestLastUsageTime, 1},
          false,
      },
      // Credential that is a PSL match of the observed form.
      {
          {PasswordForm::Scheme::kHtml, kTestPSLMatchingWebRealm,
           kTestPSLMatchingWebOrigin, "", u"", u"", u"", u"username_value_2",
           u"", true, 1},
          false,
      },
      // Credential for an Android application affiliated with the realm of the
      // observed from.
      {
          {PasswordForm::Scheme::kHtml, kTestAndroidRealm1, "", "", u"", u"",
           u"", u"username_value_3", u"", kTestLastUsageTime, 1},
          false,
      },
      // Second credential for the same Android application.
      {
          {PasswordForm::Scheme::kHtml, kTestAndroidRealm1, "", "", u"", u"",
           u"", u"username_value_3b", u"", kTestLastUsageTime, 1},
          false,
      },
      // Third credential for the same application which is username-only.
      {
          {PasswordForm::Scheme::kUsernameOnly, kTestAndroidRealm1, "", "", u"",
           u"", u"", u"username_value_3c", u"", kTestLastUsageTime, 1},
          false,
      },
      // Credential for another Android application affiliated with the realm
      // of the observed from.
      {
          {PasswordForm::Scheme::kHtml, kTestAndroidRealm2, "", "", u"", u"",
           u"", u"username_value_4", u"", kTestLastUsageTime, 1},
          false,
      },
      // Federated credential for this second Android application.
      {
          {PasswordForm::Scheme::kHtml, kTestAndroidRealm2, "", "", u"", u"",
           u"", u"username_value_4b", u"", kTestLastUsageTime, 1},
          true,
      },
      // Credential for an unrelated Android application.
      {
          {PasswordForm::Scheme::kHtml, kTestUnrelatedAndroidRealm, "", "", u"",
           u"", u"", u"username_value_5", u"", kTestLastUsageTime, 1},
          false,
      }};

  scoped_refptr<PasswordStoreImpl> store = CreatePasswordStore();
  store->Init(nullptr);

  std::vector<std::unique_ptr<PasswordForm>> all_credentials;
  for (const auto& i : kTestCredentials) {
    all_credentials.push_back(
        FillPasswordFormWithData(i.form_data, i.use_federated_login));
    store->AddLogin(*all_credentials.back());
  }

  PasswordFormDigest observed_form = {PasswordForm::Scheme::kHtml,
                                      kTestWebRealm1, GURL(kTestWebOrigin1)};

  std::vector<std::unique_ptr<PasswordForm>> expected_results;
  expected_results.push_back(
      std::make_unique<PasswordForm>(*all_credentials[0]));
  expected_results.push_back(
      std::make_unique<PasswordForm>(*all_credentials[1]));
  expected_results.push_back(
      std::make_unique<PasswordForm>(*all_credentials[2]));
  expected_results.push_back(
      std::make_unique<PasswordForm>(*all_credentials[3]));
  expected_results.push_back(
      std::make_unique<PasswordForm>(*all_credentials[5]));
  expected_results.push_back(
      std::make_unique<PasswordForm>(*all_credentials[6]));

  for (const auto& result : expected_results) {
    if (result->signon_realm != observed_form.signon_realm &&
        !IsValidAndroidFacetURI(result->signon_realm))
      result->is_public_suffix_match = true;
    if (IsValidAndroidFacetURI(result->signon_realm))
      result->is_affiliation_based_match = true;
  }

  std::vector<std::string> affiliated_android_realms;
  affiliated_android_realms.push_back(kTestAndroidRealm1);
  affiliated_android_realms.push_back(kTestAndroidRealm2);
  affiliated_android_realms.push_back(kTestAndroidRealm3);

  auto mock_helper = std::make_unique<MockAffiliatedMatchHelper>();
  mock_helper->ExpectCallToGetAffiliatedAndroidRealms(
      observed_form, affiliated_android_realms);
  store->SetAffiliatedMatchHelper(std::move(mock_helper));

  MockPasswordStoreConsumer mock_consumer;
  EXPECT_CALL(mock_consumer,
              OnGetPasswordStoreResultsConstRef(
                  UnorderedPasswordFormElementsAre(&expected_results)));

  store->GetLogins(observed_form, &mock_consumer);
  WaitForPasswordStore();
  store->ShutdownOnUIThread();
}

// When the password stored for an Android application is updated, credentials
// with the same username stored for affiliated web sites should also be updated
// automatically.
// TODO(crbug.bom/1226042): Remove this test since we no longer update passwords
// for affiliated websites through sync.
TEST_F(PasswordStoreTest, DISABLED_UpdatePasswordsStoredForAffiliatedWebsites) {
  const char16_t kTestUsername[] = u"username_value_1";
  const char16_t kTestOtherUsername[] = u"username_value_2";
  const char16_t kTestOldPassword[] = u"old_password_value";
  const char16_t kTestNewPassword[] = u"new_password_value";
  const char16_t kTestOtherPassword[] = u"other_password_value";

  /* clang-format off */
  static const PasswordFormData kTestCredentials[] = {
      // The credential for the Android application that will be updated
      // explicitly so it can be tested if the changes are correctly propagated
      // to affiliated Web credentials.
      {PasswordForm::Scheme::kHtml,
       kTestAndroidRealm1,
       "", "", u"", u"", u"",
       kTestUsername,
       kTestOldPassword, kTestLastUsageTime, 2},

      // --- Positive samples --- Credentials that the password update should be
      // automatically propagated to.

      // Credential for an affiliated web site with the same username.
      {PasswordForm::Scheme::kHtml,
       kTestWebRealm1,
       kTestWebOrigin1,
       "", u"", u"",  u"",
       kTestUsername,
       kTestOldPassword, kTestLastUsageTime, 1},
      // Credential for another affiliated web site with the same username.
      // Although the password is different than the current/old password for
      // the Android application, it should be updated regardless.
      {PasswordForm::Scheme::kHtml,
       kTestWebRealm2,
       kTestWebOrigin2,
       "", u"", u"",  u"",
       kTestUsername,
       kTestOtherPassword,kTestLastUsageTime,  1},

      // --- Negative samples --- Credentials that the password update should
      // not be propagated to.

      // Credential for another affiliated web site, but one that already has
      // the new password.
      {PasswordForm::Scheme::kHtml,
       kTestWebRealm3,
       kTestWebOrigin3,
       "", u"", u"",  u"",
       kTestUsername,
       kTestNewPassword,  kTestLastUsageTime, 1},
      // Credential for the HTTP version of an affiliated web site.
      {PasswordForm::Scheme::kHtml,
       kTestInsecureWebRealm,
       kTestInsecureWebOrigin,
       "", u"", u"",  u"",
       kTestUsername,
       kTestOldPassword,  kTestLastUsageTime, 1},
      // Credential for an affiliated web site, but with a different username.
      {PasswordForm::Scheme::kHtml,
       kTestWebRealm1,
       kTestWebOrigin1,
       "", u"", u"",  u"",
       kTestOtherUsername,
       kTestOldPassword, kTestLastUsageTime,  1},
      // Credential for a web site that is a PSL match to a web sites affiliated
      // with the Android application.
      {PasswordForm::Scheme::kHtml,
       kTestPSLMatchingWebRealm,
       kTestPSLMatchingWebOrigin,
       "poisoned", u"poisoned", u"",  u"",
       kTestUsername,
       kTestOldPassword, kTestLastUsageTime,  1},
      // Credential for an unrelated web site.
      {PasswordForm::Scheme::kHtml,
       kTestUnrelatedWebRealm,
       kTestUnrelatedWebOrigin,
       "", u"", u"",  u"",
       kTestUsername,
       kTestOldPassword, kTestLastUsageTime,  1},
      // Credential for an affiliated Android application.
      {PasswordForm::Scheme::kHtml,
       kTestAndroidRealm2,
       "", "", u"", u"", u"",
       kTestUsername,
       kTestOldPassword, kTestLastUsageTime,  1},
      // Credential for an unrelated Android application.
      {PasswordForm::Scheme::kHtml,
       kTestUnrelatedAndroidRealm,
       "", "", u"", u"", u"",
       kTestUsername,
       kTestOldPassword, kTestLastUsageTime,  1},
      // Credential for an affiliated web site with the same username, but one
      // that was updated at the same time via Sync as the Android credential.
      {PasswordForm::Scheme::kHtml,
       kTestWebRealm5,
       kTestWebOrigin5,
       "", u"", u"",  u"",
       kTestUsername,
       kTestOtherPassword, kTestLastUsageTime, 2}};
  /* clang-format on */

  // The number of positive samples in |kTestCredentials|.
  const size_t kExpectedNumberOfPropagatedUpdates = 2u;

  const bool kFalseTrue[] = {false, true};
  for (bool test_remove_and_add_login : kFalseTrue) {
    SCOPED_TRACE(testing::Message("test_remove_and_add_login: ")
                 << test_remove_and_add_login);

    scoped_refptr<PasswordStoreImpl> store = CreatePasswordStore();
    store->Init(nullptr);
    store->RemoveLoginsCreatedBetween(base::Time(), base::Time::Max(),
                                      base::NullCallback());

    // Set up the initial test data set.
    std::vector<std::unique_ptr<PasswordForm>> all_credentials;
    for (const auto& credential : kTestCredentials) {
      all_credentials.push_back(FillPasswordFormWithData(credential));
      all_credentials.back()->date_synced =
          all_credentials.back()->date_created;
      store->AddLogin(*all_credentials.back());
    }
    WaitForPasswordStore();

    // Calculate how the correctly updated test data set should look like.
    std::vector<std::unique_ptr<PasswordForm>>
        expected_credentials_after_update;
    for (size_t i = 0; i < all_credentials.size(); ++i) {
      expected_credentials_after_update.push_back(
          std::make_unique<PasswordForm>(*all_credentials[i]));
      if (i < 1 + kExpectedNumberOfPropagatedUpdates) {
        expected_credentials_after_update.back()->password_value =
            kTestNewPassword;
      }
    }

    std::vector<std::string> affiliated_web_realms;
    affiliated_web_realms.push_back(kTestWebRealm1);
    affiliated_web_realms.push_back(kTestWebRealm2);
    affiliated_web_realms.push_back(kTestWebRealm3);
    affiliated_web_realms.push_back(kTestWebRealm5);

    auto mock_helper = std::make_unique<MockAffiliatedMatchHelper>();
    mock_helper->ExpectCallToGetAffiliatedWebRealms(
        PasswordFormDigest(*expected_credentials_after_update[0]),
        affiliated_web_realms);
    store->SetAffiliatedMatchHelper(std::move(mock_helper));

    // Explicitly update the Android credential, wait until things calm down,
    // then query all passwords and expect that:
    //   1.) The positive samples in |kTestCredentials| have the new password,
    //       but the negative samples do not.
    //   2.) Change notifications are sent about the updates. Note that as the
    //       test interacts with the (Update|Add)LoginSync methods, only the
    //       derived changes should trigger notifications, the first one would
    //       normally be trigger by Sync.
    MockPasswordStoreObserver mock_observer;
    store->AddObserver(&mock_observer);
    EXPECT_CALL(mock_observer,
                OnLoginsChanged(
                    _, testing::SizeIs(kExpectedNumberOfPropagatedUpdates)));
    if (test_remove_and_add_login) {
      store->ScheduleTask(
          base::BindOnce(IgnoreResult(&PasswordStoreImpl::RemoveLoginSync),
                         store, *all_credentials[0]));
      store->ScheduleTask(base::BindOnce(
          IgnoreResult(&PasswordStoreImpl::AddLoginSync), store,
          *expected_credentials_after_update[0], /*error=*/nullptr));
    } else {
      store->ScheduleTask(base::BindOnce(
          IgnoreResult(&PasswordStoreImpl::UpdateLoginSync), store,
          *expected_credentials_after_update[0], /*error=*/nullptr));
    }
    WaitForPasswordStore();
    store->RemoveObserver(&mock_observer);

    MockPasswordStoreConsumer mock_consumer;
    EXPECT_CALL(mock_consumer, OnGetPasswordStoreResultsConstRef(
                                   UnorderedPasswordFormElementsAre(
                                       &expected_credentials_after_update)));
    store->GetAutofillableLogins(&mock_consumer);
    WaitForPasswordStore();
    store->ShutdownOnUIThread();
    store = nullptr;
    // Finish processing so that the database isn't locked next iteration.
    WaitForPasswordStore();
  }
}

TEST_F(PasswordStoreTest, GetAllLogins) {
  static constexpr PasswordFormData kTestCredentials[] = {
      {PasswordForm::Scheme::kHtml, kTestAndroidRealm1, "", "", u"", u"", u"",
       u"username_value_1", u"", kTestLastUsageTime, 1},
      {PasswordForm::Scheme::kHtml, kTestAndroidRealm2, "", "", u"", u"", u"",
       u"username_value_2", u"", kTestLastUsageTime, 1},
      {PasswordForm::Scheme::kHtml, kTestAndroidRealm3, "", "", u"", u"", u"",
       u"username_value_3", u"", kTestLastUsageTime, 1},
      {PasswordForm::Scheme::kHtml, kTestWebRealm1, kTestWebOrigin1, "", u"",
       u"", u"", u"username_value_4", u"", kTestLastUsageTime, 1},
      // A PasswordFormData with nullptr as the username_value will be converted
      // in a blocklisted PasswordForm in FillPasswordFormWithData().
      {PasswordForm::Scheme::kHtml, kTestWebRealm2, kTestWebOrigin2, "", u"",
       u"", u"", nullptr, u"", kTestLastUsageTime, 1},
      {PasswordForm::Scheme::kHtml, kTestWebRealm3, kTestWebOrigin3, "", u"",
       u"", u"", nullptr, u"", kTestLastUsageTime, 1}};

  scoped_refptr<PasswordStoreImpl> store = CreatePasswordStore();
  store->Init(nullptr);

  std::vector<std::unique_ptr<PasswordForm>> all_credentials;
  for (const auto& test_credential : kTestCredentials) {
    all_credentials.push_back(FillPasswordFormWithData(test_credential));
    store->AddLogin(*all_credentials.back());
  }

  MockPasswordStoreConsumer mock_consumer;
  std::vector<std::unique_ptr<PasswordForm>> expected_results;
  for (const auto& credential : all_credentials)
    expected_results.push_back(std::make_unique<PasswordForm>(*credential));

  EXPECT_CALL(mock_consumer,
              OnGetPasswordStoreResultsConstRef(
                  UnorderedPasswordFormElementsAre(&expected_results)));
  store->GetAllLogins(&mock_consumer);
  WaitForPasswordStore();
  store->ShutdownOnUIThread();
}

// Tests if all credentials in the store with a specific password are
// successfully transferred to the consumer.
TEST_F(PasswordStoreTest, GetLogisByPassword) {
  static constexpr char16_t tested_password[] = u"duplicated_password";
  static constexpr char16_t another_tested_password[] = u"some_other_password";
  static constexpr char16_t untested_password[] = u"and_another_password";

  // The first, third and forth credentials use the same password, but the forth
  // is blocklisted.
  static constexpr PasswordFormData kTestCredentials[] = {
      // Has the specified password:
      {PasswordForm::Scheme::kHtml, kTestAndroidRealm1, "", "", u"", u"", u"",
       u"username_value_1", tested_password, kTestLastUsageTime, 1},
      // Has another password:
      {PasswordForm::Scheme::kHtml, kTestAndroidRealm2, "", "", u"", u"", u"",
       u"username_value_2", another_tested_password, kTestLastUsageTime, 1},
      // Has the specified password:
      {PasswordForm::Scheme::kHtml, kTestAndroidRealm3, "", "", u"", u"", u"",
       u"username_value_3", tested_password, kTestLastUsageTime, 1},
      // Has a third password:
      {PasswordForm::Scheme::kHtml, kTestWebRealm1, kTestWebOrigin1, "", u"",
       u"", u"", u"username_value_4", untested_password, kTestLastUsageTime, 1},
      // A PasswordFormData with nullptr as the username_value will be converted
      // in a blocklisted PasswordForm in FillPasswordFormWithData().
      // Has the specified password, but is blocklisted.
      {PasswordForm::Scheme::kHtml, kTestWebRealm3, kTestWebOrigin3, "", u"",
       u"", u"", nullptr, tested_password, kTestLastUsageTime, 1}};

  scoped_refptr<PasswordStoreImpl> store = CreatePasswordStore();
  store->Init(nullptr);

  std::vector<std::unique_ptr<PasswordForm>> all_credentials;
  for (const auto& test_credential : kTestCredentials) {
    all_credentials.push_back(FillPasswordFormWithData(test_credential));
    store->AddLogin(*all_credentials.back());
  }

  MockPasswordStoreConsumer mock_consumer;
  std::vector<std::unique_ptr<PasswordForm>> expected_results;
  expected_results.push_back(
      std::make_unique<PasswordForm>(*all_credentials[0]));
  expected_results.push_back(
      std::make_unique<PasswordForm>(*all_credentials[2]));

  EXPECT_CALL(mock_consumer,
              OnGetPasswordStoreResultsConstRef(
                  UnorderedPasswordFormElementsAre(&expected_results)));
  store->GetLoginsByPassword(tested_password, &mock_consumer);
  WaitForPasswordStore();

  // Tries to find all credentials with |another_tested_password|.
  expected_results.clear();
  expected_results.push_back(
      std::make_unique<PasswordForm>(*all_credentials[1]));

  EXPECT_CALL(mock_consumer,
              OnGetPasswordStoreResultsConstRef(
                  UnorderedPasswordFormElementsAre(&expected_results)));
  store->GetLoginsByPassword(another_tested_password, &mock_consumer);
  WaitForPasswordStore();

  store->ShutdownOnUIThread();
}

TEST_F(PasswordStoreTest, GetAllLoginsWithAffiliationAndBrandingInformation) {
  static constexpr PasswordFormData kTestCredentials[] = {
      {PasswordForm::Scheme::kHtml, kTestAndroidRealm1, "", "", u"", u"", u"",
       u"username_value_1", u"", kTestLastUsageTime, 1},
      {PasswordForm::Scheme::kHtml, kTestAndroidRealm2, "", "", u"", u"", u"",
       u"username_value_2", u"", kTestLastUsageTime, 1},
      {PasswordForm::Scheme::kHtml, kTestAndroidRealm3, "", "", u"", u"", u"",
       u"username_value_3", u"", kTestLastUsageTime, 1},
      {PasswordForm::Scheme::kHtml, kTestWebRealm1, kTestWebOrigin1, "", u"",
       u"", u"", u"username_value_4", u"", kTestLastUsageTime, 1},
      // A PasswordFormData with nullptr as the username_value will be converted
      // in a blocklisted PasswordForm in FillPasswordFormWithData().
      {PasswordForm::Scheme::kHtml, kTestWebRealm2, kTestWebOrigin2, "", u"",
       u"", u"", nullptr, u"", kTestLastUsageTime, 1},
      {PasswordForm::Scheme::kHtml, kTestWebRealm3, kTestWebOrigin3, "", u"",
       u"", u"", nullptr, u"", kTestLastUsageTime, 1}};

  scoped_refptr<PasswordStoreImpl> store = CreatePasswordStore();
  store->Init(nullptr);

  std::vector<std::unique_ptr<PasswordForm>> all_credentials;
  for (const auto& test_credential : kTestCredentials) {
    all_credentials.push_back(FillPasswordFormWithData(test_credential));
    store->AddLogin(*all_credentials.back());
  }

  MockPasswordStoreConsumer mock_consumer;
  std::vector<std::unique_ptr<PasswordForm>> expected_results;
  for (const auto& credential : all_credentials)
    expected_results.push_back(std::make_unique<PasswordForm>(*credential));

  std::vector<MockAffiliatedMatchHelper::AffiliationAndBrandingInformation>
      affiliation_info_for_results = {
          {kTestWebRealm1, kTestAndroidName1, GURL(kTestAndroidIconURL1)},
          {kTestWebRealm2, kTestAndroidName2, GURL(kTestAndroidIconURL2)},
          {/* Pretend affiliation or branding info is unavailable. */},
          {/* Pretend affiliation or branding info is unavailable. */},
          {/* Pretend affiliation or branding info is unavailable. */},
          {/* Pretend affiliation or branding info is unavailable. */}};

  auto mock_helper = std::make_unique<MockAffiliatedMatchHelper>();
  mock_helper->ExpectCallToInjectAffiliationAndBrandingInformation(
      affiliation_info_for_results);
  store->SetAffiliatedMatchHelper(std::move(mock_helper));
  for (size_t i = 0; i < expected_results.size(); ++i) {
    expected_results[i]->affiliated_web_realm =
        affiliation_info_for_results[i].affiliated_web_realm;
    expected_results[i]->app_display_name =
        affiliation_info_for_results[i].app_display_name;
    expected_results[i]->app_icon_url =
        affiliation_info_for_results[i].app_icon_url;
  }

  EXPECT_CALL(mock_consumer,
              OnGetPasswordStoreResultsConstRef(
                  UnorderedPasswordFormElementsAre(&expected_results)));
  store->GetAllLoginsWithAffiliationAndBrandingInformation(&mock_consumer);

  // Since GetAutofillableLoginsWithAffiliationAndBrandingInformation
  // schedules a request for affiliation information to UI thread, don't
  // shutdown UI thread until there are no tasks in the UI queue.
  WaitForPasswordStore();
  store->ShutdownOnUIThread();
}

TEST_F(PasswordStoreTest, Unblocklisting) {
  static const PasswordFormData kTestCredentials[] = {
      // A PasswordFormData with nullptr as the username_value will be converted
      // in a blocklisted PasswordForm in FillPasswordFormWithData().

      // Blocklisted entry for the observed domain.
      {PasswordForm::Scheme::kHtml, kTestWebRealm1, kTestWebOrigin1, "", u"",
       u"", u"", nullptr, u"", kTestLastUsageTime, 1},
      // Blocklisted entry for a PSL match of the observed form.
      {PasswordForm::Scheme::kHtml, kTestPSLMatchingWebRealm,
       kTestPSLMatchingWebOrigin, "", u"", u"", u"", nullptr, u"",
       kTestLastUsageTime, 1},
      // Blocklisted entry for another domain
      {PasswordForm::Scheme::kHtml, kTestUnrelatedWebRealm,
       kTestUnrelatedWebOrigin, "", u"", u"", u"", nullptr, u"",
       kTestLastUsageTime, 1},
      // Non-blocklisted for the observed domain with a username.
      {PasswordForm::Scheme::kHtml, kTestWebRealm1, kTestWebOrigin1, "", u"",
       u"", u"", u"username", u"", kTestLastUsageTime, 1},
      // Non-blocklisted for the observed domain without a username.
      {PasswordForm::Scheme::kHtml, kTestWebRealm1, kTestWebOrigin1, "", u"",
       u"", u"username_element", u"", u"", kTestLastUsageTime, 1},
      // Non-blocklisted entry for a PSL match of the observed form.
      {PasswordForm::Scheme::kHtml, kTestPSLMatchingWebRealm,
       kTestPSLMatchingWebOrigin, "", u"", u"", u"", u"username", u"",
       kTestLastUsageTime, 1},
      // Non-blocklisted entry for another domain
      {PasswordForm::Scheme::kHtml, kTestUnrelatedWebRealm2,
       kTestUnrelatedWebOrigin2, "", u"", u"", u"", u"username", u"",
       kTestLastUsageTime, 1}};

  scoped_refptr<PasswordStoreImpl> store = CreatePasswordStore();
  store->Init(nullptr);

  std::vector<std::unique_ptr<PasswordForm>> all_credentials;
  for (const auto& test_credential : kTestCredentials) {
    all_credentials.push_back(FillPasswordFormWithData(test_credential));
    store->AddLogin(*all_credentials.back());
  }
  WaitForPasswordStore();

  MockPasswordStoreObserver mock_observer;
  store->AddObserver(&mock_observer);

  // Only the related non-PSL match should be deleted.
  EXPECT_CALL(mock_observer, OnLoginsChanged(_, testing::SizeIs(1u)));
  base::RunLoop run_loop;
  PasswordFormDigest observed_form_digest = {
      PasswordForm::Scheme::kHtml, kTestWebRealm1, GURL(kTestWebOrigin1)};
  store->Unblocklist(observed_form_digest, run_loop.QuitClosure());
  run_loop.Run();
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  // Unblocklisting will delete only the first credential. It should leave the
  // PSL match as well as the unrelated blocklisting entry and all
  // non-blocklisting entries.
  all_credentials.erase(all_credentials.begin());

  MockPasswordStoreConsumer mock_consumer;
  EXPECT_CALL(mock_consumer,
              OnGetPasswordStoreResultsConstRef(
                  UnorderedPasswordFormElementsAre(&all_credentials)));
  store->GetAllLogins(&mock_consumer);
  WaitForPasswordStore();

  store->RemoveObserver(&mock_observer);
  store->ShutdownOnUIThread();
}

TEST_F(PasswordStoreTest, GetAllInsecureCredentials) {
  constexpr PasswordFormData kTestCredentials[] = {
      {PasswordForm::Scheme::kHtml, "https://example.com/",
       "https://example.com/", "", u"", u"", u"", u"username", u"password",
       kTestLastUsageTime, 1},
      {PasswordForm::Scheme::kHtml, "https://2.example.com/",
       "https://2.example.com/", "", u"", u"", u"", u"username2", u"topsecret",
       kTestLastUsageTime, 1}};
  scoped_refptr<PasswordStoreImpl> store = CreatePasswordStore();
  store->Init(nullptr);

  for (const auto& data : kTestCredentials)
    store->AddLogin(*FillPasswordFormWithData(data));
  InsecureCredential insecure_credential("https://example.com/", u"username",
                                         base::Time::FromTimeT(1),
                                         InsecureType::kLeaked, IsMuted(false));
  InsecureCredential insecure_credential2(
      "https://2.example.com/", u"username2", base::Time::FromTimeT(2),
      InsecureType::kLeaked, IsMuted(false));

  store->AddInsecureCredential(insecure_credential);
  store->AddInsecureCredential(insecure_credential2);
  MockInsecureCredentialsConsumer consumer;
  EXPECT_CALL(consumer, OnGetInsecureCredentials(UnorderedElementsAre(
                            insecure_credential, insecure_credential2)));
  store->GetAllInsecureCredentials(&consumer);
  WaitForPasswordStore();
  testing::Mock::VerifyAndClearExpectations(&consumer);

  store->RemoveInsecureCredentials(insecure_credential.signon_realm,
                                   insecure_credential.username,
                                   RemoveInsecureCredentialsReason::kRemove);
  EXPECT_CALL(consumer, OnGetInsecureCredentials(
                            UnorderedElementsAre(insecure_credential2)));
  store->GetAllInsecureCredentials(&consumer);
  WaitForPasswordStore();

  store->ShutdownOnUIThread();
}

// Test GetMatchingInsecureCredentials when affiliation service isn't
// available.
TEST_F(PasswordStoreTest, GetMatchingInsecureWithoutAffiliations) {
  scoped_refptr<PasswordStoreImpl> store = CreatePasswordStore();
  store->Init(nullptr);

  constexpr PasswordFormData kTestCredentials[] = {
      {PasswordForm::Scheme::kHtml, kTestWebRealm1, kTestWebRealm1, "", u"",
       u"", u"", u"username_value", u"password", kTestLastUsageTime, 1},
      {PasswordForm::Scheme::kHtml, kTestWebRealm2, kTestWebRealm2, "", u"",
       u"", u"", u"username_value", u"topsecret", kTestLastUsageTime, 1}};
  for (const auto& data : kTestCredentials)
    store->AddLogin(*FillPasswordFormWithData(data));

  InsecureCredential credential1(kTestWebRealm1, u"username_value",
                                 base::Time::FromTimeT(1),
                                 InsecureType::kLeaked, IsMuted(false));
  InsecureCredential credential2(kTestWebRealm2, u"username_value",
                                 base::Time::FromTimeT(2),
                                 InsecureType::kLeaked, IsMuted(false));
  for (const auto& credential : {credential1, credential2})
    store->AddInsecureCredential(credential);

  MockInsecureCredentialsConsumer consumer;
  EXPECT_CALL(consumer,
              OnGetInsecureCredentials(UnorderedElementsAre(credential1)));
  store->GetMatchingInsecureCredentials(kTestWebRealm1, &consumer);
  WaitForPasswordStore();

  store->ShutdownOnUIThread();
}

// Test GetMatchingInsecureCredentials with some matching Android
// credentials.
TEST_F(PasswordStoreTest, GetMatchingInsecureWithAffiliations) {
  scoped_refptr<PasswordStoreImpl> store = CreatePasswordStore();
  store->Init(nullptr);

  constexpr PasswordFormData kTestCredentials[] = {
      {PasswordForm::Scheme::kHtml, kTestWebRealm1, kTestWebRealm1, "", u"",
       u"", u"", u"username_value", u"password", kTestLastUsageTime, 1},
      {PasswordForm::Scheme::kHtml, kTestAndroidRealm1, kTestAndroidRealm1, "",
       u"", u"", u"", u"username_value_1", u"topsecret", kTestLastUsageTime, 1},
      {PasswordForm::Scheme::kHtml, kTestWebRealm2, kTestWebRealm2, "", u"",
       u"", u"", u"username_value_2", u"topsecret2", kTestLastUsageTime, 1}};
  for (const auto& data : kTestCredentials)
    store->AddLogin(*FillPasswordFormWithData(data));

  InsecureCredential credential1(kTestWebRealm1, u"username_value",
                                 base::Time::FromTimeT(1),
                                 InsecureType::kLeaked, IsMuted(false));
  InsecureCredential credential2(kTestAndroidRealm1, u"username_value_1",
                                 base::Time::FromTimeT(2),
                                 InsecureType::kPhished, IsMuted(false));
  InsecureCredential credential3(kTestWebRealm2, u"username_value_2",
                                 base::Time::FromTimeT(3),
                                 InsecureType::kLeaked, IsMuted(false));
  for (const auto& credentials : {credential1, credential2, credential3})
    store->AddInsecureCredential(credentials);

  PasswordFormDigest observed_form = {PasswordForm::Scheme::kHtml,
                                      kTestWebRealm1, GURL(kTestWebRealm1)};
  std::vector<std::string> affiliated_android_realms = {kTestAndroidRealm1};
  auto mock_helper = std::make_unique<MockAffiliatedMatchHelper>();
  mock_helper->ExpectCallToGetAffiliatedAndroidRealms(
      observed_form, affiliated_android_realms);
  store->SetAffiliatedMatchHelper(std::move(mock_helper));

  MockInsecureCredentialsConsumer consumer;
  EXPECT_CALL(consumer, OnGetInsecureCredentials(
                            UnorderedElementsAre(credential1, credential2)));
  store->GetMatchingInsecureCredentials(kTestWebRealm1, &consumer);
  WaitForPasswordStore();

  store->ShutdownOnUIThread();
}

// Test that updating a password in the store deletes the corresponding
// insecure credential synchronously.
TEST_F(PasswordStoreTest, RemoveInsecureCredentialsSyncOnUpdate) {
  scoped_refptr<PasswordStoreImpl> store = CreatePasswordStore();
  store->Init(nullptr);

  InsecureCredential credential(kTestWebRealm1, u"username1",
                                base::Time::FromTimeT(100),
                                InsecureType::kLeaked, IsMuted(false));
  constexpr PasswordFormData kTestCredential = {PasswordForm::Scheme::kHtml,
                                                kTestWebRealm1,
                                                kTestWebOrigin1,
                                                "",
                                                u"",
                                                u"username_element_1",
                                                u"password_element_1",
                                                u"username1",
                                                u"12345",
                                                10,
                                                5};
  std::unique_ptr<PasswordForm> form(FillPasswordFormWithData(kTestCredential));
  store->AddLogin(*form);
  store->AddInsecureCredential(credential);
  WaitForPasswordStore();

  // Update the password value and immediately get the insecure passwords.
  form->password_value = u"new_password";
  store->UpdateLogin(*form);
  MockInsecureCredentialsConsumer consumer;
  store->GetAllInsecureCredentials(&consumer);
  EXPECT_CALL(consumer, OnGetInsecureCredentials(IsEmpty()));
  WaitForPasswordStore();

  store->ShutdownOnUIThread();
}

// Test that deleting a password in the store deletes the corresponding
// insecure credential synchronously.
TEST_F(PasswordStoreTest, RemoveInsecureCredentialsSyncOnDelete) {
  scoped_refptr<PasswordStoreImpl> store = CreatePasswordStore();
  store->Init(nullptr);

  InsecureCredential credential(kTestWebRealm1, u"username1",
                                base::Time::FromTimeT(100),
                                InsecureType::kLeaked, IsMuted(false));
  constexpr PasswordFormData kTestCredential = {PasswordForm::Scheme::kHtml,
                                                kTestWebRealm1,
                                                kTestWebOrigin1,
                                                "",
                                                u"",
                                                u"username_element_1",
                                                u"password_element_1",
                                                u"username1",
                                                u"12345",
                                                10,
                                                5};
  std::unique_ptr<PasswordForm> form(FillPasswordFormWithData(kTestCredential));
  store->AddLogin(*form);
  store->AddInsecureCredential(credential);
  WaitForPasswordStore();

  // Delete the password and immediately get the insecure passwords.
  store->RemoveLogin(*form);
  MockInsecureCredentialsConsumer consumer;
  store->GetAllInsecureCredentials(&consumer);
  EXPECT_CALL(consumer, OnGetInsecureCredentials(IsEmpty()));
  WaitForPasswordStore();

  store->ShutdownOnUIThread();
}

#if !defined(OS_ANDROID)
// TODO(https://crbug.com/1051914): Enable on Android after making local
// heuristics reliable.
TEST_F(PasswordStoreTest, GetAllFieldInfo) {
  FieldInfo field_info1{autofill::FormSignature(1001),
                        autofill::FieldSignature(1), autofill::USERNAME,
                        base::Time::FromTimeT(1)};
  FieldInfo field_info2{autofill::FormSignature(1002),
                        autofill::FieldSignature(10), autofill::PASSWORD,
                        base::Time::FromTimeT(2)};
  scoped_refptr<PasswordStoreImpl> store = CreatePasswordStore();
  store->Init(nullptr);

  store->AddFieldInfo(field_info1);
  store->AddFieldInfo(field_info2);
  MockPasswordStoreConsumer consumer;
  EXPECT_CALL(consumer, OnGetAllFieldInfo(
                            UnorderedElementsAre(field_info1, field_info2)));
  store->GetAllFieldInfo(&consumer);
  WaitForPasswordStore();

  store->ShutdownOnUIThread();
}

TEST_F(PasswordStoreTest, RemoveFieldInfo) {
  FieldInfo field_info1{autofill::FormSignature(1001),
                        autofill::FieldSignature(1), autofill::USERNAME,
                        base::Time::FromTimeT(100)};
  FieldInfo field_info2{autofill::FormSignature(1002),
                        autofill::FieldSignature(10), autofill::PASSWORD,
                        base::Time::FromTimeT(200)};

  FieldInfo field_info3{autofill::FormSignature(1003),
                        autofill::FieldSignature(11), autofill::PASSWORD,
                        base::Time::FromTimeT(300)};

  scoped_refptr<PasswordStoreImpl> store = CreatePasswordStore();
  store->Init(nullptr);

  store->AddFieldInfo(field_info1);
  store->AddFieldInfo(field_info2);
  store->AddFieldInfo(field_info3);

  MockPasswordStoreConsumer consumer;
  EXPECT_CALL(consumer, OnGetAllFieldInfo(UnorderedElementsAre(
                            field_info1, field_info2, field_info3)));
  store->GetAllFieldInfo(&consumer);
  WaitForPasswordStore();
  testing::Mock::VerifyAndClearExpectations(&consumer);

  store->RemoveFieldInfoByTime(base::Time::FromTimeT(150),
                               base::Time::FromTimeT(250),
                               base::NullCallback());

  EXPECT_CALL(consumer, OnGetAllFieldInfo(
                            UnorderedElementsAre(field_info1, field_info3)));
  store->GetAllFieldInfo(&consumer);
  WaitForPasswordStore();

  store->ShutdownOnUIThread();
}
#endif  // !defined(OS_ANDROID)

TEST_F(PasswordStoreTest, AddInsecureCredentialsSync) {
  scoped_refptr<PasswordStoreImpl> store = CreatePasswordStore();
  store->Init(/*prefs=*/nullptr);

  constexpr PasswordFormData kTestCredential = {
      PasswordForm::Scheme::kHtml,
      kTestWebRealm1,
      kTestWebOrigin1,
      "",
      u"",
      u"username_element_1",
      u"password_element_1",
      u"username",
      u"",
      kTestLastUsageTime,
      1,
  };

  std::unique_ptr<PasswordForm> test_form =
      FillPasswordFormWithData(kTestCredential);

  const std::vector<InsecureCredential> credentials = {
      InsecureCredential(test_form->signon_realm, test_form->username_value,
                         base::Time(), InsecureType::kLeaked, IsMuted(false)),
      InsecureCredential(test_form->signon_realm, test_form->username_value,
                         base::Time(), InsecureType::kReused, IsMuted(false))};

  AddLoginError add_login_error = AddLoginError::kDbError;
  store->ScheduleTask(
      base::BindOnce(IgnoreResult(&PasswordStoreImpl::AddLoginSync), store,
                     *test_form, &add_login_error));
  store->ScheduleTask(base::BindOnce(
      IgnoreResult(&PasswordStoreImpl::AddInsecureCredentialsSync), store,
      credentials));

  WaitForPasswordStore();
  EXPECT_EQ(add_login_error, AddLoginError::kNone);

  MockInsecureCredentialsConsumer consumer;
  EXPECT_CALL(consumer, OnGetInsecureCredentials(UnorderedElementsAre(
                            credentials[0], credentials[1])));
  store->GetAllInsecureCredentials(&consumer);
  WaitForPasswordStore();

  store->ShutdownOnUIThread();
}

TEST_F(PasswordStoreTest, UpdateInsecureCredentialsSync) {
  scoped_refptr<PasswordStoreImpl> store = CreatePasswordStore();
  store->Init(/*prefs=*/nullptr);

  constexpr PasswordFormData kTestCredential = {
      PasswordForm::Scheme::kHtml,
      kTestWebRealm1,
      kTestWebOrigin1,
      "",
      u"",
      u"username_element_1",
      u"password_element_1",
      u"username",
      u"",
      kTestLastUsageTime,
      1,
  };

  std::unique_ptr<PasswordForm> test_form =
      FillPasswordFormWithData(kTestCredential);
  store->AddLogin(*test_form);
  WaitForPasswordStore();

  // Add one insecure credentials that is of typed Leaked and is NOT muted.
  InsecureCredential credential(test_form->signon_realm,
                                test_form->username_value, base::Time(),
                                InsecureType::kLeaked, IsMuted(false));
  store->AddInsecureCredential(credential);
  WaitForPasswordStore();

  std::vector<InsecureCredential> new_credentials;
  new_credentials.push_back(credential);
  // Make that "Leaked" credentials muted
  new_credentials[0].is_muted = IsMuted(true);
  // Add another insecure credentials of type "Reused"
  new_credentials.emplace_back(
      InsecureCredential(test_form->signon_realm, test_form->username_value,
                         base::Time(), InsecureType::kReused, IsMuted(false)));

  // Update the password store with the new insecure credentials.
  store->ScheduleTask(base::BindOnce(
      IgnoreResult(&PasswordStoreImpl::UpdateInsecureCredentialsSync), store,
      *test_form, new_credentials));
  WaitForPasswordStore();

  MockInsecureCredentialsConsumer consumer;
  // Verify the password store has been updated.
  EXPECT_CALL(consumer, OnGetInsecureCredentials(UnorderedElementsAre(
                            new_credentials[0], new_credentials[1])));
  store->GetAllInsecureCredentials(&consumer);
  WaitForPasswordStore();

  store->ShutdownOnUIThread();
}

TEST_F(PasswordStoreTest, TestGetLoginRequestCancelable) {
  scoped_refptr<PasswordStoreWithMockedMetadataStore> store =
      CreatePasswordStoreWithMockedMetaData();
  store->Init(nullptr);
  WaitForPasswordStore();

  store->AddLogin(MakePasswordForm(kTestAndroidRealm1));
  WaitForPasswordStore();

  PasswordFormDigest observed_form = {PasswordForm::Scheme::kHtml,
                                      kTestWebRealm1, GURL(kTestWebRealm1)};

  // Add affiliated android form corresponding to a 'observed_form'.
  auto mock_helper = std::make_unique<MockAffiliatedMatchHelper>();
  mock_helper->ExpectCallToGetAffiliatedAndroidRealms(observed_form,
                                                      {kTestAndroidRealm1});
  store->SetAffiliatedMatchHelper(std::move(mock_helper));

  MockPasswordStoreConsumer mock_consumer;
  EXPECT_CALL(mock_consumer, OnGetPasswordStoreResultsConstRef).Times(0);
  store->GetLogins(observed_form, &mock_consumer);
  mock_consumer.CancelAllRequests();
  WaitForPasswordStore();

  store->ShutdownOnUIThread();
}

TEST_F(PasswordStoreTest, TestUnblockListEmptyStore) {
  scoped_refptr<PasswordStoreWithMockedMetadataStore> store =
      CreatePasswordStoreWithMockedMetaData();
  store->Init(nullptr);
  WaitForPasswordStore();

  MockPasswordStoreObserver observer;
  store->AddObserver(&observer);

  PasswordFormDigest digest = {PasswordForm::Scheme::kHtml, kTestWebRealm1,
                               GURL(kTestWebOrigin1)};

  base::RunLoop run_loop;
  EXPECT_CALL(observer, OnLoginsChanged).Times(0);
  store->Unblocklist(digest, run_loop.QuitClosure());
  run_loop.Run();

  store->RemoveObserver(&observer);
  store->ShutdownOnUIThread();
}

// Collection of origin-related testcases common to all platform-specific
// stores.
class PasswordStoreOriginTest : public PasswordStoreTest {
 public:
  void SetUp() override {
    PasswordStoreTest::SetUp();
    store_ = CreatePasswordStore();
    store_->Init(nullptr);
  }

  void TearDown() override {
    PasswordStoreTest::TearDown();
    store_->ShutdownOnUIThread();
  }

  PasswordStore* store() { return store_.get(); }

 private:
  scoped_refptr<PasswordStoreImpl> store_;
};

TEST_F(PasswordStoreOriginTest,
       RemoveLoginsByURLAndTimeImpl_AllFittingOriginAndTime) {
  constexpr char origin_url[] = "http://foo.example.com/";
  std::unique_ptr<PasswordForm> form =
      FillPasswordFormWithData(CreateTestPasswordFormDataByOrigin(origin_url));
  store()->AddLogin(*form);
  WaitForPasswordStore();

  MockPasswordStoreObserver observer;
  store()->AddObserver(&observer);

  const GURL origin = GURL(origin_url);
  base::RepeatingCallback<bool(const GURL&)> filter =
      base::BindRepeating(&MatchesOrigin, origin);
  base::RunLoop run_loop;
  EXPECT_CALL(observer,
              OnLoginsChanged(_, ElementsAre(PasswordStoreChange(
                                     PasswordStoreChange::REMOVE, *form))));
  store()->RemoveLoginsByURLAndTime(filter, base::Time(), base::Time::Max(),
                                    run_loop.QuitClosure());
  run_loop.Run();

  store()->RemoveObserver(&observer);
}

TEST_F(PasswordStoreOriginTest,
       RemoveLoginsByURLAndTimeImpl_SomeFittingOriginAndTime) {
  constexpr char fitting_url[] = "http://foo.example.com/";
  std::unique_ptr<PasswordForm> form =
      FillPasswordFormWithData(CreateTestPasswordFormDataByOrigin(fitting_url));
  store()->AddLogin(*form);

  const char nonfitting_url[] = "http://bar.example.com/";
  store()->AddLogin(*FillPasswordFormWithData(
      CreateTestPasswordFormDataByOrigin(nonfitting_url)));

  WaitForPasswordStore();

  MockPasswordStoreObserver observer;
  store()->AddObserver(&observer);

  const GURL fitting_origin = GURL(fitting_url);
  base::RepeatingCallback<bool(const GURL&)> filter =
      base::BindRepeating(&MatchesOrigin, fitting_origin);
  base::RunLoop run_loop;
  EXPECT_CALL(observer,
              OnLoginsChanged(_, ElementsAre(PasswordStoreChange(
                                     PasswordStoreChange::REMOVE, *form))));
  store()->RemoveLoginsByURLAndTime(filter, base::Time(), base::Time::Max(),
                                    run_loop.QuitClosure());
  run_loop.Run();

  store()->RemoveObserver(&observer);
}

TEST_F(PasswordStoreOriginTest,
       RemoveLoginsByURLAndTimeImpl_NonMatchingOrigin) {
  constexpr char origin_url[] = "http://foo.example.com/";
  std::unique_ptr<PasswordForm> form =
      FillPasswordFormWithData(CreateTestPasswordFormDataByOrigin(origin_url));
  store()->AddLogin(*form);
  WaitForPasswordStore();

  MockPasswordStoreObserver observer;
  store()->AddObserver(&observer);

  const GURL other_origin = GURL("http://bar.example.com/");
  base::RepeatingCallback<bool(const GURL&)> filter =
      base::BindRepeating(&MatchesOrigin, other_origin);
  base::RunLoop run_loop;
  EXPECT_CALL(observer, OnLoginsChanged).Times(0);
  store()->RemoveLoginsByURLAndTime(filter, base::Time(), base::Time::Max(),
                                    run_loop.QuitClosure());
  run_loop.Run();

  store()->RemoveObserver(&observer);
}

TEST_F(PasswordStoreOriginTest,
       RemoveLoginsByURLAndTimeImpl_NotWithinTimeInterval) {
  constexpr char origin_url[] = "http://foo.example.com/";
  std::unique_ptr<PasswordForm> form =
      FillPasswordFormWithData(CreateTestPasswordFormDataByOrigin(origin_url));
  store()->AddLogin(*form);
  WaitForPasswordStore();

  MockPasswordStoreObserver observer;
  store()->AddObserver(&observer);

  const GURL origin = GURL(origin_url);
  base::RepeatingCallback<bool(const GURL&)> filter =
      base::BindRepeating(&MatchesOrigin, origin);
  base::Time time_after_creation_date =
      form->date_created + base::TimeDelta::FromDays(1);
  base::RunLoop run_loop;
  EXPECT_CALL(observer, OnLoginsChanged).Times(0);
  store()->RemoveLoginsByURLAndTime(filter, time_after_creation_date,
                                    base::Time::Max(), run_loop.QuitClosure());
  run_loop.Run();

  store()->RemoveObserver(&observer);
}

}  // namespace password_manager
