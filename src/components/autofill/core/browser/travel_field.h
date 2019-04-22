// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TRAVEL_FIELD_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TRAVEL_FIELD_H_

#include <memory>

#include "components/autofill/core/browser/autofill_scanner.h"
#include "components/autofill/core/browser/form_field.h"

namespace autofill {

class TravelField : public FormField {
 public:
  ~TravelField() override;

  static std::unique_ptr<FormField> Parse(AutofillScanner* scanner);

 protected:
  void AddClassifications(FieldCandidatesMap* field_candidates) const override;

 private:
  // All of the following fields are optional.
  AutofillField* passport_;
  AutofillField* origin_;
  AutofillField* destination_;
  AutofillField* flight_;
};
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TRAVEL_FIELD_H_
