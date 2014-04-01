// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/protocol_event_buffer.h"

#include "sync/internal_api/public/events/protocol_event.h"

namespace syncer {

const size_t ProtocolEventBuffer::kBufferSize = 6;

ProtocolEventBuffer::ProtocolEventBuffer()
  : buffer_deleter_(&buffer_) {}

ProtocolEventBuffer::~ProtocolEventBuffer() {}

void ProtocolEventBuffer::RecordProtocolEvent(const ProtocolEvent& event) {
  buffer_.push_back(event.Clone().release());
  if (buffer_.size() > kBufferSize) {
    ProtocolEvent* to_delete = buffer_.front();
    buffer_.pop_front();
    delete to_delete;
  }
}

ScopedVector<ProtocolEvent>
ProtocolEventBuffer::GetBufferedProtocolEvents() const {
  ScopedVector<ProtocolEvent> ret;
  for (std::deque<ProtocolEvent*>::const_iterator it = buffer_.begin();
       it != buffer_.end(); ++it) {
    ret.push_back((*it)->Clone().release());
  }
  return ret.Pass();
}

}  // namespace syncer
