// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/submission_indicator_event.h"

namespace autofill {

SubmissionIndicatorEvent ToSubmissionIndicatorEvent(SubmissionSource source) {
  switch (source) {
    case SubmissionSource::NONE:
      return SubmissionIndicatorEvent::NONE;
    case SubmissionSource::SAME_DOCUMENT_NAVIGATION:
      return SubmissionIndicatorEvent::SAME_DOCUMENT_NAVIGATION;
    case SubmissionSource::XHR_SUCCEEDED:
      return SubmissionIndicatorEvent::XHR_SUCCEEDED;
    case SubmissionSource::FRAME_DETACHED:
      return SubmissionIndicatorEvent::FRAME_DETACHED;
    case SubmissionSource::DOM_MUTATION_AFTER_XHR:
      return SubmissionIndicatorEvent::DOM_MUTATION_AFTER_XHR;
    case SubmissionSource::PROBABLY_FORM_SUBMITTED:
      return SubmissionIndicatorEvent::PROBABLE_FORM_SUBMISSION;
    case SubmissionSource::FORM_SUBMISSION:
      return SubmissionIndicatorEvent::HTML_FORM_SUBMISSION;
  }
  // Unittests exercise this path, so do not put NOTREACHED() here.
  return SubmissionIndicatorEvent::NONE;
}

std::ostream& operator<<(std::ostream& os,
                         SubmissionIndicatorEvent submission_event) {
  switch (submission_event) {
    case SubmissionIndicatorEvent::HTML_FORM_SUBMISSION:
      os << "HTML_FORM_SUBMISSION";
      break;
    case SubmissionIndicatorEvent::SAME_DOCUMENT_NAVIGATION:
      os << "SAME_DOCUMENT_NAVIGATION";
      break;
    case SubmissionIndicatorEvent::XHR_SUCCEEDED:
      os << "XHR_SUCCEEDED";
      break;
    case SubmissionIndicatorEvent::FRAME_DETACHED:
      os << "FRAME_DETACHED";
      break;
    case SubmissionIndicatorEvent::DOM_MUTATION_AFTER_XHR:
      os << "DOM_MUTATION_AFTER_XHR";
      break;
    case SubmissionIndicatorEvent::
        PROVISIONALLY_SAVED_FORM_ON_START_PROVISIONAL_LOAD:
      os << "PROVISIONALLY_SAVED_FORM_ON_START_PROVISIONAL_LOAD";
      break;
    default:
      os << "NO_SUBMISSION";
      break;
  }
  return os;
}

}  // namespace autofill
