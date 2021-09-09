// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/graph/node_data_describer_util.h"

#include "base/i18n/time_formatting.h"
#include "base/task/task_traits.h"

namespace performance_manager {

base::Value TimeDeltaFromNowToValue(base::TimeTicks time_ticks) {
  base::TimeDelta delta = base::TimeTicks::Now() - time_ticks;

  std::u16string out;
  bool succeeded = TimeDurationFormat(delta, base::DURATION_WIDTH_WIDE, &out);
  DCHECK(succeeded);

  return base::Value(out);
}

base::Value MaybeNullStringToValue(base::StringPiece str) {
  if (str.data() == nullptr)
    return base::Value();
  return base::Value(str);
}

base::Value PriorityAndReasonToValue(
    const execution_context_priority::PriorityAndReason& priority_and_reason) {
  base::Value priority(base::Value::Type::DICTIONARY);
  priority.SetStringKey(
      "priority", base::TaskPriorityToString(priority_and_reason.priority()));
  priority.SetPath("reason",
                   MaybeNullStringToValue(priority_and_reason.reason()));
  return priority;
}

}  // namespace performance_manager
