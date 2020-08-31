// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/passwords_private/password_check_delegate.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router_factory.h"
#include "chrome/browser/password_manager/bulk_leak_check_service_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/common/extensions/api/passwords_private.h"
#include "chrome/test/base/testing_profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/core/browser/bulk_leak_check_service.h"
#include "components/password_manager/core/browser/compromised_credentials_table.h"
#include "components/password_manager/core/browser/leak_detection/bulk_leak_check.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_delegate_interface.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/browser/test_event_router.h"
#include "extensions/browser/test_event_router_observer.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

constexpr char kExampleCom[] = "https://example.com";
constexpr char kExampleOrg[] = "http://www.example.org";
constexpr char kExampleApp[] = "com.example.app";

constexpr char kTestEmail[] = "user@gmail.com";

constexpr char kUsername1[] = "alice";
constexpr char kUsername2[] = "bob";

constexpr char kPassword1[] = "s3cre3t";
constexpr char kPassword2[] = "f00b4r";

using api::passwords_private::CompromisedCredential;
using api::passwords_private::PasswordCheckStatus;
using autofill::PasswordForm;
using password_manager::BulkLeakCheckDelegateInterface;
using password_manager::BulkLeakCheckService;
using password_manager::CompromisedCredentials;
using password_manager::CompromiseType;
using password_manager::IsLeaked;
using password_manager::LeakCheckCredential;
using password_manager::TestPasswordStore;
using password_manager::prefs::kLastTimePasswordCheckCompleted;
using signin::IdentityTestEnvironment;
using ::testing::AllOf;
using ::testing::AtLeast;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::Mock;
using ::testing::Pointee;
using ::testing::UnorderedElementsAre;

using MockStartPasswordCheckCallback =
    base::MockCallback<PasswordCheckDelegate::StartPasswordCheckCallback>;

PasswordsPrivateEventRouter* CreateAndUsePasswordsPrivateEventRouter(
    Profile* profile) {
  return static_cast<PasswordsPrivateEventRouter*>(
      PasswordsPrivateEventRouterFactory::GetInstance()
          ->SetTestingFactoryAndUse(
              profile,
              base::BindRepeating([](content::BrowserContext* context) {
                return std::unique_ptr<KeyedService>(
                    PasswordsPrivateEventRouter::Create(context));
              })));
}

EventRouter* CreateAndUseEventRouter(Profile* profile) {
  // The factory function only requires that T be a KeyedService. Ensure it is
  // actually derived from EventRouter to avoid undefined behavior.
  return static_cast<EventRouter*>(
      extensions::EventRouterFactory::GetInstance()->SetTestingFactoryAndUse(
          profile, base::BindRepeating([](content::BrowserContext* context) {
            return std::unique_ptr<KeyedService>(
                std::make_unique<EventRouter>(context, nullptr));
          })));
}

scoped_refptr<TestPasswordStore> CreateAndUseTestPasswordStore(
    Profile* profile) {
  return base::WrapRefCounted(static_cast<TestPasswordStore*>(
      PasswordStoreFactory::GetInstance()
          ->SetTestingFactoryAndUse(
              profile,
              base::BindRepeating(&password_manager::BuildPasswordStore<
                                  content::BrowserContext, TestPasswordStore>))
          .get()));
}

BulkLeakCheckService* CreateAndUseBulkLeakCheckService(
    signin::IdentityManager* identity_manager,
    Profile* profile) {
  return static_cast<BulkLeakCheckService*>(
      BulkLeakCheckServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile, base::BindLambdaForTesting([identity_manager](
                                                  content::BrowserContext*) {
            return std::unique_ptr<
                KeyedService>(std::make_unique<BulkLeakCheckService>(
                identity_manager,
                base::MakeRefCounted<network::TestSharedURLLoaderFactory>()));
          })));
}

CompromisedCredentials MakeCompromised(
    base::StringPiece signon_realm,
    base::StringPiece username,
    base::TimeDelta time_since_creation = base::TimeDelta(),
    CompromiseType compromise_type = CompromiseType::kLeaked) {
  return {
      std::string(signon_realm),
      base::ASCIIToUTF16(username),
      base::Time::Now() - time_since_creation,
      compromise_type,
  };
}

PasswordForm MakeSavedPassword(base::StringPiece signon_realm,
                               base::StringPiece username,
                               base::StringPiece password = kPassword1,
                               base::StringPiece username_element = "") {
  PasswordForm form;
  form.signon_realm = std::string(signon_realm);
  form.username_value = base::ASCIIToUTF16(username);
  form.password_value = base::ASCIIToUTF16(password);
  form.username_element = base::ASCIIToUTF16(username_element);
  return form;
}

std::string MakeAndroidRealm(base::StringPiece package_name) {
  return base::StrCat({"android://hash@", package_name});
}

PasswordForm MakeSavedAndroidPassword(
    base::StringPiece package_name,
    base::StringPiece username,
    base::StringPiece app_display_name = "",
    base::StringPiece affiliated_web_realm = "") {
  PasswordForm form;
  form.signon_realm = MakeAndroidRealm(package_name);
  form.username_value = base::ASCIIToUTF16(username);
  form.app_display_name = std::string(app_display_name);
  form.affiliated_web_realm = std::string(affiliated_web_realm);
  return form;
}

