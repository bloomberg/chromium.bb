// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_LABEL_FORMATTER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_LABEL_FORMATTER_H_

#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill {

// Handles the creation of Suggestions' disambiguating labels.
class LabelFormatter {
 public:
  LabelFormatter(const std::string& app_locale,
                 ServerFieldType focused_field_type,
                 const std::vector<ServerFieldType>& field_types);
  virtual ~LabelFormatter();

  // Returns a collection of |labels| formed by extracting useful disambiguating
  // information from a collection of |profiles|.
  virtual std::vector<base::string16> GetLabels(
      const std::vector<AutofillProfile*>& profiles) const = 0;

 protected:
  const std::string& app_locale() const { return app_locale_; }
  ServerFieldType focused_field_type() const { return focused_field_type_; }
  const std::vector<ServerFieldType>& field_types() const {
    return field_types_;
  }

 private:
  // The locale for which to generate labels. This reflects the language and
  // country for which the application is translated, e.g. en_AU for Austalian
  // English.
  std::string app_locale_;

  // The field on which the user is currently focused.
  ServerFieldType focused_field_type_;

  // A collection of meaningful field types in the form with which the user is
  // interacting.
  std::vector<ServerFieldType> field_types_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_LABEL_FORMATTER_H_
