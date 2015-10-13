// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_API_MODEL_TYPE_SERVICE_H_
#define SYNC_API_MODEL_TYPE_SERVICE_H_

#include "sync/base/sync_export.h"

namespace syncer_v2 {

// Interface implemented by model types to receive updates from sync via the
// SharedModelTypeProcessor. Provides a way for sync to update the data and
// metadata for entities, as well as the model type state.
class SYNC_EXPORT ModelTypeService {
 public:
  ModelTypeService();
  virtual ~ModelTypeService();
};

}  // namespace syncer_v2

#endif  // SYNC_API_MODEL_TYPE_SERVICE_H_
