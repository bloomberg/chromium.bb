// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_prefs.h"

#include <memory>

#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/prefs/testing_pref_store.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace prefs {

class AutofillPrefsTest : public testing::Test {
 protected:
  AutofillPrefsTest() {}
  ~AutofillPrefsTest() override {}

  void SetUp() override { pref_service_ = CreatePrefServiceAndRegisterPrefs(); }

  // Creates a PrefService and registers Autofill prefs.
  std::unique_ptr<PrefService> CreatePrefServiceAndRegisterPrefs() {
    scoped_refptr<user_prefs::PrefRegistrySyncable> registry(
        new user_prefs::PrefRegistrySyncable());
    RegisterProfilePrefs(registry.get());
    PrefServiceFactory factory;
    factory.set_user_prefs(base::MakeRefCounted<TestingPrefStore>());
    return factory.Create(registry);
  }

  PrefService* pref_service() { return pref_service_.get(); }

 private:
  std::unique_ptr<PrefService> pref_service_;
};

// Tests migrating the value of the deprecated Autofill master pref to the new
// prefs and that this operation takes place only once.
TEST_F(AutofillPrefsTest, MigrateDeprecatedAutofillPrefs) {
  // All prefs should be enabled by default.
  ASSERT_TRUE(pref_service()->GetBoolean(kAutofillEnabledDeprecated));
  ASSERT_TRUE(pref_service()->GetBoolean(kAutofillProfileEnabled));
  ASSERT_TRUE(pref_service()->GetBoolean(kAutofillCreditCardEnabled));

  // Disable the deprecated master pref and make sure the new fine-grained prefs
  // are not affected by that.
  pref_service()->SetBoolean(kAutofillEnabledDeprecated, false);
  EXPECT_FALSE(pref_service()->GetBoolean(kAutofillEnabledDeprecated));
  EXPECT_TRUE(pref_service()->GetBoolean(kAutofillProfileEnabled));
  EXPECT_TRUE(pref_service()->GetBoolean(kAutofillCreditCardEnabled));

  // Migrate the deprecated master pref's value to the new fine-grained prefs.
  // Their values should become the same as the deprecated master pref's value.
  MigrateDeprecatedAutofillPrefs(pref_service());
  EXPECT_FALSE(pref_service()->GetBoolean(kAutofillProfileEnabled));
  EXPECT_FALSE(pref_service()->GetBoolean(kAutofillCreditCardEnabled));

  // Enable the deprecated master pref and make sure the new fine-grained prefs
  // are not affected by that.
  pref_service()->SetBoolean(kAutofillEnabledDeprecated, true);
  EXPECT_TRUE(pref_service()->GetBoolean(kAutofillEnabledDeprecated));
  EXPECT_FALSE(pref_service()->GetBoolean(kAutofillProfileEnabled));
  EXPECT_FALSE(pref_service()->GetBoolean(kAutofillCreditCardEnabled));

  // Migrate the deprecated master pref's value to the new fine-grained prefs.
  // Their values should not be affected when migration happens a second time.
  MigrateDeprecatedAutofillPrefs(pref_service());
  EXPECT_FALSE(pref_service()->GetBoolean(kAutofillProfileEnabled));
  EXPECT_FALSE(pref_service()->GetBoolean(kAutofillCreditCardEnabled));
}

}  // namespace prefs
}  // namespace autofill