// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_API_DATA_BATCH_H_
#define SYNC_API_DATA_BATCH_H_

#include "sync/base/sync_export.h"

namespace syncer_v2 {

// Interface used by the processor and service to communicate about data.
class SYNC_EXPORT DataBatch {
 public:
  DataBatch() {}
  virtual ~DataBatch() {}
};

}  // namespace syncer_v2

#endif  // SYNC_API_DATA_BATCH_H_
