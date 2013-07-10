// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/renderer/compositor_bindings/web_external_bitmap_impl.h"

#include "base/memory/shared_memory.h"

namespace webkit {

namespace {

SharedMemoryAllocationFunction g_memory_allocator;

}  // namespace

void SetSharedMemoryAllocationFunction(
    SharedMemoryAllocationFunction allocator) {
  g_memory_allocator = allocator;
}

WebExternalBitmapImpl::WebExternalBitmapImpl() {}

WebExternalBitmapImpl::~WebExternalBitmapImpl() {}

void WebExternalBitmapImpl::setSize(WebKit::WebSize size) {
  if (size != size_) {
    shared_memory_ = g_memory_allocator(size.width * size.height * 4);
    size_ = size;
  }
}

WebKit::WebSize WebExternalBitmapImpl::size() {
  return size_;
}

uint8* WebExternalBitmapImpl::pixels() {
  return static_cast<uint8*>(shared_memory_->memory());
}

}  // namespace webkit