// Creates matcher for a given compromised credential
auto ExpectCompromisedCredential(
    const std::string& formatted_origin,
    const std::string& detailed_origin,
    const base::Optional<std::string>& change_password_url,
    const std::string& username,
    base::TimeDelta elapsed_time_since_compromise,
    const std::string& elapsed_time_since_compromise_str,
    api::passwords_private::CompromiseType compromise_type) {
  auto change_password_url_field_matcher =
      change_password_url.has_value()
          ? Field(&CompromisedCredential::change_password_url,
                  Pointee(change_password_url.value()))
          : Field(&CompromisedCredential::change_password_url, IsNull());
  return AllOf(
      Field(&CompromisedCredential::formatted_origin, formatted_origin),
      Field(&CompromisedCredential::detailed_origin, detailed_origin),
      change_password_url_field_matcher,
      Field(&CompromisedCredential::username, username),
      Field(&CompromisedCredential::compromise_time,
            (base::Time::Now() - elapsed_time_since_compromise)
                .ToJsTimeIgnoringNull()),
      Field(&CompromisedCredential::elapsed_time_since_compromise,
            elapsed_time_since_compromise_str),
      Field(&CompromisedCredential::compromise_type, compromise_type));
}

class PasswordCheckDelegateTest : public ::testing::Test {
 public:
  PasswordCheckDelegateTest() {
    prefs_.registry()->RegisterDoublePref(kLastTimePasswordCheckCompleted, 0.0);
    scoped_feature_list_.InitAndEnableFeature(
        password_manager::features::kPasswordCheck);
  }

  void RunUntilIdle() { task_env_.RunUntilIdle(); }
  TestEventRouterObserver& event_router_observer() {
    return event_router_observer_;
  }
  IdentityTestEnvironment& identity_test_env() { return identity_test_env_; }
  TestingPrefServiceSimple prefs_;
  TestingProfile& profile() { return profile_; }
  TestPasswordStore& store() { return *store_; }
  BulkLeakCheckService* service() { return bulk_leak_check_service_; }
  PasswordCheckDelegate& delegate() { return delegate_; }

 private:
  content::BrowserTaskEnvironment task_env_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  signin::IdentityTestEnvironment identity_test_env_;
  TestingProfile profile_;
  EventRouter* event_router_ = CreateAndUseEventRouter(&profile_);
  PasswordsPrivateEventRouter* password_router_ =
      CreateAndUsePasswordsPrivateEventRouter(&profile_);
  TestEventRouterObserver event_router_observer_{event_router_};
  BulkLeakCheckService* bulk_leak_check_service_ =
      CreateAndUseBulkLeakCheckService(identity_test_env_.identity_manager(),
                                       &profile_);
  scoped_refptr<TestPasswordStore> store_ =
      CreateAndUseTestPasswordStore(&profile_);
  base::test::ScopedFeatureList scoped_feature_list_;
  PasswordCheckDelegate delegate_{&profile_};
};

}  // namespace

// Sets up the password store with a couple of passwords and compromised
// credentials. Verifies that the result is ordered in such a way that phished
// credentials are before leaked credentials and that within each group
// credentials are ordered by recency.
TEST_F(PasswordCheckDelegateTest, GetCompromisedCredentialsOrders) {
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1));
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername2));
  store().AddLogin(MakeSavedPassword(kExampleOrg, kUsername1));
  store().AddLogin(MakeSavedPassword(kExampleOrg, kUsername2));

  store().AddCompromisedCredentials(
      MakeCompromised(kExampleCom, kUsername1, base::TimeDelta::FromMinutes(1),
                      CompromiseType::kLeaked));
  store().AddCompromisedCredentials(
      MakeCompromised(kExampleCom, kUsername2, base::TimeDelta::FromMinutes(2),
                      CompromiseType::kPhished));
  store().AddCompromisedCredentials(
      MakeCompromised(kExampleOrg, kUsername2, base::TimeDelta::FromMinutes(3),
                      CompromiseType::kLeaked));
  store().AddCompromisedCredentials(
      MakeCompromised(kExampleOrg, kUsername1, base::TimeDelta::FromMinutes(4),
                      CompromiseType::kPhished));
  RunUntilIdle();

  EXPECT_THAT(
      delegate().GetCompromisedCredentials(),
      ElementsAre(ExpectCompromisedCredential(
                      "example.com", kExampleCom, kExampleCom, kUsername2,
                      base::TimeDelta::FromMinutes(2), "2 minutes ago",
                      api::passwords_private::COMPROMISE_TYPE_PHISHED),
                  ExpectCompromisedCredential(
                      "example.org", kExampleOrg, kExampleOrg, kUsername1,
                      base::TimeDelta::FromMinutes(4), "4 minutes ago",
                      api::passwords_private::COMPROMISE_TYPE_PHISHED),
                  ExpectCompromisedCredential(
                      "example.com", kExampleCom, kExampleCom, kUsername1,
                      base::TimeDelta::FromMinutes(1), "1 minute ago",
                      api::passwords_private::COMPROMISE_TYPE_LEAKED),
                  ExpectCompromisedCredential(
                      "example.org", kExampleOrg, kExampleOrg, kUsername2,
                      base::TimeDelta::FromMinutes(3), "3 minutes ago",
                      api::passwords_private::COMPROMISE_TYPE_LEAKED)));
}

