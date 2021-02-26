// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ALL_PASSWORDS_BOTTOM_SHEET_HELPER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ALL_PASSWORDS_BOTTOM_SHEET_HELPER_H_

#include "base/callback_forward.h"
#include "base/optional.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/password_manager/core/browser/password_store_consumer.h"

// This class helps to determine the visibility of the "All Passwords Sheet"
// button by requesting whether there are any passwords stored at all.
// When created, it will immediately query the store for passwords and store the
// result. If provided with an eligible focused field and a callback to update,
// the helper will trigger an update right away if passwords are found.
class AllPasswordsBottomSheetHelper
    : public password_manager::PasswordStoreConsumer {
 public:
  explicit AllPasswordsBottomSheetHelper(
      password_manager::PasswordStore* store);
  AllPasswordsBottomSheetHelper(const AllPasswordsBottomSheetHelper&) = delete;
  AllPasswordsBottomSheetHelper& operator=(
      const AllPasswordsBottomSheetHelper&) = delete;
  ~AllPasswordsBottomSheetHelper() override;

  // Returns the number of found credentials only if the helper already finished
  // querying the password store.
  base::Optional<size_t> available_credentials() const {
    return available_credentials_;
  }
  autofill::mojom::FocusedFieldType last_focused_field_type() const {
    return last_focused_field_type_;
  }

  void SetLastFocusedFieldType(
      autofill::mojom::FocusedFieldType focused_field_type);
  void SetUpdateCallback(base::OnceClosure update_callback);
  void ClearUpdateCallback();

 private:
  // PasswordStoreConsumer:
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<password_manager::PasswordForm>> results)
      override;

  // A callback used to update the suggestions if the password store provides
  // credentials and the focused field might still profit from them.
  base::OnceClosure update_callback_;

  // Stores whether the store returned credentials the sheet can show.
  base::Optional<size_t> available_credentials_ = base::nullopt;

  // Records the last focused field type to infer whether an update should be
  // triggered if the store returns suggestions.
  autofill::mojom::FocusedFieldType last_focused_field_type_ =
      autofill::mojom::FocusedFieldType::kUnknown;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ALL_PASSWORDS_BOTTOM_SHEET_HELPER_H_
