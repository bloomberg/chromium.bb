// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/memory_instrumentation/queued_request.h"

namespace memory_instrumentation {

QueuedRequest::Response::Response() {}
QueuedRequest::Response::~Response() {}

QueuedRequest::QueuedRequest(
    const base::trace_event::MemoryDumpRequestArgs& args,
    const RequestGlobalMemoryDumpInternalCallback& callback,
    bool add_to_trace)
    : args(args), callback(callback), add_to_trace(add_to_trace) {}

QueuedRequest::~QueuedRequest() {}

QueuedRequest::PendingResponse::PendingResponse(
    const mojom::ClientProcess* client,
    Type type)
    : client(client), type(type) {}

bool QueuedRequest::PendingResponse::operator<(
    const PendingResponse& other) const {
  return std::tie(client, type) < std::tie(other.client, other.type);
}

}  // namespace memory_instrumentation