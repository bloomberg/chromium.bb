// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/quick_answers_model.h"

namespace chromeos {
namespace quick_answers {
QuickAnswer::QuickAnswer() = default;
QuickAnswer::~QuickAnswer() = default;

PreprocessedOutput::PreprocessedOutput() = default;
PreprocessedOutput::PreprocessedOutput(const PreprocessedOutput& other) =
    default;
PreprocessedOutput::~PreprocessedOutput() = default;

QuickAnswersRequest::QuickAnswersRequest() = default;
QuickAnswersRequest::QuickAnswersRequest(const QuickAnswersRequest& other) =
    default;
QuickAnswersRequest::~QuickAnswersRequest() = default;
}  // namespace quick_answers
}  // namespace chromeos
