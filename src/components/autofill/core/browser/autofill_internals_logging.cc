// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_internals_logging.h"

#include <string>

#include "base/no_destructor.h"

namespace autofill {

AutofillInternalsLogging::AutofillInternalsLogging() = default;

AutofillInternalsLogging::~AutofillInternalsLogging() = default;

std::unique_ptr<AutofillInternalsLogging>& GetAutofillInternalLogging() {
  static base::NoDestructor<std::unique_ptr<AutofillInternalsLogging>> logger;
  return *logger;
}

void AutofillInternalsLogging::SetAutofillInternalsLogger(
    std::unique_ptr<AutofillInternalsLogging> logger) {
  GetAutofillInternalLogging().swap(logger);
}

void AutofillInternalsLogging::Log(const std::string& message) {
  if (GetAutofillInternalLogging()) {
    GetAutofillInternalLogging()->LogHelper(base::Value(message));
  }
}

}  // namespace autofill
