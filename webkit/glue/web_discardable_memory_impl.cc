// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/web_discardable_memory_impl.h"

namespace webkit_glue {

WebDiscardableMemoryImpl::WebDiscardableMemoryImpl()
    : discardable_(new base::DiscardableMemory()) {
}

WebDiscardableMemoryImpl::~WebDiscardableMemoryImpl() {}

bool WebDiscardableMemoryImpl::InitializeAndLock(size_t size) {
  return discardable_->InitializeAndLock(size);
}

bool WebDiscardableMemoryImpl::lock() {
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

void* WebDiscardableMemoryImpl::data() {
  return discardable_->Memory();
}

void WebDiscardableMemoryImpl::unlock() {
  discardable_->Unlock();
}

}  // namespace webkit_glue