// Verifies that the formatted timestamp associated with a compromised
// credential covers the "Just now" cases (less than a minute ago), as well as
// months and years.
TEST_F(PasswordCheckDelegateTest, GetCompromisedCredentialsHandlesTimes) {
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1));
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername2));
  store().AddLogin(MakeSavedPassword(kExampleOrg, kUsername1));
  store().AddLogin(MakeSavedPassword(kExampleOrg, kUsername2));

  store().AddCompromisedCredentials(
      MakeCompromised(kExampleCom, kUsername1, base::TimeDelta::FromSeconds(59),
                      CompromiseType::kLeaked));
  store().AddCompromisedCredentials(
      MakeCompromised(kExampleCom, kUsername2, base::TimeDelta::FromSeconds(60),
                      CompromiseType::kLeaked));
  store().AddCompromisedCredentials(
      MakeCompromised(kExampleOrg, kUsername1, base::TimeDelta::FromDays(100),
                      CompromiseType::kLeaked));
  store().AddCompromisedCredentials(
      MakeCompromised(kExampleOrg, kUsername2, base::TimeDelta::FromDays(800),
                      CompromiseType::kLeaked));
  RunUntilIdle();

  EXPECT_THAT(
      delegate().GetCompromisedCredentials(),
      ElementsAre(ExpectCompromisedCredential(
                      "example.com", kExampleCom, kExampleCom, kUsername1,
                      base::TimeDelta::FromSeconds(59), "Just now",
                      api::passwords_private::COMPROMISE_TYPE_LEAKED),
                  ExpectCompromisedCredential(
                      "example.com", kExampleCom, kExampleCom, kUsername2,
                      base::TimeDelta::FromSeconds(60), "1 minute ago",
                      api::passwords_private::COMPROMISE_TYPE_LEAKED),
                  ExpectCompromisedCredential(
                      "example.org", kExampleOrg, kExampleOrg, kUsername1,
                      base::TimeDelta::FromDays(100), "3 months ago",
                      api::passwords_private::COMPROMISE_TYPE_LEAKED),
                  ExpectCompromisedCredential(
                      "example.org", kExampleOrg, kExampleOrg, kUsername2,
                      base::TimeDelta::FromDays(800), "2 years ago",
                      api::passwords_private::COMPROMISE_TYPE_LEAKED)));
}

// Verifies that both leaked and phished credentials are ordered correctly
// within the list of compromised credentials. These credentials should be
// listed before just leaked ones and have a timestamp that corresponds to the
// most recent compromise.
TEST_F(PasswordCheckDelegateTest,
       GetCompromisedCredentialsDedupesLeakedAndCompromised) {
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1));
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername2));
  store().AddLogin(MakeSavedPassword(kExampleOrg, kUsername1));
  store().AddLogin(MakeSavedPassword(kExampleOrg, kUsername2));

  store().AddCompromisedCredentials(
      MakeCompromised(kExampleCom, kUsername1, base::TimeDelta::FromMinutes(1),
                      CompromiseType::kLeaked));
  store().AddCompromisedCredentials(
      MakeCompromised(kExampleCom, kUsername2, base::TimeDelta::FromMinutes(2),
                      CompromiseType::kLeaked));
  store().AddCompromisedCredentials(
      MakeCompromised(kExampleOrg, kUsername1, base::TimeDelta::FromMinutes(3),
                      CompromiseType::kPhished));
  store().AddCompromisedCredentials(
      MakeCompromised(kExampleOrg, kUsername2, base::TimeDelta::FromMinutes(4),
                      CompromiseType::kPhished));
  store().AddCompromisedCredentials(
      MakeCompromised(kExampleCom, kUsername1, base::TimeDelta::FromMinutes(5),
                      CompromiseType::kPhished));
  store().AddCompromisedCredentials(
      MakeCompromised(kExampleOrg, kUsername2, base::TimeDelta::FromMinutes(6),
                      CompromiseType::kLeaked));
  RunUntilIdle();

  EXPECT_THAT(
      delegate().GetCompromisedCredentials(),
      ElementsAre(
          ExpectCompromisedCredential(
              "example.com", kExampleCom, kExampleCom, kUsername1,
              base::TimeDelta::FromMinutes(1), "1 minute ago",
              api::passwords_private::COMPROMISE_TYPE_PHISHED_AND_LEAKED),
          ExpectCompromisedCredential(
              "example.org", kExampleOrg, kExampleOrg, kUsername1,
              base::TimeDelta::FromMinutes(3), "3 minutes ago",
              api::passwords_private::COMPROMISE_TYPE_PHISHED),
          ExpectCompromisedCredential(
              "example.org", kExampleOrg, kExampleOrg, kUsername2,
              base::TimeDelta::FromMinutes(4), "4 minutes ago",
              api::passwords_private::COMPROMISE_TYPE_PHISHED_AND_LEAKED),
          ExpectCompromisedCredential(
              "example.com", kExampleCom, kExampleCom, kUsername2,
              base::TimeDelta::FromMinutes(2), "2 minutes ago",
              api::passwords_private::COMPROMISE_TYPE_LEAKED)));
}

