// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PROTOCOL_EVENT_BUFFER_H_
#define SYNC_INTERNAL_API_PROTOCOL_EVENT_BUFFER_H_

#include <deque>

#include "base/memory/scoped_vector.h"
#include "sync/base/sync_export.h"

namespace syncer {

class ProtocolEvent;

// A container for ProtocolEvents.
//
// Stores at most kBufferSize events, then starts dropping the oldest events.
class SYNC_EXPORT_PRIVATE ProtocolEventBuffer {
 public:
  static const size_t kBufferSize;

  ProtocolEventBuffer();
  ~ProtocolEventBuffer();

  // Records an event.  May cause the oldest event to be dropped.
  void RecordProtocolEvent(const ProtocolEvent& event);

  // Returns the buffered contents.  Will not clear the buffer.
  ScopedVector<ProtocolEvent> GetBufferedProtocolEvents() const;

 private:
  std::deque<ProtocolEvent*> buffer_;
  STLElementDeleter<std::deque<ProtocolEvent*> > buffer_deleter_;

  DISALLOW_COPY_AND_ASSIGN(ProtocolEventBuffer);
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PROTOCOL_EVENT_BUFFER_H_
