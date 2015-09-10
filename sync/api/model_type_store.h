// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_API_MODEL_TYPE_STORE_H_
#define SYNC_API_MODEL_TYPE_STORE_H_

namespace syncer_v2 {

// Interface for store used by ModelTypeProcessor for persisting sync related
// data (entity state and data type state).
class ModelTypeStore {
 public:
  ModelTypeStore();
  virtual ~ModelTypeStore();
};

}  // namespace syncer_v2

#endif  // SYNC_API_MODEL_TYPE_STORE_H_