TEST_F(PasswordCheckDelegateTest, GetCompromisedCredentialsInjectsAndroid) {
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1));
  store().AddLogin(MakeSavedAndroidPassword(kExampleApp, kUsername2,
                                            "Example App", kExampleCom));
  // Test Android credential without affiliation information.
  store().AddLogin(MakeSavedAndroidPassword(kExampleApp, kUsername1));
  store().AddCompromisedCredentials(
      MakeCompromised(kExampleCom, kUsername1, base::TimeDelta::FromMinutes(5),
                      CompromiseType::kLeaked));
  store().AddCompromisedCredentials(
      MakeCompromised(MakeAndroidRealm(kExampleApp), kUsername2,
                      base::TimeDelta::FromDays(3), CompromiseType::kPhished));
  store().AddCompromisedCredentials(
      MakeCompromised(MakeAndroidRealm(kExampleApp), kUsername1,
                      base::TimeDelta::FromDays(4), CompromiseType::kPhished));
  RunUntilIdle();

  // Verify that the compromised credentials match what is stored in the
  // password store.
  EXPECT_THAT(
      delegate().GetCompromisedCredentials(),
      ElementsAre(ExpectCompromisedCredential(
                      "Example App", "Example App", kExampleCom, kUsername2,
                      base::TimeDelta::FromDays(3), "3 days ago",
                      api::passwords_private::COMPROMISE_TYPE_PHISHED),
                  ExpectCompromisedCredential(
                      "App (com.example.app)", kExampleApp, base::nullopt,
                      kUsername1, base::TimeDelta::FromDays(4), "4 days ago",
                      api::passwords_private::COMPROMISE_TYPE_PHISHED),
                  ExpectCompromisedCredential(
                      "example.com", kExampleCom, kExampleCom, kUsername1,
                      base::TimeDelta::FromMinutes(5), "5 minutes ago",
                      api::passwords_private::COMPROMISE_TYPE_LEAKED)));
}

// Test that a change to compromised credential notifies observers.
TEST_F(PasswordCheckDelegateTest, OnGetCompromisedCredentials) {
  const char* const kEventName =
      api::passwords_private::OnCompromisedCredentialsChanged::kEventName;

  // Verify that the event was not fired during construction.
  EXPECT_FALSE(base::Contains(event_router_observer().events(), kEventName));

  // Verify that the event gets fired once the compromised credential provider
  // is initialized.
  RunUntilIdle();
  EXPECT_EQ(events::PASSWORDS_PRIVATE_ON_COMPROMISED_CREDENTIALS_INFO_CHANGED,
            event_router_observer().events().at(kEventName)->histogram_value);
  event_router_observer().ClearEvents();

  // Verify that a subsequent call to AddCompromisedCredentials results in the
  // expected event.
  store().AddCompromisedCredentials(MakeCompromised(kExampleCom, kUsername1));
  RunUntilIdle();
  EXPECT_EQ(events::PASSWORDS_PRIVATE_ON_COMPROMISED_CREDENTIALS_INFO_CHANGED,
            event_router_observer().events().at(kEventName)->histogram_value);
}

TEST_F(PasswordCheckDelegateTest,
       GetPlaintextCompromisedPasswordRejectsWrongId) {
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1));
  store().AddCompromisedCredentials(MakeCompromised(kExampleCom, kUsername1));
  RunUntilIdle();

  CompromisedCredential credential =
      std::move(delegate().GetCompromisedCredentials().at(0));
  EXPECT_EQ(0, credential.id);

  // Purposefully set a wrong id and verify that trying to get a plaintext
  // password fails.
  credential.id = 1;
  EXPECT_EQ(base::nullopt,
            delegate().GetPlaintextCompromisedPassword(std::move(credential)));
}

TEST_F(PasswordCheckDelegateTest,
       GetPlaintextCompromisedPasswordRejectsWrongSignonRealm) {
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1));
  store().AddCompromisedCredentials(MakeCompromised(kExampleCom, kUsername1));
  RunUntilIdle();

  CompromisedCredential credential =
      std::move(delegate().GetCompromisedCredentials().at(0));
  EXPECT_EQ(kExampleCom, credential.signon_realm);

  // Purposefully set a wrong signon realm and verify that trying to get a
  // plaintext password fails.
  credential.signon_realm = kExampleOrg;
  EXPECT_EQ(base::nullopt,
            delegate().GetPlaintextCompromisedPassword(std::move(credential)));
}

TEST_F(PasswordCheckDelegateTest,
       GetPlaintextCompromisedPasswordRejectsWrongUsername) {
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1));
  store().AddCompromisedCredentials(MakeCompromised(kExampleCom, kUsername1));
  RunUntilIdle();

  CompromisedCredential credential =
      std::move(delegate().GetCompromisedCredentials().at(0));
  EXPECT_EQ(kUsername1, credential.username);

  // Purposefully set a wrong username and verify that trying to get a
  // plaintext password fails.
  credential.signon_realm = kUsername2;
  EXPECT_EQ(base::nullopt,
            delegate().GetPlaintextCompromisedPassword(std::move(credential)));
}

TEST_F(PasswordCheckDelegateTest,
       GetPlaintextCompromisedPasswordReturnsCorrectPassword) {
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1, kPassword1));
  store().AddCompromisedCredentials(MakeCompromised(kExampleCom, kUsername1));
  RunUntilIdle();

  CompromisedCredential credential =
      std::move(delegate().GetCompromisedCredentials().at(0));
  EXPECT_EQ(0, credential.id);
  EXPECT_EQ(kExampleCom, credential.signon_realm);
  EXPECT_EQ(kUsername1, credential.username);
  EXPECT_EQ(nullptr, credential.password);

  base::Optional<CompromisedCredential> opt_credential =
      delegate().GetPlaintextCompromisedPassword(std::move(credential));
  ASSERT_TRUE(opt_credential.has_value());
  EXPECT_EQ(0, opt_credential->id);
  EXPECT_EQ(kExampleCom, opt_credential->signon_realm);
  EXPECT_EQ(kUsername1, opt_credential->username);
  EXPECT_EQ(kPassword1, *opt_credential->password);
}

