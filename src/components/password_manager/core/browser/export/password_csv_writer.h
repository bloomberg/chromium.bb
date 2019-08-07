// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_EXPORT_PASSWORD_CSV_WRITER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_EXPORT_PASSWORD_CSV_WRITER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"

namespace autofill {
struct PasswordForm;
}

namespace password_manager {

// Static-only class bundling together the API for serializing passwords into
// CSV format.
class PasswordCSVWriter {
 public:
  // Creates a CSV representation of the forms stored in |password|. Note that
  // this loses all the metadata except for the origin, username and password.
  static std::string SerializePasswords(
      const std::vector<std::unique_ptr<autofill::PasswordForm>>& passwords);

 private:
  // Converts |form| into a single line in the CSV format. Metadata are lost,
  // see SerializePasswords.
  static std::map<std::string, std::string> PasswordFormToRecord(
      const autofill::PasswordForm& form);

  DISALLOW_IMPLICIT_CONSTRUCTORS(PasswordCSVWriter);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_EXPORT_PASSWORD_CSV_WRITER_H_
