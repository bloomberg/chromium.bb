// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/plugin_array_buffer_var.h"

#include <stdlib.h>

#include <limits>

namespace ppapi {

PluginArrayBufferVar::PluginArrayBufferVar(uint32 size_in_bytes)
    : buffer_(size_in_bytes) {
}

PluginArrayBufferVar::~PluginArrayBufferVar() {
}

void* PluginArrayBufferVar::Map() {
  if (buffer_.empty())
    return NULL;
  return &(buffer_[0]);
}

void PluginArrayBufferVar::Unmap() {
  // We don't actually use shared memory yet, so do nothing.
}

uint32 PluginArrayBufferVar::ByteLength() {
  return buffer_.size();
}

}  // namespace ppapi

