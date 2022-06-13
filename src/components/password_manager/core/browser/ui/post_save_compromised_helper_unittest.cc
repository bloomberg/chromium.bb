// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/post_save_compromised_helper.h"

#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/mock_password_store_interface.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

using BubbleType = PostSaveCompromisedHelper::BubbleType;
using prefs::kLastTimePasswordCheckCompleted;
using testing::_;
using testing::Return;

constexpr char kSignonRealm[] = "https://example.com/";
constexpr char16_t kUsername[] = u"user";
constexpr char16_t kUsername2[] = u"user2";
constexpr char16_t kUsername3[] = u"user3";

InsecureCredential CreateInsecureCredential(
    base::StringPiece16 username,
    PasswordForm::Store store = PasswordForm::Store::kProfileStore,
    IsMuted muted = IsMuted(false)) {
  InsecureCredential compromised(kSignonRealm, std::u16string(username),
                                 base::Time(), InsecureType::kLeaked, muted);
  compromised.in_store = store;
  return compromised;
}

// Creates a form.
PasswordForm CreateForm(
    base::StringPiece signon_realm,
    base::StringPiece16 username,
    PasswordForm::Store store = PasswordForm::Store::kProfileStore) {
  PasswordForm form;
  form.signon_realm = std::string(signon_realm);
  form.username_value = std::u16string(username);
  form.in_store = store;
  return form;
}

}  // namespace

class PostSaveCompromisedHelperTest : public testing::Test {
 public:
  PostSaveCompromisedHelperTest() {
    mock_profile_store_ = new MockPasswordStoreInterface;
    prefs_.registry()->RegisterDoublePref(kLastTimePasswordCheckCompleted, 0.0);
  }

  ~PostSaveCompromisedHelperTest() override {
    mock_profile_store_->ShutdownOnUIThread();
  }

  void ExpectGetLoginsCall(std::vector<PasswordForm> password_forms) {
    EXPECT_CALL(*profile_store(), GetAutofillableLogins)
        .WillOnce(testing::WithArg<0>(
            [password_forms](base::WeakPtr<PasswordStoreConsumer> consumer) {
              std::vector<std::unique_ptr<PasswordForm>> results;
              for (auto& form : password_forms)
                results.push_back(
                    std::make_unique<PasswordForm>(std::move(form)));
              consumer->OnGetPasswordStoreResults(std::move(results));
            }));
  }

  void WaitForPasswordStore() { task_environment_.RunUntilIdle(); }

  MockPasswordStoreInterface* profile_store() {
    return mock_profile_store_.get();
  }
  virtual MockPasswordStoreInterface* account_store() { return nullptr; }
  TestingPrefServiceSimple* prefs() { return &prefs_; }

 protected:
  TestingPrefServiceSimple prefs_;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  scoped_refptr<MockPasswordStoreInterface> mock_profile_store_;
};

TEST_F(PostSaveCompromisedHelperTest, DefaultState) {
  PostSaveCompromisedHelper helper({}, kUsername);
  EXPECT_EQ(BubbleType::kNoBubble, helper.bubble_type());
  EXPECT_EQ(0u, helper.compromised_count());
}

TEST_F(PostSaveCompromisedHelperTest, EmptyStore) {
  prefs()->SetDouble(kLastTimePasswordCheckCompleted,
                     (base::Time::Now() - base::Minutes(1)).ToDoubleT());
  PostSaveCompromisedHelper helper({}, kUsername);
  base::MockCallback<PostSaveCompromisedHelper::BubbleCallback> callback;
  EXPECT_CALL(callback, Run(BubbleType::kNoBubble, 0));
  ExpectGetLoginsCall({});
  helper.AnalyzeLeakedCredentials(profile_store(), account_store(), prefs(),
                                  callback.Get());
  WaitForPasswordStore();
  EXPECT_EQ(BubbleType::kNoBubble, helper.bubble_type());
  EXPECT_EQ(0u, helper.compromised_count());
}

