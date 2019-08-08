// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_generation_state.h"

#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_task_environment.h"
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
  form.type = autofill::PasswordForm::TYPE_GENERATED;
  return form;
}

MATCHER_P(FormHasUniqueKey, key, "") {
  return ArePasswordFormUniqueKeyEqual(arg, key);
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
      generation_state_(&form_saver_) {}

PasswordGenerationStateTest::~PasswordGenerationStateTest() {
  mock_store_->ShutdownOnUIThread();
}

// Check that presaving a password for the first time results in adding it.
TEST_F(PasswordGenerationStateTest, PresaveGeneratedPassword_New) {
  const PasswordForm generated = CreateGenerated();

  EXPECT_CALL(store(), AddLogin(generated));
  state().PresaveGeneratedPassword(generated);
  EXPECT_TRUE(state().HasGeneratedPassword());
}

// Check that presaving a password for the second time results in updating it.
TEST_F(PasswordGenerationStateTest, PresaveGeneratedPassword_Replace) {
  PasswordForm generated = CreateGenerated();

  EXPECT_CALL(store(), AddLogin(generated));
  state().PresaveGeneratedPassword(generated);

  PasswordForm generated_updated = generated;
  generated_updated.password_value = ASCIIToUTF16("newgenpwd");
  EXPECT_CALL(store(), UpdateLoginWithPrimaryKey(generated_updated,
                                                 FormHasUniqueKey(generated)));
  state().PresaveGeneratedPassword(generated_updated);
  EXPECT_TRUE(state().HasGeneratedPassword());
}

// Check that presaving a password for the third time results in updating it.
TEST_F(PasswordGenerationStateTest, PresaveGeneratedPassword_ReplaceTwice) {
  PasswordForm generated = CreateGenerated();

  EXPECT_CALL(store(), AddLogin(generated));
  state().PresaveGeneratedPassword(generated);

  PasswordForm generated_updated = generated;
  generated_updated.password_value = ASCIIToUTF16("newgenpwd");
  EXPECT_CALL(store(), UpdateLoginWithPrimaryKey(generated_updated,
                                                 FormHasUniqueKey(generated)));
  state().PresaveGeneratedPassword(generated_updated);

  generated = generated_updated;
  generated_updated.password_value = ASCIIToUTF16("newgenpwd2");
  generated_updated.username_value = ASCIIToUTF16("newusername");
  EXPECT_CALL(store(), UpdateLoginWithPrimaryKey(generated_updated,
                                                 FormHasUniqueKey(generated)));
  state().PresaveGeneratedPassword(generated_updated);
  EXPECT_TRUE(state().HasGeneratedPassword());
}

// Check that presaving a password followed by a call to save a pending
// credential (as new) results in replacing the presaved password with the
// pending one.
TEST_F(PasswordGenerationStateTest, PresaveGeneratedPassword_ThenSaveAsNew) {
  PasswordForm generated = CreateGenerated();

  EXPECT_CALL(store(), AddLogin(_));
  state().PresaveGeneratedPassword(generated);

  // User edits after submission.
  PasswordForm pending = generated;
  pending.password_value = ASCIIToUTF16("edited_password");
  pending.username_value = ASCIIToUTF16("edited_username");
  EXPECT_CALL(store(),
              UpdateLoginWithPrimaryKey(pending, FormHasUniqueKey(generated)));
  state().CommitGeneratedPassword(pending, {} /* best_matches */,
                                  nullptr /* credentials_to_update */);
  EXPECT_FALSE(state().HasGeneratedPassword());
}

// Check that presaving a password followed by a call to save a pending
// credential (as update) results in replacing the presaved password with the
// pending one.
TEST_F(PasswordGenerationStateTest, PresaveGeneratedPassword_ThenUpdate) {
  PasswordForm generated = CreateGenerated();

  EXPECT_CALL(store(), AddLogin(_));
  state().PresaveGeneratedPassword(generated);

  PasswordForm pending = generated;
  pending.username_value = ASCIIToUTF16("edited_username");

  PasswordForm old_saved = CreateSaved();
  old_saved.preferred = false;
  PasswordForm old_psl_saved = CreateSavedPSL();
  old_psl_saved.password_value = pending.password_value;
  std::vector<autofill::PasswordForm> credentials_to_update = {old_psl_saved};

  EXPECT_CALL(store(),
              UpdateLoginWithPrimaryKey(pending, FormHasUniqueKey(generated)));
  EXPECT_CALL(store(), UpdateLogin(old_saved));
  EXPECT_CALL(store(), UpdateLogin(old_psl_saved));

  old_saved.preferred = true;
  state().CommitGeneratedPassword(
      pending, {{old_saved.username_value, &old_saved}} /* best_matches */,
      &credentials_to_update);
  EXPECT_FALSE(state().HasGeneratedPassword());
}

// Check that removing a presaved password removes the presaved password.
TEST_F(PasswordGenerationStateTest, PasswordNoLongerGenerated) {
  PasswordForm generated = CreateGenerated();

  EXPECT_CALL(store(), AddLogin(_));
  state().PresaveGeneratedPassword(generated);

  EXPECT_CALL(store(), RemoveLogin(generated));
  state().PasswordNoLongerGenerated();
  EXPECT_FALSE(state().HasGeneratedPassword());
}

// Check that removing the presaved password and then presaving again results in
// adding the second presaved password as new.
TEST_F(PasswordGenerationStateTest, PasswordNoLongerGenerated_AndPresaveAgain) {
  PasswordForm generated = CreateGenerated();

  EXPECT_CALL(store(), AddLogin(generated));
  state().PresaveGeneratedPassword(generated);

  EXPECT_CALL(store(), RemoveLogin(generated));
  state().PasswordNoLongerGenerated();

  generated.username_value = ASCIIToUTF16("newgenusername");
  generated.password_value = ASCIIToUTF16("newgenpwd");
  EXPECT_CALL(store(), AddLogin(generated));
  state().PresaveGeneratedPassword(generated);
  EXPECT_TRUE(state().HasGeneratedPassword());
}

// Check that presaving a password once in original and then once in clone
// results in the clone calling update, not a fresh save.
TEST_F(PasswordGenerationStateTest, PresaveGeneratedPassword_CloneUpdates) {
  PasswordForm generated = CreateGenerated();

  EXPECT_CALL(store(), AddLogin(generated));
  state().PresaveGeneratedPassword(generated);

  std::unique_ptr<FormSaver> cloned_saver = form_saver().Clone();
  std::unique_ptr<PasswordGenerationState> cloned_state =
      state().Clone(cloned_saver.get());
  EXPECT_TRUE(cloned_state->HasGeneratedPassword());
  PasswordForm generated_updated = generated;
  generated_updated.username_value = ASCIIToUTF16("newname");
  EXPECT_CALL(store(), UpdateLoginWithPrimaryKey(generated_updated,
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
