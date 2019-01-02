// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/feed_journal_mutation.h"

#include <utility>

#include "components/feed/core/feed_journal_operation.h"

namespace feed {

JournalMutation::JournalMutation(std::string journal_name)
    : journal_name_(std::move(journal_name)) {}

JournalMutation::~JournalMutation() = default;

void JournalMutation::AddAppendOperation(std::string value) {
  operations_list_.emplace_back(
      JournalOperation::CreateAppendOperation(std::move(value)));
}

void JournalMutation::AddCopyOperation(std::string to_journal_name) {
  operations_list_.emplace_back(
      JournalOperation::CreateCopyOperation(std::move(to_journal_name)));
}

void JournalMutation::AddDeleteOperation() {
  operations_list_.emplace_back(JournalOperation::CreateDeleteOperation());
}

const std::string& JournalMutation::journal_name() {
  return journal_name_;
}

bool JournalMutation::Empty() {
  return operations_list_.empty();
}

JournalOperation JournalMutation::TakeFristOperation() {
  JournalOperation operation = std::move(operations_list_.front());
  operations_list_.pop_front();
  return operation;
}

}  // namespace feed