TEST_F(PostSaveCompromisedHelperTest, RandomSite_FullStore) {
  prefs()->SetDouble(kLastTimePasswordCheckCompleted,
                     (base::Time::Now() - base::Minutes(1)).ToDoubleT());
  PostSaveCompromisedHelper helper({}, kUsername);
  base::MockCallback<PostSaveCompromisedHelper::BubbleCallback> callback;
  EXPECT_CALL(callback, Run(BubbleType::kNoBubble, _));

  PasswordForm form = CreateForm(kSignonRealm, kUsername2);
  ExpectGetLoginsCall({form});
  helper.AnalyzeLeakedCredentials(profile_store(), account_store(), prefs(),
                                  callback.Get());
  WaitForPasswordStore();
  EXPECT_EQ(BubbleType::kNoBubble, helper.bubble_type());
}

TEST_F(PostSaveCompromisedHelperTest, CompromisedSite_ItemStayed) {
  prefs()->SetDouble(kLastTimePasswordCheckCompleted,
                     (base::Time::Now() - base::Minutes(1)).ToDoubleT());
  PasswordForm form1 = CreateForm(kSignonRealm, kUsername);
  form1.password_issues.insert({InsecureType::kLeaked, InsecurityMetadata()});
  PasswordForm form2 = CreateForm(kSignonRealm, kUsername2);
  form2.password_issues.insert({InsecureType::kLeaked, InsecurityMetadata()});

  PostSaveCompromisedHelper helper({{CreateInsecureCredential(kUsername)}},
                                   kUsername);
  base::MockCallback<PostSaveCompromisedHelper::BubbleCallback> callback;
  ExpectGetLoginsCall({form1, form2});
  EXPECT_CALL(callback, Run(BubbleType::kNoBubble, 2));
  helper.AnalyzeLeakedCredentials(profile_store(), account_store(), prefs(),
                                  callback.Get());
  WaitForPasswordStore();
  EXPECT_EQ(BubbleType::kNoBubble, helper.bubble_type());
}

TEST_F(PostSaveCompromisedHelperTest, CompromisedSite_ItemGone) {
  prefs()->SetDouble(kLastTimePasswordCheckCompleted,
                     (base::Time::Now() - base::Minutes(1)).ToDoubleT());
  std::vector<InsecureCredential> saved = {
      CreateInsecureCredential(kUsername),
      CreateInsecureCredential(kUsername2)};

  PasswordForm form1 = CreateForm(kSignonRealm, kUsername);
  PasswordForm form2 = CreateForm(kSignonRealm, kUsername2);
  form2.password_issues.insert({InsecureType::kLeaked, InsecurityMetadata()});

  PostSaveCompromisedHelper helper({saved}, kUsername);
  base::MockCallback<PostSaveCompromisedHelper::BubbleCallback> callback;
  EXPECT_CALL(callback, Run(BubbleType::kPasswordUpdatedWithMoreToFix, 1));
  ExpectGetLoginsCall({form1, form2});
  helper.AnalyzeLeakedCredentials(profile_store(), account_store(), prefs(),
                                  callback.Get());
  WaitForPasswordStore();
  EXPECT_EQ(BubbleType::kPasswordUpdatedWithMoreToFix, helper.bubble_type());
  EXPECT_EQ(1u, helper.compromised_count());
}

TEST_F(PostSaveCompromisedHelperTest, FixedLast_BulkCheckNeverDone) {
  PostSaveCompromisedHelper helper({{CreateInsecureCredential(kUsername)}},
                                   kUsername);
  base::MockCallback<PostSaveCompromisedHelper::BubbleCallback> callback;
  EXPECT_CALL(callback, Run(BubbleType::kNoBubble, 0));
  EXPECT_CALL(*profile_store(), GetAutofillableLogins).Times(0);
  helper.AnalyzeLeakedCredentials(profile_store(), account_store(), prefs(),
                                  callback.Get());
  WaitForPasswordStore();
  EXPECT_EQ(BubbleType::kNoBubble, helper.bubble_type());
  EXPECT_EQ(0u, helper.compromised_count());
}

