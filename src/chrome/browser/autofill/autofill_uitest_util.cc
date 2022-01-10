// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_uitest_util.h"
#include "base/memory/raw_ptr.h"

#include "base/run_loop.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "content/public/test/test_utils.h"

namespace autofill {

// This class is used to wait for asynchronous updates to PersonalDataManager
// to complete.
class PdmChangeWaiter : public PersonalDataManagerObserver {
 public:
  explicit PdmChangeWaiter(Profile* base_profile)
      : alerted_(false),
        has_run_message_loop_(false),
        base_profile_(base_profile) {
    PersonalDataManagerFactory::GetForProfile(base_profile_)->AddObserver(this);
  }

  PdmChangeWaiter(const PdmChangeWaiter&) = delete;
  PdmChangeWaiter& operator=(const PdmChangeWaiter&) = delete;

  ~PdmChangeWaiter() override {}

  // PersonalDataManagerObserver:
  void OnPersonalDataChanged() override {
    if (has_run_message_loop_) {
      base::RunLoop::QuitCurrentWhenIdleDeprecated();
      has_run_message_loop_ = false;
    }
    alerted_ = true;
  }

  void OnInsufficientFormData() override { OnPersonalDataChanged(); }

  void Wait() {
    if (!alerted_) {
      has_run_message_loop_ = true;
      content::RunMessageLoop();
    }
    PersonalDataManagerFactory::GetForProfile(base_profile_)
        ->RemoveObserver(this);
  }

 private:
  bool alerted_;
  bool has_run_message_loop_;
  raw_ptr<Profile> base_profile_;
};

static PersonalDataManager* GetPersonalDataManager(Profile* profile) {
  return PersonalDataManagerFactory::GetForProfile(profile);
}

void AddTestProfile(Profile* base_profile, const AutofillProfile& profile) {
  PdmChangeWaiter observer(base_profile);
  GetPersonalDataManager(base_profile)->AddProfile(profile);

  // AddProfile is asynchronous. Wait for it to finish before continuing the
  // tests.
  observer.Wait();
}

void SetTestProfile(Profile* base_profile, const AutofillProfile& profile) {
  std::vector<AutofillProfile> profiles;
  profiles.push_back(profile);
  SetTestProfiles(base_profile, &profiles);
}

void SetTestProfiles(Profile* base_profile,
                     std::vector<AutofillProfile>* profiles) {
  PdmChangeWaiter observer(base_profile);
  GetPersonalDataManager(base_profile)->SetProfiles(profiles);
  observer.Wait();
}

void AddTestCreditCard(Profile* base_profile, const CreditCard& card) {
  PdmChangeWaiter observer(base_profile);
  GetPersonalDataManager(base_profile)->AddCreditCard(card);

  // AddCreditCard is asynchronous. Wait for it to finish before continuing the
  // tests.
  observer.Wait();
}

void AddTestServerCreditCard(Profile* base_profile, const CreditCard& card) {
  PdmChangeWaiter observer(base_profile);
  GetPersonalDataManager(base_profile)->AddFullServerCreditCard(card);

  // AddCreditCard is asynchronous. Wait for it to finish before continuing the
  // tests.
  observer.Wait();
}

void AddTestAutofillData(Profile* base_profile,
                         const AutofillProfile& profile,
                         const CreditCard& card) {
  AddTestProfile(base_profile, profile);
  PdmChangeWaiter observer(base_profile);
  GetPersonalDataManager(base_profile)->AddCreditCard(card);
  observer.Wait();
}

void WaitForPersonalDataChange(Profile* base_profile) {
  PdmChangeWaiter observer(base_profile);
  observer.Wait();
}

void WaitForPersonalDataManagerToBeLoaded(Profile* base_profile) {
  PersonalDataManager* pdm =
      autofill::PersonalDataManagerFactory::GetForProfile(base_profile);
  while (!pdm->IsDataLoaded())
    WaitForPersonalDataChange(base_profile);
}

void GenerateTestAutofillPopup(
    AutofillExternalDelegate* autofill_external_delegate) {
  int query_id = 1;
  FormData form;
  FormFieldData field;
  field.is_focusable = true;
  field.should_autocomplete = true;
  gfx::RectF bounds(100.f, 100.f);

  ContentAutofillDriver* driver = static_cast<ContentAutofillDriver*>(
      absl::get<AutofillDriver*>(autofill_external_delegate->GetDriver()));
  driver->AskForValuesToFill(query_id, form, field, bounds, false);

  std::vector<Suggestion> suggestions = {Suggestion(u"Test suggestion")};
  autofill_external_delegate->OnSuggestionsReturned(
      query_id, suggestions, /*autoselect_first_suggestion=*/false);
}

}  // namespace autofill
