// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "SkDiscardableMemory_chrome.h"

SkDiscardableMemoryChrome::SkDiscardableMemoryChrome()
    : discardable_(new base::DiscardableMemory()) {
}

SkDiscardableMemoryChrome::~SkDiscardableMemoryChrome() {
}

bool SkDiscardableMemoryChrome::lock() {
  base::LockDiscardableMemoryStatus status = discardable_->Lock();
  switch (status) {
    case base::DISCARDABLE_MEMORY_SUCCESS:
      return true;
    case base::DISCARDABLE_MEMORY_PURGED:
      discardable_->Unlock();
      return false;
    default:
      discardable_.reset();
      return false;
  }
}

void* SkDiscardableMemoryChrome::data() {
  return discardable_->Memory();
}

void SkDiscardableMemoryChrome::unlock() {
  discardable_->Unlock();
}

bool SkDiscardableMemoryChrome::InitializeAndLock(size_t bytes) {
  return discardable_->InitializeAndLock(bytes);
}

SkDiscardableMemory* SkDiscardableMemory::Create(size_t bytes) {
  if (!base::DiscardableMemory::Supported()) {
    return NULL;
  }
  scoped_ptr<SkDiscardableMemoryChrome> discardable(
      new SkDiscardableMemoryChrome());
  if (discardable->InitializeAndLock(bytes))
    return discardable.release();
  return NULL;
}
