// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/capturer/shared_buffer.h"

const bool kReadOnly = true;

namespace remoting {

SharedBuffer::SharedBuffer(uint32 size)
    : id_(0),
      size_(size) {
  shared_memory_.CreateAndMapAnonymous(size);
}

SharedBuffer::SharedBuffer(
    int id, base::SharedMemoryHandle handle, uint32 size)
    : id_(id),
      shared_memory_(handle, kReadOnly),
      size_(size) {
  shared_memory_.Map(size);
}

SharedBuffer::SharedBuffer(
    int id, base::SharedMemoryHandle handle, base::ProcessHandle process,
    uint32 size)
    : id_(id),
      shared_memory_(handle, kReadOnly, process),
      size_(size) {
  shared_memory_.Map(size);
}

SharedBuffer::~SharedBuffer() {
}

}  // namespace remoting
