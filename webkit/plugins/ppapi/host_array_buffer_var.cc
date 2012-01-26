// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/host_array_buffer_var.h"

using ppapi::ArrayBufferVar;
using WebKit::WebArrayBuffer;

namespace webkit {
namespace ppapi {

HostArrayBufferVar::HostArrayBufferVar(uint32 size_in_bytes)
    : buffer_(WebArrayBuffer::create(size_in_bytes, 1 /* element_size */)) {
}

HostArrayBufferVar::HostArrayBufferVar(const WebArrayBuffer& buffer)
    : buffer_(buffer) {
}

HostArrayBufferVar::~HostArrayBufferVar() {
}

void* HostArrayBufferVar::Map() {
  return buffer_.data();
}

void HostArrayBufferVar::Unmap() {
  // We do not used shared memory on the host side. Nothing to do.
}

uint32 HostArrayBufferVar::ByteLength() {
  return buffer_.byteLength();
}

}  // namespace ppapi
}  // namespace webkit