TEST_F(PostSaveCompromisedHelperTest, FixedLast_BulkCheckDoneLongAgo) {
  prefs()->SetDouble(kLastTimePasswordCheckCompleted,
                     (base::Time::Now() - base::Days(5)).ToDoubleT());
  std::vector<InsecureCredential> saved = {CreateInsecureCredential(kUsername)};
  PostSaveCompromisedHelper helper({saved}, kUsername);
  base::MockCallback<PostSaveCompromisedHelper::BubbleCallback> callback;
  EXPECT_CALL(callback, Run(BubbleType::kNoBubble, 0));
  EXPECT_CALL(*profile_store(), GetAutofillableLogins).Times(0);
  helper.AnalyzeLeakedCredentials(profile_store(), account_store(), prefs(),
                                  callback.Get());
  WaitForPasswordStore();
  EXPECT_EQ(BubbleType::kNoBubble, helper.bubble_type());
  EXPECT_EQ(0u, helper.compromised_count());
}

TEST_F(PostSaveCompromisedHelperTest, FixedLast_BulkCheckDoneRecently) {
  prefs()->SetDouble(kLastTimePasswordCheckCompleted,
                     (base::Time::Now() - base::Minutes(1)).ToDoubleT());
  std::vector<InsecureCredential> saved = {CreateInsecureCredential(kUsername)};
  PostSaveCompromisedHelper helper({saved}, kUsername);
  base::MockCallback<PostSaveCompromisedHelper::BubbleCallback> callback;
  EXPECT_CALL(callback, Run(BubbleType::kPasswordUpdatedSafeState, 0));
  ExpectGetLoginsCall({CreateForm(kSignonRealm, kUsername)});
  helper.AnalyzeLeakedCredentials(profile_store(), account_store(), prefs(),
                                  callback.Get());
  WaitForPasswordStore();
  EXPECT_EQ(BubbleType::kPasswordUpdatedSafeState, helper.bubble_type());
  EXPECT_EQ(0u, helper.compromised_count());
}

TEST_F(PostSaveCompromisedHelperTest, BubbleShownEvenIfIssueIsMuted) {
  prefs()->SetDouble(kLastTimePasswordCheckCompleted,
                     (base::Time::Now() - base::Minutes(1)).ToDoubleT());
  std::vector<InsecureCredential> saved = {CreateInsecureCredential(
      kUsername, PasswordForm::Store::kProfileStore, IsMuted(true))};
  PostSaveCompromisedHelper helper({saved}, kUsername);
  base::MockCallback<PostSaveCompromisedHelper::BubbleCallback> callback;
  EXPECT_CALL(callback, Run(BubbleType::kPasswordUpdatedSafeState, 0));
  ExpectGetLoginsCall({CreateForm(kSignonRealm, kUsername)});
  helper.AnalyzeLeakedCredentials(profile_store(), account_store(), prefs(),
                                  callback.Get());
  WaitForPasswordStore();
  EXPECT_EQ(BubbleType::kPasswordUpdatedSafeState, helper.bubble_type());
  EXPECT_EQ(0u, helper.compromised_count());
}

