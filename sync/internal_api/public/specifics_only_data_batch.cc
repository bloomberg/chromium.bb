// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/specifics_only_data_batch.h"

namespace syncer_v2 {

void SpecificsOnlyDataBatch::Put(
    const std::string& client_key,
    scoped_ptr<sync_pb::EntitySpecifics> specifics) {
  // TODO(skym): Implementation.
}

}  // namespace syncer_v2
