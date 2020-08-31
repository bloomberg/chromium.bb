// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_QUICK_ANSWERS_UTILS_QUICK_ANSWERS_METRICS_H_
#define CHROMEOS_COMPONENTS_QUICK_ANSWERS_UTILS_QUICK_ANSWERS_METRICS_H_

#include "chromeos/components/quick_answers/quick_answers_model.h"

namespace base {

class TimeDelta;

}  // namespace base

namespace chromeos {
namespace quick_answers {

// Record the status of loading quick answers with status and duration.
void RecordLoadingStatus(LoadStatus status, const base::TimeDelta duration);

// Record loading result with result type and network latency.
void RecordResult(ResultType result_type, const base::TimeDelta duration);

// Record quick answers user clicks with result type and duration between result
// fetch finish and user clicks.
void RecordClick(ResultType result_type, const base::TimeDelta duration);

// Record selected text length to learn about usage pattern.
void RecordSelectedTextLength(int length);

// Record active impression with result type and impression duration.
void RecordActiveImpression(ResultType result_type,
                            const base::TimeDelta duration);

// Record user interaction with the consent UI with how many times the user has
// seen the consent and impression duration.
void RecordConsentInteraction(ConsentInteractionType type,
                              int nth_impression,
                              const base::TimeDelta duration);

// Record consent impression with how many times the user has seen the consent.
void RecordConsentImpression(int nth_impression);

}  // namespace quick_answers
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_QUICK_ANSWERS_UTILS_QUICK_ANSWERS_METRICS_H_
