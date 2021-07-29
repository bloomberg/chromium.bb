// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/passwords/ios_chrome_password_check_manager.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "components/password_manager/core/browser/bulk_leak_check_service.h"
#include "components/password_manager/core/browser/mock_bulk_leak_check_service.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/passwords/ios_chrome_bulk_leak_check_service_factory.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_check_manager_factory.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
constexpr char kExampleCom[] = "https://example.com";

constexpr char kUsername1[] = "alice";
constexpr char16_t kUsername116[] = u"alice";
constexpr char kUsername2[] = "bob";

constexpr char kPassword1[] = "s3cre3t";
constexpr char16_t kPassword116[] = u"s3cre3t";
constexpr char kPassword2[] = "bett3r_S3cre3t";
constexpr char16_t kPassword216[] = u"bett3r_S3cre3t";

using password_manager::PasswordForm;
using password_manager::BulkLeakCheckServiceInterface;
using password_manager::CredentialWithPassword;
using password_manager::MockBulkLeakCheckService;
using password_manager::InsecureCredential;
using password_manager::InsecureType;
using password_manager::InsecureCredentialTypeFlags;
using password_manager::IsLeaked;
using password_manager::LeakCheckCredential;
using password_manager::TestPasswordStore;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::StrictMock;
using ::testing::Pair;

using InsecureCredentialsView =
    password_manager::InsecureCredentialsManager::CredentialsView;

struct MockPasswordCheckManagerObserver
    : IOSChromePasswordCheckManager::Observer {
  MOCK_METHOD(void,
              CompromisedCredentialsChanged,
              (InsecureCredentialsView),
              (override));
  MOCK_METHOD(void,
              PasswordCheckStatusChanged,
              (PasswordCheckState),
              (override));
};

scoped_refptr<TestPasswordStore> CreateAndUseTestPasswordStore(
    ChromeBrowserState* _browserState) {
  return base::WrapRefCounted(static_cast<password_manager::TestPasswordStore*>(
      IOSChromePasswordStoreFactory::GetInstance()
          ->SetTestingFactoryAndUse(
              _browserState,
              base::BindRepeating(&password_manager::BuildPasswordStore<
                                  web::BrowserState, TestPasswordStore>))
          .get()));
}

MockBulkLeakCheckService* CreateAndUseBulkLeakCheckService(
    ChromeBrowserState* _browserState) {
  return static_cast<MockBulkLeakCheckService*>(
      IOSChromeBulkLeakCheckServiceFactory::GetInstance()
          ->SetTestingFactoryAndUse(
              _browserState, base::BindLambdaForTesting([](web::BrowserState*) {
                return std::unique_ptr<KeyedService>(
                    std::make_unique<MockBulkLeakCheckService>());
              })));
}

InsecureCredential MakeInsecureCredential(
    base::StringPiece signon_realm,
    base::StringPiece16 username,
    base::TimeDelta time_since_creation = base::TimeDelta(),
    InsecureType compromise_type = InsecureType::kLeaked) {
  return InsecureCredential(std::string(signon_realm), std::u16string(username),
                            base::Time::Now() - time_since_creation,
                            compromise_type, password_manager::IsMuted(false));
}

PasswordForm MakeSavedPassword(
    base::StringPiece signon_realm,
    base::StringPiece16 username,
    base::StringPiece16 password = kPassword116,
    base::StringPiece16 username_element = base::StringPiece16()) {
  PasswordForm form;
  form.signon_realm = std::string(signon_realm);
  form.username_value = std::u16string(username);
  form.password_value = std::u16string(password);
  form.username_element = std::u16string(username_element);
  form.in_store = PasswordForm::Store::kProfileStore;
  // TODO(crbug.com/1223022): Once all places that operate changes on forms
  // via UpdateLogin properly set |password_issues|, setting them to an empty
  // map should be part of the default constructor.
  form.password_issues =
      base::flat_map<InsecureType, password_manager::InsecurityMetadata>();
  return form;
}

// Creates matcher for a given compromised credential
auto ExpectCompromisedCredential(const std::string& signon_realm,
                                 const base::StringPiece16& username,
                                 const base::StringPiece16& password,
                                 base::TimeDelta elapsed_time_since_compromise,
                                 InsecureCredentialTypeFlags insecure_type) {
  return AllOf(Field(&CredentialWithPassword::signon_realm, signon_realm),
               Field(&CredentialWithPassword::username, username),
               Field(&CredentialWithPassword::password, password),
               Field(&CredentialWithPassword::create_time,
                     (base::Time::Now() - elapsed_time_since_compromise)),
               Field(&CredentialWithPassword::insecure_type, insecure_type));
}

