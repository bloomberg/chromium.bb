// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/data_batch_impl.h"

namespace syncer_v2 {

DataBatchImpl::DataBatchImpl() {}

DataBatchImpl::~DataBatchImpl() {}

void DataBatchImpl::Put(const std::string& client_tag,
                        scoped_ptr<EntityData> specifics) {
  tag_data_pairs_.push_back(TagAndData(client_tag, std::move(specifics)));
}

bool DataBatchImpl::HasNext() const {
  return tag_data_pairs_.size() > read_index_;
}

TagAndData DataBatchImpl::Next() {
  DCHECK(HasNext());
  return std::move(tag_data_pairs_[read_index_++]);
}

}  // namespace syncer_v2
