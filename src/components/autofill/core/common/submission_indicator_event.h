// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_SUBMISSION_INDICATOR_EVENT_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_SUBMISSION_INDICATOR_EVENT_H_

#include <ostream>

#include "components/autofill/core/common/submission_source.h"

namespace autofill {

// Events observed by the Password Manager that indicate either that a form is
// potentially being submitted, or that a form has already been successfully
// submitted. Recorded into a UMA histogram, so order of enumerators should
// not be changed.
enum class SubmissionIndicatorEvent {
  NONE,
  HTML_FORM_SUBMISSION,
  SAME_DOCUMENT_NAVIGATION,
  XHR_SUCCEEDED,
  FRAME_DETACHED,
  DEPRECATED_MANUAL_SAVE,  // obsolete
  DOM_MUTATION_AFTER_XHR,
  PROVISIONALLY_SAVED_FORM_ON_START_PROVISIONAL_LOAD,
  DEPRECATED_FILLED_FORM_ON_START_PROVISIONAL_LOAD,            // unused
  DEPRECATED_FILLED_INPUT_ELEMENTS_ON_START_PROVISIONAL_LOAD,  // unused
  PROBABLE_FORM_SUBMISSION,
  SUBMISSION_INDICATOR_EVENT_COUNT
};

SubmissionIndicatorEvent ToSubmissionIndicatorEvent(SubmissionSource source);

// For testing.
std::ostream& operator<<(std::ostream& os,
                         SubmissionIndicatorEvent submission_event);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_SUBMISSION_INDICATOR_EVENT_H_
