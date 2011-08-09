// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/password_form_dom_manager.h"

#include "base/logging.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPasswordFormData.h"
#include "webkit/glue/form_field.h"

using WebKit::WebFormElement;
using WebKit::WebInputElement;
using WebKit::WebPasswordFormData;

namespace webkit_glue {

PasswordFormFillData::PasswordFormFillData() : wait_for_username(false) {
}

PasswordFormFillData::~PasswordFormFillData() {
}

PasswordForm* PasswordFormDomManager::CreatePasswordForm(
    const WebFormElement& webform) {
  WebPasswordFormData web_password_form(webform);
  if (web_password_form.isValid())
    return new PasswordForm(web_password_form);
  return NULL;
}

// static
void PasswordFormDomManager::InitFillData(
    const PasswordForm& form_on_page,
    const PasswordFormMap& matches,
    const PasswordForm* const preferred_match,
    bool wait_for_username_before_autofill,
    PasswordFormFillData* result) {
  // Note that many of the |FormField| members are not initialized for
  // |username_field| and |password_field| because they are currently not used
  // by the password autocomplete code.
  FormField username_field;
  username_field.name = form_on_page.username_element;
  username_field.value = preferred_match->username_value;
  FormField password_field;
  password_field.name = form_on_page.password_element;
  password_field.value = preferred_match->password_value;

  // Fill basic form data.
  result->basic_data.origin = form_on_page.origin;
  result->basic_data.action = form_on_page.action;
  result->basic_data.fields.push_back(username_field);
  result->basic_data.fields.push_back(password_field);
  result->wait_for_username = wait_for_username_before_autofill;

  // Copy additional username/value pairs.
  PasswordFormMap::const_iterator iter;
  for (iter = matches.begin(); iter != matches.end(); iter++) {
    if (iter->second != preferred_match)
      result->additional_logins[iter->first] = iter->second->password_value;
  }
}

}  // namespace webkit_glue
