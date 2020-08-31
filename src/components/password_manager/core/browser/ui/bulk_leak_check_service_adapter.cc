// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/bulk_leak_check_service_adapter.h"

#include <memory>
#include <tuple>

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/leak_detection/bulk_leak_check.h"
#include "components/password_manager/core/browser/leak_detection/encryption_utils.h"
#include "components/password_manager/core/browser/leak_detection_delegate.h"
#include "components/password_manager/core/browser/ui/credential_utils.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "components/prefs/pref_service.h"

namespace password_manager {

BulkLeakCheckServiceAdapter::BulkLeakCheckServiceAdapter(
    SavedPasswordsPresenter* presenter,
    BulkLeakCheckService* service,
    PrefService* prefs)
    : presenter_(presenter), service_(service), prefs_(prefs) {
  DCHECK(presenter_);
  DCHECK(service_);
  DCHECK(prefs_);
  presenter_->AddObserver(this);
}

BulkLeakCheckServiceAdapter::~BulkLeakCheckServiceAdapter() {
  presenter_->RemoveObserver(this);
}

bool BulkLeakCheckServiceAdapter::StartBulkLeakCheck(
    const void* key,
    LeakCheckCredential::Data* data) {
  if (service_->state() == BulkLeakCheckService::State::kRunning)
    return false;

  // Even though the BulkLeakCheckService performs canonicalization eventually
  // we do it here to de-dupe credentials that have the same canonicalized form.
  SavedPasswordsPresenter::SavedPasswordsView saved_passwords =
      presenter_->GetSavedPasswords();
  base::flat_set<CanonicalizedCredential> canonicalized(saved_passwords.begin(),
                                                        saved_passwords.end());

  // Build the list of LeakCheckCredentials and forward them to the service to
  // start the check.
  std::vector<LeakCheckCredential> credentials;
  credentials.reserve(canonicalized.size());

  for (const auto& credential : canonicalized) {
    credentials.emplace_back(credential.canonicalized_username,
                             credential.password);
    if (key) {
      DCHECK(data);
      credentials.back().SetUserData(key, data->Clone());
    }
  }

  service_->CheckUsernamePasswordPairs(std::move(credentials));
  return true;
}

void BulkLeakCheckServiceAdapter::StopBulkLeakCheck() {
  service_->Cancel();
}

BulkLeakCheckService::State BulkLeakCheckServiceAdapter::GetBulkLeakCheckState()
    const {
  return service_->state();
}

size_t BulkLeakCheckServiceAdapter::GetPendingChecksCount() const {
  return service_->GetPendingChecksCount();
}

void BulkLeakCheckServiceAdapter::OnEdited(const autofill::PasswordForm& form) {
  if (CanStartLeakCheck(*prefs_)) {
    // Here no extra canonicalization is needed, as there are no other forms we
    // could de-dupe before we pass it on to the service.
    std::vector<LeakCheckCredential> credentials;
    credentials.emplace_back(form.username_value, form.password_value);
    service_->CheckUsernamePasswordPairs(std::move(credentials));
  }
}

}  // namespace password_manager