// Test that changing a compromised password fails if the ids don't match.
TEST_F(PasswordCheckDelegateTest, ChangeCompromisedCredentialIdMismatch) {
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1));
  store().AddCompromisedCredentials(MakeCompromised(kExampleCom, kUsername1));
  RunUntilIdle();

  CompromisedCredential credential =
      std::move(delegate().GetCompromisedCredentials().at(0));
  EXPECT_EQ(0, credential.id);
  credential.id = 1;

  EXPECT_FALSE(delegate().ChangeCompromisedCredential(credential, "new_pass"));
}

// Test that changing a compromised password fails if the underlying compromised
// credential no longer exists.
TEST_F(PasswordCheckDelegateTest, ChangeCompromisedCredentialStaleData) {
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1));
  store().AddCompromisedCredentials(MakeCompromised(kExampleCom, kUsername1));
  RunUntilIdle();

  CompromisedCredential credential =
      std::move(delegate().GetCompromisedCredentials().at(0));

  store().RemoveLogin(MakeSavedPassword(kExampleCom, kUsername1));
  RunUntilIdle();

  EXPECT_FALSE(delegate().ChangeCompromisedCredential(credential, "new_pass"));
}

// Test that changing a compromised password succeeds.
TEST_F(PasswordCheckDelegateTest, ChangeCompromisedCredentialSuccess) {
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1, kPassword1));
  store().AddCompromisedCredentials(MakeCompromised(kExampleCom, kUsername1));
  RunUntilIdle();

  CompromisedCredential credential =
      std::move(delegate().GetCompromisedCredentials().at(0));
  EXPECT_EQ(0, credential.id);
  EXPECT_EQ(kExampleCom, credential.signon_realm);
  EXPECT_EQ(kUsername1, credential.username);
  EXPECT_EQ(base::UTF8ToUTF16(kPassword1),
            store().stored_passwords().at(kExampleCom).at(0).password_value);

  EXPECT_TRUE(delegate().ChangeCompromisedCredential(credential, kPassword2));
  RunUntilIdle();

  EXPECT_EQ(base::UTF8ToUTF16(kPassword2),
            store().stored_passwords().at(kExampleCom).at(0).password_value);
}

// Test that changing a compromised password removes duplicates from store.
TEST_F(PasswordCheckDelegateTest, ChangeCompromisedCredentialRemovesDupes) {
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1, kPassword1));
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1, kPassword1,
                                     "different_element"));
  store().AddCompromisedCredentials(MakeCompromised(kExampleCom, kUsername1));
  RunUntilIdle();

  EXPECT_EQ(2u, store().stored_passwords().at(kExampleCom).size());

  CompromisedCredential credential =
      std::move(delegate().GetCompromisedCredentials().at(0));
  EXPECT_TRUE(delegate().ChangeCompromisedCredential(credential, kPassword2));
  RunUntilIdle();

  EXPECT_EQ(1u, store().stored_passwords().at(kExampleCom).size());
  EXPECT_EQ(
      kPassword2,
      base::UTF16ToUTF8(
          store().stored_passwords().at(kExampleCom).at(0).password_value));
}

// Test that removing a compromised password fails if the ids don't match.
TEST_F(PasswordCheckDelegateTest, RemoveCompromisedCredentialIdMismatch) {
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1));
  store().AddCompromisedCredentials(MakeCompromised(kExampleCom, kUsername1));
  RunUntilIdle();

  CompromisedCredential credential =
      std::move(delegate().GetCompromisedCredentials().at(0));
  EXPECT_EQ(0, credential.id);
  credential.id = 1;

  EXPECT_FALSE(delegate().RemoveCompromisedCredential(credential));
}

// Test that removing a compromised password fails if the underlying compromised
// credential no longer exists.
TEST_F(PasswordCheckDelegateTest, RemoveCompromisedCredentialStaleData) {
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1));
  store().AddCompromisedCredentials(MakeCompromised(kExampleCom, kUsername1));
  RunUntilIdle();

  CompromisedCredential credential =
      std::move(delegate().GetCompromisedCredentials().at(0));
  store().RemoveLogin(MakeSavedPassword(kExampleCom, kUsername1));
  RunUntilIdle();

  EXPECT_FALSE(delegate().RemoveCompromisedCredential(credential));
}

// Test that removing a compromised password succeeds.
TEST_F(PasswordCheckDelegateTest, RemoveCompromisedCredentialSuccess) {
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1, kPassword1));
  store().AddCompromisedCredentials(MakeCompromised(kExampleCom, kUsername1));
  RunUntilIdle();

  CompromisedCredential credential =
      std::move(delegate().GetCompromisedCredentials().at(0));
  EXPECT_TRUE(delegate().RemoveCompromisedCredential(credential));
  RunUntilIdle();
  EXPECT_TRUE(store().IsEmpty());

  // Expect another removal of the same credential to fail.
  EXPECT_FALSE(delegate().RemoveCompromisedCredential(credential));
}

