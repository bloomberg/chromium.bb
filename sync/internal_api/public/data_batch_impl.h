// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_DATA_BATCH_IMPL_H_
#define SYNC_INTERNAL_API_PUBLIC_DATA_BATCH_IMPL_H_

#include <stddef.h>

#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "sync/api/data_batch.h"
#include "sync/api/entity_data.h"

namespace syncer_v2 {

// An implementation of DataBatch that's purpose is to transfer ownership of
// EntityData objects. As soon as this batch recieves the EntityData, it owns
// them until Next() is invoked, when it gives up ownerhsip. Because a vector
// is used internally, this impl is unaware when duplcate client_tags are used,
// and it is the caller's job to avoid this.
class SYNC_EXPORT DataBatchImpl : public DataBatch {
 public:
  DataBatchImpl();
  ~DataBatchImpl() override;

  // Takes ownership of the data tied to a given tag used for storage. Put
  // should be called at most once for any given client_tag. Data will be
  // readable in the same order that they are put into the batch.
  void Put(const std::string& client_tag, scoped_ptr<EntityData> entity_data);

  // DataBatch implementation.
  bool HasNext() const override;
  TagAndData Next() override;

 private:
  std::vector<TagAndData> tag_data_pairs_;
  size_t read_index_ = 0;

  DISALLOW_COPY_AND_ASSIGN(DataBatchImpl);
};

}  // namespace syncer_v2

#endif  // SYNC_INTERNAL_API_PUBLIC_DATA_BATCH_IMPL_H_
