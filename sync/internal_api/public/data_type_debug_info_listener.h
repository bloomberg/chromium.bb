// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_DATA_TYPE_DEBUG_INFO_LISTENER_H_
#define SYNC_INTERNAL_API_PUBLIC_DATA_TYPE_DEBUG_INFO_LISTENER_H_

#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/data_type_association_stats.h"

namespace syncer {

struct SYNC_EXPORT DataTypeConfigurationStats {
  DataTypeConfigurationStats();
  ~DataTypeConfigurationStats();

  // The datatype that was configured.
  ModelType model_type;

  // Waiting time before downloading starts.
  base::TimeDelta download_wait_time;

  // Time spent on downloading data for first-sync data types.
  base::TimeDelta download_time;

  // Waiting time for association of higher priority types to finish before
  // asking association manager to associate.
  base::TimeDelta association_wait_time_for_high_priority;

  // Types configured before this type.
  ModelTypeSet high_priority_types_configured_before;
  ModelTypeSet same_priority_types_configured_before;

  // Association stats.
  DataTypeAssociationStats association_stats;
};

// Interface for the sync internals to listen to external sync events.
class DataTypeDebugInfoListener {
 public:
  // Notify the listener that configuration of one data type has completed.
  virtual void OnSingleDataTypeConfigureComplete(
      const DataTypeConfigurationStats& configuration_stats) = 0;

  // Notify the listener that configuration has completed and sync has begun.
  virtual void OnConfigureComplete() = 0;
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_DATA_TYPE_DEBUG_INFO_LISTENER_H_
