// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_generation_state.h"

#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/simple_test_clock.h"
#include "components/password_manager/core/browser/form_saver_impl.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

using autofill::PasswordForm;
using base::ASCIIToUTF16;
using testing::_;

constexpr char kURL[] = "https://example.in/login";
constexpr char kSubdomainURL[] = "https://m.example.in/login";
constexpr time_t kTime = 123456789;
constexpr time_t kAnotherTime = 987654321;

// Creates a dummy saved credential.
PasswordForm CreateSaved() {
  PasswordForm form;
  form.origin = GURL(kURL);
  form.signon_realm = form.origin.spec();
  form.action = GURL("https://login.example.org");
  form.username_value = ASCIIToUTF16("old_username");
  form.password_value = ASCIIToUTF16("12345");
  return form;
}

// Creates a dummy saved PSL credential.
PasswordForm CreateSavedPSL() {
  PasswordForm form;
  form.origin = GURL(kSubdomainURL);
  form.signon_realm = form.origin.spec();
  form.action = GURL("https://login.example.org");
  form.username_value = ASCIIToUTF16("old_username2");
  form.password_value = ASCIIToUTF16("passw0rd");
  form.is_public_suffix_match = true;
  return form;
}

// Creates a dummy generated password.
PasswordForm CreateGenerated() {
  PasswordForm form;
  form.origin = GURL(kURL);
  form.signon_realm = form.origin.spec();
  form.action = GURL("https://signup.example.org");
  form.username_value = ASCIIToUTF16("MyName");
  form.password_value = ASCIIToUTF16("Strong password");
  form.preferred = true;
  form.type = autofill::PasswordForm::Type::kGenerated;
  return form;
}

MATCHER_P(FormHasUniqueKey, key, "") {
  return ArePasswordFormUniqueKeysEqual(arg, key);
}

class PasswordGenerationStateTest : public testing::Test {
 public:
  PasswordGenerationStateTest();
  ~PasswordGenerationStateTest() override;

  MockPasswordStore& store() { return *mock_store_; }
  PasswordGenerationState& state() { return generation_state_; }
  FormSaverImpl& form_saver() { return form_saver_; }

 private:
  // For the MockPasswordStore.
  base::test::ScopedTaskEnvironment task_environment_;
  scoped_refptr<MockPasswordStore> mock_store_;
  // Test with the real form saver for better robustness.
  FormSaverImpl form_saver_;
  PasswordGenerationState generation_state_;
};

PasswordGenerationStateTest::PasswordGenerationStateTest()
    : mock_store_(new testing::StrictMock<MockPasswordStore>()),
      form_saver_(mock_store_.get()),
      generation_state_(&form_saver_) {
  auto clock = std::make_unique<base::SimpleTestClock>();
  clock->SetNow(base::Time::FromTimeT(kTime));
  generation_state_.set_clock(std::move(clock));
}

PasswordGenerationStateTest::~PasswordGenerationStateTest() {
  mock_store_->ShutdownOnUIThread();
}

// Check that presaving a password for the first time results in adding it.
TEST_F(PasswordGenerationStateTest, PresaveGeneratedPassword_New) {
  const PasswordForm generated = CreateGenerated();
  PasswordForm generated_with_date = generated;
  generated_with_date.date_created = base::Time::FromTimeT(kTime);

  EXPECT_CALL(store(), AddLogin(generated_with_date));
  state().PresaveGeneratedPassword(generated);
  EXPECT_TRUE(state().HasGeneratedPassword());
}

// Check that presaving a password for the second time results in updating it.
TEST_F(PasswordGenerationStateTest, PresaveGeneratedPassword_Replace) {
  PasswordForm generated = CreateGenerated();
  PasswordForm generated_with_date = generated;
  generated_with_date.date_created = base::Time::FromTimeT(kTime);

  EXPECT_CALL(store(), AddLogin(generated_with_date));
  state().PresaveGeneratedPassword(generated);

  PasswordForm generated_updated = generated;
  generated_updated.password_value = ASCIIToUTF16("newgenpwd");
  generated_with_date = generated_updated;
  generated_with_date.date_created = base::Time::FromTimeT(kTime);
  EXPECT_CALL(store(), UpdateLoginWithPrimaryKey(generated_with_date,
                                                 FormHasUniqueKey(generated)));
  state().PresaveGeneratedPassword(generated_updated);
  EXPECT_TRUE(state().HasGeneratedPassword());
}

