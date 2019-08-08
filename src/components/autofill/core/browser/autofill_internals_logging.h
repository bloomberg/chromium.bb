// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_INTERNALS_LOGGING_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_INTERNALS_LOGGING_H_

#include <string>

#include "base/macros.h"
#include "base/values.h"

namespace autofill {

class AutofillInternalsLogging {
 public:
  AutofillInternalsLogging();
  virtual ~AutofillInternalsLogging();

  // Main API function that is called when something is logged.
  static void Log(const std::string& message);

  static void SetAutofillInternalsLogger(
      std::unique_ptr<AutofillInternalsLogging> logger);

 private:
  virtual void LogHelper(const base::Value& message) = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_INTERNALS_LOGGING_H_