// Returns vector of pairs with username, password only.
std::vector<std::pair<std::string, std::string>> GetUsernamesAndPasswords(
    const std::vector<password_manager::PasswordForm>& forms) {
  std::vector<std::pair<std::string, std::string>> result;
  result.reserve(forms.size());
  for (const auto& form : forms) {
    result.emplace_back(base::UTF16ToUTF8(form.username_value),
                        base::UTF16ToUTF8(form.password_value));
  }

  return result;
}

class IOSChromePasswordCheckManagerTest : public PlatformTest {
 public:
  IOSChromePasswordCheckManagerTest()
      : browser_state_(TestChromeBrowserState::Builder().Build()),
        bulk_leak_check_service_(
            CreateAndUseBulkLeakCheckService(browser_state_.get())),
        store_(CreateAndUseTestPasswordStore(browser_state_.get())) {
    manager_ = IOSChromePasswordCheckManagerFactory::GetForBrowserState(
        browser_state_.get());
  }

  void RunUntilIdle() { task_env_.RunUntilIdle(); }

  void FastForwardBy(base::TimeDelta time) { task_env_.FastForwardBy(time); }

  ChromeBrowserState* browser_state() { return browser_state_.get(); }
  TestPasswordStore& store() { return *store_; }
  MockBulkLeakCheckService* service() { return bulk_leak_check_service_; }
  IOSChromePasswordCheckManager& manager() { return *manager_; }

 private:
  web::WebTaskEnvironment task_env_{
      web::WebTaskEnvironment::Options::DEFAULT,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<ChromeBrowserState> browser_state_;
  MockBulkLeakCheckService* bulk_leak_check_service_;
  scoped_refptr<TestPasswordStore> store_;
  scoped_refptr<IOSChromePasswordCheckManager> manager_;
};
}  // namespace

// Sets up the password store with a password and compromised
// credential. Verifies that the result is matching expectation.
TEST_F(IOSChromePasswordCheckManagerTest, GetCompromisedCredentials) {
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername116));

  store().AddInsecureCredential(MakeInsecureCredential(
      kExampleCom, kUsername116, base::TimeDelta::FromMinutes(1),
      InsecureType::kLeaked));
  RunUntilIdle();
  EXPECT_THAT(manager().GetCompromisedCredentials(),
              ElementsAre(ExpectCompromisedCredential(
                  kExampleCom, kUsername116, kPassword116,
                  base::TimeDelta::FromMinutes(1),
                  InsecureCredentialTypeFlags::kCredentialLeaked)));
}

// Test that we don't create an entry in the password store if IsLeaked is
// false.
TEST_F(IOSChromePasswordCheckManagerTest, NoLeakedFound) {
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername116, kPassword116));
  RunUntilIdle();

  static_cast<BulkLeakCheckServiceInterface::Observer*>(&manager())
      ->OnCredentialDone(LeakCheckCredential(kUsername116, kPassword116),
                         IsLeaked(false));
  RunUntilIdle();

  EXPECT_THAT(manager().GetCompromisedCredentials(), IsEmpty());
}

// Test that a found leak creates a compromised credential in the password
// store.
TEST_F(IOSChromePasswordCheckManagerTest, OnLeakFoundCreatesCredential) {
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername116, kPassword116));
  RunUntilIdle();

  static_cast<BulkLeakCheckServiceInterface::Observer*>(&manager())
      ->OnCredentialDone(LeakCheckCredential(kUsername116, kPassword116),
                         IsLeaked(true));
  RunUntilIdle();

  EXPECT_THAT(manager().GetCompromisedCredentials(),
              ElementsAre(ExpectCompromisedCredential(
                  kExampleCom, kUsername116, kPassword116,
                  base::TimeDelta::FromMinutes(0),
                  InsecureCredentialTypeFlags::kCredentialLeaked)));
}

// Verifies that the case where the user has no saved passwords is reported
// correctly.
TEST_F(IOSChromePasswordCheckManagerTest, GetPasswordCheckStatusNoPasswords) {
  EXPECT_EQ(PasswordCheckState::kNoPasswords,
            manager().GetPasswordCheckState());
}

