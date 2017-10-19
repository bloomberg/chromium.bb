// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/inspector/ConsoleMessageStorage.h"

#include "core/inspector/ConsoleMessage.h"
#include "core/probe/CoreProbes.h"

namespace blink {

static const unsigned kMaxConsoleMessageCount = 1000;

ConsoleMessageStorage::ConsoleMessageStorage() : expired_count_(0) {}

void ConsoleMessageStorage::AddConsoleMessage(ExecutionContext* context,
                                              ConsoleMessage* message) {
  probe::consoleMessageAdded(context, message);
  DCHECK(messages_.size() <= kMaxConsoleMessageCount);
  if (messages_.size() == kMaxConsoleMessageCount) {
    ++expired_count_;
    messages_.pop_front();
  }
  messages_.push_back(message);
}

void ConsoleMessageStorage::Clear() {
  messages_.clear();
  expired_count_ = 0;
}

size_t ConsoleMessageStorage::size() const {
  return messages_.size();
}

ConsoleMessage* ConsoleMessageStorage::at(size_t index) const {
  return messages_[index].Get();
}

int ConsoleMessageStorage::ExpiredCount() const {
  return expired_count_;
}

void ConsoleMessageStorage::Trace(blink::Visitor* visitor) {
  visitor->Trace(messages_);
}

}  // namespace blink
