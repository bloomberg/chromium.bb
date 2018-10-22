// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/metrics/public/cpp/ukm_source_id.h"

#include "base/atomic_sequence_num.h"
#include "base/logging.h"
#include "base/rand_util.h"

namespace ukm {

SourceId AssignNewSourceId() {
  return base::UkmSourceId::New().ToInt64();
}

SourceId ConvertToSourceId(int64_t other_id, SourceIdType id_type) {
  return base::UkmSourceId::FromOtherId(other_id, id_type).ToInt64();
}

SourceIdType GetSourceIdType(SourceId source_id) {
  return base::UkmSourceId::FromInt64(source_id).GetType();
}

}  // namespace ukm
