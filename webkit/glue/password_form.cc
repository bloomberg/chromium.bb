// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/password_form.h"

namespace webkit_glue {

PasswordForm::PasswordForm()
    : scheme(SCHEME_HTML),
      ssl_valid(false),
      preferred(false),
      blacklisted_by_user(false) {
}

PasswordForm::PasswordForm(const WebKit::WebPasswordFormData& web_password_form)
    : scheme(SCHEME_HTML),
      signon_realm(web_password_form.signonRealm.utf8()),
      origin(web_password_form.origin),
      action(web_password_form.action),
      submit_element(web_password_form.submitElement),
      username_element(web_password_form.userNameElement),
      username_value(web_password_form.userNameValue),
      password_element(web_password_form.passwordElement),
      password_value(web_password_form.passwordValue),
      old_password_element(web_password_form.oldPasswordElement),
      old_password_value(web_password_form.oldPasswordValue),
      ssl_valid(false),
      preferred(false),
      blacklisted_by_user(false) {
}

PasswordForm::~PasswordForm() {
}

}  // namespace webkit_glue