// Check that presaving a password for the third time results in updating it.
TEST_F(PasswordGenerationStateTest, PresaveGeneratedPassword_ReplaceTwice) {
  PasswordForm generated = CreateGenerated();
  PasswordForm generated_with_date = generated;
  generated_with_date.date_created = base::Time::FromTimeT(kTime);

  EXPECT_CALL(store(), AddLogin(generated_with_date));
  state().PresaveGeneratedPassword(generated);

  PasswordForm generated_updated = generated;
  generated_updated.password_value = ASCIIToUTF16("newgenpwd");
  generated_with_date = generated_updated;
  generated_with_date.date_created = base::Time::FromTimeT(kTime);
  EXPECT_CALL(store(), UpdateLoginWithPrimaryKey(generated_with_date,
                                                 FormHasUniqueKey(generated)));
  state().PresaveGeneratedPassword(generated_updated);

  generated = generated_updated;
  generated_updated.password_value = ASCIIToUTF16("newgenpwd2");
  generated_updated.username_value = ASCIIToUTF16("newusername");
  generated_with_date = generated_updated;
  generated_with_date.date_created = base::Time::FromTimeT(kTime);
  EXPECT_CALL(store(), UpdateLoginWithPrimaryKey(generated_with_date,
                                                 FormHasUniqueKey(generated)));
  state().PresaveGeneratedPassword(generated_updated);
  EXPECT_TRUE(state().HasGeneratedPassword());
}

// Check that presaving a password followed by a call to save a pending
// credential (as new) results in replacing the presaved password with the
// pending one.
TEST_F(PasswordGenerationStateTest, PresaveGeneratedPassword_ThenSaveAsNew) {
  const PasswordForm generated = CreateGenerated();

  EXPECT_CALL(store(), AddLogin(_));
  state().PresaveGeneratedPassword(generated);

  // User edits after submission.
  PasswordForm pending = generated;
  pending.password_value = ASCIIToUTF16("edited_password");
  pending.username_value = ASCIIToUTF16("edited_username");
  PasswordForm generated_with_date = pending;
  generated_with_date.date_created = base::Time::FromTimeT(kTime);
  EXPECT_CALL(store(), UpdateLoginWithPrimaryKey(generated_with_date,
                                                 FormHasUniqueKey(generated)));
  state().CommitGeneratedPassword(pending, {} /* matches */,
                                  base::string16() /* old_password */);
  EXPECT_TRUE(state().HasGeneratedPassword());
}

// Check that presaving a password followed by a call to save a pending
// credential (as update) results in replacing the presaved password with the
// pending one.
TEST_F(PasswordGenerationStateTest, PresaveGeneratedPassword_ThenUpdate) {
  PasswordForm generated = CreateGenerated();

  EXPECT_CALL(store(), AddLogin(_));
  state().PresaveGeneratedPassword(generated);

  PasswordForm related_password = CreateSaved();
  related_password.username_value = ASCIIToUTF16("username");
  related_password.username_element = ASCIIToUTF16("username_field");
  related_password.password_value = ASCIIToUTF16("old password");

  PasswordForm related_psl_password = CreateSavedPSL();
  related_psl_password.username_value = ASCIIToUTF16("username");
  related_psl_password.password_value = ASCIIToUTF16("old password");

  PasswordForm unrelated_password = CreateSaved();
  unrelated_password.preferred = true;
  unrelated_password.username_value = ASCIIToUTF16("another username");
  unrelated_password.password_value = ASCIIToUTF16("some password");

  PasswordForm unrelated_psl_password = CreateSavedPSL();
  unrelated_psl_password.preferred = true;
  unrelated_psl_password.username_value = ASCIIToUTF16("another username");
  unrelated_psl_password.password_value = ASCIIToUTF16("some password");

  generated.username_value = ASCIIToUTF16("username");
  PasswordForm generated_with_date = generated;
  generated_with_date.date_created = base::Time::FromTimeT(kTime);

  EXPECT_CALL(store(),
              UpdateLoginWithPrimaryKey(generated_with_date,
                                        FormHasUniqueKey(CreateGenerated())));

  PasswordForm related_password_expected = related_password;
  related_password_expected.password_value = generated.password_value;
  EXPECT_CALL(store(), UpdateLogin(related_password_expected));

  PasswordForm related_psl_password_expected = related_psl_password;
  related_psl_password_expected.password_value = generated.password_value;
  EXPECT_CALL(store(), UpdateLogin(related_psl_password_expected));

  PasswordForm unrelated_password_expected = unrelated_password;
  unrelated_password_expected.preferred = false;
  EXPECT_CALL(store(), UpdateLogin(unrelated_password_expected));

  state().CommitGeneratedPassword(
      generated,
      {&related_password, &related_psl_password, &unrelated_password,
       &unrelated_psl_password} /* matches */,
      ASCIIToUTF16("old password"));
  EXPECT_TRUE(state().HasGeneratedPassword());
}