TEST_F(PostSaveCompromisedHelperTest, MutedIssuesNotIncludedToCount) {
  prefs()->SetDouble(kLastTimePasswordCheckCompleted,
                     (base::Time::Now() - base::Minutes(1)).ToDoubleT());
  std::vector<InsecureCredential> saved = {CreateInsecureCredential(kUsername)};
  PostSaveCompromisedHelper helper({saved}, kUsername);
  base::MockCallback<PostSaveCompromisedHelper::BubbleCallback> callback;
  EXPECT_CALL(callback, Run(BubbleType::kPasswordUpdatedWithMoreToFix, 1));
  PasswordForm form1 = CreateForm(kSignonRealm, kUsername);
  PasswordForm form2 = CreateForm(kSignonRealm, kUsername2);
  form2.password_issues.insert({InsecureType::kLeaked, InsecurityMetadata()});
  PasswordForm form3 = CreateForm(kSignonRealm, kUsername3);
  form3.password_issues.insert(
      {InsecureType::kLeaked, InsecurityMetadata(base::Time(), IsMuted(true))});

  ExpectGetLoginsCall({form1, form2, form3});
  helper.AnalyzeLeakedCredentials(profile_store(), account_store(), prefs(),
                                  callback.Get());
  WaitForPasswordStore();
  EXPECT_EQ(BubbleType::kPasswordUpdatedWithMoreToFix, helper.bubble_type());
  EXPECT_EQ(1u, helper.compromised_count());
}

namespace {
class PostSaveCompromisedHelperWithTwoStoreTest
    : public PostSaveCompromisedHelperTest {
 public:
  PostSaveCompromisedHelperWithTwoStoreTest() {
    mock_account_store_ = new MockPasswordStoreInterface;
  }

  ~PostSaveCompromisedHelperWithTwoStoreTest() override = default;

  MockPasswordStoreInterface* account_store() override {
    return mock_account_store_.get();
  }

 private:
  scoped_refptr<MockPasswordStoreInterface> mock_account_store_;
};

}  // namespace

TEST_F(PostSaveCompromisedHelperWithTwoStoreTest,
       CompromisedSiteInAccountStore_ItemStayed) {
  prefs()->SetDouble(kLastTimePasswordCheckCompleted,
                     (base::Time::Now() - base::Minutes(1)).ToDoubleT());
  InsecureCredential profile_store_compromised_credential =
      CreateInsecureCredential(kUsername, PasswordForm::Store::kProfileStore);
  InsecureCredential account_store_compromised_credential =
      CreateInsecureCredential(kUsername, PasswordForm::Store::kAccountStore);

  std::vector<InsecureCredential> compromised_credentials = {
      profile_store_compromised_credential,
      account_store_compromised_credential};

  PostSaveCompromisedHelper helper({compromised_credentials}, kUsername);
  EXPECT_CALL(*profile_store(), GetAutofillableLogins)
      .WillOnce(testing::WithArg<0>([](base::WeakPtr<PasswordStoreConsumer>
                                           consumer) {
        std::vector<std::unique_ptr<PasswordForm>> results;
        results.push_back(std::make_unique<PasswordForm>(
            CreateForm(kSignonRealm, kUsername)));
        results.back()->password_issues.insert(
            {InsecureType::kLeaked, InsecurityMetadata()});
        consumer->OnGetPasswordStoreResults(std::move(results));
      }));
  EXPECT_CALL(*account_store(), GetAutofillableLogins)
      .WillOnce(testing::WithArg<0>([](base::WeakPtr<PasswordStoreConsumer>
                                           consumer) {
        std::vector<std::unique_ptr<PasswordForm>> results;
        results.push_back(std::make_unique<PasswordForm>(CreateForm(
            kSignonRealm, kUsername, PasswordForm::Store::kAccountStore)));
        results.back()->password_issues.insert(
            {InsecureType::kLeaked, InsecurityMetadata()});
        consumer->OnGetPasswordStoreResults(std::move(results));
      }));
  base::MockCallback<PostSaveCompromisedHelper::BubbleCallback> callback;
  EXPECT_CALL(callback, Run(BubbleType::kNoBubble, _));
  helper.AnalyzeLeakedCredentials(profile_store(), account_store(), prefs(),
                                  callback.Get());
  WaitForPasswordStore();
  EXPECT_EQ(BubbleType::kNoBubble, helper.bubble_type());
}

}  // namespace password_manager
