// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_data_validation.h"

#include "base/ranges/algorithm.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "url/gurl.h"

namespace autofill {

const size_t kMaxDataLength = 1024;

// Allow enough space for all countries (roughly 300 distinct values) and all
// timezones (roughly 400 distinct values), plus some extra wiggle room.
const size_t kMaxListSize = 512;

bool IsValidString(const std::string& str) {
  return str.size() <= kMaxDataLength;
}

bool IsValidString16(const std::u16string& str) {
  return str.size() <= kMaxDataLength;
}

bool IsValidGURL(const GURL& url) {
  return url.is_empty() || url.is_valid();
}

bool IsValidFormFieldData(const FormFieldData& field) {
  return IsValidString16(field.label) && IsValidString16(field.name) &&
         IsValidString16(field.value) &&
         IsValidString(field.form_control_type) &&
         IsValidString(field.autocomplete_attribute) &&
         IsValidOptionVector(field.options);
}

bool IsValidFormData(const FormData& form) {
  return IsValidString16(form.name) && IsValidGURL(form.url) &&
         IsValidGURL(form.action) && form.fields.size() <= kMaxListSize &&
         base::ranges::all_of(form.fields, &IsValidFormFieldData);
}

bool IsValidPasswordFormFillData(const PasswordFormFillData& form) {
  return IsValidString16(form.name) && IsValidGURL(form.url) &&
         IsValidGURL(form.action) &&
         IsValidFormFieldData(form.username_field) &&
         IsValidFormFieldData(form.password_field) &&
         IsValidString(form.preferred_realm) &&
         base::ranges::all_of(form.additional_logins, [](const auto& login) {
           return IsValidString16(login.username) &&
                  IsValidString16(login.password) && IsValidString(login.realm);
         });
}

bool IsValidOptionVector(const std::vector<SelectOption>& options) {
  return options.size() <= kMaxListSize &&
         base::ranges::all_of(options, &IsValidString16,
                              &SelectOption::content);
}

bool IsValidString16Vector(const std::vector<std::u16string>& v) {
  return v.size() <= kMaxListSize && base::ranges::all_of(v, &IsValidString16);
}

bool IsValidFormDataVector(const std::vector<FormData>& v) {
  return v.size() <= kMaxListSize && base::ranges::all_of(v, &IsValidFormData);
}

}  // namespace autofill