// Tests that we don't create an entry in the database if there is no matching
// saved password.
TEST_F(PasswordCheckDelegateTest, OnLeakFoundDoesNotCreateCredential) {
  identity_test_env().MakeAccountAvailable(kTestEmail);
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1, kPassword1));
  RunUntilIdle();
  delegate().StartPasswordCheck();
  store().RemoveLogin(MakeSavedPassword(kExampleCom, kUsername1, kPassword1));
  RunUntilIdle();
  static_cast<BulkLeakCheckDelegateInterface*>(service())->OnFinishedCredential(
      LeakCheckCredential(base::ASCIIToUTF16(kUsername1),
                          base::ASCIIToUTF16(kPassword1)),
      IsLeaked(true));
  RunUntilIdle();

  EXPECT_THAT(store().compromised_credentials(), IsEmpty());
}

// Test that we don't create an entry in the password store if IsLeaked is
// false.
TEST_F(PasswordCheckDelegateTest, NoLeakedFound) {
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1, kPassword1));
  RunUntilIdle();

  static_cast<BulkLeakCheckDelegateInterface*>(service())->OnFinishedCredential(
      LeakCheckCredential(base::ASCIIToUTF16(kUsername1),
                          base::ASCIIToUTF16(kPassword1)),
      IsLeaked(false));
  RunUntilIdle();

  EXPECT_THAT(store().compromised_credentials(), IsEmpty());
}

// Test that a found leak creates a compromised credential in the password
// store.
TEST_F(PasswordCheckDelegateTest, OnLeakFoundCreatesCredential) {
  identity_test_env().MakeAccountAvailable(kTestEmail);
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1, kPassword1));
  RunUntilIdle();

  delegate().StartPasswordCheck();
  static_cast<BulkLeakCheckDelegateInterface*>(service())->OnFinishedCredential(
      LeakCheckCredential(base::ASCIIToUTF16(kUsername1),
                          base::ASCIIToUTF16(kPassword1)),
      IsLeaked(true));
  RunUntilIdle();

  EXPECT_THAT(store().compromised_credentials(),
              ElementsAre(CompromisedCredentials{
                  .signon_realm = kExampleCom,
                  .username = base::ASCIIToUTF16(kUsername1),
                  .create_time = base::Time::Now(),
                  .compromise_type = CompromiseType::kLeaked,
              }));
}

// Test that a found leak creates a compromised credential in the password
// store for each combination of the same canonicalized username and password.
TEST_F(PasswordCheckDelegateTest, OnLeakFoundCreatesMultipleCredential) {
  const std::string kUsername2Upper = base::ToUpperASCII(kUsername2);
  const std::string kUsername2Email = base::StrCat({kUsername2, "@email.com"});

  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1, kPassword1));
  store().AddLogin(MakeSavedPassword(kExampleOrg, kUsername1, kPassword1));
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername2Upper, kPassword2));
  store().AddLogin(MakeSavedPassword(kExampleOrg, kUsername2Email, kPassword2));
  RunUntilIdle();

  identity_test_env().MakeAccountAvailable(kTestEmail);
  delegate().StartPasswordCheck();
  static_cast<BulkLeakCheckDelegateInterface*>(service())->OnFinishedCredential(
      LeakCheckCredential(base::ASCIIToUTF16(kUsername1),
                          base::ASCIIToUTF16(kPassword1)),
      IsLeaked(true));
  static_cast<BulkLeakCheckDelegateInterface*>(service())->OnFinishedCredential(
      LeakCheckCredential(base::ASCIIToUTF16(kUsername2Email),
                          base::ASCIIToUTF16(kPassword2)),
      IsLeaked(true));
  RunUntilIdle();

  EXPECT_THAT(store().compromised_credentials(),
              UnorderedElementsAre(
                  CompromisedCredentials{
                      .signon_realm = kExampleCom,
                      .username = base::ASCIIToUTF16(kUsername1),
                      .create_time = base::Time::Now(),
                      .compromise_type = CompromiseType::kLeaked,
                  },
                  CompromisedCredentials{
                      .signon_realm = kExampleOrg,
                      .username = base::ASCIIToUTF16(kUsername1),
                      .create_time = base::Time::Now(),
                      .compromise_type = CompromiseType::kLeaked,
                  },
                  CompromisedCredentials{
                      .signon_realm = kExampleCom,
                      .username = base::ASCIIToUTF16(kUsername2Upper),
                      .create_time = base::Time::Now(),
                      .compromise_type = CompromiseType::kLeaked,
                  },
                  CompromisedCredentials{
                      .signon_realm = kExampleOrg,
                      .username = base::ASCIIToUTF16(kUsername2Email),
                      .create_time = base::Time::Now(),
                      .compromise_type = CompromiseType::kLeaked,
                  }));
}

// Verifies that the case where the user has no saved passwords is reported
// correctly.
TEST_F(PasswordCheckDelegateTest, GetPasswordCheckStatusNoPasswords) {
  EXPECT_EQ(api::passwords_private::PASSWORD_CHECK_STATE_NO_PASSWORDS,
            delegate().GetPasswordCheckStatus().state);
}

// Verifies that the case where the check is idle is reported correctly.
TEST_F(PasswordCheckDelegateTest, GetPasswordCheckStatusIdle) {
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1));
  RunUntilIdle();

  EXPECT_EQ(api::passwords_private::PASSWORD_CHECK_STATE_IDLE,
            delegate().GetPasswordCheckStatus().state);
}

// Verifies that the case where the user is signed out is reported correctly.
TEST_F(PasswordCheckDelegateTest, GetPasswordCheckStatusSignedOut) {
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1));
  RunUntilIdle();

  delegate().StartPasswordCheck();
  EXPECT_EQ(api::passwords_private::PASSWORD_CHECK_STATE_SIGNED_OUT,
            delegate().GetPasswordCheckStatus().state);
}