// Verifies that the case where the user has saved passwords is reported
// correctly.
TEST_F(IOSChromePasswordCheckManagerTest, GetPasswordCheckStatusIdle) {
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername116));
  RunUntilIdle();

  EXPECT_EQ(PasswordCheckState::kIdle, manager().GetPasswordCheckState());
}

// Checks that the default kLastTimePasswordCheckCompleted pref value is
// treated as no completed run yet.
TEST_F(IOSChromePasswordCheckManagerTest,
       LastTimePasswordCheckCompletedNotSet) {
  EXPECT_EQ(base::Time(), manager().GetLastPasswordCheckTime());
}

// Checks that a transition into the idle state after starting a check results
// in resetting the kLastTimePasswordCheckCompleted pref to the current time.
TEST_F(IOSChromePasswordCheckManagerTest, LastTimePasswordCheckCompletedReset) {
  manager().StartPasswordCheck();
  RunUntilIdle();

  static_cast<BulkLeakCheckServiceInterface::Observer*>(&manager())
      ->OnStateChanged(BulkLeakCheckServiceInterface::State::kIdle);

  EXPECT_THAT(base::Time::Now(), manager().GetLastPasswordCheckTime());
}

// Tests whether adding and removing an observer works as expected.
TEST_F(IOSChromePasswordCheckManagerTest,
       NotifyObserversAboutCompromisedCredentialChanges) {
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername116));

  RunUntilIdle();
  StrictMock<MockPasswordCheckManagerObserver> observer;
  manager().AddObserver(&observer);

  // Adding a compromised credential should notify observers.
  EXPECT_CALL(observer, PasswordCheckStatusChanged);
  EXPECT_CALL(
      observer,
      CompromisedCredentialsChanged(ElementsAre(ExpectCompromisedCredential(
          kExampleCom, kUsername116, kPassword116,
          base::TimeDelta::FromMinutes(1),
          InsecureCredentialTypeFlags::kCredentialLeaked))));
  store().AddInsecureCredential(MakeInsecureCredential(
      kExampleCom, kUsername116, base::TimeDelta::FromMinutes(1)));
  RunUntilIdle();

  // After an observer is removed it should no longer receive notifications.
  manager().RemoveObserver(&observer);
  EXPECT_CALL(observer, CompromisedCredentialsChanged).Times(0);
  store().AddInsecureCredential(MakeInsecureCredential(
      kExampleCom, kUsername116, base::TimeDelta::FromMinutes(1),
      InsecureType::kPhished));
  RunUntilIdle();
}

// Tests whether adding and removing an observer works as expected.
TEST_F(IOSChromePasswordCheckManagerTest, NotifyObserversAboutStateChanges) {
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername116));
  RunUntilIdle();
  StrictMock<MockPasswordCheckManagerObserver> observer;
  manager().AddObserver(&observer);

  EXPECT_CALL(observer, PasswordCheckStatusChanged(PasswordCheckState::kIdle));
  static_cast<BulkLeakCheckServiceInterface::Observer*>(&manager())
      ->OnStateChanged(BulkLeakCheckServiceInterface::State::kIdle);

  RunUntilIdle();

  EXPECT_EQ(PasswordCheckState::kIdle, manager().GetPasswordCheckState());

  // After an observer is removed it should no longer receive notifications.
  manager().RemoveObserver(&observer);
  EXPECT_CALL(observer, PasswordCheckStatusChanged).Times(0);
  static_cast<BulkLeakCheckServiceInterface::Observer*>(&manager())
      ->OnStateChanged(BulkLeakCheckServiceInterface::State::kRunning);
  RunUntilIdle();
}

// Tests password deleted.
TEST_F(IOSChromePasswordCheckManagerTest, DeletePassword) {
  PasswordForm form = MakeSavedPassword(kExampleCom, kUsername116);
  store().AddLogin(form);
  RunUntilIdle();

  store().AddInsecureCredential(MakeInsecureCredential(
      kExampleCom, kUsername116, base::TimeDelta::FromMinutes(1),
      InsecureType::kLeaked));
  RunUntilIdle();
  EXPECT_THAT(manager().GetCompromisedCredentials(),
              ElementsAre(ExpectCompromisedCredential(
                  kExampleCom, kUsername116, kPassword116,
                  base::TimeDelta::FromMinutes(1),
                  InsecureCredentialTypeFlags::kCredentialLeaked)));

  manager().DeleteCompromisedPasswordForm(form);
  RunUntilIdle();

  EXPECT_THAT(manager().GetCompromisedCredentials(), IsEmpty());
}

