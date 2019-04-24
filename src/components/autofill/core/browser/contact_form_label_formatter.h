// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_CONTACT_FORM_LABEL_FORMATTER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_CONTACT_FORM_LABEL_FORMATTER_H_

#include <set>
#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/label_formatter.h"

namespace autofill {

// A LabelFormatter that creates Suggestions' disambiguating labels for forms
// containing name and phone or email fields.
class ContactFormLabelFormatter : public LabelFormatter {
 public:
  ContactFormLabelFormatter(const std::string& app_locale,
                            ServerFieldType focused_field_type,
                            const std::vector<ServerFieldType>& field_types,
                            const std::set<FieldTypeGroup>& field_type_groups);

  ~ContactFormLabelFormatter() override;

  std::vector<base::string16> GetLabels(
      const std::vector<AutofillProfile*>& profiles) const override;

 private:
  // A collection of field types that can be used to make labels. This
  // collection excludes the focused_field_type_.
  std::vector<ServerFieldType> field_types_for_labels_;

  // A collection of meaningful FieldTypeGroups in the form with which the user
  // is interacting.
  std::set<FieldTypeGroup> field_type_groups_;

  // A collection of meaningful FieldTypeGroups in the form with which the user
  // is interacting minus the focused field's corresponding FieldTypeGroup.
  std::set<FieldTypeGroup> filtered_field_type_groups_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_CONTACT_FORM_LABEL_FORMATTER_H_
