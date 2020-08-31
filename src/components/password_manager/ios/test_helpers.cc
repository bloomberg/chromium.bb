// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/ios/test_helpers.h"

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/password_manager/ios/account_select_fill_data.h"
#include "url/gurl.h"

using autofill::FieldRendererId;
using autofill::FormRendererId;
using autofill::PasswordFormFillData;
using password_manager::FillData;

namespace test_helpers {

void SetPasswordFormFillData(const std::string& origin,
                             const char* form_name,
                             uint32_t unique_renderer_id,
                             const char* username_field,
                             uint32_t username_unique_id,
                             const char* username_value,
                             const char* password_field,
                             uint32_t password_unique_id,
                             const char* password_value,
                             const char* additional_username,
                             const char* additional_password,
                             bool wait_for_username,
                             PasswordFormFillData* form_data) {
  form_data->origin = GURL(origin);
  form_data->name = base::UTF8ToUTF16(form_name);
  form_data->form_renderer_id = FormRendererId(unique_renderer_id);
  autofill::FormFieldData username;
  username.name = base::UTF8ToUTF16(username_field);
  username.unique_renderer_id = FieldRendererId(username_unique_id);
  username.unique_id = base::UTF8ToUTF16(username_field);
  username.value = base::UTF8ToUTF16(username_value);
  form_data->username_field = username;
  autofill::FormFieldData password;
  password.name = base::UTF8ToUTF16(password_field);
  password.unique_renderer_id = FieldRendererId(password_unique_id);
  password.unique_id = base::UTF8ToUTF16(password_field);
  password.value = base::UTF8ToUTF16(password_value);
  form_data->password_field = password;
  if (additional_username) {
    autofill::PasswordAndMetadata additional_password_data;
    additional_password_data.username = base::UTF8ToUTF16(additional_username);
    additional_password_data.password = base::UTF8ToUTF16(additional_password);
    additional_password_data.realm.clear();
    form_data->additional_logins.push_back(additional_password_data);
  }
  form_data->wait_for_username = wait_for_username;
}

void SetFillData(const std::string& origin,
                 uint32_t form_id,
                 uint32_t username_field_id,
                 const char* username_value,
                 uint32_t password_field_id,
                 const char* password_value,
                 FillData* fill_data) {
  DCHECK(fill_data);
  fill_data->origin = GURL(origin);
  fill_data->form_id = FormRendererId(form_id);
  fill_data->username_element_id = FieldRendererId(username_field_id);
  fill_data->username_value = base::UTF8ToUTF16(username_value);
  fill_data->password_element_id = FieldRendererId(password_field_id);
  fill_data->password_value = base::UTF8ToUTF16(password_value);
}

}  // namespace  test_helpers