// Tests duplicated passwords deleted.
TEST_F(IOSChromePasswordCheckManagerTest, DeleteDuplicatedPasswords) {
  std::vector<PasswordForm> passwords = {
      MakeSavedPassword(kExampleCom, kUsername116, kPassword116, u"element_1"),
      MakeSavedPassword(kExampleCom, kUsername116, kPassword116, u"element_2")};

  store().AddLogin(passwords[0]);
  store().AddLogin(passwords[1]);
  RunUntilIdle();
  EXPECT_EQ(2u, store().stored_passwords().at(kExampleCom).size());

  manager().DeletePasswordForm(passwords[0]);
  RunUntilIdle();
  EXPECT_TRUE(store().stored_passwords().at(kExampleCom).empty());
}

// Tests password value is updated properly.
TEST_F(IOSChromePasswordCheckManagerTest, EditPassword) {
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername116, kPassword116));
  RunUntilIdle();

  EXPECT_TRUE(manager().EditPasswordForm(
      store().stored_passwords().at(kExampleCom).at(0), kUsername1,
      kPassword2));
  RunUntilIdle();

  EXPECT_THAT(
      GetUsernamesAndPasswords(store().stored_passwords().at(kExampleCom)),
      ElementsAre(Pair(kUsername1, kPassword2)));
}

// Tests username value is updated properly.
TEST_F(IOSChromePasswordCheckManagerTest, EditUsername) {
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername116, kPassword116));
  RunUntilIdle();

  EXPECT_TRUE(manager().EditPasswordForm(
      store().stored_passwords().at(kExampleCom).at(0), kUsername2,
      kPassword1));
  RunUntilIdle();

  EXPECT_THAT(
      GetUsernamesAndPasswords(store().stored_passwords().at(kExampleCom)),
      ElementsAre(Pair(kUsername2, kPassword1)));
}

// Tests username and password values are updated properly.
TEST_F(IOSChromePasswordCheckManagerTest, EditUsernameAndPassword) {
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername116, kPassword116));
  RunUntilIdle();

  EXPECT_TRUE(manager().EditPasswordForm(
      store().stored_passwords().at(kExampleCom).at(0), kUsername2,
      kPassword2));
  RunUntilIdle();

  EXPECT_THAT(
      GetUsernamesAndPasswords(store().stored_passwords().at(kExampleCom)),
      ElementsAre(Pair(kUsername2, kPassword2)));
}

// Tests compromised password value is updated properly.
TEST_F(IOSChromePasswordCheckManagerTest, EditCompromisedPassword) {
  PasswordForm form = MakeSavedPassword(kExampleCom, kUsername116);
  store().AddLogin(form);
  RunUntilIdle();

  store().AddInsecureCredential(MakeInsecureCredential(
      kExampleCom, kUsername116, base::TimeDelta::FromMinutes(1),
      InsecureType::kLeaked));
  RunUntilIdle();

  manager().EditCompromisedPasswordForm(form, kPassword2);
  RunUntilIdle();

  EXPECT_EQ(kPassword216,
            store().stored_passwords().at(kExampleCom).at(0).password_value);
}

// Tests expected delay is being added.
TEST_F(IOSChromePasswordCheckManagerTest, CheckFinishedWithDelay) {
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername116));

  RunUntilIdle();
  StrictMock<MockPasswordCheckManagerObserver> observer;
  manager().AddObserver(&observer);
  manager().StartPasswordCheck();
  RunUntilIdle();

  EXPECT_CALL(observer, PasswordCheckStatusChanged(PasswordCheckState::kIdle))
      .Times(0);
  static_cast<BulkLeakCheckServiceInterface::Observer*>(&manager())
      ->OnStateChanged(BulkLeakCheckServiceInterface::State::kIdle);
  FastForwardBy(base::TimeDelta::FromSeconds(1));

  EXPECT_CALL(observer, PasswordCheckStatusChanged(PasswordCheckState::kIdle))
      .Times(0);
  FastForwardBy(base::TimeDelta::FromSeconds(1));

  EXPECT_CALL(observer, PasswordCheckStatusChanged(PasswordCheckState::kIdle))
      .Times(1);
  FastForwardBy(base::TimeDelta::FromSeconds(1));
  manager().RemoveObserver(&observer);
}
