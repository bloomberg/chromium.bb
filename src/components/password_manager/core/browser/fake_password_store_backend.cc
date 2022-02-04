// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/fake_password_store_backend.h"

#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/psl_matching_helper.h"
#include "components/sync/model/proxy_model_type_controller_delegate.h"

namespace password_manager {

FakePasswordStoreBackend::FakePasswordStoreBackend() = default;
FakePasswordStoreBackend::~FakePasswordStoreBackend() = default;

base::WeakPtr<PasswordStoreBackend> FakePasswordStoreBackend::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void FakePasswordStoreBackend::InitBackend(
    RemoteChangesReceived remote_form_changes_received,
    base::RepeatingClosure sync_enabled_or_disabled_cb,
    base::OnceCallback<void(bool)> completion) {
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(completion), /*success=*/true));
}

void FakePasswordStoreBackend::Shutdown(base::OnceClosure shutdown_completed) {
  std::move(shutdown_completed).Run();
}

void FakePasswordStoreBackend::GetAllLoginsAsync(LoginsOrErrorReply callback) {
  base::SequencedTaskRunnerHandle::Get()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FakePasswordStoreBackend::GetAllLoginsInternal,
                     base::Unretained(this)),
      std::move(callback));
}

void FakePasswordStoreBackend::GetAutofillableLoginsAsync(
    LoginsOrErrorReply callback) {
  base::SequencedTaskRunnerHandle::Get()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FakePasswordStoreBackend::GetAutofillableLoginsInternal,
                     base::Unretained(this)),
      std::move(callback));
}

void FakePasswordStoreBackend::FillMatchingLoginsAsync(
    LoginsReply callback,
    bool include_psl,
    const std::vector<PasswordFormDigest>& forms) {
  base::SequencedTaskRunnerHandle::Get()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FakePasswordStoreBackend::FillMatchingLoginsInternal,
                     base::Unretained(this), forms, include_psl),
      std::move(callback));
}

void FakePasswordStoreBackend::AddLoginAsync(
    const PasswordForm& form,
    PasswordStoreChangeListReply callback) {
  base::SequencedTaskRunnerHandle::Get()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FakePasswordStoreBackend::AddLoginInternal,
                     base::Unretained(this), form),
      std::move(callback));
}

void FakePasswordStoreBackend::UpdateLoginAsync(
    const PasswordForm& form,
    PasswordStoreChangeListReply callback) {
  base::SequencedTaskRunnerHandle::Get()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FakePasswordStoreBackend::UpdateLoginInternal,
                     base::Unretained(this), form),
      std::move(callback));
}

void FakePasswordStoreBackend::RemoveLoginAsync(
    const PasswordForm& form,
    PasswordStoreChangeListReply callback) {
  base::SequencedTaskRunnerHandle::Get()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FakePasswordStoreBackend::RemoveLoginInternal,
                     base::Unretained(this), form),
      std::move(callback));
}

void FakePasswordStoreBackend::RemoveLoginsByURLAndTimeAsync(
    const base::RepeatingCallback<bool(const GURL&)>& url_filter,
    base::Time delete_begin,
    base::Time delete_end,
    base::OnceCallback<void(bool)> sync_completion,
    PasswordStoreChangeListReply callback) {
  NOTREACHED();
}

void FakePasswordStoreBackend::RemoveLoginsCreatedBetweenAsync(
    base::Time delete_begin,
    base::Time delete_end,
    PasswordStoreChangeListReply callback) {
  NOTREACHED();
}

void FakePasswordStoreBackend::DisableAutoSignInForOriginsAsync(
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
    base::OnceClosure completion) {
  base::SequencedTaskRunnerHandle::Get()->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(
          &FakePasswordStoreBackend::DisableAutoSignInForOriginsInternal,
          base::Unretained(this), origin_filter),
      std::move(completion));
}

SmartBubbleStatsStore* FakePasswordStoreBackend::GetSmartBubbleStatsStore() {
  return nullptr;
}

FieldInfoStore* FakePasswordStoreBackend::GetFieldInfoStore() {
  return nullptr;
}

std::unique_ptr<syncer::ProxyModelTypeControllerDelegate>
FakePasswordStoreBackend::CreateSyncControllerDelegate() {
  NOTIMPLEMENTED();
  return nullptr;
}

void FakePasswordStoreBackend::ClearAllLocalPasswords() {
  NOTIMPLEMENTED();
}

LoginsResult FakePasswordStoreBackend::GetAllLoginsInternal() {
  LoginsResult result;
  for (const auto& elements : stored_passwords_) {
    for (const auto& stored_form : elements.second) {
      result.push_back(std::make_unique<PasswordForm>(stored_form));
    }
  }
  return result;
}