// Verifies that the case where the check is running is reported correctly and
// the progress indicator matches expectations.
TEST_F(PasswordCheckDelegateTest, GetPasswordCheckStatusRunning) {
  identity_test_env().MakeAccountAvailable(kTestEmail);
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1));
  RunUntilIdle();

  delegate().StartPasswordCheck();
  PasswordCheckStatus status = delegate().GetPasswordCheckStatus();
  EXPECT_EQ(api::passwords_private::PASSWORD_CHECK_STATE_RUNNING, status.state);
  EXPECT_EQ(0, *status.already_processed);
  EXPECT_EQ(1, *status.remaining_in_queue);

  // Make sure that even though the store is emptied after starting a check we
  // don't report a negative number for already processed.
  store().RemoveLogin(MakeSavedPassword(kExampleCom, kUsername1));
  RunUntilIdle();

  status = delegate().GetPasswordCheckStatus();
  EXPECT_EQ(api::passwords_private::PASSWORD_CHECK_STATE_RUNNING, status.state);
  EXPECT_EQ(0, *status.already_processed);
  EXPECT_EQ(1, *status.remaining_in_queue);
}

// Verifies that the case where the check is canceled is reported correctly.
TEST_F(PasswordCheckDelegateTest, GetPasswordCheckStatusCanceled) {
  identity_test_env().MakeAccountAvailable(kTestEmail);
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1));
  RunUntilIdle();

  delegate().StartPasswordCheck();
  EXPECT_EQ(api::passwords_private::PASSWORD_CHECK_STATE_RUNNING,
            delegate().GetPasswordCheckStatus().state);

  delegate().StopPasswordCheck();
  EXPECT_EQ(api::passwords_private::PASSWORD_CHECK_STATE_CANCELED,
            delegate().GetPasswordCheckStatus().state);
}

// Verifies that the case where the user is offline is reported correctly.
TEST_F(PasswordCheckDelegateTest, GetPasswordCheckStatusOffline) {
  identity_test_env().MakeAccountAvailable(kTestEmail);
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1));
  RunUntilIdle();

  delegate().StartPasswordCheck();
  identity_test_env().WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError::FromConnectionError(net::ERR_TIMED_OUT));

  EXPECT_EQ(api::passwords_private::PASSWORD_CHECK_STATE_OFFLINE,
            delegate().GetPasswordCheckStatus().state);
}

// Verifies that the case where the user hits another error (e.g. invalid
// credentials) is reported correctly.
TEST_F(PasswordCheckDelegateTest, GetPasswordCheckStatusOther) {
  identity_test_env().MakeAccountAvailable(kTestEmail);
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1));
  RunUntilIdle();

  delegate().StartPasswordCheck();
  identity_test_env().WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER));

  EXPECT_EQ(api::passwords_private::PASSWORD_CHECK_STATE_SIGNED_OUT,
            delegate().GetPasswordCheckStatus().state);
}

// Test that a change to the saved passwords notifies observers.
TEST_F(PasswordCheckDelegateTest,
       NotifyPasswordCheckStatusChangedAfterPasswordChange) {
  const char* const kEventName =
      api::passwords_private::OnPasswordCheckStatusChanged::kEventName;

  // Verify that the event was not fired during construction.
  EXPECT_FALSE(base::Contains(event_router_observer().events(), kEventName));

  // Verify that the event gets fired once the saved passwords provider is
  // initialized.
  RunUntilIdle();
  EXPECT_EQ(events::PASSWORDS_PRIVATE_ON_PASSWORD_CHECK_STATUS_CHANGED,
            event_router_observer().events().at(kEventName)->histogram_value);
  event_router_observer().ClearEvents();

  // Verify that a subsequent call to AddLogin() results in the expected event.
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1));
  RunUntilIdle();
  EXPECT_EQ(events::PASSWORDS_PRIVATE_ON_PASSWORD_CHECK_STATUS_CHANGED,
            event_router_observer().events().at(kEventName)->histogram_value);
}

// Test that a change to the bulk leak check notifies observers.
TEST_F(PasswordCheckDelegateTest,
       NotifyPasswordCheckStatusChangedAfterStateChange) {
  const char* const kEventName =
      api::passwords_private::OnPasswordCheckStatusChanged::kEventName;

  // Verify that the event was not fired during construction.
  EXPECT_FALSE(base::Contains(event_router_observer().events(), kEventName));

  // Verify that the event gets fired once the saved passwords provider is
  // initialized.
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1));
  RunUntilIdle();
  EXPECT_EQ(events::PASSWORDS_PRIVATE_ON_PASSWORD_CHECK_STATUS_CHANGED,
            event_router_observer().events().at(kEventName)->histogram_value);
  event_router_observer().ClearEvents();

  // Verify that a subsequent call to StartPasswordCheck() results in the
  // expected event.
  delegate().StartPasswordCheck();
  EXPECT_EQ(events::PASSWORDS_PRIVATE_ON_PASSWORD_CHECK_STATUS_CHANGED,
            event_router_observer().events().at(kEventName)->histogram_value);
}

// Checks that the default kLastTimePasswordCheckCompleted pref value is
// treated as no completed run yet.
TEST_F(PasswordCheckDelegateTest, LastTimePasswordCheckCompletedNotSet) {
  PasswordCheckStatus status = delegate().GetPasswordCheckStatus();
  EXPECT_THAT(status.elapsed_time_since_last_check, IsNull());
}

