// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_SPECIFICS_ONLY_DATA_BATCH_H_
#define SYNC_INTERNAL_API_PUBLIC_SPECIFICS_ONLY_DATA_BATCH_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "sync/api/data_batch.h"
#include "sync/protocol/sync.pb.h"

namespace syncer_v2 {

// An implementation of DataBatch that only takes the simplest data from the
// service. The processor may need to help populate some fields.
class SpecificsOnlyDataBatch : public DataBatch {
 public:
  // Takes ownership of the specifics tied to a given key used for storage. Put
  // should be called at most once for any given client_key. Multiple uses of
  // the same client_key is currently undefined.
  void Put(const std::string& client_key,
           scoped_ptr<sync_pb::EntitySpecifics> specifics);
};

}  // namespace syncer_v2

#endif  // SYNC_INTERNAL_API_PUBLIC_SPECIFICS_ONLY_DATA_BATCH_H_