LoginsResult FakePasswordStoreBackend::GetAutofillableLoginsInternal() {
  LoginsResult result;
  for (const auto& elements : stored_passwords_) {
    for (const auto& stored_form : elements.second) {
      if (!stored_form.blocked_by_user)
        result.push_back(std::make_unique<PasswordForm>(stored_form));
    }
  }
  return result;
}

LoginsResult FakePasswordStoreBackend::FillMatchingLoginsInternal(
    const std::vector<PasswordFormDigest>& forms,
    bool include_psl) {
  std::vector<std::unique_ptr<PasswordForm>> results;
  for (const auto& form : forms) {
    std::vector<std::unique_ptr<PasswordForm>> matched_forms =
        FillMatchingLoginsHelper(form, include_psl);
    results.insert(results.end(),
                   std::make_move_iterator(matched_forms.begin()),
                   std::make_move_iterator(matched_forms.end()));
  }
  return results;
}

LoginsResult FakePasswordStoreBackend::FillMatchingLoginsHelper(
    const PasswordFormDigest& form,
    bool include_psl) {
  std::vector<std::unique_ptr<PasswordForm>> matched_forms;
  for (const auto& elements : stored_passwords_) {
    // The code below doesn't support PSL federated credential. It's doable but
    // no tests need it so far.
    const bool realm_matches = elements.first == form.signon_realm;
    const bool realm_psl_matches =
        IsPublicSuffixDomainMatch(elements.first, form.signon_realm);
    if (realm_matches || (realm_psl_matches && include_psl) ||
        (form.scheme == PasswordForm::Scheme::kHtml &&
         password_manager::IsFederatedRealm(elements.first, form.url))) {
      const bool is_psl = !realm_matches && realm_psl_matches;
      for (const auto& stored_form : elements.second) {
        // Repeat the condition above with an additional check for origin.
        if (realm_matches || realm_psl_matches ||
            (form.scheme == PasswordForm::Scheme::kHtml &&
             stored_form.url.DeprecatedGetOriginAsURL() ==
                 form.url.DeprecatedGetOriginAsURL() &&
             password_manager::IsFederatedRealm(stored_form.signon_realm,
                                                form.url))) {
          matched_forms.push_back(std::make_unique<PasswordForm>(stored_form));
          matched_forms.back()->is_public_suffix_match = is_psl;
        }
      }
    }
  }
  return matched_forms;
}

PasswordStoreChangeList FakePasswordStoreBackend::AddLoginInternal(
    const PasswordForm& form) {
  PasswordStoreChangeList changes;
  auto& passwords_for_signon_realm = stored_passwords_[form.signon_realm];
  auto iter = std::find_if(
      passwords_for_signon_realm.begin(), passwords_for_signon_realm.end(),
      [&form](const auto& password) {
        return ArePasswordFormUniqueKeysEqual(form, password);
      });

  if (iter != passwords_for_signon_realm.end()) {
    changes.emplace_back(PasswordStoreChange::REMOVE, *iter);
    changes.emplace_back(PasswordStoreChange::ADD, form);
    *iter = form;
    iter->in_store = PasswordForm::Store::kProfileStore;
    return changes;
  }

  changes.emplace_back(PasswordStoreChange::ADD, form);
  passwords_for_signon_realm.push_back(form);
  passwords_for_signon_realm.back().in_store =
      PasswordForm::Store::kProfileStore;
  return changes;
}

PasswordStoreChangeList FakePasswordStoreBackend::UpdateLoginInternal(
    const PasswordForm& form) {
  PasswordStoreChangeList changes;
  std::vector<PasswordForm>& forms = stored_passwords_[form.signon_realm];
  for (auto& stored_form : forms) {
    if (ArePasswordFormUniqueKeysEqual(form, stored_form)) {
      stored_form = form;
      stored_form.in_store = PasswordForm::Store::kProfileStore;
      changes.push_back(PasswordStoreChange(PasswordStoreChange::UPDATE, form));
    }
  }
  return changes;
}

void FakePasswordStoreBackend::DisableAutoSignInForOriginsInternal(
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter) {
  for (auto& realm : stored_passwords_) {
    if (origin_filter.Run(GURL(realm.first))) {
      for (auto& form : realm.second) {
        form.skip_zero_click = true;
      }
    }
  }
}

PasswordStoreChangeList FakePasswordStoreBackend::RemoveLoginInternal(
    const PasswordForm& form) {
  PasswordStoreChangeList changes;
  std::vector<PasswordForm>& forms = stored_passwords_[form.signon_realm];
  auto it = forms.begin();
  while (it != forms.end()) {
    if (ArePasswordFormUniqueKeysEqual(form, *it)) {
      it = forms.erase(it);
      changes.push_back(PasswordStoreChange(PasswordStoreChange::REMOVE, form));
    } else {
      ++it;
    }
  }
  return changes;
}

}  // namespace password_manager
