// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_FULLY_SERIALIZED_METADATA_BATCH_H_
#define SYNC_INTERNAL_API_PUBLIC_FULLY_SERIALIZED_METADATA_BATCH_H_

#include <string>

#include "sync/api/metadata_batch.h"

namespace syncer_v2 {

// An implementation of MetadataBatch meant for services that have no need to
// understand any of the sync metadata, and simply store/read it in a fully
// serialized form.
class FullySerializedMetadataBatch : public MetadataBatch {
 public:
  // Incorperates the given metadata at the given storage key into the batch,
  // handling deserialization. Multiple invocations with the same client key is
  // currently undefined.
  void PutMetadata(const std::string& client_key,
                   const std::string& serialized_metadata);

  // Incorperates the given global metadata into the batch, handling
  // deserialization. Multiple invocations will use the last value.
  void PutGlobalMetadata(const std::string& serialized_global_metadata);
};

}  // namespace syncer_v2

#endif  // SYNC_INTERNAL_API_PUBLIC_FULLY_SERIALIZED_METADATA_BATCH_H_
