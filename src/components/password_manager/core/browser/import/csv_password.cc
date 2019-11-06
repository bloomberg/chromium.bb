// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/import/csv_password.h"

#include <utility>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/import/csv_field_parser.h"
#include "url/gurl.h"

namespace password_manager {

using ::autofill::PasswordForm;

namespace {

// Convert() unescapes a CSV field |str| and converts the result to a 16-bit
// string. |str| is assumed to exclude the outer pair of quotation marks, if
// originally present.
base::string16 Convert(base::StringPiece str) {
  std::string str_copy(str);
  base::ReplaceSubstringsAfterOffset(&str_copy, 0, "\"\"", "\"");
  return base::UTF8ToUTF16(str_copy);
}

}  // namespace

CSVPassword::CSVPassword(const ColumnMap& map, base::StringPiece csv_row)
    : map_(map), row_(csv_row) {}

CSVPassword::~CSVPassword() = default;

bool CSVPassword::Parse(PasswordForm* form) const {
  // |map_| must be an (1) injective and (2) surjective (3) partial map. (3) is
  // enforced by its type, (2) is checked later in the code and (1) follows from
  // (2) and the following size() check.
  if (map_.size() != kLabelCount)
    return false;

  size_t field_idx = 0;
  CSVFieldParser parser(row_);
  GURL origin;
  base::StringPiece username;
  base::StringPiece password;
  bool username_set = false;
  while (parser.HasMoreFields()) {
    base::StringPiece field;
    if (!parser.NextField(&field))
      return false;
    auto meaning_it = map_.find(field_idx++);
    if (meaning_it == map_.end())
      continue;
    switch (meaning_it->second) {
      case Label::kOrigin:
        if (!base::IsStringASCII(field))
          return false;
        origin = GURL(field);
        break;
      case Label::kUsername:
        username = field;
        username_set = true;
        break;
      case Label::kPassword:
        password = field;
        break;
    }
  }
  // While all of origin, username and password must be set in the CSV data row,
  // username is permitted to be an empty string, while password and origin are
  // not.
  if (!origin.is_valid() || !username_set || password.empty())
    return false;
  if (!form)
    return true;
  // There is currently no way to import non-HTML credentials.
  form->scheme = PasswordForm::Scheme::kHtml;
  // GURL::GetOrigin() returns an empty GURL for Android credentials due
  // to the non-standard scheme ("android://"). Hence the following
  // explicit check is necessary to set |signon_realm| correctly for both
  // regular and Android credentials.
  form->signon_realm = IsValidAndroidFacetURI(origin.spec())
                           ? origin.spec()
                           : origin.GetOrigin().spec();
  form->origin = std::move(origin);
  form->username_value = Convert(username);
  form->password_value = Convert(password);
  return true;
}

PasswordForm CSVPassword::ParseValid() const {
  PasswordForm result;
  bool success = Parse(&result);
  DCHECK(success);
  return result;
}

}  // namespace password_manager