// Check that removing a presaved password removes the presaved password.
TEST_F(PasswordGenerationStateTest, PasswordNoLongerGenerated) {
  PasswordForm generated = CreateGenerated();

  EXPECT_CALL(store(), AddLogin(_));
  state().PresaveGeneratedPassword(generated);

  generated.date_created = base::Time::FromTimeT(kTime);
  EXPECT_CALL(store(), RemoveLogin(generated));
  state().PasswordNoLongerGenerated();
  EXPECT_FALSE(state().HasGeneratedPassword());
}

// Check that removing the presaved password and then presaving again results in
// adding the second presaved password as new.
TEST_F(PasswordGenerationStateTest, PasswordNoLongerGenerated_AndPresaveAgain) {
  PasswordForm generated = CreateGenerated();
  PasswordForm generated_with_date = generated;
  generated_with_date.date_created = base::Time::FromTimeT(kTime);

  EXPECT_CALL(store(), AddLogin(generated_with_date));
  state().PresaveGeneratedPassword(generated);

  EXPECT_CALL(store(), RemoveLogin(generated_with_date));
  state().PasswordNoLongerGenerated();

  generated.username_value = ASCIIToUTF16("newgenusername");
  generated.password_value = ASCIIToUTF16("newgenpwd");
  generated_with_date = generated;
  generated_with_date.date_created = base::Time::FromTimeT(kTime);
  EXPECT_CALL(store(), AddLogin(generated_with_date));
  state().PresaveGeneratedPassword(generated);
  EXPECT_TRUE(state().HasGeneratedPassword());
}

// Check that presaving a password once in original and then once in clone
// results in the clone calling update, not a fresh save.
TEST_F(PasswordGenerationStateTest, PresaveGeneratedPassword_CloneUpdates) {
  PasswordForm generated = CreateGenerated();
  PasswordForm generated_with_date = generated;
  generated_with_date.date_created = base::Time::FromTimeT(kTime);

  EXPECT_CALL(store(), AddLogin(generated_with_date));
  state().PresaveGeneratedPassword(generated);

  std::unique_ptr<FormSaver> cloned_saver = form_saver().Clone();
  std::unique_ptr<PasswordGenerationState> cloned_state =
      state().Clone(cloned_saver.get());
  std::unique_ptr<base::SimpleTestClock> clock(new base::SimpleTestClock);
  clock->SetNow(base::Time::FromTimeT(kAnotherTime));
  cloned_state->set_clock(std::move(clock));

  EXPECT_TRUE(cloned_state->HasGeneratedPassword());
  PasswordForm generated_updated = generated;
  generated_updated.username_value = ASCIIToUTF16("newname");
  generated_with_date = generated_updated;
  generated_with_date.date_created = base::Time::FromTimeT(kAnotherTime);
  EXPECT_CALL(store(), UpdateLoginWithPrimaryKey(generated_with_date,
                                                 FormHasUniqueKey(generated)));
  cloned_state->PresaveGeneratedPassword(generated_updated);
  EXPECT_TRUE(cloned_state->HasGeneratedPassword());
}

// Check that a clone can still work after the original is destroyed.
TEST_F(PasswordGenerationStateTest, PresaveGeneratedPassword_CloneSurvives) {
  auto original = std::make_unique<PasswordGenerationState>(&form_saver());
  const PasswordForm generated = CreateGenerated();

  EXPECT_CALL(store(), AddLogin(_));
  original->PresaveGeneratedPassword(generated);

  std::unique_ptr<FormSaver> cloned_saver = form_saver().Clone();
  std::unique_ptr<PasswordGenerationState> cloned_state =
      original->Clone(cloned_saver.get());
  original.reset();
  EXPECT_CALL(store(), UpdateLoginWithPrimaryKey(_, _));
  cloned_state->PresaveGeneratedPassword(generated);
}

}  // namespace
}  // namespace password_manager
