// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store_util.h"
#include "components/password_manager/core/browser/password_store_backend.h"
#include "components/password_manager/core/browser/password_store_change.h"

namespace password_manager {

PasswordChanges JoinPasswordStoreChanges(
    const std::vector<PasswordChangesOrError>& changes_to_join) {
  PasswordStoreChangeList joined_changes;
  for (const auto& changes_or_error : changes_to_join) {
    if (absl::holds_alternative<PasswordStoreBackendError>(changes_or_error))
      return absl::nullopt;
    const PasswordChanges& changes =
        absl::get<PasswordChanges>(changes_or_error);
    if (!changes.has_value())
      return absl::nullopt;
    std::copy(changes->begin(), changes->end(),
              std::back_inserter(joined_changes));
  }
  return joined_changes;
}

LoginsResult GetLoginsOrEmptyListOnFailure(LoginsResultOrError result) {
  if (absl::holds_alternative<PasswordStoreBackendError>(result)) {
    return {};
  }
  return std::move(absl::get<LoginsResult>(result));
}

PasswordChanges GetPasswordChangesOrEmptyListOnFailure(
    PasswordChangesOrError result) {
  if (absl::holds_alternative<PasswordStoreBackendError>(result)) {
    return PasswordStoreChangeList();
  }
  return std::move(absl::get<PasswordChanges>(result));
}

}  // namespace password_manager
