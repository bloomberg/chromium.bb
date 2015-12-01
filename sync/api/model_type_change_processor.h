// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_API_MODEL_TYPE_CHANGE_PROCESSOR_H_
#define SYNC_API_MODEL_TYPE_CHANGE_PROCESSOR_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "sync/api/entity_data.h"
#include "sync/base/sync_export.h"

namespace syncer {
class SyncError;
}  // namespace syncer

namespace syncer_v2 {

class MetadataChangeList;

// Interface used by the ModelTypeService to inform sync of local
// changes.
class SYNC_EXPORT ModelTypeChangeProcessor {
 public:
  ModelTypeChangeProcessor();
  virtual ~ModelTypeChangeProcessor();

  // Inform the processor of a new or updated entity.
  virtual void Put(const std::string& client_key,
                   const std::string& non_unique_name,
                   const sync_pb::EntitySpecifics& specifics,
                   MetadataChangeList* metadata_change_list) = 0;

  // Inform the processor of a deleted entity.
  virtual void Delete(const std::string& client_key,
                      MetadataChangeList* metadata_change_list) = 0;
};

}  // namespace syncer_v2

#endif  // SYNC_API_MODEL_TYPE_CHANGE_PROCESSOR_H_