// Checks that a non-default kLastTimePasswordCheckCompleted pref value is
// treated as a completed run, and formatted accordingly.
TEST_F(PasswordCheckDelegateTest, LastTimePasswordCheckCompletedIsSet) {
  profile().GetPrefs()->SetDouble(
      kLastTimePasswordCheckCompleted,
      (base::Time::Now() - base::TimeDelta::FromMinutes(5)).ToDoubleT());

  PasswordCheckStatus status = delegate().GetPasswordCheckStatus();
  EXPECT_THAT(status.elapsed_time_since_last_check,
              Pointee(std::string("5 minutes ago")));
}

// Checks that a transition into the idle state after starting a check results
// in resetting the kLastTimePasswordCheckCompleted pref to the current time.
TEST_F(PasswordCheckDelegateTest, LastTimePasswordCheckCompletedReset) {
  delegate().StartPasswordCheck();
  RunUntilIdle();

  service()->set_state_and_notify(BulkLeakCheckService::State::kIdle);
  PasswordCheckStatus status = delegate().GetPasswordCheckStatus();
  EXPECT_THAT(status.elapsed_time_since_last_check,
              Pointee(std::string("Just now")));
}

// Checks that processing a credential by the leak check updates the progress
// correctly and raises the expected event.
TEST_F(PasswordCheckDelegateTest, OnCredentialDoneUpdatesProgress) {
  const char* const kEventName =
      api::passwords_private::OnPasswordCheckStatusChanged::kEventName;

  identity_test_env().MakeAccountAvailable(kTestEmail);
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1, kPassword1));
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername2, kPassword2));
  store().AddLogin(MakeSavedPassword(kExampleOrg, kUsername1, kPassword1));
  store().AddLogin(MakeSavedPassword(kExampleOrg, kUsername2, kPassword2));
  RunUntilIdle();

  const auto event_iter = event_router_observer().events().find(kEventName);
  delegate().StartPasswordCheck();
  EXPECT_EQ(events::PASSWORDS_PRIVATE_ON_PASSWORD_CHECK_STATUS_CHANGED,
            event_iter->second->histogram_value);
  auto status = PasswordCheckStatus::FromValue(
      event_iter->second->event_args->GetList().front());
  EXPECT_EQ(api::passwords_private::PASSWORD_CHECK_STATE_RUNNING,
            status->state);
  EXPECT_EQ(0, *status->already_processed);
  EXPECT_EQ(4, *status->remaining_in_queue);

  static_cast<BulkLeakCheckDelegateInterface*>(service())->OnFinishedCredential(
      LeakCheckCredential(base::ASCIIToUTF16(kUsername1),
                          base::ASCIIToUTF16(kPassword1)),
      IsLeaked(false));

  status = PasswordCheckStatus::FromValue(
      event_iter->second->event_args->GetList().front());
  EXPECT_EQ(api::passwords_private::PASSWORD_CHECK_STATE_RUNNING,
            status->state);
  EXPECT_EQ(2, *status->already_processed);
  EXPECT_EQ(2, *status->remaining_in_queue);

  static_cast<BulkLeakCheckDelegateInterface*>(service())->OnFinishedCredential(
      LeakCheckCredential(base::ASCIIToUTF16(kUsername2),
                          base::ASCIIToUTF16(kPassword2)),
      IsLeaked(false));

  status = PasswordCheckStatus::FromValue(
      event_iter->second->event_args->GetList().front());
  EXPECT_EQ(api::passwords_private::PASSWORD_CHECK_STATE_RUNNING,
            status->state);
  EXPECT_EQ(4, *status->already_processed);
  EXPECT_EQ(0, *status->remaining_in_queue);
}

// Tests that StopPasswordCheck() invokes pending callbacks before
// initialization finishes.
TEST_F(PasswordCheckDelegateTest,
       StopPasswordCheckRespondsCancelsBeforeInitialization) {
  MockStartPasswordCheckCallback callback1;
  MockStartPasswordCheckCallback callback2;
  delegate().StartPasswordCheck(callback1.Get());
  delegate().StartPasswordCheck(callback2.Get());

  EXPECT_CALL(callback1, Run(BulkLeakCheckService::State::kIdle));
  EXPECT_CALL(callback2, Run(BulkLeakCheckService::State::kIdle));
  delegate().StopPasswordCheck();

  Mock::VerifyAndClearExpectations(&callback1);
  Mock::VerifyAndClearExpectations(&callback2);
  RunUntilIdle();
}

// Tests that pending callbacks get invoked once initialization finishes.
TEST_F(PasswordCheckDelegateTest,
       StartPasswordCheckRunsCallbacksAfterInitialization) {
  identity_test_env().MakeAccountAvailable(kTestEmail);
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1));
  RunUntilIdle();

  MockStartPasswordCheckCallback callback1;
  MockStartPasswordCheckCallback callback2;
  EXPECT_CALL(callback1, Run(BulkLeakCheckService::State::kRunning));
  EXPECT_CALL(callback2, Run(BulkLeakCheckService::State::kRunning));

  // Use a local delegate instead of |delegate()| so that the Password Store can
  // be set-up prior to constructing the object.
  PasswordCheckDelegate delegate(&profile());
  delegate.StartPasswordCheck(callback1.Get());
  delegate.StartPasswordCheck(callback2.Get());
  RunUntilIdle();
}

}  // namespace extensions
