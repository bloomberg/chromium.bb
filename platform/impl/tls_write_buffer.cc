// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/tls_write_buffer.h"

#include "platform/api/logging.h"
#include "platform/api/tls_connection.h"

namespace openscreen {
namespace platform {

TlsWriteBuffer::~TlsWriteBuffer() = default;

TlsWriteBuffer::TlsWriteBuffer(TlsWriteBuffer::Observer* observer)
    : observer_(observer) {
  OSP_DCHECK(observer_);
}

size_t TlsWriteBuffer::Write(const void* data, size_t len) {
  // TODO(rwkeane): Implement this method.
  OSP_UNIMPLEMENTED();
  return 0;
}

absl::Span<const uint8_t> TlsWriteBuffer::GetReadableRegion() {
  // TODO(rwkeane): Implement this method.
  OSP_UNIMPLEMENTED();
  return absl::Span<const uint8_t>();
}

void TlsWriteBuffer::Consume(size_t byte_count) {
  // TODO(rwkeane): Implement this method.
  OSP_UNIMPLEMENTED();
}

// NOTE: In follow-up CL, it will be useful to have this as its own method, so
// please tolerate it for now.
void TlsWriteBuffer::NotifyWriteBufferFill(double fraction) {
  observer_->NotifyWriteBufferFill(fraction);
}

}  // namespace platform
}  // namespace openscreen
