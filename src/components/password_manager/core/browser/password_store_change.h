// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_CHANGE_H__
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_CHANGE_H__

#include <ostream>
#include <vector>

#include "components/autofill/core/common/password_form.h"

namespace password_manager {

class PasswordStoreChange {
 public:
  enum Type {
    ADD,
    UPDATE,
    REMOVE,
  };

  PasswordStoreChange(Type type, const autofill::PasswordForm& form)
      : type_(type), form_(form) {}
  virtual ~PasswordStoreChange() {}

  Type type() const { return type_; }
  const autofill::PasswordForm& form() const { return form_; }

  bool operator==(const PasswordStoreChange& other) const {
    return type() == other.type() &&
           form().signon_realm == other.form().signon_realm &&
           form().origin == other.form().origin &&
           form().action == other.form().action &&
           form().submit_element == other.form().submit_element &&
           form().username_element == other.form().username_element &&
           form().username_value == other.form().username_value &&
           form().password_element == other.form().password_element &&
           form().password_value == other.form().password_value &&
           form().new_password_element == other.form().new_password_element &&
           form().new_password_value == other.form().new_password_value &&
           form().preferred == other.form().preferred &&
           form().date_created == other.form().date_created &&
           form().blacklisted_by_user == other.form().blacklisted_by_user;
  }

 private:
  Type type_;
  autofill::PasswordForm form_;
};

typedef std::vector<PasswordStoreChange> PasswordStoreChangeList;

// For testing.
std::ostream& operator<<(std::ostream& os,
                         const PasswordStoreChange& password_store_change);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_CHANGE_H_
