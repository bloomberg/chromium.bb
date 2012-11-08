// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_DATA_TYPE_DEBUG_INFO_LISTENER_H_
#define SYNC_INTERNAL_API_PUBLIC_DATA_TYPE_DEBUG_INFO_LISTENER_H_

#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/data_type_association_stats.h"

namespace syncer {

// Interface for the sync internals to listen to external sync events.
class DataTypeDebugInfoListener {
 public:
  // Notify the listener that a datatype's association has completed.
  virtual void OnDataTypeAssociationComplete(
      const DataTypeAssociationStats& association_stats) = 0;

  // Notify the listener that configuration has completed and sync has begun.
  virtual void OnConfigureComplete() = 0;
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_DATA_TYPE_DEBUG_INFO_LISTENER_H_
