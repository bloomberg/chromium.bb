// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_API_MODEL_TYPE_CHANGE_PROCESSOR_H_
#define SYNC_API_MODEL_TYPE_CHANGE_PROCESSOR_H_

#include "sync/base/sync_export.h"

namespace syncer_v2 {

// Interface used by the ModelTypeService to inform sync of local
// changes.
class SYNC_EXPORT ModelTypeChangeProcessor {
 public:
  ModelTypeChangeProcessor();
  virtual ~ModelTypeChangeProcessor();
};

}  // namespace syncer_v2

#endif  // SYNC_API_MODEL_TYPE_CHANGE_PROCESSOR_H_
