// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/sessions/ordered_commit_set.h"

#include <algorithm>

#include "base/logging.h"

namespace syncer {
namespace sessions {

OrderedCommitSet::OrderedCommitSet() {}

OrderedCommitSet::~OrderedCommitSet() {}

void OrderedCommitSet::AddCommitItem(const int64 metahandle,
                                     ModelType type) {
  if (!HaveCommitItem(metahandle)) {
    inserted_metahandles_.insert(metahandle);
    metahandle_order_.push_back(metahandle);
    types_.push_back(type);
    types_in_list_.Put(type);
  }
}

void OrderedCommitSet::AddCommitItems(
    const std::vector<int64> metahandles,
    ModelType type) {
  for (std::vector<int64>::const_iterator it = metahandles.begin();
       it != metahandles.end(); ++it) {
    AddCommitItem(*it, type);
  }
}

void OrderedCommitSet::Append(const OrderedCommitSet& other) {
  for (size_t i = 0; i < other.Size(); ++i) {
    CommitItem item = other.GetCommitItemAt(i);
    AddCommitItem(item.meta, item.group);
  }
}

void OrderedCommitSet::AppendReverse(const OrderedCommitSet& other) {
  for (int i = other.Size() - 1; i >= 0; i--) {
    CommitItem item = other.GetCommitItemAt(i);
    AddCommitItem(item.meta, item.group);
  }
}

void OrderedCommitSet::Truncate(size_t max_size) {
  if (max_size < metahandle_order_.size()) {
    for (size_t i = max_size; i < metahandle_order_.size(); ++i) {
      inserted_metahandles_.erase(metahandle_order_[i]);
    }

    metahandle_order_.resize(max_size);
    types_.resize(max_size);
  }
}

void OrderedCommitSet::Clear() {
  inserted_metahandles_.clear();
  metahandle_order_.clear();
  types_.clear();
  types_in_list_.Clear();
}

OrderedCommitSet::CommitItem OrderedCommitSet::GetCommitItemAt(
    const size_t position) const {
  DCHECK(position < Size());
  CommitItem return_item = {metahandle_order_[position],
      types_[position]};
  return return_item;
}

bool OrderedCommitSet::HasBookmarkCommitId() const {
  for (size_t i = 0; i < types_.size(); i++) {
    if (types_[i] == BOOKMARKS)
      return true;
  }
  return false;
}

void OrderedCommitSet::operator=(const OrderedCommitSet& other) {
  inserted_metahandles_ = other.inserted_metahandles_;
  metahandle_order_ = other.metahandle_order_;
  types_ = other.types_;
}

}  // namespace sessions
}  // namespace syncer

