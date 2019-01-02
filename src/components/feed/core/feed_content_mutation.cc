// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/feed_content_mutation.h"

#include <utility>

#include "base/logging.h"
#include "components/feed/core/feed_content_operation.h"

namespace feed {

ContentMutation::ContentMutation() = default;

ContentMutation::~ContentMutation() = default;

void ContentMutation::AppendDeleteOperation(std::string key) {
  operations_list_.emplace_back(
      ContentOperation::CreateDeleteOperation(std::move(key)));
}

void ContentMutation::AppendDeleteAllOperation() {
  operations_list_.emplace_back(ContentOperation::CreateDeleteAllOperation());
}

void ContentMutation::AppendDeleteByPrefixOperation(std::string prefix) {
  operations_list_.emplace_back(
      ContentOperation::CreateDeleteByPrefixOperation(std::move(prefix)));
}

void ContentMutation::AppendUpsertOperation(std::string key,
                                            std::string value) {
  operations_list_.emplace_back(ContentOperation::CreateUpsertOperation(
      std::move(key), std::move(value)));
}

bool ContentMutation::Empty() {
  return operations_list_.empty();
}

ContentOperation ContentMutation::TakeFristOperation() {
  ContentOperation operation = std::move(operations_list_.front());
  operations_list_.pop_front();
  return operation;
}

}  // namespace feed
