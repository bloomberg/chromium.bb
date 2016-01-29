// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_API_DATA_BATCH_H_
#define SYNC_API_DATA_BATCH_H_

#include <algorithm>
#include <string>
#include <utility>

#include "base/memory/scoped_ptr.h"
#include "sync/api/entity_data.h"
#include "sync/base/sync_export.h"

namespace syncer_v2 {

typedef std::pair<std::string, scoped_ptr<EntityData>> TagAndData;

// Interface used by the processor to read data requested from the service.
class SYNC_EXPORT DataBatch {
 public:
  DataBatch() {}
  virtual ~DataBatch() {}

  // Returns if the data batch has another pair or not.
  virtual bool HasNext() const = 0;

  // Returns a pair of storage tag and owned entity data object. Invoking this
  // method will remove the pair from the batch, and should not be called if
  // HasNext() returns false.
  virtual TagAndData Next() = 0;
};

}  // namespace syncer_v2

#endif  // SYNC_API_DATA_BATCH_H_
