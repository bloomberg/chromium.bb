// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/password_form_dom_manager.h"

#include "base/logging.h"
#include "third_party/WebKit/WebKit/chromium/public/WebInputElement.h"
#include "third_party/WebKit/WebKit/chromium/public/WebPasswordFormData.h"
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
  DCHECK(preferred_match);
  // Fill basic form data.
  result->basic_data.origin = form_on_page.origin;
  result->basic_data.action = form_on_page.action;
  // TODO(jhawkins): Is it right to use an empty string for the form control
  // type?  I don't think the password autocomplete really cares, but we should
  // correct this anyway.
  // TODO(dhollowa): Similarly, |size| ideally should be set from the form
  // control itself.  But it is currently unused.
  result->basic_data.fields.push_back(
      FormField(string16(),
                form_on_page.username_element,
                preferred_match->username_value,
                string16(),
                0));
  result->basic_data.fields.push_back(
      FormField(string16(),
                form_on_page.password_element,
                preferred_match->password_value,
                string16(),
                0));
  result->wait_for_username = wait_for_username_before_autofill;

  // Copy additional username/value pairs.
  PasswordFormMap::const_iterator iter;
  for (iter = matches.begin(); iter != matches.end(); iter++) {
    if (iter->second != preferred_match)
      result->additional_logins[iter->first] = iter->second->password_value;
  }
}

}  // namespace webkit_glue
